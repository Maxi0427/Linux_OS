#ifndef USER_H
#define USER_H
#include "f.h"
#include "pcb.h"
#include "queue.h"
#include "errors.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>
#include <signal.h>

// extern struct Pcb *current_pcb;
// extern struct queue *q;
// extern struct queue *queues[3];

enum S_SIGNALS {
    S_SIGTERM,
    S_SIGCONT,
    S_SIGSTOP,
    S_SIGCHLD
};


enum CHANGE {
    CHANGED  ,
    NO_CHANGE
};

bool W_WIFEXITED(int status);
bool W_WIFSTOPPED(int status);
bool W_WIFSIGNALED(int status);

pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1, bool fg);
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);

/**
 * @brief sends the signal sig to the thread referenced by pid.
 * @param pid pid of process to which we wish to send signal
 * @param sig signal to be sent to process referenced by pid
 * @return It returns 0 on success, -1 on error.
 */
int p_kill(pid_t pid, int sig);

/// @brief Exits thread unconditionally
/// @param  None
void p_exit(void);
int p_nice(pid_t pid, int priority);
void p_sleep(unsigned int ticks);
void sleeps(char *ticks);
void echo(char **argStr,int writefd);
void kill_as_process(pid_t pid, int sig);
void print_all_process_info();

/**
 * @brief Dummy function for testing
 * @param str
*/
void dummy(char *str);

/**
 * @brief Creates the shell process
*/
void spawn_shell();

void interactive_shell();

static bool has_terminal_control = true;



/**
 * @brief Gives terminal control to process specified by pid
 * @param pid Process PID 
*/
int p_tcset(pid_t pid);

/**
 * @brief Returns active process's PID
 * @return PID of current process
*/
pid_t p_getpid(int opt);

static sigset_t intmask;

void k_block_sigset(sigset_t *mask);
void k_unblock_sigset(sigset_t *mask);

/**
 * @brief Saves active context and switches to scheduler context
*/
void p_switch_to_scheduler();

void p_boot_kernel();

/// @brief zombify and orphanify
void zombie_child();
void zombify();
void orphan_child();
void orphanify();

int custom_read(int fd, char *buf, ssize_t n);

/**
 * @brief busy process which is basically an infinite loop
*/
void busy_process();

/**
 * @brief Used for nice command
 * @param priority The nice value to be set for given command
*/
void p_nice_next(int priority);

/**
 * @brief Used for logout command and Ctrl-D exit
*/
void p_shutdown();
#endif
