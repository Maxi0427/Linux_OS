#ifndef JQ_H
#define JQ_H

#include "Job.h"

struct j_queue
{
    struct Job *head;
    struct Job *tail;
    int size;
};

struct j_queue *create_j_queue();
void push_job(struct j_queue *q, struct Job *j);
struct Job *pop_job(struct j_queue *q);
void free_j_queue(struct j_queue *q);
struct Job *find_job_with_id(struct j_queue *q, int job_id);
struct Job *find_last_stopped_job(struct j_queue *jobs_list);
void remove_job(struct j_queue *q, struct Job *p);
void print_j_list(struct j_queue *ll);
#endif