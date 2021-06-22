/**
 * MULTITHREAD FILESYSTEM TEST
 * This has to be compiled using '-fno-stack-protector' to allow heavy workload
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <queue.h>
#include <string.h>
#include <utils.h>
#include <server.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define MSG_PERROR(s) perror(ANSI_COLOR_CYAN s ANSI_COLOR_RESET);
#define ONLY_MSG_ERR ANSI_COLOR_CYAN "INTERNAL" ANSI_COLOR_RESET

// READ CONFIG FILE
// #define TEST_LOG
#define TEST_FILESYS
#define LOG_TEST 16384

// TEST FILESYSTEM
#define NCLIENTS 4
#define NFILES 10
#define FDSTART 20

/*sprintf(ret, "%d_%d_" #s "_%s", i, s, strerror_r(errno,buf,100)); // questo resetta errno*/
#define log(s, i)                                                        \
    res = (s);                                                           \
    strerror_r(errno, buf, 200);                                         \
    if (res == -1)                                                       \
    {                                                                    \
        perror(ANSI_COLOR_CYAN "INTERNAL FATAL ERROR" ANSI_COLOR_RESET); \
        exit(EXIT_FAILURE);                                              \
    }                                                                    \
    sprintf(ret, "%d__%d__" #s "__%s", i, res, buf);                     \
    puts(ret);                                                           \
    LoggerLog(ret, strlen(ret));

void initPaths(char **paths);
void initClients(Client **clients);
void initEvicted(evictedFile **evicted);
void freeArr(void **arr, size_t n, void (*myfree)(void *arg));
Client *rClient();
char *rFile();

char *gen_random(char *s, const int len);
void *fakeWorker(void *arg);

// SCAFFOLDING
#define NROUTINES 5
#define ITERATIONS 1000
#define MSGLEN 15
#define NWORKERS 20

// STUBS
Client *clients[NCLIENTS];
char *paths[NFILES];

int main(void)
{
    errno = 0;
    srand(time(NULL));

    // STUBS init
    initPaths(paths);
    initClients(clients);

    LoggerCreate("log.txt");
    storeInit(100, 100, 1);

    // PseudoWorker
    pthread_t workers[NWORKERS];
    for (int i = 0; i < NWORKERS; i++)
    {
        ec_nz(pthread_create(&workers[i], NULL, fakeWorker, &i), exit(EXIT_FAILURE));
    }

    for (int i = 0; i < NWORKERS; i++)
    {
        ec_nz(pthread_join(workers[i], NULL), exit(EXIT_FAILURE));
    }

    // free stuff
    storeDestroy();
    freeArr((void **)paths, NFILES, NULL);
    freeArr((void **)clients, NCLIENTS, NULL);
    LoggerFlush();
    LoggerDelete();

    MSG_PERROR("messaggio");
    perror(ONLY_MSG_ERR);
    return 0;
}

void *fakeWorker(void *arg)
{
    int t = *((int *)arg);
    Client *c;
    char *f;
    char msg[MSGLEN], ret[400], buf[200];
    int res = 0;
    queue *list = NULL;
    evictedFile *victim = NULL;
    *buf = '\0';
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        // printf("IT\t%d\n",t);
        c = rClient();
        f = rFile();

        int routine = rand() % NROUTINES;
        switch (routine)
        {
        case 0: //append
            LoggerLog("BEGIN-rt0", strlen("BEGIN-rt0"));
            log(openFile(f, 0, 0, c, NULL), t);
            log(appendToFile(f, gen_random(msg, MSGLEN), MSGLEN, c, &list, 0), t);
            if (list)
            {
                queueDestroy(list);
                list = NULL;
            }
            log(closeFile(f, c), t);
            LoggerLog("END-rt0", strlen("END-rt0"));
            break;
        case 1: //readN
            LoggerLog("BEGIN-rt1", strlen("BEGIN-rt1"));
            log(readNfiles(10, &list, c), t);
            if (list)
            {
                queueDestroy(list);
                list = NULL;
            }
            LoggerLog("END-rt1", strlen("END-rt1"));
            break;
        case 2: //lock
            LoggerLog("BEGIN-rt2", strlen("BEGIN-rt2"));
            log(lockFile(f, c), t);
            log(lockFile(rFile(), c), t);
            log(lockFile(rFile(), c), t);
            log(unlockFile(f, c), t);
            log(unlockFile(rFile(), c), t); //NOTE:
            log(unlockFile(rFile(), c), t); // These two aren't the ones above
            LoggerLog("END-rt2", strlen("END-rt2"));
            break;
        case 3: //write
            LoggerLog("BEGIN-rt3", strlen("BEGIN-rt3"));
            log(openFile(f, 1, 1, c, NULL), t);
            log(appendToFile(f, gen_random(msg, MSGLEN), MSGLEN, c, &list, 1), t);
            if (list)
            {
                queueDestroy(list);
                list = NULL;
            }
            log(closeFile(f, c), t);
            LoggerLog("END-rt3", strlen("END-rt3"));
            break;
        case 4: //remove
            LoggerLog("BEGIN-rt4", strlen("BEGIN-rt4"));
            log(removeFile(f, c, &victim), t);
            if (victim)
            {
                freeEvicted(victim);
                victim = NULL;
            }
            LoggerLog("END-rt4", strlen("END-rt4"));
            break;
        default:
            break;
        }
    }
    return NULL;
}

#define PATH_MSG "path"
void initPaths(char **paths /*, char **contents*/)
{
    puts("InitFiles");
    for (size_t i = 0; i < NFILES; i++)
    {
        char tmp[20];
        sprintf(tmp, "%ld", i);
        assert(paths[i] = calloc(strlen(PATH_MSG) + strlen(tmp) + 1, sizeof(char)));
        strcat(paths[i], PATH_MSG);
        strcat(paths[i], tmp);
        // (paths[i])[strlen(PATH_MSG) + strlen(tmp)] = '\0';
        printf("P%ld:%s \n", i, paths[i]);
    }
}

void initEvicted(evictedFile **evicted)
{
    puts("InitEvicted");
    for (size_t i = 0; i < NFILES; i++)
    {
        // assert(evicted[i] = malloc(sizeof(evictedFile)));
        evicted[i] = NULL;
    }
}

void initClients(Client **clients)
{
    puts("InitClients");
    for (size_t i = 0; i < NCLIENTS; i++)
    {
        assert(clients[i] = malloc(sizeof(Client)));
        clients[i]->fd = FDSTART + i;
    }
}

void freeArr(void **arr, size_t n, void (*myfree)(void *arg))
{
    for (size_t i = 0; i < n; i++)
    {
        if (arr[i])
            myfree ? myfree(arr[i]) : free(arr[i]);
    }
}

Client *rClient()
{
    return clients[rand() % NCLIENTS];
}

char *rFile()
{
    return paths[rand() % NFILES];
}

char *rContent()
{
}

char *gen_random(char *s, const int len)
{
    static const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i)
    {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
    return s;
}
// void initRead(char **contents) {
//     puts("InitRead");
//     for (size_t i = 0; i < NFILES; i++)
//     {
//         assert(contents[i] = malloc(sizeof(evictedFile)));
//     }
// }