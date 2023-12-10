#include "Job.h"
#include "utils.h"
// #include <stdlib.h>

struct Job *create_job(struct parsed_command *cmd,
                       char *rawCmd,
                       int group_id,
                       enum jobStatus status,
                       enum jobGround ground)
{
    struct Job *j = (struct Job *)malloc(sizeof(struct Job));
    j->cmd = cmd;
    j->rawCmd = (char *)malloc(strlen(rawCmd) + 1);
    memsetter(j->rawCmd, 0, strlen(rawCmd) + 1);
    string_copy(rawCmd, j->rawCmd);
    j->group_id = group_id;
    j->status = status;
    j->ground = ground;
    j->count_finished = 0;
    j->next = NULL;
    return j;
}

void free_job(struct Job *job)
{
    free(job->cmd);
    free(job->rawCmd);
    free(job);
}