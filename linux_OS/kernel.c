#include "kernel.h"

struct queue *get_next_queue();
int get_clock_ticks();
// int get_counter();

static int count = 0, clock_ticks = 0;
pid_t pid_count = 1;
typedef struct queue q;
static q *low, *mid, *high, *blocked, *stopped;
static ucontext_t *schedulerContext, *activeContext = NULL, *idleContext, *shellContext;
static ucontext_t mainContext;
static int how_ended = TIMED_OUT;
static struct Pcb *current_pcb = NULL; 
FILE *log_fp;
sigset_t signal_set;
static bool idle_run = false;
static pid_t tc_pid = 1;
static bool change_priority;
static int _new_priority;

struct Pcb *fg_pcb = NULL;
#include <valgrind/valgrind.h>

int get_how_ended() { return how_ended; }

void set_how_ended(int how) { how_ended = how; }

static void setStack(stack_t *stack)
{
    void *sp = malloc(2*SIGSTKSZ);
    // Needed to avoid valgrind errors
    VALGRIND_STACK_REGISTER(sp, sp + 2*SIGSTKSZ);

    *stack = (stack_t){.ss_sp = sp, .ss_size = 2*SIGSTKSZ};
}

void make_context(ucontext_t *ucp, void (*func)(), int argc, char *argv[], bool link_to_scheduler)
{
    // puts("make_context");
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);
    setStack(&ucp->uc_stack);
    if (link_to_scheduler) ucp->uc_link = schedulerContext;
    else ucp->uc_link = NULL;
    // TODO: Change this as cases arise
    if (argc == 0)
        makecontext(ucp, func, 0);
    else if (argc != 0 && (strcmp(argv[0], "touch") == 0))
    {
        makecontext(ucp, func, 1, argv);
    }
    else if (argc != 0 && (strcmp(argv[0], "cat") == 0))
    {
        makecontext(ucp, func, 1, argv);
    }
    else if (argc != 0 && (strcmp(argv[0], "rm") == 0))
    {
        int fsize = 0;
        printf("size: %d\n", fsize);
        while (argv[fsize] != NULL)
            fsize += 1;
        makecontext(ucp, func, 2, argv, fsize);
    }
    else if (argc == 1) {
        makecontext(ucp, func, 2, argv[1]);
    }
    else if (argc == 2) makecontext(ucp, func, 3, argv[1], argv[2]);
    else if (argc == 3) makecontext(ucp, func, 4, argv[1], argv[2], argv[3]);
    else if (argc == 4) makecontext(ucp, func, 5, argv[1], argv[2], argv[3], argv[4]);
} 

struct queue *get_next_queue()
{
    int next_counter = get_counter();
    struct queue *next_q = low;
    if (next_counter < 9)   
        next_q = high;
    else if (next_counter >=9 && next_counter < 15)
        next_q = mid;
    return next_q;
}

int get_counter()
{
    // int c = rand() % 19;
    int c = count++ % 19;
    return c;
}

void initialise_queues()
{
    low = create_queue();
    mid = create_queue();
    high = create_queue();
    blocked = create_queue();
    stopped = create_queue();
}

struct Pcb *get_pcb(pid_t pi) { 
    // Check all queues for the pcb with given pid
    k_block_sigset(&intmask);
    struct Pcb *p = get_pcb_with_pid(low, pi);
    if (p == NULL) { p = get_pcb_with_pid(mid, pi); }
    if (p == NULL) { p = get_pcb_with_pid(high, pi); }
    if (p == NULL) { p = get_pcb_with_pid(blocked, pi); }
    if (p == NULL) { p = get_pcb_with_pid(stopped, pi); }
    k_unblock_sigset(&intmask);
    return p;
}

struct queue *get_pcbs_with_pgid(pid_t pgid) { 
    // Check all queues for all pcbs with given pgid
    struct queue *result = (struct queue *)malloc(sizeof(struct queue));
    
    get_pcbs_with_pgid_in_queue(low, pgid, result);
    get_pcbs_with_pgid_in_queue(mid, pgid, result);
    get_pcbs_with_pgid_in_queue(high, pgid, result);
    get_pcbs_with_pgid_in_queue(blocked, pgid, result);
    get_pcbs_with_pgid_in_queue(stopped, pgid, result);

    return result;
}

void print_all_queues_info() {
    k_block_sigset(&intmask);
    print_queue_info(low);
    print_queue_info(mid);
    print_queue_info(high);
    print_queue_info(blocked);
    print_queue_info(stopped);
    k_unblock_sigset(&intmask);
}

