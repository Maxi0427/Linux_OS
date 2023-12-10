#include "queue.h"
#include <stdio.h>
#include "kernel.h"

struct queue *create_queue()
{
    struct queue *q = (struct queue *)malloc(sizeof(struct queue));
    if (q == NULL) return NULL;
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

// void push(struct queue *q, struct Pcb *new_pcb)
// {
//     if (q->head == NULL) {
//         q->head = new_pcb;
//     } 
//     else {
//         q->tail->next = new_pcb;
//     }
//     q->tail = new_pcb;
//     q->size++;
// }

void push(struct queue *q, struct Pcb *new_pcb)
{
    new_pcb->next = NULL;
    new_pcb->prev = NULL;
    if (q->size == 0) { // Empty queue
        q->head = new_pcb;
    } 
    else {
        q->tail->next = new_pcb;
        new_pcb->prev = q->tail;
    }
    q->tail = new_pcb;
    q->size++;
}

// struct Pcb *pop(struct queue *q)
// {
//     if (q->size == 0) return NULL;
//     if (q->size == 1) q->tail = NULL;
//     struct Pcb *p = q->head;
//     q->head = p->next;
//     q->size--;
//     return p; 
// }

void push_to_front(struct queue *q, struct Pcb *new_pcb)
{
    if (q->size == 0) { // Empty queue 
        q->tail = new_pcb;
    } 
    else {
        q->head->prev = new_pcb;
        new_pcb->next = q->head;
    }
    q->head = new_pcb;
    q->size++;
}

/**
 * @brief Pops and returns the Pcb struct pointer of the process at the front of the queue
 * @param q Queue of PCBs
*/
struct Pcb *pop(struct queue *q) 
{
    if (q->size == 0) return NULL;
    if (q->size == 1) q->tail = NULL;
    struct Pcb *p = q->head;
    q->head = p->next;
    if (q->head != NULL) q->head->prev = NULL;
    q->size--;
    if (q->size == 0) {
        q->head = NULL;
    }
    return p; 
}

struct Pcb *pop_child(struct queue *q) 
{
    if (q->size == 0) return NULL;
    if (q->size == 1) q->tail = NULL;
    struct Pcb *p = q->head;
    q->head = p->next_child;
    if (q->head != NULL) q->head->prev_child = NULL;
    q->size--;
    if (q->size == 0) {
        q->head = NULL;
    }
    return p; 
}

void print_queue(struct queue *q)
{
    struct Pcb *p = q->head;
    while (p != NULL) {
        fprintf(stderr, "PID: %d\t\tPPID: %d\n", p->pid, p->ppid); //Print as required 
        p = p->next;
    }
}

void print_queue_info(struct queue *q) {
    if (q == NULL) return;
    // May replace print_queue if possible
    struct Pcb *p = q->head;
    while (p != NULL) {
        // fprintf(stderr, "PID: %d\n", p->pid);
        // fprintf(stderr, "PPID: %d\n", p->ppid);
        // fprintf(stderr, "PRI: %d\n", p->priority);
        // fprintf(stderr, "STAT: %d\n", p->status);
        // fprintf(stderr, "CMD: %s\n", p->name);

        fprintf(stderr, "PID: %d\t\tPPID: %d\t\tPRI: %i\t\tSTAT: %i\t\tCMD: %s\n", 
            p->pid, p->ppid, p->priority, p->status, p->name); //Print as required 
        p = p->next;
    }
}


void free_queue(struct queue *q)
{
    struct Pcb *p;
    int size = q->size;
    for (int i = 0; i < size; i++)
    {
        p = pop(q);
        free_pcb(p);
    }
}

void free_child_queue(struct queue *q)
{
    struct Pcb *p;
    int size = q->size;
    for (int i = 0; i < size; i++)
    {
        p = pop_child(q);
        free_pcb(p);
    }
}

// void free_queue(struct queue *q)
// {
//     struct Pcb *p = q->head;
//     while (p != NULL) {
//         struct Pcb *t = p->next;
//         free_pcb(p);
//         p = t;
//     }
//     // free(q);
// }

// void free_child_queue(struct queue *q)
// {
//     struct Pcb *p = q->head;
//     while (p != NULL) {
//         struct Pcb *t = p->next_child;
//         free_pcb(p);
//         p = t;
//     }
//     // free(q);
// }

struct Pcb *front(struct queue *q)
{
    return q->head;
}

bool is_empty(struct queue *q) 
{
    return q->size == 0;
}

struct Pcb *get_pcb_with_pid(struct queue *q, pid_t pid)
{
    struct Pcb *h = q->head;
    while (h != NULL) {
        if (h->pid == pid)
            return h;
        h = h->next;
    }
    return NULL;
}

// Get (update) a queue of all pcbs with a given pgid
void get_pcbs_with_pgid_in_queue(struct queue *q, pid_t pgid, struct queue *result)
{
    struct Pcb *h = q->head;
    while (h != NULL) {
        if (h->pgid == pgid)
            push(result, h);
        h = h->next;
    }
}

// void push_child(struct Pcb *parent_pcb, struct Pcb *child_pcb)
// {
//     struct queue *q = parent_pcb->children_pcb;
//     if (q->head == NULL) {
//         q->head = child_pcb;
//     } 
//     else {
//         q->tail->next_child = child_pcb;
//     }
//     q->tail = child_pcb;
//     q->size++;
//     parent_pcb->number_of_children++;
// }

// void add_to_blocked_list(struct Pcb *p) {
//     p->next = blocked_list;
//     blocked_list = p;
// }


bool check_if_pcb_in_queue(struct queue *q, struct Pcb *p)
{
     struct Pcb *t = q->head;
     while (t != NULL) {
        if (t == p) return true;
        t = t->next;
     }
     return false;
}

void remove_pcb(struct queue *q, struct Pcb *p) 
{
    if (check_if_pcb_in_queue(q, p) == false) return;
    if (q->size == 0) {
        // TODO: set errno
        return;
    }
    if (q->size > 1) {
        if (p == q->head) {
            q->head = p->next;
            q->head->prev = NULL;
        }
        else if (p == q->tail) {
            q->tail = p->prev;
            q->tail->next = NULL;
        }
        else {
            p->prev->next = p->next;
            p->next->prev = p->prev;
        }
    }
    else if (q->size == 1) {
        q->tail = NULL;
        q->head = NULL;
    }
    p->next = NULL;
    p->prev = NULL;
    q->size--;
}

struct Pcb *get_child_pcb_with_pid(struct queue *q, pid_t pid)
{
    struct Pcb *h = q->head;
    while (h != NULL) {
        if (h->pid == pid)
            return h;
        h = h->next_child;
    }
    return NULL;
}
