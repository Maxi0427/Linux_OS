#ifndef SHELL_H
#define SHELL_H

#include "user.h"
#include "Job.h"
#include "j_queue.h"
#include "f.h"
#include "utils.h"
#include "stress.h"

enum SHELL_MODE {
    INTERACTIVE,
    SHELL_SCRIPT
};

void print_finished();
void poll_background_jobs();
void fg_pipeline(struct Job *j);

int parse(char *str);

/**
 * @brief Used to handle kill command
 * @param args The input command line arguments
 * @param arg_count The number of arguments
 * @return -1 on error, 0 otherwise
*/
int handle_kills(char *args[], int arg_count);
#endif