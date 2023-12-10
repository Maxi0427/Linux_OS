#ifndef Q_H
#define Q_H

#include "pcb.h"
#include <signal.h>
struct queue
{
    struct Pcb *head;
    struct Pcb *tail;
    int size;
};

/// @brief Allocates memory for a queue of PCBs
/// @return Returns the pointer to the queue struct on success and NULL on failure
struct queue *create_queue();

extern struct Pcb *blocked_list;

void push(struct queue *q, struct Pcb *new_pcb);

struct Pcb *pop(struct queue *q); // Removes and returns front element
void print_queue(struct queue *q);
void print_queue_info(struct queue *q);

/// @brief Frees all PCBs in the queue 
/// @param q Reference to queue
void free_queue(struct queue *q);

/// @brief Frees all PCBs in the children queue 
/// @param q Reference to queue
void free_child_queue(struct queue *q);


/**
 * @brief Returns pointer to front element
 * @param q Reference to queue
*/
struct Pcb *front(struct queue *q); 


bool is_empty(struct queue *q);

struct Pcb *get_pcb_with_pid(struct queue *q, pid_t pid);

/**
 * @brief Looks for the PCB in children queue of parent PCB
 * @param q Children queue
 * @param pid PID of child process
*/
struct Pcb *get_child_pcb_with_pid(struct queue *q, pid_t pid);


void get_pcbs_with_pgid_in_queue(struct queue *q, pid_t pgid, struct queue *result);
// void push_child(struct Pcb *parent_pcb, struct Pcb *child_pcb);

void add_to_blocked_list(struct Pcb *p);

/// @brief Removes process from the queue but does not free the memory
/// @param q Queue from which process is to be removed
/// @param p Pointer to the PCB of the process which is to be removed from queue q
void remove_pcb(struct queue *q, struct Pcb *p); 
#endif
