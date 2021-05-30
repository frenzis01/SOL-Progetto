#include <stdio.h>
#include <queue.h>
#include <string.h>
#include <utils.h>
#include <server.h>

#include <math.h> // calculate size_t->char[] len

#include <assert.h>

// READ CONFIG FILE
#define TEST_LOG
#define TEST_FILESYS
#define LOG_TEST 16384

void initFiles(char **paths, char **contents);
void initClients(Client **clients);

int main(void)
{
// TEST FILESYSTEM
#ifdef TEST_FILESYS
#define NCLIENTS 4
#define NFILES 10
#define FDSTART 20

    // Create some clients
    Client *clients[NCLIENTS];
    evictedFile *evicted[NFILES];

    //Create some files
    char *paths[NFILES];
    char *contents[NFILES];

    initFiles(paths, contents);
    initClients(clients);

    storeInit(100, 10000);

    for (size_t i = 0; i < NFILES; i++)
    {
        openFile(paths[i],1,0,clients[i%NCLIENTS],NULL,evicted[0]);
    }

#endif

// TEST LOG
#ifdef LOGTEST
    int myerr = 0;
    LoggerCreate(&myerr, "log.txt");
    LoggerLog("FIRST LINE ", strlen("FIRST LINE "), &myerr);
    LoggerFlush(&myerr);

    for (size_t i = 1; i < LOG_TEST; i++)
    {

        char buf[2000] = "Some stuff ";
        char nbuf[2000];
        sprintf(nbuf, "%ld", i);
        strcat(buf, nbuf);

        LoggerLog(buf, strlen(buf), &myerr);
    }
    LoggerFlush(&myerr);

    LoggerDelete(&myerr);
#endif
    return 0;
}

#define PATH_MSG "path "
void initFiles(char **paths, char **contents)
{
    for (size_t i = 0; i < NFILES; i++)
    {
        char tmp[20];
        sprintf(tmp, "%ld", i);
        assert(paths[i] = calloc(strlen(PATH_MSG) + strlen(tmp) + 1, sizeof(char)));
        strcat(paths[i], PATH_MSG);
        strcat(paths[i], tmp);
        printf("P%ld:%s", i, paths[i]);
    }
}

void initEvicted(evictedFile **evicted)
{
    for (size_t i = 0; i < NFILES; i++)
    {
        assert(evicted[i] = malloc(sizeof(evictedFile)));
    }
}

void initClients(Client **clients)
{
    for (size_t i = 0; i < NCLIENTS; i++)
    {
        clients[i]->fd = FDSTART + i;
    }
}