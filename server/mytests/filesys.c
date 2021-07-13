#define _POSIX_C_SOURCE 200809L

#include <queue.h>
#include <conn.h>
#include <filesys.h>
#include <icl_hash.h>

#include <stdio.h>
#include <string.h>
#include <utils.h>
#include <errno.h>
#include <math.h> // calculate size_t->char[] len
#include <assert.h>

#define BCYN "\x1b[36m"
#define REG "\x1b[0m"
#define MSG_PERROR(s) perror(BCYN s REG);
#define ONLY_MSG_ERR BCYN "INTERNAL" REG

#define TEST_FILESYS

void initPaths(char **paths);
void initClients(Client **clients);
void initEvicted(evictedFile **evicted);
void freeArr(void **arr, size_t n, void (*myfree)(void *arg));

int main(void)
{

// TEST FILESYSTEM
#define NCLIENTS 4
#define NFILES 10
#define FDSTART 20
#ifdef TEST_FILESYS

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

    storeInit(100, 100, 1);

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
    assert(10 == readNfiles(0, &readFiles, clients[0]));
    // printf("\tREADNFILES RESULT\n");
    // queueCallback(readFiles,printEvicted);
    // puts("\tREADNFILES END");
    queueDestroy(readFiles);

    // APPENDTOFILE
    assert(!appendToFile(paths[0], MSG, strlen(MSG) + 1, clients[0], &evictedfiles, 0));

    // READNFILES
    readNfiles(0, &readFiles, clients[0]);
    //puts("\tREADNFILES RESULT");
    //queueCallback(readFiles,printEvicted);
    //puts("\tREADNFILES END");
    queueDestroy(readFiles);

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
    assert(!lockFile(paths[1], clients[1]));
    assert(lockFile(paths[1], clients[2]) == 1);
    assert(lockFile(paths[1], clients[3]) == 1);

    assert(unlockFile(paths[1], clients[1]) == clients[2]->fd);
    assert(!lockFile(paths[1], clients[2]));
    assert(unlockFile(paths[1], clients[2]) == clients[3]->fd);
    assert(unlockFile(paths[1], clients[3]) == 0);

    // CLOSEFILE
    assert(closeFile(paths[1], clients[1]) == 0);
    assert(closeFile(paths[1], clients[1]) == 1); // EACCES
    assert(closeFile(paths[1], clients[3]) == 1); // EACCES

    assert(!lockFile(paths[2], clients[2]));
    assert(!unlockFile(paths[2], clients[2]));
    // queueCallback(store.files,printFnode); //LRU ok!

    assert(!storeDestroy()); // sembra funzionare

    // NEW TEST EVICTION
    printf("\n\n\n\n");
    puts("---------- EVICTION TEST");
    initEvicted(evicted);

    storeInit(5, 30, 1);

    // OPENFILE senza O_LOCK
    puts("PUSHING 10 files BUT maxSize = 5");
    for (size_t i = 1; i < NFILES; i++)
    {
        // printf("%ld\n", store.currNfiles);
        // queueCallback(store.files,printFnode); //LRU ok!
        assert(!openFile(paths[i], 1, 0, clients[i % NCLIENTS], &evicted[0]));
        if (evicted[0])
        {
            printEvicted(evicted[0]);
            freeEvicted(evicted[0]);
        }
    }
    queueCallback(store.files, printFnode); //LRU ok!

    puts("debug");
    assert(0 == lockFile(paths[5], clients[1]));
    assert(1 == lockFile(paths[5], clients[2]));
    assert(1 == lockFile(paths[5], clients[3]));

    // only five file files are in the storage
    // strlen(MSG) == 14
    assert(0 == appendToFile(paths[5], MSG, strlen(MSG) + 1, clients[5 % NCLIENTS], &evictedfiles, 0));
    assert(0 == appendToFile(paths[5], MSG, strlen(MSG) + 1, clients[5 % NCLIENTS], &evictedfiles, 0));
    assert(0 == appendToFile(paths[6], MSG, strlen(MSG) + 1, clients[6 % NCLIENTS], &evictedfiles, 0));
    queueCallback(evictedfiles, printEvicted); //
    queueDestroy(evictedfiles);

    // let's try to evict lockers
    assert(0 == lockFile(paths[6], clients[1]));
    assert(1 == lockFile(paths[6], clients[2]));
    assert(1 == lockFile(paths[6], clients[3]));

    assert(clients[2]->fd == unlockFile(paths[6], clients[1]));
    // clietns[2] should have acquired the lock
    assert((clients[2])->fd == ((fnode *)(((data *)(icl_hash_find(store.fdict, paths[6])))->data))->lockedBy);
    queue *notify = storeRemoveClient(clients[2]);
    assert(clients[3] == queueFind(notify,clients[3],NULL));
    assert((clients[3])->fd == ((fnode *)(((data *)(icl_hash_find(store.fdict, paths[6])))->data))->lockedBy);
    queueDestroy(notify);

    storeStats();
    storeDestroy();

    freeArr((void **)paths, NFILES, NULL);
    freeArr((void **)clients, NCLIENTS, NULL);

#endif
    return 0;
}

#define PATH_MSG "path"
void initPaths(char **paths)
{
    puts("InitFiles");
    for (size_t i = 0; i < NFILES; i++)
    {
        char tmp[20];
        sprintf(tmp, "%ld", i);
        assert(paths[i] = calloc(strlen(PATH_MSG) + strlen(tmp) + 1, sizeof(char)));
        strcat(paths[i], PATH_MSG);
        strcat(paths[i], tmp);
        printf("P%ld:%s \n", i, paths[i]);
    }
}

void initEvicted(evictedFile **evicted)
{
    puts("InitEvicted");
    for (size_t i = 0; i < NFILES; i++)
    {
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
