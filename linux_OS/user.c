#include "user.h"
#include "kernel.h"


bool W_WIFEXITED(int status) {
    return (status == FINISHED);
}
bool W_WIFSTOPPED(int status) {
    return (status == STOPPED);
}
bool W_WIFSIGNALED(int status) {
    return (status == TERMINATED);
}

bool W_WIFCONTINUED(int status) {
    return status == 4;
}
// Continued: 4
// No change: 0

/*
 * DESCRIPTION   Sets the priority of the thread pid to priority.
 */
int p_nice(pid_t pid, int priority)
{
    if (k_nice(pid, priority) == -1)
    {
        // p_perror("bad nice");
        return -1;
    }
    return 0;
}

/*
 * DESCRIPTION   Sets the calling process to blocked until ticks of the system clock elapse,
                 and then sets the thread to running.
 */
void p_sleep(unsigned int ticks)
{
    k_sleep(ticks);
}

void sleeps(char *ticks)
{
    p_sleep(atoi(ticks));
    puts("In sleep about to p_exit");
    p_exit();
}

void script(char **){

}
void echo(char **argStr,int writefd) {
    // fprintf(stderr, "%s\n", argStr);
    //puts(argStr);
    
    int opfd=1;
    if ((writefd)!=1)
        opfd=(writefd);
    else 
        opfd=f_STDOUT_FILENO;
    int i=1;
    int j=0;
    int length=0;
    while (argStr[i]!=  NULL)
    {
        while(argStr[i][j]!=NULL)
        {
            length+=1;
            j+=1;
        }
        j=0;
        
        //printf("writefd: %d\n",argStr[i]);
        f_write(opfd, argStr[i], length);
        length=0;
        i+=1;
    }
    f_write(opfd,"\n", 2);
    
    p_exit();
}

void kill_as_process(pid_t pid, int sig) 
{
    int stat = p_kill(pid, sig);
    if (stat == -1) {
        p_perror("Illegal kill");
    }
    else if (sig == S_SIGCONT && stat == NO_CHANGE) {
        p_perror("Not allowed");
    }
    p_exit();
}


void print_all_process_info() {
    print_all_queues_info();
    p_exit();
}

// void p_sleep(unsigned int ticks)
// {
//     if (!current_pcb) {
//         return;
//     }

//     // Set the current process as blocked
//     current_pcb->status = BLOCKED;
//     current_pcb->sleeping = true;
//     current_pcb->sleep_time = ticks;

//     // Remove the active process from the current priority queue
//     remove_pcb(queues[current_pcb->priority + 1], current_pcb);

//     // Add the active process to the blocked list
//     add_to_blocked_list(current_pcb);

//     // Save the active process's context and switch to the scheduler context
//     ucontext_t *saved_context = current_pcb->uc;
//     current_pcb = NULL;
//     swapcontext(saved_context, get_scheduler_context());
// }

/*
 * DESCRIPTION   Forks a new thread that retains most of the attributes of the parent thread (see k_process_create). 
                 Once the thread is spawned, it executes the function referenced by func with its argument array argv. 
                 fd0 is the file descriptor for the input file, and fd1 is the file descriptor for the output file.
 * RETURNS       the pid of the child thread on success
 */
pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1, bool fg)
{
    // puts("p_spawn");
    struct Pcb *child_pcb = k_process_create_start(func, argv, fd0, fd1, fg);
    // fprintf(stderr, "p_spawn created child name: %s\n", child_pcb->name);
    switch_to_scheduler();
    if (get_active_pcb() == child_pcb) return 0;
    return child_pcb->pid;
}

