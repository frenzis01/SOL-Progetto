#ifndef MY_PARSER
#define MY_PARSER

#include <queue.h>
#include <utils.h>

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>

typedef struct {
    char flag;
    void *arg;
} Option;

int parser(int argc, char **argv, queue **opList);

void printUsage(char *exe);

/**
 * @returns 1 if arg1->flag == arg2->flag, 0 otherwise
 */
int cmpFlagOption(void *arg1, void *arg2);

/**
 * Frees an option with a simple void* as arg
 */
void freeSimpleOption (void *arg);

/**
 * Frees an option with a queue* as arg
 */
void freeQueueOption (void *arg);


#endif