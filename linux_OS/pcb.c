#include "pcb.h"
#include "kernel.h"
#include <stdio.h>

struct Pcb *create_pcb(ucontext_t *uc, pid_t pid, pid_t ppid, pid_t pgid, int priority, int status, struct parsed_command *cmd)
{
    struct Pcb *new_pcb = (struct Pcb *)malloc(sizeof(struct Pcb));
    if (new_pcb == NULL)
    {
        puts("PCB MALLOC 1 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->next = NULL;
    new_pcb->uc = uc;
    new_pcb->pid = pid;
    new_pcb->ppid = ppid;
    new_pcb->pgid = pgid;
    new_pcb->priority = priority;
    new_pcb->status = status;
    new_pcb->stateChangeType = 0;
    new_pcb->number_of_children = 0;
    new_pcb->sleep_time = 0;
    new_pcb->children_pcb = (struct queue *)malloc(sizeof(struct queue));
    if (new_pcb->children_pcb == NULL)
    {
        puts("PCB MALLOC 2 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->zombies = (struct queue *)malloc(sizeof(struct queue));
    if (new_pcb->zombies == NULL)
    {
        puts("PCB MALLOC 3 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->exited_child = 0;
    new_pcb->next_child = NULL;
    new_pcb->sleeping = false;
    new_pcb->prev = NULL;
    new_pcb->prev_child = NULL;
    new_pcb->cmd = cmd;
    new_pcb->state_change = false;
    new_pcb->changed_child_pcb = NULL;
    new_pcb->has_tc = true;
    return new_pcb;
}

struct Pcb *create_pcb_no_context(pid_t pid, pid_t ppid, pid_t pgid, int priority, int status)
{
    struct Pcb *new_pcb = (struct Pcb *)malloc(sizeof(struct Pcb));
    if (new_pcb == NULL)
    {
        puts("PCB MALLOC 1 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->next = NULL;
    new_pcb->pid = pid;
    new_pcb->ppid = ppid;
    new_pcb->pgid = pgid;
    new_pcb->priority = priority;
    new_pcb->status = status;
    new_pcb->stateChangeType = 0;
    new_pcb->number_of_children = 0;
    new_pcb->sleep_time = 0;
    new_pcb->children_pcb = create_queue();
    if (new_pcb->children_pcb == NULL)
    {
        puts("PCB MALLOC 2 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->zombies = create_queue();
     if (new_pcb->zombies == NULL)
    {
        puts("PCB MALLOC 3 failed");
        exit(EXIT_FAILURE);
    }
    new_pcb->exited_child = 0;
    new_pcb->next_child = NULL;
    new_pcb->sleeping = false;
    new_pcb->prev = NULL;
    new_pcb->prev_child = NULL;
    new_pcb->waiting = false;
    new_pcb->state_change = false;
    new_pcb->changed_child_pcb = NULL;
    new_pcb->has_tc = true;
    return new_pcb;
}
void free_pcb(struct Pcb *p)
{
    // fprintf(stderr, "freeing pcb calling pid: %d\n", get_active_pcb()->pid);
    // fprintf(stderr, "freeing pcb with pid: %d\n", p->pid);
    // puts("free_pcb 1");
    free(p->children_pcb);
    // puts("free_pcb 2");
    free(p->zombies);
    // puts("free_pcb 3");
    free(p->uc->uc_stack.ss_sp);
    free(p->uc);
    // puts("free_pcb 4");
    // puts(p->name);
    free(p->name);
    // puts("free_pcb 5");
    free(p);
    // puts("free_pcb 6");
}

void add_child_to_parent_q(struct Pcb *parent_pcb, struct Pcb *child_pcb)
{
    struct queue *q = parent_pcb->children_pcb;
    if (q->size == 0) {
        q->head = child_pcb;
    } 
    else {
        q->tail->next_child = child_pcb;
        child_pcb->prev_child = q->tail;
    }
    q->tail = child_pcb;
    q->size++;
    parent_pcb->number_of_children++;
}

bool check_if_child_pcb_in_queue(struct queue *q, struct Pcb *p)
{
     struct Pcb *t = q->head;
     while (t != NULL) {
        if (t == p) return true;
        t = t->next_child;
     }
     return false;
}

void remove_child_pcb(struct Pcb *parent_pcb, struct Pcb *child_pcb) 
{
    struct queue *q = parent_pcb->children_pcb;
    if (check_if_child_pcb_in_queue(q, child_pcb) == false) return;
    if (q->size == 0) {
        // TODO: set errno
        return;
    }
    if (q->size > 1) {
        if (child_pcb == q->head) {
            q->head = child_pcb->next_child;
            q->head->prev_child = NULL;
        }
        else if (child_pcb == q->tail) {
            q->tail = child_pcb->prev_child;
            q->tail->next_child = NULL;
            // q->tail->next = NULL;
        }
        else {
            child_pcb->prev_child->next_child = child_pcb->next_child;
            child_pcb->next_child->prev_child = child_pcb->prev_child;
        }
    }
    else if (q->size == 1) {
        q->tail = NULL;
        q->head = NULL;
    }
    child_pcb->next_child = NULL;
    child_pcb->prev_child = NULL;
    q->size--;
    parent_pcb->number_of_children--;
}


void delete_child_pcb(struct Pcb *parent_pcb, struct Pcb *child_pcb) 
{
    struct queue *q = parent_pcb->children_pcb;
    if (q->size == 0) {
        // TODO: set errno
        return;
    }
    if (q->size > 1) {
        if (child_pcb == q->head) {
            q->head = child_pcb->next_child;
            q->head->prev_child = NULL;
        }
        else if (child_pcb == q->tail) {
            q->tail = child_pcb->prev_child;
            q->tail->next_child = NULL;
            // q->tail->next = NULL;
        }
        else {
            child_pcb->prev_child->next_child = child_pcb->next_child;
            child_pcb->next_child->prev_child = child_pcb->prev_child;
        }
    }
    else if (q->size == 1) {
        q->tail = NULL;
        q->head = NULL;
    }
    free_pcb(child_pcb);
    q->size--;
    parent_pcb->number_of_children--;
}

void add_child_to_parent_zombies(struct Pcb *parent_pcb, struct Pcb *child_pcb)
{
    // fprintf(get_log_fp(), "[%3d]\t\tZOMBIE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), child_pcb->pid, child_pcb->priority, child_pcb->name);
    child_pcb->next = NULL;
    struct queue *q = parent_pcb->zombies;
    if (q->head == NULL) {
        q->head = child_pcb;
    } 
    else {
        q->tail->next = child_pcb;
        child_pcb->prev = q->tail;
    }
    q->tail = child_pcb;
    q->size++;
}