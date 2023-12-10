#include "utils.h"

void memsetter(char *buf, char val, unsigned long int len)
{
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = val;
    }
}

void print_out(char *str, int len)
{
    if (!write(STDERR_FILENO, str, len))
    {
        perror("Writing error");
        _exit(EXIT_FAILURE);
    }
}

void debug(char *msg)
{
    if (DEBUG)
    {
        fprintf(stderr, "%s\n", msg);
    }
}

int extractInt(char str[])
{
    // extract the first digit from the passed string
    // returns zero on failure (as zero is invalid for
    // the intended use case)
    int maybeInt;

    // copy the string onto the heap because strtok modifies it
    int strLength = strlen(str);
    char newStr[strLength + 1];
    memsetter(newStr, 0, strLength + 1);
    for (int i = 0; i < strLength; i++)
    {
        newStr[i] = str[i];
    }

    // check each token. If it's a number, return that
    char *token = strtok(newStr, " ");
    while (token != NULL)
    {
        // atoi returns 0 for non-digit strings
        maybeInt = atoi(token);
        if (maybeInt)
        {
            if (maybeInt < 1)
                return 0;
            return maybeInt;
        }
        token = strtok(NULL, " ");
    }

    return 0;
}


void string_copy(const char *from, char *to)
{
    for (int i = 0; i < strlen(from); i++)
        to[i] = from[i];
}

int myStrNCpy(char *dest, char *src, int n)
{
    // replicating the behavior of strncpy as we are not allowed to use it
    if (DEBUG && strlen(src) > n)
    {
        char *msg = "warning: truncating string during copy";
        write(STDERR_FILENO, msg, strlen(msg));
    }

    int i = 0;
    for (char *c = src; i < n && *c != '\0'; c++)
    {
        dest[i] = *c;
        i++;
    }

    return 0;
}

void prompt() {
    fprintf(stderr, "$ ");
}

void newline() {
    fprintf(stderr, "\n");
}
