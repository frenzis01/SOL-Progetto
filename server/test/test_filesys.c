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

void initPaths(char **paths);
void initClients(Client **clients);
void initEvicted(evictedFile **evicted);
void freeArr(void **arr, size_t n, void (*myfree)(void *arg));
void freeEvicted(void *arg);

int main(void)
{
    int myerr = 0;
// TEST FILESYSTEM
#ifdef TEST_FILESYS
#define NCLIENTS 4
#define NFILES 10
#define FDSTART 20

    // Create some clients
    Client *clients[NCLIENTS];
    evictedFile *evicted[NFILES];
    queue *evictedfiles;

    //Create some files
    char *paths[NFILES];
    // char *contents[NFILES];

    initPaths(paths);
    initClients(clients);
    initEvicted(evicted);

    storeInit(100, 100);

    // OPENFILE O_CREAT,O_LOCK
    assert(!openFile(paths[0], 1, 1, clients[0], &evicted[0]));

    // OPENFILE senza O_LOCK
    for (size_t i = 1; i < NFILES; i++)
    {
        assert(!openFile(paths[i], 1, 0, clients[i % NCLIENTS], &evicted[0]));
        if (evicted[0])
        {
            printEvicted(evicted[0]);
            free(evicted[0]);
        }
    }

    queueCallback(store.files, printFnode); //LRU ok!

#define MSG "Content test 0"

    // WRITEFILE success
    //      nota: andrebbe fatta strlen(MSG)+1 per includere il carattere di terminazione
    assert(!appendToFile(paths[0], MSG, strlen(MSG), clients[0], &evictedfiles, 1));
    // printf("%d\n",appendToFile(paths[0], MSG, strlen(MSG), clients[0], &evictedfiles, 1));
    // perror("append");

    // WRITEFILE fails
    assert(2 == appendToFile(paths[0], '\0', 1, clients[0], &evictedfiles, 1));

    // READFILE
    assert(!readFile(paths[0], &evicted[1], clients[0], 0));
    // printf("\tREADFILE PATH: %.*s\n", (int)strlen(paths[0]), evicted[1]->path);
    // printf("\tREADFILE_CONTENT 1: %.*s\n", (int)evicted[1]->size, evicted[1]->content);

    freeEvicted(evicted[1]);

    // READNFILES
    queue *readFiles = NULL;
    readNfiles(0, &readFiles, clients[0]);
    // puts("\tREADNFILES RESULT");
    // queueCallback(readFiles,printEvicted);
    // puts("\tREADNFILES END");s
    queueDestroy(readFiles, &myerr);

    // APPENDTOFILE
    assert(!appendToFile(paths[0], MSG, strlen(MSG) + 1, clients[0], &evictedfiles, 0));

    // READNFILES
    readNfiles(0, &readFiles, clients[0]);
    //puts("\tREADNFILES RESULT");
    //queueCallback(readFiles,printEvicted);
    //puts("\tREADNFILES END");
    queueDestroy(readFiles, &myerr);

    // REMOVEFILE
    assert(!removeFile(paths[0], clients[0], &evicted[0]));
    assert(!errno);
    // printEvicted(evicted[0]);
    freeEvicted(evicted[0]);

    // LOCKFILE / UNLOCKFILE
    // Simple lock/unlock
    assert(!lockFile(paths[1], clients[1]));

    assert(!appendToFile(paths[1], MSG, strlen(MSG) + 1, clients[1], &evictedfiles, 0));
    assert(openFile(paths[1], 0, 0, clients[2], &evicted[0]) == 1); //this fails
    assert(unlockFile(paths[1], clients[2]) == 1);

    assert(unlockFile(paths[1], clients[1]) == 0);

    // LockersPending
    assert(!lockFile(paths[1], clients[1]));
    assert(lockFile(paths[1], clients[2]) == 1);
    assert(lockFile(paths[1], clients[3]) == 1);

    assert(unlockFile(paths[1], clients[1]) == clients[2]->fd);
    assert(unlockFile(paths[1], clients[2]) == clients[3]->fd);
    assert(unlockFile(paths[1], clients[3]) == 0);

    // CLOSEFILE
    assert(closeFile(paths[1], clients[1]) == 0);
    assert(closeFile(paths[1], clients[1]) == -1); // EACCES
    assert(closeFile(paths[1], clients[3]) == -1);

    assert(!lockFile(paths[2], clients[2]));
    assert(!unlockFile(paths[2], clients[2]));
    // queueCallback(store.files,printFnode); //LRU ok!

    assert(!storeDestroy()); // sembra funzionare

    // NEW TEST EVICTION
    printf("\n\n\n\n");
    puts("---------- EVICTION TEST");
    initEvicted(evicted);

    storeInit(5, 30);

    // OPENFILE senza O_LOCK
    puts("PUSHING 10 files BUT maxSize = 5");
    for (size_t i = 1; i < NFILES; i++)
    {
        printf("%ld\n", store.currNfiles);
        // queueCallback(store.files,printFnode); //LRU ok!
        assert(!openFile(paths[i], 1, 0, clients[i % NCLIENTS], &evicted[0]));
        if (evicted[0])
        {
            printEvicted(evicted[0]);
            free(evicted[0]);
        }
    }
    queueCallback(store.files, printFnode); //LRU ok!

    // only five file files are in the storage
    // strlen(MSG) == 14
    assert(0 == appendToFile(paths[5], MSG, strlen(MSG) + 1, clients[5 % NCLIENTS], &evictedfiles, 0));
    assert(0 == appendToFile(paths[5], MSG, strlen(MSG) + 1, clients[5 % NCLIENTS], &evictedfiles, 0));
    assert(0 == appendToFile(paths[6], MSG, strlen(MSG) + 1, clients[6 % NCLIENTS], &evictedfiles, 0));
    // assert(1 == cmpPathChar(((evictedFile*)(queue(evictedfiles,&myerr)))->path,paths[5]));
    queueCallback(evictedfiles, printEvicted);
    queueDestroy(evictedfiles,&myerr);


    storeDestroy();

    freeArr((void **)paths, NFILES, NULL);
    // freeArr((void**)contents,NFILES,NULL);
    freeArr((void **)clients, NCLIENTS, NULL);
    // freeArr((void **)evicted, NFILES, freeEvicted);

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
    printf("FINE ERRNO=%d\n", errno);
    return 0;
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

// void initRead(char **contents) {
//     puts("InitRead");
//     for (size_t i = 0; i < NFILES; i++)
//     {
//         assert(contents[i] = malloc(sizeof(evictedFile)));
//     }
// }