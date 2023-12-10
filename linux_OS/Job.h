#ifndef JOB_H
#define JOB_H

#include "parser.h"
#include <stdlib.h>

static int JOBCOUNT = 1;

enum jobStatus
{
    J_RUNNING,
    J_STOPPED,
    J_FINISHED,
    J_TERMINATED
};
enum jobGround
{
    BG,
    FG
};

struct Job
{
    struct parsed_command *cmd;
    char *rawCmd; // the unparsed command as a single string
    int group_id;
    struct Job *next;
    struct Job *prev;
    int job_id;
    enum jobStatus status;
    enum jobGround ground;
    int count_finished;
};

struct Job *create_job(struct parsed_command *cmd,
                       char *rawCmd,
                       int group_id,
                       enum jobStatus status,
                       enum jobGround ground);
void free_job(struct Job *job);
#endif