void wait_log(struct Pcb *calling_pcb)
{
    fprintf(get_log_fp(), "[%3d]\t\tWAITED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), calling_pcb->pid, calling_pcb->priority, calling_pcb->name);
    fflush(get_log_fp());
}

pid_t p_wait_assist(pid_t pid, int *wstatus, bool nohang, struct Pcb *calling_pcb, struct Pcb *child_pcb)
{
    pid_t p1 = -1;
    if (child_pcb == NULL)
    {
        child_pcb = get_child_pcb_with_pid(calling_pcb->children_pcb, pid);
        if (child_pcb == NULL)
        {
            child_pcb = get_pcb_with_pid(calling_pcb->zombies, pid);
        }
        if (child_pcb == NULL)
        {
            // p_perror: invalid pid
        }
    }
    while (!nohang)
    {
        p1 = k_waitpid(calling_pcb, child_pcb, wstatus, pid, nohang);
        if (p1 == 0) // calling process unblocked
            continue;
        if (p1 == pid)
            return pid;
        else
        {
            // p_perror something
            return -1;
        }
    }
    if (nohang)
    {
        // dprintf(STDERR_FILENO, "p_w_as waiting on child: %d\n", pid); 
        p1 = k_waitpid(calling_pcb, child_pcb, wstatus, pid, nohang);
        return p1;
    }
    return p1;
}

/*
 * DESCRIPTION   Sets the calling thread as blocked (if nohang is false) until a child of the calling thread changes state. 
                 It is similar to Linux waitpid(2). If nohang is true, p_waitpid does not block but returns immediately.
 * RETURNS       The pid of the child which has changed state on success
 */

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang)
{
    /*
       pid < -1: wait for any child process whose process group ID is equal to the absolute value of pid
       pid == -1: wait for any child process
       pid == 0: wait for any child process whose process group ID is equal to that of the calling process at the time of the call to waitpid()
       pid > 0: wait for the child whose process ID is equal to the value of pid
    */

    // Put calling process on blocked queue
    
    // sleep(10);
    struct Pcb *calling_pcb = get_active_pcb(), *child_pcb = NULL;
    calling_pcb->waiting = true;
    pid_t p1 = -1;
    // puts("HERE 1");
    // print_queue(calling_pcb->zombies);
    if (pid != -1) {
        p1 = p_wait_assist(pid, wstatus, nohang, calling_pcb, child_pcb);
        return p1;
    }
    else {
        if (nohang)
        {
            p1 = k_waitpid(calling_pcb, child_pcb, wstatus, pid, nohang);
            if (p1 >= 0)
                return p1;
            else
            {
                // p_perror something
                return -1;
            }
            // struct queue *q = calling_pcb->children_pcb;
            // if (q->size == 0)
            // {
            //     // set num no child process
            //     return -1;
            // }
            // child_pcb = q->head;
            // while (child_pcb != NULL)
            // {
            //     // dprintf(STDERR_FILENO, "p_wait waiting on child: %d\n", child_pcb->pid); 
            //     p1 = p_wait_assist(child_pcb->pid, wstatus, true, calling_pcb, child_pcb);
            //     if (p1 == -1)
            //     {
            //         // p_perror
            //         return -1;
            //     }
            //     if (p1 > 0)
            //         return p1;
            //     child_pcb = child_pcb->next_child;
            // }
            // return -1;
        }
        while (!nohang)
        {
            p1 = k_waitpid(calling_pcb, child_pcb, wstatus, pid, nohang);
            if (p1 == 0) // calling process unblocked
                continue;
            if (p1 > 1)
                return p1;
            else
            {
                // p_perror something
                return -1;
            }
        }
    }
    return 0;
}


/*
 * DESCRIPTION   Sends the signal sig to the thread referenced by pid.
 * RETURNS       0 on success, or -1 on error
 */
int p_kill(pid_t pid, int sig)
{
    struct Pcb *calling_pcb = get_active_pcb();
    // dprintf(2, "p_kill PID: %d\tPID: %d\tName: %s\n", pid, calling_pcb->pid, calling_pcb->name);
    struct Pcb *pcb = calling_pcb;
    if (calling_pcb->pid != pid)
    {
        pcb = get_pcb(pid);
    }
    int success = CHANGED;

    // Send to process with exact pid
    if (pid > 0) {
        if (pcb != NULL) {
            // dprintf(2, "PID: %d\tCALLING PID: %d\tCalling NAME: %s\n", pid, calling_pcb->pid, calling_pcb->name);
            pcb->has_tc = false;
            success = k_process_kill(pcb, sig);
            // if (success == -1) return -1;
            // dprintf(2, "SUCCESS: %d\n", success);
            return success;
        }
    }
    // Send to every process in pgid of _calling_ process
    else if (pid == 0) {
        struct queue *pgid_queue = get_pcbs_with_pgid(calling_pcb->pgid); // should include calling pcb
        // Iterate through queue of pcbs with pgid
        struct Pcb *p = pgid_queue->head;
        while (p != NULL) {
            k_process_kill(p, sig);
            p->status = TERMINATED;
            success = 0;
            p = p->next;
        }
        free_queue(pgid_queue);
    }
    // Send to every process for which the calling process has permission to send signals, except for process 1 (init)
    else if (pid == -1) {
        // Unsure if required
    }
    // Send to every process in pgid of _process_ with pid
    else if (pid < -1) {
        struct queue *pgid_queue = get_pcbs_with_pgid(pcb->pgid); // should include pcb
        // Iterate through queue of pcbs with pgid
        struct Pcb *p = pgid_queue->head;
        while (p != NULL) {
            k_process_kill(p, sig);
            success = 0;
        }
    }
    // set errno?
    return success;
}

