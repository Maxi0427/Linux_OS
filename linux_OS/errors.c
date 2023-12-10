#include "errors.h"
#include <stdio.h>
void set_errno(enum ERRORS num)
{
    error_number = num;
}

void p_perror(char *str)
{
    switch (error_number)
    {
    case PID_INVALID:
        dprintf(2, "Invalid PID: %s\n", str);
        break;
    default:
        dprintf(2, "%s\n", str);
        break;
    }
    set_errno(DEFAULT);
}