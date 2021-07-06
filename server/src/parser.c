#define _POSIX_C_SOURCE 200809L
#include <parser.h>

#define CHK_MULT(o)                                                                            \
    if (queueFind(opQ, &op, NULL))                                                             \
    {                                                                                          \
        puts(ANSI_COLOR_RED "Option -" #o " is specified multiple times.\n" ANSI_COLOR_RESET); \
        /*printUsage(argv[0]);*/                                                               \
        return -1;                                                                             \
    }

#define CHK_PREV_W                                                                                      \
        if (!(*opList)->tail || (((Option *)((*opList)->tail->data))->flag != 'w' &&                    \
                                 ((Option *)((*opList)->tail->data))->flag != 'W'))                     \
        {                                                                                               \
            puts(ANSI_COLOR_RED "Option -D requires -W or -w as precedent option.\n" ANSI_COLOR_RESET); \
            continue;                                                                                   \
        }                                                                                               \

#define CHK_PREV_R                                                                                      \
        if (!(*opList)->tail || (((Option *)((*opList)->tail->data))->flag != 'r' &&                    \
                                 ((Option *)((*opList)->tail->data))->flag != 'R'))                     \
        {                                                                                               \
            puts(ANSI_COLOR_RED "Option -d requires -R or -r as precedent option.\n" ANSI_COLOR_RESET); \
            continue;                                                                                   \
        }                                                                                               \

int storeArgs(queue **res, char *args);

/**
 * Parses client's command line and stores the gotten the result in an Option queue
 * @param argc: the number of items in argv
 * @param argv: the command line options and arguments array
 * @param opList: Option queue obtained from parsing
 * @return: 0 success, -1 failure
 */
int parser(int argc, char **argv, queue **opList)
{
    queue *opQ = *opList;
    Option *op = NULL;
    ec_z(*opList = queueCreate(freeSimpleOption, cmpFlagOption), free(op); return -1);
    /*parse command line*/
    int option = 0;
    while ((option = getopt(argc, argv, "--f:W:D:r:l:u:c:d:hR::t::p::w:E:")) != -1)
    {
        ec_z(op = malloc(sizeof(Option)), goto parser_cleanup);
        op->flag = option;
        op->arg = NULL;

        switch (option)
        {
        case 'D': /*If capacity misses occur on the server, save the files it gets to us in the specified dirname directory*/
            CHK_PREV_W;
            ec_z(op->arg = malloc(strlen(optarg) + 1), goto parser_cleanup);
            strncpy(op->arg, optarg, strlen(optarg) + 1);
            break;
        case 'f': /*Sets the socket file path up to the specified socketPath*/
            CHK_MULT(f);
            ec_z(op->arg = malloc(strlen(optarg) + 1), goto parser_cleanup);
            strncpy(op->arg, optarg, strlen(optarg) + 1);
            break;
        case 'd': /* Saves files read with -r or -R option in the specified dirname directory */
            CHK_PREV_R;
            ec_z(op->arg = malloc(strlen(optarg) + 1), goto parser_cleanup);
            strncpy(op->arg, optarg, strlen(optarg) + 1);
            break;
        case 'W': /*Sends to the server the specified files*/
        case 'r': /*Reads the specified files from the server*/
        case 'l': /* Locks the specified files */
        case 'u': /* Unlocks the specified files */
        case 'c': /* Removes the specified files from the server */
            ec_neg1(storeArgs((queue **)(&(op->arg)), optarg), goto parser_cleanup);
            break;
        case 'R': /* Reads n random files from the server (if n=0, reads every file) */
            if (optind < argc && argv[optind][0] != '-')
                optarg = argv[optind];
            if (optarg) // if n is specified
            {
                char *save = NULL,
                     *bkp,
                     *item;

                ec_z(bkp = malloc(strlen(optarg) + 1), goto parser_cleanup);
                strncpy(bkp, optarg, strlen(optarg) + 1);
                item = strtok_r(bkp, "n=", &save);

                // n
                ec_z(op->arg = malloc(sizeof(int)), goto parser_cleanup);
                if (!item || !isInteger(item, op->arg))
                {
                    puts(ANSI_COLOR_RED "-R option requires a path and (optionally) an integer.\n" ANSI_COLOR_RESET);
                    // printUsage(argv[0]);
                    free(op->arg);
                    free(op);
                    continue;
                }
                // TODO item strtok ok? free?
                free(bkp);
            }
            break;
        case 't': /* Specifies the time between two consecutive requests to the server */
            ec_z(op->arg = malloc(sizeof(int)), goto parser_cleanup);
            if (optind < argc && argv[optind][0] != '-')
                optarg = argv[optind];
            if (optarg && !isInteger(optarg, (int *)(op->arg)))
            {
                puts(ANSI_COLOR_RED "-t option requires an (optional) integer.\n" ANSI_COLOR_RESET);
                // printUsage(argv[0]);
                free(op->arg);
                free(op);
                continue; //TODO continue o goto parser_cleanup?
            }
            break;
        case 'p': /* Prints information about operation performed on the server */
            CHK_MULT(p);
            break;
        case 'w': /* Sends to the server n files from the specified dirname directory. If n=0, sends every file in dirname */
        {
            char *save = NULL,
                 *item,
                 *bkp,
                 *dir;
            int *n = NULL;
            // op->arg will be a queue with two nodes: dirname :: n
            ec_z(op->arg = queueCreate(free, NULL), goto parser_cleanup);

            ec_z(bkp = malloc(strlen(optarg) + 1), goto parser_cleanup);
            strncpy(bkp, optarg, strlen(optarg) + 1);
            item = strtok_r(bkp, ",", &save);

            if (item)
            {
                // dir
                ec_z(dir = malloc(strlen(item) + 1), goto parser_cleanup);
                strncpy(dir, item, strlen(item) + 1);
                ec_nz(queueEnqueue((queue *)(op->arg), dir), goto parser_cleanup);
                // n
                item = strtok_r(NULL, "n=", &save);
            }
            if (item)
            {
                ec_z(n = malloc(sizeof(int)), goto parser_cleanup);

                if (!isInteger(item, n))
                {
                    puts(ANSI_COLOR_RED "-w option requires a path and (optionally) an integer.\n" ANSI_COLOR_RESET);
                    // printUsage(argv[0]);
                    queueDestroy(op->arg);
                    free(op);
                    continue;
                }
            }
            ec_nz(queueEnqueue((queue *)(op->arg), n), goto parser_cleanup);
            // TODO item strtok ok? free?
            free(bkp);
            break;
        }
        case 'E': /* Sets default dir for evicted files */
        {
            if (optind < argc && argv[optind][0] != '-')
                optarg = argv[optind];
            if (optarg)
            {
                ec_z(op->arg = malloc(strlen(optarg) + 1), goto parser_cleanup);
                strncpy(op->arg, optarg, strlen(optarg) + 1);
            }
            break;
        }
        case '?':
            printf(ANSI_COLOR_RED "Unknown option '-%c'\n" ANSI_COLOR_RESET, optopt);
            // printUsage(argv[0]);
            free(op);
            continue;
        case 'h':
            CHK_MULT(h);
            break;
        default:
            free(op);
            continue;
        }
        ec_nz(queueEnqueue(*opList, op), goto parser_cleanup;);
    }
    return 0;

parser_cleanup:
    free(op);
    op = NULL;
    queueDestroy(*opList);
    return -1;
}

/**
 * Free what's inside str_arr and the str_arr itself
 * @param str_arr: the array to free
 * @param i_max: the length of the str_arr
 */
void freeStuff(char **str_arr, int i_max)
{
    int i = 0;
    while (str_arr != NULL && i < i_max)
    {
        /* printf("%s , ", str_arr[i]); */
        free(str_arr[i++]);
    }
    if (str_arr != NULL)
        free(str_arr);
    /* puts(""); */
}

int cmpString(void *a, void *b)
{
    if (strcmp((char *)a, (char *)b) == 0)
        return 1;
    return 0;
}

/**
 * Store arg in *str_arr_ptr. arg is a list of comma-separated strings.
 * @param res: pointer to the queue where we will store the option arguments
 * @param arg: the comma-separated strings
 * @return: 0 success, -1 error
 */
int storeArgs(queue **res, char *args)
{
    char *save = NULL,
         *item,
         *bkp = NULL,
         *newArg = NULL;

    ec_z(*res = queueCreate(free, cmpString), return -1);

    ec_z(bkp = malloc(strlen(args) + 1), goto storeArgs_cleanup);
    strncpy(bkp, args, strlen(args) + 1);

    item = strtok_r(bkp, ",", &save);
    while (item)
    {
        ec_z(newArg = malloc(sizeof(char) * strlen(item) + 1), goto storeArgs_cleanup);
        strncpy(newArg, item, strlen(item) + 1);
        ec_nz(queueEnqueue(*res, newArg), goto storeArgs_cleanup);

        item = strtok_r(NULL, ",", &save);
    }
    free(bkp);
    return 0;

storeArgs_cleanup:
    free(newArg);
    newArg = NULL;
    free(bkp);
    queueDestroy(*res);
    return -1;
}



// TODO "  -E,\tSets the default eviction dir. If no dir is specified removes the current default dir"
/**
 * Print the usage of this program
 * @param exe: the name of the executable
 */
void printUsage(char *exe)
{
    printf("\nUsage: %s -h\n"
           "Usage: %s -f socketPath [-w dirname [n=0]] [-Dd dirname] [-p]"
           "[-t [0]] [-R [n=0]] [-Wrluc file1 [,file2,file3 ...]]\n"
           "Options:\n"
           "  -h,\tPrints this message\n"
           "  -E,\tSets the dir for openFile and removeFile eviction. If no dir is specified removes the current dir"
           "  -f,\tSets the socket file path up to the specified socketPath\n"
           "  -w,\tSends to the server n files from the specified dirname directory. If n=0, sends every file in dirname\n"
           "  -W,\tSends to the server the specified files\n"
           "  -D,\tIf capacity misses occur on the server, save the files it gets to us in the specified dirname directory\n"
           "  -r,\tReads the specified files from the server\n"
           "  -R,\tReads n random files from the server (if n=0, reads every file)\n"
           "  -d,\tSaves files read with -r or -R option in the specified dirname directory\n"
           "  -t,\tSpecifies the time between two consecutive requests to the server\n"
           "  -l,\tLocks the specified files\n"
           "  -u,\tUnlocks the specified files\n"
           "  -c,\tRemoves the specified files from the server\n"
           "  -p,\tPrints information about operation performed on the server\n",
           exe, exe);
}

int cmpFlagOption(void *arg1, void *arg2)
{
    Option *a = arg1, *b = arg2;
    if (a->flag == b->flag)
        return 1;
    return 0;
}

void freeSimpleOption(void *arg)
{
    Option *o = arg;
    free(o->arg);
    free(arg);
}

void freeQueueOption(void *arg)
{
    Option *o = arg;
    queueDestroy((queue *)(o->arg));
    o->arg = NULL;
    free(arg);
}
