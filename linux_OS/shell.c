#include "shell.h"
#define MAX_LEN 512
int mode = INTERACTIVE;
int job_id = 1;
bool start = true;
struct j_queue *jobs_list;



int get_arg_count(struct parsed_command *cmd)
{
    int argc = 0;
    if (cmd->num_commands == 1) 
    {
        while (cmd->commands[0][argc] != NULL)
        {
            argc++;
        }
        return argc;
    }
    else {
        //set err num
        return -1;
    }
}

void get_args_from_cmd(struct parsed_command *cmd, int arg_count, char *args[])
{
    // int i = 0;
    for (int i = 0; i < arg_count; i++)
    // while (cmd->commands[0][i] != NULL)
    {
        args[i] = cmd->commands[0][i];
        // i++;
    }
}

void non_interactive()
{
    mode = SHELL_SCRIPT;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    while ((nread = getline(&line, &len, stdin)) != -1)
    {
        if (line[nread - 1] == '\n')
        {
            line[nread - 1] = 0;
        }
        parse(line);
    }
    free(line);
}

int handle_command(struct parsed_command *cmd, char *raw_cmd, void (*func)(), char *args[])
{
    // strip the ampersand off the end of any bg commands
    if (cmd->is_background) {
        strtok(raw_cmd, "&");
        // has_terminal_control = false;
    }

    if (cmd->num_commands > 1)
    {
        perror("Not supported");
        free(cmd);
        return -1;
    }
    pid_t pid;
    struct Job *new_job;

    int read_fd = 0;
    int write_fd = 1;
    pid_t group_id = -1;
    if (cmd->stdout_file != NULL)
        {
            if (cmd->is_file_append)
                write_fd=f_open(cmd->stdout_file,2);
            else
                write_fd=f_open(cmd->stdout_file,0);  
        }
        
    pid = p_spawn(func, args, read_fd, write_fd, !cmd->is_background);
    if (pid > 0) // Parent
    {   
        group_id = pid;
        if (!cmd->is_background)
        {
            new_job = create_job(cmd, raw_cmd, group_id, J_RUNNING, FG);
            // give tc if errors arise
        }
        else
        {
            new_job = create_job(cmd, raw_cmd, group_id, J_RUNNING, BG);
        }

        if (cmd->is_background)
        {
            // if the command is running in the bg, make sure the shell has control
            push_job(jobs_list, new_job);
            new_job->job_id = job_id++;
            if (p_tcset(1) == -1)
            {
                perror("TC error");
                // p_perror("Terminal control error");
            }
        }
        bool terminated = false;
        if (!cmd->is_background)
        {
            int status = 0;
            // puts("Gonna wait");
            // wait until the child is finished or stopped
            if (p_waitpid(pid, &status, false) != pid)
            {
                perror("waitpid error");
            }
            if (W_WIFEXITED(status))
            { // normal exit
                new_job->count_finished++;
            }
            else if (W_WIFSIGNALED(status))
            { // child terminated by signal
                // tcsetpgrp(STDOUT_FILENO, getpid());
                terminated = true;
            }
            else if (W_WIFSTOPPED(status))
            {
                // child stopped
                new_job->status = J_STOPPED;
                new_job->ground = BG;
                push_job(jobs_list, new_job);
                new_job->job_id = job_id++;
                // tcsetpgrp(STDOUT_FILENO, getpid());
                fprintf(stderr, "\nStopped: %s\n", raw_cmd);
            }

            if (terminated)
            {
                free_job(new_job);
            }
            else if (new_job->cmd->num_commands == new_job->count_finished)
            { // Entire pipeline executed and exited
                free_job(new_job);
                // tcsetpgrp(STDIN_FILENO, getpid());
                p_tcset(1);
            }
            p_tcset(1);
        }
        else
        {
            fprintf(stderr, "Running: %s\n", raw_cmd);
        }
        print_finished();
    }
    else
    {
        puts("Shouldn't be here");
        fprintf(stderr, "My PID: %d\n", p_getpid(0));
    }
    return pid;
}

