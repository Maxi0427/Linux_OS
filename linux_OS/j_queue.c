#include "j_queue.h"
#include <stdio.h>

struct j_queue *create_j_queue()
{
    struct j_queue *q = (struct j_queue *)malloc(sizeof(struct j_queue));
    if (q == NULL) return NULL;
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

void push_job(struct j_queue *q, struct Job *j)
{
    j->next = NULL;
    j->prev = NULL;
    if (q->size == 0) { // Empty queue
        q->head = j;
    } 
    else {
        q->tail->next = j;
        j->prev = q->tail;
    }
    q->tail = j;
    q->size++;
}

struct Job *pop_job(struct j_queue *q) 
{
    if (q->size == 0) return NULL;
    if (q->size == 1) q->tail = NULL;
    struct Job *p = q->head;
    q->head = p->next;
    if (q->head != NULL) q->head->prev = NULL;
    q->size--;
    if (q->size == 0) {
        q->head = NULL;
    }
    return p; 
}

void free_j_queue(struct j_queue *q)
{
    struct Job *j;
    int size = q->size;
    for (int i = 0; i < size; i++)
    {
        j = pop_job(q);
        free_job(j);
    }
    free(q);
}

struct Job *find_job_with_id(struct j_queue *q, int job_id)
{
    struct Job *h = q->head;
    while (h != NULL) {
        if (h->job_id == job_id)
            return h;
        h = h->next;
    }
    return NULL;
}

struct Job *find_last_stopped_job(struct j_queue *jobs_list)
{
    struct Job *t = jobs_list->head;
    struct Job *last_stopped_job = NULL;
    while (t != NULL)
    {
        if (t->status == J_STOPPED)
        {
            if (last_stopped_job == NULL)
                last_stopped_job = t;
            else if (t->job_id > last_stopped_job->job_id)
                last_stopped_job = t;
        }
        t = t->next;
    }
    return last_stopped_job;
}

void remove_job(struct j_queue *q, struct Job *p) 
{
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
    q->size--;
}

void print_j_list(struct j_queue *ll)
{
    // prints the linked list to stderr
    // numbering jobs by their position in the list

    // fprintf(stderr, "LL size: %i\n", ll->size);
    struct Job *j = ll->head;

    int i = 0;
    while (j != NULL)
    {
        if (j->status == J_STOPPED)
        {
            fprintf(stderr, "[%d] %s (stopped)\n", j->job_id, j->rawCmd);
        }
        else if (j->status == J_RUNNING)
        {
            fprintf(stderr, "[%d] %s (running)\n", j->job_id, j->rawCmd);
        }
        // else if (j->status == J_FINISHED)
        // {
        //     fprintf(stderr, "[%d] %s (finished)\n", j->job_id, j->rawCmd);
        // }
        j = j->next;
        i++;
    }
}