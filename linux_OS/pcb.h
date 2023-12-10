#ifndef PCB_H
#define PCB_H

#include <unistd.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <stdbool.h>
#include "parser.h"
#include "queue.h"

enum status {
    READY,
    RUNNING,
    BLOCKED,
    STOPPED,
    TERMINATED,
    FINISHED,
    ZOMBIE,
    ORPHAN
};

struct Pcb
{
    ucontext_t *uc;
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    int priority;
    int status;
    int stateChangeType; // 0 if no change; 1 if terminated normally; 2 if stopped; 3 if terminated by a signal; 4 if continued
    int fd[2];
    struct Pcb *next;
    struct Pcb *prev;
    struct Pcb *next_child;
    struct Pcb *prev_child;
    struct queue *children_pcb;
    struct queue *zombies;
    struct Pcb *parent_pcb;
    int number_of_children;
    int exited_child;
    char *name;
    bool sleeping;
    unsigned int sleep_time;
    bool waiting;
    pid_t waiting_on;
    struct parsed_command *cmd;
    bool state_change;
    struct Pcb *changed_child_pcb;
    bool has_tc;
    bool fg;
    bool reading;
};

struct Pcb *create_pcb(ucontext_t *uc, pid_t pid, pid_t ppid, pid_t pgid, int priority, int status, struct parsed_command *cmd);
struct Pcb *create_pcb_no_context(pid_t pid, pid_t ppid, pid_t pgid, int priority, int status);
void free_pcb(struct Pcb *pcb);
void add_child_to_parent_q(struct Pcb *parent_pcb, struct Pcb *child_pcb);

/// @brief Removes child process from the list of children of the parent process but does not free the memory
/// @param parent_pcb PCB of parent process
/// @param child_pcb PCB of child process 
void remove_child_pcb(struct Pcb *parent_pcb, struct Pcb *child_pcb);

/// @brief Removes child process from the list of children of the parent process and frees associated memory
/// @param parent_pcb 
/// @param child_pcb 
void delete_child_pcb(struct Pcb *parent_pcb, struct Pcb *child_pcb);

/// @brief Add the child PCB to parent PCB's zombie queue
/// @param parent_pcb 
/// @param child_pcb 
void add_child_to_parent_zombies(struct Pcb *parent_pcb, struct Pcb *child_pcb);

#endif