int parse(char *str)
{
    struct parsed_command *cmd;
    int i = parse_command(str, &cmd);

    if (i < 0)
    {
        perror("parse_command");
        exit(EXIT_FAILURE);
    }
    if (i > 0)
    {
        printf("syntax error: %d\n", i);
        return -1;
    }
    void (*func)() = NULL;
    int arg_count = get_arg_count(cmd);
    if (arg_count == -1) {
        // p_perror("Too many commands\n");
        return -1;
    }
    char *args[arg_count+1];
    // else has_terminal_control = true;
    get_args_from_cmd(cmd, arg_count, args);
    args[arg_count] = NULL;
    // int argI = 0;
    // while (argI < arg_count && args[argI] != NULL) {
    //     fprintf(stderr, "arg %i: %s\n", argI, args[argI]);
    //     argI++;
    // }
    
    
    if (strcmp(cmd->commands[0][0], "jobs") == 0)           /* jobs */
    {
        print_j_list(jobs_list);
        free(cmd);
        return 0;
    }
    if (strcmp(cmd->commands[0][0], "fg") == 0)        /* fg */
    {
        // Check string if it has job id and get it if it does
        // we can't use the tokenized commands because we don't know
        // their size. Extract the first int we see or get 0 (invalid job)

        int job_id = extractInt(str);
        if (job_id)
        {
            struct Job *j = find_job_with_id(jobs_list, job_id);
            if (j == NULL)
            {
                fprintf(stderr, "fg invalid: %d: no such job\n", job_id);
            }
            else
            {
                if (j->status == J_RUNNING)
                {
                    j->ground = FG;
                    fprintf(stderr, "%s\n", j->rawCmd);
                    // tcsetpgrp(STDOUT_FILENO, j->group_id);
                    fg_pipeline(j);
                }
                else if (j->status == J_STOPPED)
                {
                    j->ground = FG;
                    j->status = J_RUNNING;
                    fprintf(stderr, "Restarting: %s\n", j->rawCmd);
                    // tcsetpgrp(STDIN_FILENO, j->group_id);
                    if (p_tcset(j->group_id) == -1)
                    {
                        p_perror("tc issue");
                        return -1;
                    }
                    if (p_kill(j->group_id, S_SIGCONT) == -1)
                    {
                        if (p_tcset(1) == -1)
                        {
                            p_perror("tc issue");
                            return -1;
                        }
                        p_perror("p_kill SIGCONT failed");
                    }
                    fg_pipeline(j);
                }
                else if (j->status == J_FINISHED)
                {
                    fprintf(stderr, "fg invalid: %d: no such job\n", job_id);
                    fprintf(stderr, "Finished: %s\n", j->rawCmd);
                    remove_job(jobs_list, j);
                }
            }
        }
        else
        {
            if (jobs_list->size == 0)
            {
                fprintf(stderr, "fg invalid: no jobs in background\n");
                free(cmd);
                return 0;
            }
            struct Job *j = jobs_list->tail;
            struct Job *last_stopped_job = find_last_stopped_job(jobs_list);
            if (last_stopped_job != NULL)
            {
                j = last_stopped_job;
            }
            if (j == NULL)
            {
                fprintf(stderr, "fg invalid: %d: no such job\n", job_id);
            }
            else
            {
                if (j->status == J_RUNNING)
                {
                    // debug("j is running");
                    j->ground = FG;
                    fprintf(stderr, "%s\n", j->rawCmd);
                    // tcsetpgrp(STDOUT_FILENO, j->group_id);
                    if (p_tcset(j->group_id) == -1)
                    {
                        perror("tc issue");
                        return -1;
                    }
                    fg_pipeline(j);
                }
                else if (j->status == J_STOPPED)
                {
                    // debug("j is stopped");
                    j->ground = FG;
                    j->status = J_RUNNING;
                    fprintf(stderr, "Restarting: %s\n", j->rawCmd);
                    // tcsetpgrp(STDOUT_FILENO, j->group_id);
                    // tcsetpgrp(STDIN_FILENO, j->group_id);
                    if (p_tcset(j->group_id) == -1)
                    {
                        perror("tc issue");
                        return -1;
                    }
                    if (p_kill(j->group_id, S_SIGCONT) == -1)
                    {
                        // tcsetpgrp(STDOUT_FILENO, getpid());
                        // tcsetpgrp(STDIN_FILENO, getpid());
                        if (p_tcset(1) == -1)
                        {
                            perror("tc issue");
                            return -1;
                        }
                        perror("killpg SIGCONT failed");
                    }
                    fg_pipeline(j);
                }
                else if (j->status == J_FINISHED)
                {
                    // debug("j is finished");
                    fprintf(stderr, "fg invalid: %d: no such job\n", job_id);
                    fprintf(stderr, "Finished: %s\n", j->rawCmd);
                    remove_job(jobs_list, j);
                }
            }
        }
        free(cmd);
        return 0;
    }
    if (strcmp(cmd->commands[0][0], "bg") == 0)        /* bg */
    {
        int job_id = extractInt(str);
        if (job_id)
        {
            struct Job *j = find_job_with_id(jobs_list, job_id);
            if (j == NULL)
            {
                fprintf(stderr, "bg invalid: %d: no such job\n", job_id);
                free(cmd);
                return 0;
            }
            if (j->status == J_RUNNING)
            {
                fprintf(stderr, "bg invalid: %d: job already running in background\n", job_id);
                free(cmd);
                return 0;
            }
            j->status = J_RUNNING;
            if (p_kill(j->group_id, S_SIGCONT) == -1)
            {
                perror("killpg failed");
            }
            else
            {
                fprintf(stderr, "Running: %s\n", j->rawCmd);
            }
        }
        else
        {
            if (jobs_list->size == 0)
            {
                fprintf(stderr, "bg invalid: no jobs in background\n");
                free(cmd);
                return 0;
            }
            struct Job *last_stopped_job = find_last_stopped_job(jobs_list);
            if (last_stopped_job == NULL)
            {
                fprintf(stderr, "bg invalid: no stopped job in background\n");
            }
            else
            {
                if (p_kill(last_stopped_job->group_id, S_SIGCONT) == -1)
                {
                    perror("killpg failed");
                }
                else
                {
                    last_stopped_job->status = J_RUNNING;
                    fprintf(stderr, "Running: %s\n", last_stopped_job->rawCmd);
                }
            }
        }
        free(cmd);
        return 0;
    }
    // nice priority command [arg] (S)
    // set priority of command to priority, and execute command
    if (strcmp(cmd->commands[0][0], "nice") == 0)
    {
        int priority = atoi(cmd->commands[0][1]);
        if (priority  < -1 || priority > 1)
        {
            p_perror("Invalid nice value");
            free(cmd);
            return 0;
        }
        p_nice_next(priority);
        // Isolate command to be executed; get pid of created process through return value of parse()
        int prefixLen = strlen(cmd->commands[0][0]) + strlen(cmd->commands[0][1]);
        char *argCmd = str + prefixLen + 2;
        // dprintf(2, "argCmd: %s\n", argCmd);
        free(cmd);
        return parse(argCmd);
    }
    // nice_pid priority pid (S) 
    // adjust the nice level of process pid to priority priority.
    else if (strcmp(cmd->commands[0][0], "nice_pid") == 0)
    {
        int priority = atoi(cmd->commands[0][1]);
        pid_t pid = atoi(cmd->commands[0][2]);

        if (priority >= -1 && priority <= 1) {
            if (p_nice(pid, priority) == -1)
            {
                p_perror("nice_pid failed");
                // _exit(EXIT_FAILURE);
            }
        }
        else {
            p_perror("Invalid priority value entered");
        }
            
        free(cmd);
        // continue;
        return 0;
    }
    // man (S) 
    // list all available commands.
    else if (strcmp(cmd->commands[0][0], "man") == 0)
    {
        printf("cat:\t\tThe usual cat from bash, etc.\n");
        printf("sleep:\t\tsleep for n seconds.\n");
        printf("busy:\t\tbusy wait indefinitely.\n");
        printf("echo:\t\tsimilar to echo(1) in the VM.\n");
        printf("ls:\t\tlist all files  in the working directory (similar to ls -il in bash), same formatting as ls in the standalone PennFAT\n");
        printf("touch:\t\tcreate an empty file if it does not exist, or update its timestamp otherwise.\n");
        printf("mv:\t\trename source to destination.\n");
        printf("cp:\t\tcopy source to destination.\n");
        printf("rm:\t\tremove files.\n");
        printf("chmod:\t\tsimilar to chmod(1) in the VM.\n");
        printf("ps:\t\tlist all processes on PennOS. Display grid, ppid, and priority.\n");
        printf("kill:\t\tsend the specified signal to the specified processes, where signal is either term (the default), stop, or cont, corresponding to S_SIGTERM, S_SIGSTOP, and S_SIGCONT, respectively. Similar to /bin/kill in the VM.\n");
        printf("zombify:\tspawn a zombie child.\n");
        printf("orphanify:\tspawn an orphan child.\n");
        printf("nice priority:\tset the priority of the command to priority and execute the command.\n");
        printf("nice_pid:\tadjust the nice level of process pid to priority.\n");
        printf("man:\t\tlist all available commands.\n");
        printf("bg:\t\tcontinue the last stopped job, or the job specified by the id.\n");
        printf("fg:\t\tbring the last stopped or backgrounded job to the foreground, or the job specified by the id.\n");
        printf("jobs:\t\tlist all jobs.\n");
        printf("logout:\t\texit the shell and shutdown PennOS.\n");
        free(cmd);
        // continue;
        return 0;
    }
    // logout (S) exit the shell and shutdown PennOS.
    else if (strcmp(cmd->commands[0][0], "logout") == 0)
    {
        free(cmd);
        p_shutdown();
        // exit(EXIT_SUCCESS);
    }

    // ---------------------- INDEPENDENTLY SCHEDULED PROCESSES ----------------------
    else if (strcmp(cmd->commands[0][0], "cat") == 0)       /* cat */
    {
        func = f_cat;
    }
    else if (strcmp(cmd->commands[0][0], "sleep") == 0)     /* sleep */
    {
        func = sleeps;
        if (arg_count != 2)
        {
            // set err num for p_perror in interactive_shell
            perror("Missing args");
            return -1;
        }
        if (atoi(args[1]) <= 0)
        {
            perror("Invalid args");
            return -1;
        }
    }
    else if (strcmp(cmd->commands[0][0], "busy") == 0)      /* busy */
    {
        func = busy_process;
    } 
    else if (strcmp(cmd->commands[0][0], "echo") == 0)      /* echo */
    {
        // if (arg_count < 2) return 0;
        // int i = 2;
        // while (cmd->commands[0][i] != NULL) {
        //     // fprintf(stderr, "i: %i\tcmd: %s\n", i, cmd->commands[0][i]);
        //     strcat(args[1], cmd->commands[0][i]);
        //     int len = strlen(args[1]);
        //     args[1][len] = ' ';
        //     args[1][len + 1] = '\0';
        //     i++;
        // }
        func = echo;

        // TODO: add space after first arg (original args[1]) without overwriting args[2]
    }
    else if (strcmp(cmd->commands[0][0], "ls") == 0) /* ls */
    {
        func = f_ls;
    }
    else if (strcmp(cmd->commands[0][0], "chmod") == 0) /* chmod */
    {
        func = f_chmod;
    }
    else if (strcmp(cmd->commands[0][0], "touch") == 0) /* touch */
    {
        func = f_touch;
    }
    else if (strcmp(cmd->commands[0][0], "mv") == 0) /* mv */
    {
        func = f_mv;
    }
    else if (strcmp(cmd->commands[0][0], "cp") == 0) /* cp */
    {
        func = f_copy;
    }
    else if (strcmp(cmd->commands[0][0], "rm") == 0) /* rm */
    {
        func = f_rm;
    }
    else if (strcmp(cmd->commands[0][0], "ps") == 0) /* ps */
    {
        func = print_all_process_info;
    }
    else if (strcmp(cmd->commands[0][0], "kill") == 0)          /* kill */
    {
        // int pid = 0;
        // int sig = S_SIGTERM;
        
        if (arg_count < 2) {
            free(cmd);
            return 0;
        }
        // if (arg_count < 3) 
        //     pid = atoi(cmd->commands[0][1]);
        // else {
        //     pid = atoi(cmd->commands[0][2]);
        //     if (strcmp(cmd->commands[0][1], "-term") == 0) {
        //         sig = S_SIGTERM;
        //     }
        //     else if (strcmp(cmd->commands[0][1], "-stop") == 0) {
        //         sig = S_SIGSTOP;
        //     }
        //     else if (strcmp(cmd->commands[0][1], "-cont") == 0) {
        //         sig = S_SIGCONT;
        //     }
        // }
        // return handle_command(cmd, str, func, args);
        int val = handle_kills(args, arg_count);
        free(cmd);
        return 0;
        // args[1] = pid;
        // args[2] = sig;
    }
    else if (strcmp(cmd->commands[0][0], "dummy") == 0)         /* dummy */
    {
        func = dummy;
        if (arg_count != 2)
        {
            // set err num for p_perror in interactive_shell
            perror("Missing args");
            return -1;
        }
        // free(cmd); Figure out where to free cmd later
    }
    else if (strcmp(args[0], "zombify") == 0) {
        func = zombify;
    } 
    // orphanify(S*)
    else if (strcmp(args[0], "orphanify") == 0) {
        func = orphanify;
    }
    else if (strcmp(args[0], "hang") == 0)
    {
        hang();
        free(cmd);
        return 0;
    }
    else if (strcmp(args[0], "nohang") == 0)
    {
        nohang();
        free(cmd);
        return 0;
    }
    else if (strcmp(args[0], "recur") == 0)
    {
        recur();
        free(cmd);
        return 0;
    }
    else                                                        /* invalid cmd */
    {
        //set err num
        p_perror("Invalid cmd");
        return 0;
    }
    handle_command(cmd, str, func, args); 
    return 0;
    // return 0;
}