void inc_clock()
{
    k_block_sigset(&intmask);
    clock_ticks += 1;
    // Take care of clock ticks in sleeping processes
    struct Pcb *p = get_blocked_queue()->head;
    struct Pcb *temp = NULL;
    if (get_blocked_queue()->size > 0)
    {
        while (p != NULL)
        {
            if (p->sleeping && p->status == BLOCKED)
            {
                p->sleep_time--;
                // fprintf(stderr, "SLEEP LEFT: %d\n", p->sleep_time);
                if (p->sleep_time == 0)
                {
                    temp = p;
                }
            }
            p = p->next;
            if (temp != NULL && temp->sleep_time == 0)
            {
                temp->status = READY;
                fprintf(get_log_fp(), "[%3d]\t\tUNBLOCKED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), temp->pid, temp->priority, temp->name);
                remove_pcb(blocked, temp);
                push(get_queue_with_priority(temp->priority), temp);
                // fprintf(stderr, "Q SIZE: %d\n", get_queue_with_priority(temp->priority)->size);
                temp = NULL;
            }
        }
    }
    // TODO: Iterate through sleeping processes in stopped queue as well?
    // Based on https://edstem.org/us/courses/32172/discussion/2863007
    k_unblock_sigset(&intmask);
}

static void alarm_handler(int signum) // SIGALRM
{
    // puts("SIGALRM");
    k_block_sigset(&intmask);
    inc_clock();
    fflush(get_log_fp());
    set_how_ended(TIMED_OUT);
    struct Pcb *p = get_active_pcb();
    // p = get_active_pcb();
    if (p != NULL && p->sleeping) {
        // puts("alarm_handler sleeping");
        k_unblock_sigset(&intmask);
        setcontext(schedulerContext);
    }
    else if (p != NULL && p->status!=BLOCKED) {
        q *Q = get_queue_with_priority(p->priority);
        p->status = READY;
        push(Q, p);
        k_unblock_sigset(&intmask);
        swapcontext(activeContext, schedulerContext);
    }
    else {
        k_unblock_sigset(&intmask);
        setcontext(schedulerContext);
    }
}

static void set_alarm_handler(void)
{
    struct sigaction act;

    act.sa_handler = alarm_handler;
    act.sa_flags = SA_RESTART;
    sigemptyset(&act.sa_mask);

    sigaction(SIGALRM, &act, NULL);
}

static void set_timer(void)
{
    struct itimerval it;

    it.it_interval = (struct timeval){.tv_usec = 100000};
    it.it_value = it.it_interval;

    if (setitimer(ITIMER_REAL, &it, NULL) != 0) {
        perror("Timer setting failed");
        exit(EXIT_FAILURE);
    } 
}

struct queue *get_queue_with_priority(int priority)
{
    if (priority == -1) return high;
    if (priority == 0) return mid;
    if (priority == 1) return low;
    return NULL;
}

void free_queues()
{
    free_queue(low);
    free_queue(mid);
    free_queue(high);
    free_queue(blocked);
}

struct Pcb *get_active_pcb()
{
    return current_pcb;
}

ucontext_t *get_active_context()
{
    return activeContext;
}

void set_active_pcb_and_context(struct Pcb *p)
{
    if (p != NULL)
        activeContext = p->uc;
    else activeContext = NULL;
    // if (p->has_tc && p->fg)
    // {
    //     set_tc_pid(p->pid);
    // }
    current_pcb = p;
}

int get_num_of_ready_processes()
{
    return get_queue_with_priority(-1)->size + get_queue_with_priority(0)->size + get_queue_with_priority(1)->size; 
}

void setup_log()
{
    log_fp = fopen("log1.txt", "w+");
    if (log_fp == NULL)
    {
        perror("File not open");
    }
    // fprintf(log_fp, "Testing\n");
}

FILE *get_log_fp()
{
    return log_fp;
}

void setup_idle_process() 
{
    idleContext = (ucontext_t *)malloc(sizeof(ucontext_t));
    if (idleContext == NULL) {
        perror("Idle context could not be malloced");
        exit(EXIT_FAILURE);
    }
    sigemptyset(&signal_set);
    make_context(idleContext, idle_process, 0, NULL, false);
    
}

void idle_process()
{
    while (true)
    {
        idle_run = true;
        // puts("IDLE 1");
        // if (!(get_active_pcb()->sleeping && get_active_pcb()->status == BLOCKED))
        // {
        //     puts("NULLING PCB and CTX");
        //     set_active_pcb_and_context(NULL);
        // }
        // if (get_active_pcb()->pid == 1)
        // {
        //     struct Pcb *waiting_on = get_pcb(get_active_pcb()->waiting_on);
        //     if (waiting_on->status == BLOCKED)
        //     {
        //         set_active_pcb_and_context(waiting_on);
        //     }
        // }
        sigsuspend(&signal_set);

        // fprintf(stderr, "NAME: %s\n", get_active_pcb()->name);
        // puts("IDLE 2");
        // swapcontext(idleContext, get_active_pcb()->uc);
        // puts("SHOULDNT");
        // switch_to_scheduler();
    }
}

struct queue *get_blocked_queue()
{
    return blocked;
}

struct queue *get_next_valid_queue() // Should only be called after checking that there is at least 1 process in some Q
{
    q *Q = get_next_queue();
    while (is_empty(Q))
        Q = get_next_queue();
    return Q;
}

int get_clock_ticks() { return clock_ticks;}

void scheduler()
{
    // if (current_pcb->sleeping && curr) {

    // }
    k_block_sigset(&intmask);
    if (get_how_ended() == FIN_RUN) {
        set_how_ended(TIMED_OUT);
        fprintf(get_log_fp(), "[%3d]\t\tEXITED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), current_pcb->pid, current_pcb->priority, current_pcb->name);
        if (current_pcb->parent_pcb == NULL) {
            fprintf(get_log_fp(), "[%3d]\t\tORPHAN\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), current_pcb->pid, current_pcb->priority, current_pcb->name);
        }
        inc_clock();
        k_process_cleanup(current_pcb);
    }
    if (get_num_of_ready_processes() == 0) {
        k_unblock_sigset(&intmask);
        setcontext(idleContext);
        puts("IDLE ENDED");
    }
    if (get_how_ended() == TIMED_OUT) {
        if (get_num_of_ready_processes() == 0) 
        {
            perror("PROBLEM");
        }
        q *Q = get_next_valid_queue();
        struct Pcb *curr = pop(Q);
        // if (curr->status != READY) {
        //     if (curr->status == BLOCKED) push(get_blocked_queue(), curr);
        //     setcontext(idleContext);
        // }
        if (curr == NULL) {
            perror("BAD ACCESS");
        }
        if (curr->status == READY)
        {
            set_active_pcb_and_context(curr);
            curr->status = RUNNING;
        } 
        else if (curr->status == STOPPED)
        {
            puts("HOW the HELL");
        }
        fprintf(get_log_fp(), "[%3d]\t\tSCHEDULE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), curr->pid, curr->priority, curr->name);
        fflush(get_log_fp());
        current_pid = curr->pid;
        // if (curr->fg && curr->has_tc)
        //         k_tcset(curr->pid);
        // if (strcmp(curr->name, "sleep") == 0)
        // fprintf(stderr, "SETTING CURRENT PID: %d\n", current_pid);
        k_unblock_sigset(&intmask);
        setcontext(curr->uc);
        // set_active_pcb_and_context(curr);
        // fprintf(get_log_fp(), "[%3d]\t\tSCHEDULE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), curr->pid, curr->priority, curr->name);
        // setcontext(curr->uc);
    } 
    // else {
    //     // Not sure if required
    // }
    
}

struct Pcb *k_process_create(struct Pcb *parent)
{
    // puts("k_process_create");
    struct Pcb *child_pcb = NULL;
    if (parent != NULL)
    {
        // puts("k_process_create 1");
        // if (parent == NULL)  puts("k_process_create pare");
        // printf(" Parent pid: %d", parent->pid);
        int priority = 0;
        if (change_priority && parent->pid == 1) // Only shell can set new priority now
        {
            change_priority = false;
            priority = _new_priority;
            _new_priority = 0;
        }
        child_pcb = create_pcb_no_context(++pid_count, parent->pid, parent->pgid, priority, READY);
        // puts("k_process_create 2");
        add_child_to_parent_q(parent, child_pcb);
        child_pcb->parent_pcb = parent;
        child_pcb->uc = (ucontext_t *)malloc(sizeof(ucontext_t));
        push(mid, child_pcb);
        
        //  TODO: Move fprintf to p_spawn
        // fprintf(get_log_fp(), "[%3d]\t\tCREATE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), pid, 0, name);
    }
    else
    { // SHELL
        child_pcb = create_pcb_no_context(1, 0, 1, -1, READY);
        //  puts("k_process_create 3");
        child_pcb->parent_pcb = NULL;
        child_pcb->name = (char *)malloc(sizeof("shell\0"));
        strcpy(child_pcb->name, "shell\0");
        child_pcb->uc = (ucontext_t *)malloc(sizeof(ucontext_t));
        shellContext = child_pcb->uc;
        push(high, child_pcb);
    }

    // puts("k_process_create 4");
    // push(get_queue_with_priority(child_pcb->priority), child_pcb);
    return child_pcb;
}


struct Pcb *k_process_create_start(void (*func)(), char *argv[], int fd0, int fd1, bool fg)
{
    k_block_sigset(&intmask);
    struct Pcb *child_pcb = NULL; 
    if (func == interactive_shell) {
        child_pcb = k_process_create(NULL);
        if (child_pcb == NULL) {
            p_perror("Fatal error. Exiting...");
            exit(EXIT_FAILURE);
        }
        make_context(child_pcb->uc, func, 0, NULL, true);
        fprintf(get_log_fp(), "[%3d]\t\tCREATE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), child_pcb->pid, -1, "shell\0"); // child_pcb->cmd->commands[0][0]);
        fflush(get_log_fp());
        child_pcb->fg = fg;
        set_fg_pcb(child_pcb);
        k_unblock_sigset(&intmask);
        child_pcb->fd[0] = 0;
        child_pcb->fd[1] = 1;
        return child_pcb;
    }
    else
    {
        // Create new thread as a child of the current active pcb
        struct Pcb *parent_pcb = get_active_pcb();
        child_pcb = k_process_create(parent_pcb);
        // Get argc
        int argc = 0;
        if (argv != NULL) {
            while (argv[argc] != NULL)
            {
                argc++;
            }
            // fprintf(stderr, "argc is %d\n", argc);
        }
        // Call make_context
        child_pcb->name = (char *)malloc(strlen(argv[0])+1);
        memset(child_pcb->name, 0, strlen(argv[0])+1);
        strcpy(child_pcb->name, argv[0]);
        if (fg)
        {
            set_tc_pid(child_pcb->pid);
            set_fg_pcb(child_pcb);
        }
        child_pcb->fg = fg;
        child_pcb->has_tc = fg;
        if (strcmp(argv[0], "cat") == 0)
        {
            getcontext(child_pcb->uc);
            sigemptyset(&child_pcb->uc->uc_sigmask);
            setStack(&child_pcb->uc->uc_stack);
            child_pcb->uc->uc_link = schedulerContext;
            int fsize = 0;
            while (argv[fsize] != NULL)
                fsize += 1;
            makecontext(child_pcb->uc, func, 3, argv, fsize, fd1);
        }else if (strcmp(argv[0], "echo")==0){
            getcontext(child_pcb->uc);
            sigemptyset(&child_pcb->uc->uc_sigmask);
            setStack(&child_pcb->uc->uc_stack);
            child_pcb->uc->uc_link = schedulerContext;
            makecontext(child_pcb->uc, func, 2, argv, fd1);
        }
        else{
            make_context(child_pcb->uc, func, argc, argv, true);
        }
    }
    // TODO: File descriptors, pipe
    // printf("TIME: %d\n",get_clock_ticks());
    // printf("TEST: %s\n", argv[0]);
    child_pcb->fd[0] = fd0;
    child_pcb->fd[1] = fd1;
    fprintf(get_log_fp(), "[%3d]\t\tCREATE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), child_pcb->pid, 0, child_pcb->name);
    fflush(get_log_fp());
    // fclose(get_log_fp());
    k_unblock_sigset(&intmask);
    return child_pcb;
}

void k_process_block(struct Pcb *p, struct Pcb *cp)
{
    k_block_sigset(&intmask);
    if (p->status == READY) 
    {
        // puts("READY HERE");
        remove_pcb(get_queue_with_priority(p->priority), p);
    }
    else if (p->status == STOPPED) remove_pcb(stopped, p);
    else if ((p->waiting == true) && (cp != NULL) && (cp->status != READY)) {
        k_unblock_sigset(&intmask);
        return;
    }
    p->status = BLOCKED;
    fprintf(get_log_fp(), "[%3d]\t\tBLOCKED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), p->pid, p->priority, p->name);
    fflush(get_log_fp());
    push(get_blocked_queue(), p);
    // current_pcb = NULL; // Not sure about this
    k_unblock_sigset(&intmask);
    swapcontext(p->uc, schedulerContext);
}

pid_t k_waitpid(struct Pcb *calling_pcb, struct Pcb *child_pcb, int *wstatus, pid_t pid, bool nohang)
{
    k_block_sigset(&intmask);
    pid_t rpid = -1;
    if (pid == -1) {
        struct queue *q = calling_pcb->zombies;
        if (q->size > 0)
        {
            child_pcb = q->head;
            goto label1;
        }
        else
        {
            q = calling_pcb->children_pcb;
            if (q->size == 0)
            {
                // set num
                return -1;
            }
            child_pcb = q->head;
            while (child_pcb != NULL)
            {
                if (child_pcb->state_change == true)
                    goto label1;
                child_pcb = child_pcb->next_child;
            }
            if (nohang)
            {
                k_unblock_sigset(&intmask);
                return 0;
            }
            k_unblock_sigset(&intmask);
            k_process_block(calling_pcb, NULL);
            k_tcset(calling_pcb->pid);
            return 0;
        }
    }
    label1:
    rpid = child_pcb->pid;
    // dprintf(2, "About to reap: %d\n", rpid);
    if (child_pcb->status == STOPPED)
    {
        *wstatus = STOPPED;
        calling_pcb->waiting = false;
        child_pcb->state_change = false;
        wait_log(calling_pcb);
        // dprintf(2, "reap 1: %d\n", rpid);
        k_unblock_sigset(&intmask);
        
        return rpid;
    }
    if (child_pcb->status == TERMINATED)
    {
        remove_child_pcb(calling_pcb, child_pcb);
        remove_pcb(calling_pcb->zombies, child_pcb);
        free_pcb(child_pcb);
        *wstatus = TERMINATED;
        calling_pcb->waiting = false;
        wait_log(calling_pcb);
        // dprintf(2, "reap 2: %d\n", rpid);
        k_unblock_sigset(&intmask);
        return rpid;
    }
    if (child_pcb->status == FINISHED)
    {
        remove_child_pcb(calling_pcb, child_pcb);
        remove_pcb(calling_pcb->zombies, child_pcb);
        free_pcb(child_pcb);
        *wstatus = FINISHED;
        calling_pcb->waiting = false;
        wait_log(calling_pcb);
        // dprintf(2, "reap 3: %d\n", rpid);
        k_unblock_sigset(&intmask);
        return rpid;
    }
    // dprintf(2, "reap 4: %d\n", rpid);
    if (nohang) 
    {
        k_unblock_sigset(&intmask);
        return 0;
    }
    calling_pcb->changed_child_pcb = child_pcb; // Monitoring this child for status change
    calling_pcb->waiting_on = child_pcb->pid;
    // k_tcset(calling_pcb->pid);
    // dprintf(2, "Waiting on: %s\n", child_pcb->name);
    k_unblock_sigset(&intmask);
    k_process_block(calling_pcb, child_pcb);
    // Take terminal control
    k_tcset(calling_pcb->pid); // set_tc_pid(1);
    return 0;
}

void k_exit(struct Pcb *calling_pcb)
{
    k_block_sigset(&intmask);
    // puts("EXITING");
    fprintf(get_log_fp(), "[%3d]\t\tEXITED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), calling_pcb->pid, calling_pcb->priority, calling_pcb->name);
    fflush(get_log_fp());
    calling_pcb->state_change = true;
    if (calling_pcb->parent_pcb != NULL)
    {
        fprintf(get_log_fp(), "[%3d]\t\tZOMBIE\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), calling_pcb->pid, calling_pcb->priority, calling_pcb->name);
        fflush(get_log_fp());
        add_child_to_parent_zombies(calling_pcb->parent_pcb, calling_pcb);
        calling_pcb->status = FINISHED;
        calling_pcb->parent_pcb->changed_child_pcb = calling_pcb;
        calling_pcb->parent_pcb->exited_child += 1;
        if (tc_pid == calling_pcb->pid)
        {
            tc_pid = calling_pcb->parent_pcb->pid;
            k_tcset(calling_pcb->parent_pcb->pid); // calling_pcb->has_tc) {
            if (calling_pcb->parent_pcb->reading)
                calling_pcb->parent_pcb->has_tc = true;
        }
        k_process_kill(calling_pcb->parent_pcb, S_SIGCHLD);
    }
    else
    {
        // fprintf(get_log_fp(), "[%3d]\t\tORPHAN\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), calling_pcb->pid, calling_pcb->priority, calling_pcb->name);
        // TODO: orphan suicide by switching to some reserved context which will clear the memory associated
        if (calling_pcb->pid == 1 )
        {
            // puts("DAMMMMMNNNNNN");
            // k_unblock_sigset(&intmask);
            // setcontext(&mainContext);
        }
    }
    struct queue *cq = calling_pcb->children_pcb;
    int size = cq->size;
    for (int i = 0; i < size; i++)
    {
        struct Pcb *child_pcb = pop(cq);
        fprintf(get_log_fp(), "[%3d]\t\tORPHAN\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), child_pcb->pid, child_pcb->priority, child_pcb->name);
        fflush(get_log_fp());
        if (child_pcb->status == READY)
            remove_pcb(get_queue_with_priority(child_pcb->priority), child_pcb);
        else if (child_pcb->status == BLOCKED)
            remove_pcb(blocked, child_pcb);
        else if (child_pcb->status == STOPPED)
            remove_pcb(stopped, child_pcb);
        k_process_cleanup(child_pcb);
        free_pcb(child_pcb);
    }
    calling_pcb->stateChangeType = 1; // 1: exited normally
    k_unblock_sigset(&intmask);
}

int k_process_kill(struct Pcb *process, int signal)
{
    struct Pcb *parent = process->parent_pcb, *curr = current_pcb;
    struct queue *children = process->children_pcb;
    // dprintf(2, "k_p_kill to process: %d\n", process->pid);
    // puts(process->name);
    // dprintf(2, "k_p_kill TC process: %d\n", tc_pid);
    if (signal == S_SIGTERM) 
    {
        k_block_sigset(&intmask);
        // fprintf(stderr, "[%3d]\t\tSIGNALED\t\t%d\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->status, process->name);
        // fflush(get_log_fp());
        process->has_tc = false;
        if (process->status == ZOMBIE && parent == NULL)
        {
            // set num
            return -1;
        }
        if (process->status != ZOMBIE) // Safety check
        {
            dprintf(2, "k_p_kill sigterm process: %d\n", process->pid);
            if (process->status == READY)//get_active_pcb() != process && get_active_pcb()->status == READY) 
            {
                remove_pcb(get_queue_with_priority(process->priority), process);
            }
            else if (process->status == BLOCKED) {
                remove_pcb(get_blocked_queue(), process);
            }
            else if (process->status == STOPPED)
                remove_pcb(stopped, process);
            else if (process->status != RUNNING) {
                //set err num
                return -1;
            }
            process->state_change = true;
            process->status = TERMINATED;
            k_process_cleanup(process); 
            if (parent != NULL)
            {
                // process->status = ZOMBIE;
                // remove_child_pcb(parent, process);
                if (tc_pid == process->pid)
                {
                    tc_pid = parent->pid;
                    k_tcset(parent->pid);
                }
                // add_child_to_parent_zombies(parent, process);
                if (parent->waiting && parent->status == BLOCKED) // && (parent->waiting_on == process->pid)
                {
                    // Can be used for waitpid & W_WIF funcs
                    process->state_change = true;
                    parent->changed_child_pcb = process;
                    fprintf(get_log_fp(), "[%3d]\t\tUNBLOCKED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), parent->pid, parent->priority, parent->name);
                    fflush(get_log_fp());
                    parent->status = READY;
                    remove_pcb(get_blocked_queue(), parent);
                    push(get_queue_with_priority(parent->priority), parent);
                }
            }
            // Record that state changed, for waitpid
            process->stateChangeType = 3; // 3: killed
            // free resources when parent calls waitpid on (zombied) process
        }
        k_unblock_sigset(&intmask);
        switch_to_scheduler();
    }
    else if (signal == S_SIGSTOP) {
        k_block_sigset(&intmask);
        // if (get_active_pcb()->pid == 1)
        // {
        //     if (fg_pcb->status == BLOCKED && fg_pcb->sleeping && fg_pcb->sleep_time > 0)
        //         process = fg_pcb;
        // }
        fprintf(get_log_fp(), "[%3d]\t\tSTOPPED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->name);
        fflush(get_log_fp());
        if (process->status == READY) {
            remove_pcb(get_queue_with_priority(process->priority), process);
        }
        else if (process->status == BLOCKED && process->sleeping) {
            remove_pcb(blocked, process);
        }
        else if (process->status == BLOCKED)
        {
            if (process->pid != 1)
                remove_pcb(blocked, process);
            else
            {
                // set num
                return -1;
            }
        }
        else if (process->status != RUNNING) {
            // set num
            return -1;
        }
        push(stopped, process);
        process->status = STOPPED;
        process->has_tc = false;
        process->fg = false;
        process->state_change = true;
        if (parent->status == BLOCKED && parent->waiting)
        {
            process->state_change = true;
            parent->changed_child_pcb = process;
            parent->status = READY;
            remove_pcb(get_blocked_queue(), parent);
            push(get_queue_with_priority(parent->priority), parent);
            fprintf(get_log_fp(), "[%3d]\t\tUNBLOCKED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), parent->pid, parent->priority, parent->name);
            fflush(get_log_fp());
        }
        process->stateChangeType = 2; // 2: stopped
        k_unblock_sigset(&intmask);
        switch_to_scheduler();
    }
    else if (signal == S_SIGCONT) {
        // Put process on ready queue unless previously sleeping or waiting (when stopped)
        k_block_sigset(&intmask);
        if (process->status != STOPPED) {
            k_unblock_sigset(&intmask);
            return NO_CHANGE;
        }
        if (process->status == STOPPED && process->sleeping)
        {
            remove_pcb(stopped, process);
            if (process->sleep_time > 0)
            {
                process->status = BLOCKED;
                push(blocked, process);
            }
            else if (process->sleep_time == 0)
            {
                process->status = READY;
                push(get_queue_with_priority(process->priority), process);
            }
            fprintf(get_log_fp(), "[%3d]\t\tCONTINUED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->name);
            fflush(get_log_fp());
            k_unblock_sigset(&intmask);
            return CHANGED;
        }
        // if (process->sleeping || process->waiting) {
        //     puts("THIS ACTUALLY RUNS?");
        //     process->status = BLOCKED;
        //     k_unblock_sigset(&intmask);
        //     return NO_CHANGE;
        // } 
        if (process->status == STOPPED && process->reading) {
            // if (process->fd[0] == STDIN_FILENO && !process->has_tc)
            //     return NO_CHANGE;
            // fprintf(stderr, "TC_PID: %d\n", tc_pid);
            if (process->fd[0] == STDIN_FILENO && process->pid != tc_pid)
            {
                // puts("YUP ITS HERE");
                // fprintf(stderr, "has_tc: %d\tPID: %d\n", process->has_tc, process->pid);
                // fprintf(stderr, "Parent has_tc: %d\tPID: %d\n", process->parent_pcb->has_tc, process->parent_pcb->pid);
                k_unblock_sigset(&intmask);
                return NO_CHANGE;
            }
            process->status = READY;
            // k_block_sigset(&intmask);
            remove_pcb(stopped, process);
            push(get_queue_with_priority(process->priority), process);
        }
        else {
            process->status = READY;
            remove_pcb(stopped, process);
            push(get_queue_with_priority(process->priority), process);
        }
        fprintf(get_log_fp(), "[%3d]\t\tCONTINUED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->name);
        process->state_change = true;
        process->stateChangeType = 4; // 4: continued
        k_unblock_sigset(&intmask);
    }
    else if (signal == S_SIGCHLD) { 
        // To be delivered to parent when child calls exit. The process is the parent process in function args
        // if (process->changed_child_pcb->status == FINISHED)
        // {
        //     add_child_to_parent_zombies(process, process->changed_child_pcb);
        //     fprintf(stderr, "Zombie added\n");
        // }
        if (process->waiting && process->status == BLOCKED)
        {
            fprintf(get_log_fp(), "[%3d]\t\tUNBLOCKED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->name);
            fflush(get_log_fp());
            process->status = READY;
            remove_pcb(get_blocked_queue(), process);
            push(get_queue_with_priority(process->priority), process);
        }
    }
    return CHANGED;
}


void k_process_cleanup(struct Pcb *process)
{
    // Only called for cleaning process child tree when process is terminated
    // block signals (not sure if required)
    if (process == NULL) return;
    // dprintf(2, "k_process_cleanup start: %d\n", process->pid);
    // if (process->status == READY)
    //     remove_pcb(get_queue_with_priority(process->priority), process);
    // else if (process->status == BLOCKED)
    //     remove_pcb(get_blocked_queue(), process);
    // else if (process->status == STOPPED)
    //     remove_pcb(stopped, process);
    process->status = TERMINATED;
    if (process->pid != 1) {
        fprintf(get_log_fp(), "[%3d]\t\tSIGNALED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), process->pid, process->priority, process->name);
        fflush(get_log_fp());
    }
    struct queue *children = process->children_pcb;
    if (children->size > 0)
    {
        struct Pcb *p = children->head;
        int size = children->size;
        for (int i = 0; i < size; i++)
        {
            if (p->status != ZOMBIE)
                k_process_cleanup(p);
            p = p->next_child;
        }
    }
    // Free zombie q
    // free_queue(process->zombies);
    // free children list
    free_child_queue(process->children_pcb);
    // puts("FREE");
    // dprintf(2, "k_process_cleanup end: %d\n", process->pid);
    // Will have to ask if required: check parent and set itself to parent zombies or free itself

    //unblock signals
}

// void k_process_cleanup(struct Pcb *process)
// {
//     // block signals
//     struct Pcb *parent = process->parent_pcb;
//     struct queue *children = process->children_pcb;
//     if (process != current_pcb) // Might not happen but added for safety
//     {
//         remove_pcb(get_queue_with_priority(process->priority), process);
//     }
//     if (children->size > 0)
//     {
//         struct Pcb *p = children->head;
//         while (p != NULL)
//         {
//             if (p->status == READY) remove_pcb(get_queue_with_priority(p->priority), p);
//             else if (p->status == BLOCKED) remove_pcb(get_blocked_queue(), p);
//             else if (p->status == STOPPED) remove_pcb(stopped, p);
//             fprintf(get_log_fp(), "[%3d]\t\tSIGNALED\t\t%d\t\t%d\t\t%s\n", get_clock_ticks(), p->pid, p->priority, p->name);
//             p = p->next_child;
//         }
//         // set each child's status to ORPHANED if not already zombied
//     }
//     // Free zombie q
//     // free children list
//     // check parent and set iteself to parent zombies or free itself

//     //unblock signals
// }


void k_sleep(unsigned int ticks)
{
    struct Pcb *curr = get_active_pcb();
    curr->sleeping = true;
    curr->sleep_time = ticks;
    k_process_block(curr, NULL);
    puts("UNBLOCKED");
}

ucontext_t *get_scheduler_context(void) {
    return schedulerContext;
}

ucontext_t *get_shell_context(void) {
    return shellContext;
}

void init_scheduler_context()
{
    schedulerContext = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(schedulerContext);
    sigemptyset(&schedulerContext->uc_sigmask);
    sigaddset(&schedulerContext->uc_sigmask, SIGALRM);
    setStack(&schedulerContext->uc_stack);
    schedulerContext->uc_link = NULL;
    makecontext(schedulerContext, scheduler, 0);
}

void switch_to_scheduler()
{
    // if (idle_run) {
    //     puts("IDLE_RUN");
    //     idle_run = false;
    //     swapcontext(idleContext, schedulerContext);
    //     return;
    // }
    if (activeContext == NULL)
    {
        // puts("Active context null");
        setcontext(schedulerContext);
    }
    else
    {
        struct Pcb *p = get_active_pcb();
        if (p != NULL && p->status == STOPPED) {
            swapcontext(activeContext, schedulerContext);
        }
        else if (p != NULL && p->status == TERMINATED) {
            setcontext(schedulerContext);
            // swapcontext(activeContext, schedulerContext);
        }
        else if (p != NULL && p->sleeping && p->sleep_time > 0) {
            // puts ("switch_to_scheduler SLEEP");
            dprintf(2, "SLEEP TIME REMAINING: %u\n", p->sleep_time);
            // setcontext(schedulerContext);
            swapcontext(activeContext, schedulerContext);
        }
        else if (p != NULL)//p->status != STOPPED && p->status != BLOCKED)
        {
            q *Q = get_queue_with_priority(p->priority);
            p->status = READY;
            push(Q, p);
            swapcontext(activeContext, schedulerContext);
        }
        else if ( p == NULL)
        {
            p_perror("Unreachable case");
            setcontext(schedulerContext);
        }
    }
}

void shutdown()
{
    struct Pcb *shell_pcb = get_active_pcb();
    k_process_cleanup(shell_pcb);
    free_pcb(shell_pcb);
    free(low);
    free(mid);
    free(high);
    free(blocked);
    free(stopped);
    // free(&(schedulerContext->uc_stack));
    free(schedulerContext->uc_stack.ss_sp);
    free(schedulerContext);
    free(idleContext->uc_stack.ss_sp);
    free(idleContext);
    fclose(get_log_fp());
    puts("SHUTTING DOWN");
    exit(EXIT_SUCCESS); // Used since we want to exit the process here
}

void k_boot_kernel()
{
    static bool started = false;
    initialise_queues();
    init_scheduler_context();
    setup_idle_process();
    setup_log();
    // puts("Setting up logout");
    // char *argv[] = {"logout", NULL};
    getcontext(&mainContext);
    sigemptyset(&mainContext.uc_sigmask);
    sigaddset(&mainContext.uc_sigmask, SIGALRM);
    setStack(&mainContext.uc_stack);
    mainContext.uc_link = NULL;
    makecontext(&mainContext, shutdown, 0);
    if (!started) {
        started = true;
        signal(SIGINT, handle_sigint);
        signal(SIGTSTP, handle_sigtstp);
        set_alarm_handler();
        set_timer();
    }
    // puts("logout setted up");
    
    // mainContext
    // Uncomment for idle context demo if required
    // activeContext = schedulerContext;
    // setcontext(activeContext);
}


void k_block_sigset(sigset_t *mask)
{
    if (sigprocmask(SIG_BLOCK, mask, NULL) == -1) {
        perror("Unable to block");
    }
}

void k_unblock_sigset(sigset_t *mask)
{
    if (sigprocmask(SIG_UNBLOCK, mask, NULL) == -1) {
        perror("Unable to unblock");
    }
}

int k_tcset(pid_t pid)
{
    k_block_sigset(&intmask);
    struct Pcb *p = get_active_pcb();
    if (p->pid != pid) {
        // p->has_tc = false;
        p = get_pcb(pid);
    }
    if (p == NULL) {
        // set err num
        return -1;
    }
    // if (p->status == STOPPED) 
    // {
    //     puts("Did not expect this to run");
    //     k_process_kill(p, S_SIGCONT);
    // }
    // dprintf(2, "Giving tc to process: %s\n", p->name);
    p->has_tc = true;
    p->fg = true;
    tc_pid = p->pid;
    fg_pcb = p;
    // fprintf(get_log_fp(), "k_tcset has_tc: %d\n", p->has_tc);
    // fprintf(get_log_fp(), "GIVING TC from PID: %d\t to PID: %d\n", get_active_pcb()->pid, p->pid);
    // fflush(get_log_fp());
    // has_terminal_control = true;
    k_unblock_sigset(&intmask);
    return 0;
}

void set_tc_pid(pid_t pid)
{
    tc_pid = pid;
}

pid_t get_tc_pid()
{
    return tc_pid;
}



struct Pcb *get_fg_pcb()
{
    return fg_pcb;
}

void set_fg_pcb(struct Pcb *p)
{
    fg_pcb = p;
}

void handle_sigint(int signal)
{
    k_block_sigset(&intmask);
    pid_t pid = tc_pid;
    dprintf(2, "SIGTERM To PID: %d\n", pid);
    if (pid == 1) // Shell
    {
        newline();
        prompt();
        k_unblock_sigset(&intmask);
    }
    else
    {
        // puts("HERE");
        enum CHANGE c = p_kill(pid, S_SIGTERM);
        // fprintf(stderr, "PID: %d\n", p_getpid());
        k_unblock_sigset(&intmask);
        // p_switch_to_scheduler();
    }
    
}

void handle_sigtstp(int signal)
{
    k_block_sigset(&intmask);
    pid_t pid = tc_pid;//p_getpid(0);
    dprintf(2, "sigtstp to: %d\n", pid);
    // fprintf(stderr, "SIG To PID: %d\tFGPID: %d\tFGNAME: %s\n", pid, fg_pcb->pid, fg_pcb->name);
    if (pid == 1)
    {
        newline();
        prompt();
        k_unblock_sigset(&intmask);
    }
    else
    {
        struct Pcb *p = get_active_pcb();
        if (p->pid != pid) {
            p = get_pcb(pid);
        }
        enum CHANGE c = k_process_kill(p, S_SIGSTOP);
        k_unblock_sigset(&intmask);
        // c will be used
        // k_unblock_sigset(&intmask);
        // p_switch_to_scheduler();
    }
}


// void k_boot_kernel()
// {
//     initialise_queues();
//     init_scheduler_context();
//     setup_idle_process();
//     setup_log();
//     set_alarm_handler();
//     set_timer();
//     signal(SIGINT, handle_sigint);
//     signal(SIGTSTP, handle_sigtstp);   
//     // Uncomment for idle context demo if required
//     // activeContext = schedulerContext;
//     // setcontext(activeContext);
// }

int k_nice(pid_t pid, int priority)
{
    k_block_sigset(&intmask);
    // Find the PCB with the given PID in the full queue
    struct Pcb *target_pcb = get_pcb(pid);
    if (target_pcb == NULL)
    {
        // set err num
        set_errno(PID_INVALID);
        return -1;
    }
    int previous_priority = target_pcb->priority;
    if (previous_priority == priority)
        return 0;
    target_pcb->priority = priority;
    // Remove the PCB from the previous priority queue
    if (target_pcb->status == READY) {
        // Add the PCB to the new priority queue
        push(get_queue_with_priority(priority), target_pcb);
        remove_pcb(get_queue_with_priority(previous_priority), target_pcb);
    }
    k_unblock_sigset(&intmask);
    return 0;
}


void k_shutdown()
{
    // dprintf(2, "k_shutdown\n");
    setcontext(&mainContext);
}

void k_change_priority_for_new(int priority)
{
    k_block_sigset(&intmask);
    change_priority = true;
    _new_priority = priority;
    k_unblock_sigset(&intmask);
}