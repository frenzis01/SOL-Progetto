#define _POSIX_C_SOURCE 200809L

#include <parser.h>
#include <queue.h>

#include <stdlib.h>
#include <stdio.h>

void printFlags(void *arg);

int main(int argc, char **argv)
{
    int x = 0, *n;
    queue *optlist = NULL;
    char *arg = NULL;
    setenv("POSIXLY_CORRECT","0",1);

    x = parser(argc, argv, &optlist);
    printf("Parser Result : %d\n", x);
    // queue rundown
    Option *curr = queueDequeue(optlist);
    while (curr)
    {
        printf("-%c ", curr->flag);
        switch (curr->flag)
        {
        // case 'p': /* Prints information about operation performed on the server */
        case 'f': /*Sets the socket file path up to the specified socketPath*/
        case 'D': /*If capacity misses occur on the server, save the files it gets to us in the specified dirname directory*/
        case 'd': /* Saves files read with -r or -R option in the specified dirname directory */
            printf("%s", (char *)curr->arg);
            free(curr->arg);
            break;
        case 'W': /*Sends to the server the specified files*/
        case 'r': /*Reads the specified files from the server*/
        case 'l': /* Locks the specified files */
        case 'u': /* Unlocks the specified files */
        case 'c': /* Removes the specified files from the server */
            arg = queueDequeue(curr->arg);
            while (arg)
            {
                printf("%s,", arg);
                free(arg);
                arg = queueDequeue(curr->arg);
            }
            queueDestroy(curr->arg);
            break;
        case 'R': /* Reads n random files from the server (if n=0, reads every file) */
        case 'E':
        case 't': /* Specifies the time between two consecutive requests to the server */
            if (curr->arg)
                printf("%d", *(int *)(curr->arg));
            free(curr->arg);
            break;
        case 'w': /* Sends to the server n files from the specified dirname directory. If n=0, sends every file in dirname */
            //dequeue
            arg = queueDequeue(curr->arg);
            printf("%s,", arg);
            free(arg);
            n = queueDequeue(curr->arg);
            if (n)
                printf("%d", *n);
            free(n);
            queueDestroy(curr->arg);
            break;
        case 'h':
            printUsage(argv[0]);
            break;
        default:;
        }
        puts("");
        free(curr);
        curr = queueDequeue(optlist);
    }
    queueDestroy(optlist);
    return 0;
}

void printFlags(void *arg)
{
    Option *o = arg;
    printf("%c\n", o->flag);
    return;
}