void interactive_shell()
{
    if (start)
    {
        jobs_list = create_j_queue();
        start = false;
    }

    while (1)
    {
        has_terminal_control = true;
        p_tcset(1);
        prompt();
        char command[MAX_LEN];
        memset(command, 0, MAX_LEN);
        int read_bytes = custom_read(STDIN_FILENO, command, MAX_LEN);
        if (read_bytes < 0)
        {
            perror("Reading error");
            _exit(EXIT_FAILURE);
        }
        poll_background_jobs();
        if (read_bytes == 0)
        { // To exit shell when ctrl+d is input as first char
            newline();
            free_j_queue(jobs_list);
            if (parse("logout\0") == -1)
                p_perror("Couldn't exit. Please try again");
            // _exit(EXIT_SUCCESS);
        }
        if (command[0] == '\n')
        {
            print_finished();
            continue;
        }
        // To re-prompt if ctrl-d is entered after something else in the shell
        if (command[read_bytes - 1] != '\n')
        {
            newline();
            print_finished();
            continue;
        }
        // Process input command
        command[read_bytes - 1] = '\0';
        if (parse(command) == -1)
        {
            // p_perror
            print_finished();
            prompt();
        }
        else
            print_finished();

    }
}



void fg_pipeline(struct Job *j)
{
    bool terminated = false, exited = false;
    int status = 0;
    // fprintf(stderr, "fg_pipeline has_tc: %d\tPID: %d\n", get_active_pcb()->has_tc, get_active_pcb()->pid);
    // fprintf(stderr, "fg_pipeline has_tc: %d\tCHILD PID: %d\n", get_active_pcb()->children_pcb->head->has_tc, get_active_pcb()->children_pcb->head->pid);
    // wait until the child is finished, stopped or terminated
    if (p_waitpid(j->group_id, &status, false) == -1)
    {
        p_perror("waitpid error");
    }
    if (W_WIFEXITED(status))
    { // normal exit
        exited = true;
    }
    else if (W_WIFSIGNALED(status))
    { // child terminated by signal
        // p_tcset(1);
        terminated = true;
    }
    else if (W_WIFSTOPPED(status))
    {
        // child stopped
        j->status = J_STOPPED;
        j->ground = BG;
        fprintf(stderr, "\nStopped: %s\n", j->rawCmd);
    }
    p_tcset(1);
    if (terminated)
    {
        j->status = J_TERMINATED;
        print_finished();
    }
    else if (exited)
    { // Entire pipeline executed and exited
        remove_job(jobs_list, j);
        // p_tcset(1);
    }
    
}