/*
 * DESCRIPTION   exits the current thread unconditionally.
 */
void p_exit(void)
{
   struct Pcb *calling_pcb = get_active_pcb();
//    dprintf(2, "p_exit pid: %d\n", calling_pcb->pid);
   if (calling_pcb == NULL) perror("PROBLEM in P_EXIT");
   k_exit(calling_pcb);
}

int p_tcset(pid_t pid) 
{
    return k_tcset(pid);
}

int custom_read(int fd, char *buf, ssize_t n) {
    struct Pcb *curr = get_active_pcb();
    if (fd == STDIN_FILENO && !curr->has_tc) 
    {
        // curr->has_tc = false;
        // puts("Stopping the reading process");
        k_process_kill(curr, S_SIGSTOP);
        // puts("SHouldn'T RUN");
    }
    curr->reading = true;
    ssize_t rb = read(fd, buf, n);
    curr->reading = false;
    // printf("READ BYTES: %ld\tREAD: %s\n", rb, buf);
    return rb;
}

void dummy(char *str) {
    char c[100];
    memset(c, 0, 100);
    custom_read(STDIN_FILENO, c, 50);
    dprintf(STDERR_FILENO, "%s\n", str);
    if (strcmp(str, "A") == 0 || strcmp(str, "D") == 0)
    {
        int c = 0;
        while(c < 10) {
            dprintf(STDERR_FILENO, "Still in %s\n", str);
            usleep(50000);
            c++;
        }
    }
    else usleep(50000);
    printf("EXITING %s\n", str);
    p_exit();
}


void spawn_shell()
{
    if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGALRM) == -1)) {
        perror("Failed to initialize the signal mask");
        exit(EXIT_FAILURE);
    }
    pid_t sh_pid = p_spawn(interactive_shell, NULL, 0, 1, true);
    switch_to_scheduler();
    // printf("Shell PID: %d\t\tPPID: %d\n", sh_pid, get_pcb(sh_pid)->ppid);
}

pid_t p_getpid(int opt)
{
    // struct Pcb *fg_pcb = get_fg_pcb();
    // if (fg_pcb->sleeping && fg_pcb->sleep_time > 0 && fg_pcb->status == BLOCKED)
    // {
    //     return fg_pcb->pid;
    // }
    if (opt == 1)
        return get_tc_pid();
    return get_active_pcb()->pid;
}

void p_switch_to_scheduler()
{
    switch_to_scheduler();
}

void p_boot_kernel()
{
    k_boot_kernel();
    puts("Loading PENN-OS");
}

// ------ ZOMBIFY ------
void zombie_child() {
	p_exit();
}

void zombify() {
    char *argv[] = {"zombie\0", NULL};
	p_spawn(zombie_child, argv, 0, 1, true);

	while (1);
	p_exit();
}

// ------ ORPHANIFY ------
void orphan_child() {
	while (1);
}

void orphanify() {
    char *argv[] = {"orphan\0", NULL};
	p_spawn(orphan_child, argv, 0, 1, true);
	p_exit();
}

void busy_process()
{
    while (true)
    {
        ;
    }
    p_exit();
}

void p_shutdown()
{
    k_shutdown();
}

void p_nice_next(int priority)
{
    k_change_priority_for_new(priority);
}