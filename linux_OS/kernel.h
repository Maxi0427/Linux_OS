#ifndef KER_H
#define KER_H
#include "queue.h"
#include "user.h"
#include "utils.h"
#include <stdio.h>
enum HOW_ENDED {
    TIMED_OUT,
    FIN_RUN
};


static sigset_t intmask;
void idle_process();
struct Pcb *get_active_pcb();
int get_counter();
void boot_kernel();
void setup_log();
FILE *get_log_fp();
int get_num_of_ready_processes();
struct Pcb *k_process_create(struct Pcb *parent);

/**
 * @brief Delivers signal to specified process
 * @param process The process being signaled
 * @param signal The kind of signal being delivered
 * @return CHANGE 0, NO CHANGE 1 where CHANGE indicates that a process changed state as a result of the signal
*/
int k_process_kill(struct Pcb *process, int signal);
void k_process_cleanup(struct Pcb *process);
void k_sleep(unsigned int ticks);
struct queue *get_queue_with_priority(int priority);
struct queue *get_blocked_queue();
struct Pcb *get_pcb(pid_t pi); // Check all queues for the pcb with given pid

ucontext_t *get_scheduler_context(void);

ucontext_t *get_shell_context(void);
// void initialise_queues();
// void setup_idle_process();
// void init_scheduler_context();


void make_context(ucontext_t *ucp, void (*func)(), int argc, char *argv[], bool link_to_scheduler);
struct queue *get_pcbs_with_pgid(pid_t pgid);

/**
 * @brief Getter for clock ticks
*/
int get_clock_ticks();

/**
 * @brief Saves active context and switches to scheduler context
*/
void switch_to_scheduler();




/**
 * @brief Blocks the process 
 * @param p Refers to the PCB of the process to be blocked 
 * @param cp Optional parameter referring to child process being waited on leading to parent getting blocked
*/
void k_process_block(struct Pcb *p, struct Pcb *cp);

/**
 * @brief Kernel side assist function for p_exit
 * @param calling_pcb PCB of the process calling p_exit
*/
void k_exit(struct Pcb *calling_pcb);

/**
 * @brief Kernel side function used for setting terminal control
 * @param pid PID of the process being given terminal control
 * @param tc_pid Used for tracking which process (with this PID) has terminal control
 * @return 0 on success and -1 on error
*/
int k_tcset(pid_t pid);


pid_t k_waitpid(struct Pcb *calling_pcb, struct Pcb *child_pcb, int *wstatus, pid_t pid, bool nohang);


void wait_log(struct Pcb *calling_pcb);
static pid_t current_pid = 0;

void k_boot_kernel();

void set_tc_pid(pid_t pid);
pid_t get_tc_pid();

struct Pcb *get_fg_pcb();
void set_fg_pcb(struct Pcb *p);

struct Pcb *k_process_create_start(void (*func)(), char *argv[], int fd0, int fd1, bool fg);

/**
 * @brief Sets the priority of the process 
 * @param pid PID of the process 
 * @param priority Priority you want to set for the process
 * @return 0 on success, -1 on error
*/
int k_nice(pid_t pid, int priority);

void k_shutdown();

void k_change_priority_for_new(int priority);

void handle_sigtstp(int signal);
void handle_sigint(int signal);
#endif