void print_finished()
{
    struct Job *curr = jobs_list->head;
    while (curr != NULL)
    {
        if (curr->status == J_FINISHED)
        {
            fprintf(stderr, "Finished: %s\n", curr->rawCmd);
            struct Job *temp = curr;
            curr = curr->next;
            remove_job(jobs_list, temp);
        }
        else if (curr->status == J_TERMINATED)
        {
            p_tcset(1);
            struct Job *temp = curr;
            curr = curr->next;
            remove_job(jobs_list, temp);
        }
        else
        {
            curr = curr->next;
        }
    }
}

void poll_background_jobs()
{
    struct Job *curr = jobs_list->head;
    while (curr != NULL)
    {
        if (curr->status == J_RUNNING)
        {
            int num_commands = curr->cmd->num_commands, commands_finished = curr->count_finished;
            for (int i = 0; i < num_commands - commands_finished; i++)
            {
                int status = -1;
                pid_t pid1 = p_waitpid(curr->group_id, &status, true);
                if (pid1 == -1)
                {
                    perror("waitpid failed");
                    exit(EXIT_FAILURE);
                }
                if (W_WIFEXITED(status))
                {
                    curr->count_finished++;
                }
                if (W_WIFSTOPPED(status))
                {
                    curr->status = J_STOPPED;
                    fprintf(stderr, "\nStopped: %s\n", curr->rawCmd);
                    break;
                }
            }
            if (num_commands == curr->count_finished)
            {
                curr->status = J_FINISHED;
            }
        }
        curr = curr->next;
    }
}

int main(int argc, char *argv[])
{
    p_boot_kernel();
    // printf("%s\n",argv[1]);
    fatfs_init(argv[1]); //please do not delete it. Needed for file system
    spawn_shell();
    f_unmount();
    return 0;
}

int handle_kills(char *args[], int arg_count)
{
    // TODO: Parser args and use handle_command with func kill_as_process
    return 0;
}



