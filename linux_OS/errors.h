#ifndef ERR_H
#define ERR_H

enum ERRORS
{
    DEFAULT,
    PID_INVALID
};

static int error_number = 0;

void p_perror(char *str);
void set_errno(enum ERRORS num);

#endif