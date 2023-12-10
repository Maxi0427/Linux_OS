#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "parser.h"

#define DEBUG 0

void memsetter(char *buf, char val, unsigned long int len);
void print_out(char *str, int len);
void debug(char *msg);

void string_copy(const char *from, char *to);
int extractInt(char str[]);
int myStrNCpy(char *dest, char *src, int n);

void prompt();

void newline();

#endif