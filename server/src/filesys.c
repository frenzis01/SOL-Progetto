// #define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <filesys.h>
#include <logger.h>

#include <server.h> // needed for Client*


// ERRORS TO KEEP IN MIND
#pragma region
/**
 * MUTEX ERRORS (locks should never explode)
 *  
 * 
 * The pthread_mutex_lock() and pthread_mutex_trylock() functions shall fail if:
EINVAL
    The mutex was created with the protocol attribute having the value PTHREAD_PRIO_PROTECT and the calling thread's priority is higher than the mutex's current priority ceiling.

The pthread_mutex_lock(), pthread_mutex_trylock(), and pthread_mutex_unlock() functions may fail if:
EINVAL
    The value specified by mutex does not refer to an initialized mutex object. 
EAGAIN
    The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded.

The pthread_mutex_lock() function may fail if:
EDEADLK
    The current thread already owns the mutex.

The pthread_mutex_unlock() function may fail if:
EPERM
    The current thread does not own the mutex.
 * 
 */
#pragma endregion

#pragma region
// output colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

// error messages
#define E_DATA_INTERNAL ANSI_COLOR_RED "INTERNAL ERROR: DATA STRUCTURE FAILURE" ANSI_COLOR_RESET

#define E_STR_LOCK 1
#define E_STR_COND 2
#define E_STR_INVARG 3
#define E_STR_QUEUE 4
#define E_STR_NOENT 5

#define LOCKFILE pthread_mutex_lock(&(fptr->mutex))
#define UNLOCKFILE pthread_mutex_unlock(&(fptr->mutex))
#define LOCKORDERING pthread_mutex_lock(&(fptr->ordering))
#define UNLOCKORDERING pthread_mutex_unlock(&(fptr->ordering))
#define LOCKSTORE pthread_mutex_lock(&(store.lockStore))
#define UNLOCKSTORE pthread_mutex_unlock(&(store.lockStore))
#define WAITGO pthread_cond_wait(&(fptr->go), &(fptr->mutex))
#define SIGNALGO pthread_cond_signal(&(fptr->go))

#define eq_z(s, e, c)                               \
    if (!errno && !(s))                             \
    {                                               \
        errno = e;                                  \
        perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
        c;                                          \
    }

#define eo(c)                                              \
    if (errno != 0)                                        \
    {                                                      \
        perror(ANSI_COLOR_RED "Storage" ANSI_COLOR_RESET); \
        c;                                                 \
    }

#define eok(c)  \
    if (!errno) \
    {           \
        c;      \
    }

#define ec_nz_f(s, c)                               \
    if (!errno && (s))                              \
    {                                               \
        perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
        c;                                          \
    }

#define ec_z_f(s, c)                                \
    if (!errno && !(s))                             \
    {                                               \
        perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
        c;                                          \
    }
#pragma endregion

evictedFile *copyFnode(fnode *tmp, _Bool keepLockers);
fnode *initFile(char *path);
// Store operations
int storeInsert(fnode *fptr);
int storeDelete(fnode *fptr, evictedFile **toRet);
queue *storeCleaner(size_t sizeNeeded, char *pathToAvoid);
fnode *storeSearch(char *path);
evictedFile *storeEviction(_Bool ignoreLocks, char *pathToAvoid, _Bool ignoreEmpty);
_Bool storeIsEmpty();
_Bool storeIsFull();
// Compare functions
int cmpEvicted(void *a, void *b);
int cmpPath(void *a, void *b);
int cmpFd(void *a, void *b);
int cmpFile(void *a, void *b);
int cmpPathChar(void *a, void *b);

void freeNothing(void *arg) { return; };
void freeFile_LOCKERS(void *arg);
void printFD(void *arg);

/**
 * @param readN -- 1 if readFile was called by readNfiles, 0 otherwise
 * @returns 0 success, -1 internal error, 1 permission error (errno set)
 */
int readFile(char *path, evictedFile **toRet, Client *client, _Bool readN)
{
    errno = 0;
    fnode *fptr;
    if (!readN)
    { // If readFile was called from readNfiles, it already owns lockStore
        ec_nz_f(LOCKSTORE, return -1);
    }

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        if (!readN)
        {
            ec_nz(UNLOCKSTORE, return -1);
        }
        return 1; // file non trovato
    }

    // READERS / WRITERS paradigm

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    if (!readN)
    {
        ec_nz_f(UNLOCKSTORE, return -1);
    }

    while (fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKORDERING;
                    UNLOCKFILE;
                    return -1;
                });
    }

    fptr->readers++; // Readers count update

    ec_nz_f(UNLOCKORDERING, return -1); // avanti il prossimo
    ec_nz_f(UNLOCKFILE, return -1);

    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd) || (!queueFind(fptr->openBy, client, cmpFd) && !readN))
    {
        errno = EACCES;
    }
    else
    {
        // READ - BINARY
        ec_z_f(*toRet = copyFnode(fptr, 0), /* errno set. perror has already been called */);
    }

    // READ DONE

    // TODO what should I do if something explodes in this section ?
    // Nothing should explode here; in that case I should probably shutdown
    ec_nz(LOCKFILE, /* try to go ahead regardless, we'll deal with this on exit*/);

    // TODO is this okay here? (controlla anche le altre funzioni)
    fptr->fdCanWrite = 0;
    fptr->readers--;
    if (fptr->readers == 0)
        SIGNALGO;
    UNLOCKFILE;

    if (errno == EACCES)
        return 1;
    if (errno)
        return -1;
    return 0; // success
}

/**
 * @param n #{files to be read}, If <= 0 then read every file in the storage
 * @param client Needed to check O_LOCK, not if he's an opener, for each file examinated
 * @returns 0 success, -1 internal error, #filesLeft 
 * 
 */
int readNfiles(int n, queue **toRet, Client *client)
{
    errno = 0;
    int nread = 0;
    ec_z(*toRet = queueCreate(freeEvicted, NULL), return -1);
    char *content = NULL;
    evictedFile *qnode = NULL;
    data *curr = NULL; //we'll need this in case of FIFO evictPolicy

    // Holds STORE for the entire process to ensure that the files read are distinguished
    ec_nz_f(LOCKSTORE, return -1);

    if (n <= 0 || n > store.currNfiles)
        n = store.currNfiles;

    /**
     * Brief loop explaination:
     * 'n' is the #files we still have to read, and gets decremented on each iteration
     * If a file has been skipped (O_LOCK by !client) , then n++ to avoid considering it a read file
     * 
     * 'nread': We need a second counter to check if we read all the files in the storage
     * (why? what if n=currNfiles and we skip a file because of O_LOCK?)
     * 
     * @note we have two slightly different behaviour based on the evict policy
     */
    while (!errno && nread != store.currNfiles && n--)
    {
        fnode *tmp = NULL;
        if (store.evictPolicy == 1)
        { // LRU
            tmp = queuePeek(store.files);
            // readFile will perform a storeSearch, which will move the head to the tail
            // of the list, changing tmp on every iteration
        }
        else
        { //FIFO
            // TODO should i add curr != NULL in while(guard)? No, if !curr => nread = currNfiles = 0
            //  (in that case, i should init curr before the while)

            // We need to iterate through the list 'manually'
            if (!curr)
                curr = (store.files)->head;
            else
                curr = curr->next;

            if (!curr)
                break; //TODO will i ever enter this? No...

            tmp = curr->data;
        }

        // if queueEnqueue or readFile fails, it sets errno, and we'll exit from the loop
        // LOCKSTORE held, so we are sure to find tmp->path during readFile
        int readFileRes = readFile(tmp->path, &qnode, client, 1);
        if (readFileRes == 1)
        {
            n++;       // O_LOCK || !OPENED => File not read
            errno = 0; // reset errno to stay in the loop
        }
        else
            queueEnqueue(*toRet, qnode);
        nread++;
    }
    UNLOCKSTORE;
    eo(return -1); //internal error occurred during readFile||queueEnqueue||UNLOCKSTORE
    return n;
}

/**
 * Appends 'content' to a file; if necessary evicts files from the storage until it reaches the desired
 * capacity. If the updated file would exceed the maximum storage size by itself, no files are evicted,
 * and the function fails.
 *   
 * @note appendToFile and writeFile are almost identical, but permission checking differs a bit
 * @param evicted -- evicted files if any. Each one contains the corresponding queue of lockersPending,
 *  which must be notified
 * @param writeFlag -- needed to check permission if this has to behave as a writeFile
 * 
 * @returns 0 success, -1 internal error, 1 permission/not found/size error (EACCES | ENOENT | EFBIG),
 *  2 writeFile permission error (EACCES)
*/
int appendToFile(char *path, char *content, size_t size, Client *client, queue **evicted, int writeFlag)
{
    errno = 0;
    fnode *fptr;
    int myerr;
    *evicted = NULL;

    ec_nz_f(LOCKSTORE, return -1);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    // the append content by itself might exceed maxSize
    if (size > store.maxSize)
    {
        errno = EFBIG;
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    // ec_nz_f(UNLOCKSTORE, return -1);
    /**
     * We can't UNLOCKSTORE here and re-acquire later to check store's capacity,
     * because it might lead to a deadlock if another thread calls readNfiles between
     * the two locks
     */

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKORDERING;
                    UNLOCKFILE;
                    return -1;
                });
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1);
    ec_nz_f(UNLOCKFILE, return -1);

    int failure = 0;

    if (size + fptr->size > store.maxSize)
    {
        errno = EFBIG;
        failure = 1;
    }
    else
    {
        // if (O_LOCK || !OPENED)
        if ((fptr->lockedBy && fptr->lockedBy != client->fd) || !queueFind(fptr->openBy, client, cmpFd))
        {
            errno = EACCES;
            failure = 1;
        }
        else if (writeFlag && fptr->fdCanWrite != client->fd)
        {
            errno = EACCES;
            failure = 2;
        }
        else
        {
            // CHECK CAPACITY

            // (slightly unnecessary)
            if (!errno && store.currSize + size > store.maxSize)
            {
                *evicted = storeCleaner(size, path);
            }
            eok(store.currSize += size);

            // PERFORM OPERATION
            eok(fptr->content = realloc(fptr->content, fptr->size + size));
            eok(
                {
                    memcpy(fptr->content + fptr->size, content, size);
                    fptr->size += size;
                });
            eo(failure = -1);
        }
    }
    UNLOCKSTORE;

    LOCKFILE;

    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz(SIGNALGO, {});

    UNLOCKFILE;
    if (errno && errno != EACCES && errno != EFBIG)
        return -1;
    return failure;
}

/**
 * Adds a client to the openers' list of a file. O_LOCK and O_CREAT lead to failure if 
 * the file was already locked (by a diff client) or already existed. \n 
 * O_CREAT might lead to the eviction of one file from the storage.
 * 
 * @note (O_CREAT && O_LOCK && success) => Enables writeFile flag on the created file
 * @param createF -- O_CREAT
 * @param lockF -- O_LOCK
 * @param evicted -- O_CREAT might imply evicting a file. If NULL the evicted file iss freed
 * and not returned
 * 
 * @returns 0 success, -1 internal error, 1 notFound/alreadyExisted/alreadyLocked error 
 * (ENOENT | EADDRINUSE | EACCES)
*/
int openFile(char *path, int createF, int lockF, Client *client, evictedFile **evicted)
{
    errno = 0;
    fnode *fptr;
    evictedFile *victim = NULL;

    ec_nz_f(LOCKSTORE, return -1);

    fptr = storeSearch(path);
    if (!fptr && errno == ENOMEM)
        return -1; // LRU re-insertion failed

    if (!createF && !fptr)
    {
        errno = ENOENT;
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }
    if (createF && fptr)
    {
        errno = EADDRINUSE;
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }
    if (createF) // in this case, we have to create the file
    {
        // HOLDS LOCKSTORE
        int ignoreLocks = 0;
        while (storeIsFull()) // #files == maxNfiles
        {
            // This loop gets executed at most two times.
            // The second one ignores O_LOCK

            // 'path' isn't crucial here, we know for sure that
            // there isn't a file with the same path
            victim = storeEviction(ignoreLocks, path, 0);
            if (evicted)
                *evicted = victim;
            else
                freeEvicted(victim);
            ignoreLocks++;

            eo(UNLOCKSTORE; return -1;); //storeEviction failed
        }

        fptr = initFile(path);
        eo(freeFile(fptr); UNLOCKSTORE; return -1;);
        if (lockF)
        {
            fptr->lockedBy = client->fd;
            fptr->fdCanWrite = client->fd; // next OP might be writeFile
        }
        else
        {
            fptr->fdCanWrite = 0;
        }
        queueEnqueue(fptr->openBy, client);

        LOCKFILE;
        // store already locked

        storeInsert(fptr);
        UNLOCKSTORE;
        ec_nz(SIGNALGO, {});
        UNLOCKFILE;

        //TODO should i free stuff? No, i'll call storeDestroy in case of failure
        eo(return -1);
        return 0; //success
    }

    // In this case we have to open an existing file

    // openFile acts as a Writer

    // TODO qui va bene così? o meglio come nella append?
    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, UNLOCKORDERING; return -1);
    ec_nz_f(UNLOCKSTORE, UNLOCKFILE; UNLOCKORDERING; return -1);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKSTORE;
                    UNLOCKFILE;
                    UNLOCKORDERING;
                    return -1;
                });
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1);
    ec_nz_f(UNLOCKFILE, return -1);

    // O_LOCK check
    if ((fptr->lockedBy && fptr->lockedBy != client->fd))
    {
        errno = EACCES;
    }
    else
    {
        fptr->fdCanWrite = 0; // TODO okay here? or better after the last 'LOCKFILE'

        if (!queueFind(fptr->openBy, client, cmpFd)) // No duplicates allowed
            queueEnqueue(fptr->openBy, client);

        if (lockF) // If we got here, fptr's O_LOCK isn't currently owned by anyone
            fptr->lockedBy = client->fd;
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(SIGNALGO, {});

    UNLOCKFILE;
    if (errno == EACCES)
        return 1;
    else if (errno)
        return -1;

    return 0;
}

/**
 * Sets O_LOCK flag for the requesting client. \n \n 
 * A client doesn't need to be an opener to lock the file
 * @returns 0 success, -1 internal error, 1 O_LOCK owned by a different client or file not found 
 * (EACCES | ENOENT)
 * 
 */
int lockFile(char *path, Client *client)
{
    errno = 0;
    fnode *fptr;
    queue *evicted = NULL;

    ec_nz_f(LOCKSTORE, return -1);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    ec_nz_f(UNLOCKSTORE, return -1);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKORDERING;
                    UNLOCKFILE;
                    return -1;
                });
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1);
    ec_nz_f(UNLOCKFILE, return -1);

    // if (O_LOCK)      !OPENED non ci interessa
    if (fptr->lockedBy && fptr->lockedBy != client->fd)
    {
        queueEnqueue(fptr->lockersPending, client);
        if (!errno)
            errno = EACCES;
    }
    else
    {
        fptr->fdCanWrite = 0;
        fptr->lockedBy = client->fd;
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(SIGNALGO, {});

    UNLOCKFILE;
    if (errno == EACCES)
        return 1;
    else if (errno)
        return -1;
    return 0;
}

/**
 * @note 'return 1', isn't a real error
 * @returns [fd] of new lock owner,0 success && !lockersPending, -1 internal error,
 *  1 client doesn't own O_LOCK or file not found (EACCES | ENOENT) 
 */
int unlockFile(char *path, Client *client)
{
    errno = 0;
    fnode *fptr;

    ec_nz_f(LOCKSTORE, return -1);
    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    ec_nz_f(UNLOCKSTORE, return -1);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKORDERING;
                    UNLOCKFILE;
                    return -1;
                });
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1);
    ec_nz_f(UNLOCKFILE, return -1);

    int toRet = 0;

    if (fptr->lockedBy != client->fd)
    {
        errno = EACCES;
        toRet = 1;
    }
    else
    {
        fptr->fdCanWrite = 0;

        Client *tmp = queueDequeue(fptr->lockersPending);
        if (tmp)
        {
            fptr->lockedBy = (tmp)->fd;
            toRet = (tmp)->fd;
        }
        else
            fptr->lockedBy = 0;
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(SIGNALGO, {});

    UNLOCKFILE;
    if (errno && errno != EACCES)
        return -1;
    return toRet;
}

/**
 * @note closeFile works regardless of O_LOCK flags
 * @returns 0 success, -1 internal error, 1 client isn't an opener 
 * or file not found (EACCES | ENOENT)
 */
int closeFile(char *path, Client *client)
{
    errno = 0;
    fnode *fptr;

    ec_nz(LOCKSTORE, return -1;);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    ec_nz_f(UNLOCKSTORE, return -1);

    while (fptr->readers || fptr->writers)
    {
        ec_nz(WAITGO, {});
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1);
    ec_nz_f(UNLOCKFILE, return -1);

    if (queueRemove(fptr->openBy, client, cmpFd) == NULL)
    {
        // The requesting client not being an opener isn't a big deal
        //  but I should report it anyway
        errno = EACCES;
    }

    fptr->fdCanWrite = 0;

    LOCKFILE;
    fptr->writers--;
    ec_nz(SIGNALGO, {});
    UNLOCKFILE;

    if (errno == EACCES)
        return 1;
    else if (errno)
        return -1;
    return 0; // success
}

/**
 * Removes one file from the storage. \n 
 * A client must be the O_LOCK owner, but it doesn't have to be an opener
 * @returns 0 success, -1 internal error, 1 client isn't O_LOCK owner
 */
int removeFile(char *path, Client *client, evictedFile **evicted)
{
    errno = 0;
    fnode *fptr;

    ec_nz_f(LOCKSTORE, return -1);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING, return -1);
    ec_nz_f(LOCKFILE, return -1);
    // UNLOCKSTORE;  // we must be sure to the only thread active when we remove the file

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO,
                {
                    UNLOCKORDERING;
                    UNLOCKFILE;
                    return -1;
                });
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING, return -1); // Superflue (?), non abbiamo sbloccato lo store
    ec_nz_f(UNLOCKFILE, return -1);     // TODO RICORDA! NON SUPERFLUE, ALTRIMENTI QUANDO DEALLOCO
                                        // IL FILE, DISTRUGGO MUTEX SU CUI C'ERA UNA LOCK ATTIVA
                                        // (La mia, del thread che chiama removeFile)
    
    // if (!O_LOCK_byClient)
    if (fptr->lockedBy != client->fd)
    {
        errno = EACCES;
        LOCKFILE;
        fptr->writers--;
        ec_nz(SIGNALGO, {});
        UNLOCKFILE;
    }
    else
    {
        storeDelete(fptr, evicted);
    }

    // NON devo lockare il file, perchè è stato rimosso

    UNLOCKSTORE;
    if (errno == EACCES)
        return 1;
    if (errno)
        return -1;
    return 0; // success
}

/**
 * 
 * @description
 */
queue *storeRemoveClient(Client *client)
{
    queue *notifyLockers = queueCreate(free, cmpFd);
    ec_nz_f(LOCKSTORE, return NULL);

    data *curr = store.files->head;
    fnode *fptr = NULL;

    while (!errno && curr)
    {
        fptr = curr->data;

        LOCKORDERING;
        eok(LOCKFILE);

        while (!errno && fptr->readers || fptr->writers)
            ec_nz(WAITGO, {});

        eok(UNLOCKORDERING);
        eok(UNLOCKFILE);

        eok(queueRemove(fptr->openBy, client, NULL));

        if (!errno && fptr->lockedBy == client->fd)
        {
            Client *tmp = queueDequeue(fptr->lockersPending);
            if (tmp)
            {
                fptr->lockedBy = (tmp)->fd;
                queueEnqueue(notifyLockers, tmp);
            }
            else
                fptr->lockedBy = 0;
        }
        else if (!errno)
            queueRemove(fptr->lockersPending, client, NULL);

        curr = curr->next;
    }

    UNLOCKSTORE;
    eo(queueDestroy(notifyLockers); return NULL);
    return notifyLockers;
}

fnode *initFile(char *path)
{
    errno = 0;

    fnode *fptr;
    ec_z(fptr = malloc(sizeof(fnode)), return NULL);

    // TODO use PATH_LENGTH_MAX?
    ec_z(fptr->path = calloc(strnlen(path, INT_MAX) + 1, sizeof(char)), free(fptr); return NULL;);
    memcpy(fptr->path, path, strnlen(path, INT_MAX) + 1);

    fptr->content = NULL;
    fptr->size = 0;
    // TODO should it free the queues? NO, se tengo una struttura con tutti i client
    fptr->lockersPending = queueCreate(freeNothing, cmpFd);
    eok(fptr->openBy = queueCreate(freeNothing, cmpFd));
    fptr->lockedBy = 0;
    fptr->readers = 0;
    fptr->writers = 0;
    // è corretto NULL qui ???
    eok(pthread_cond_init(&(fptr->go), NULL));
    eok(pthread_mutex_init(&(fptr->mutex), NULL));
    eok(pthread_mutex_init(&(fptr->ordering), NULL));

    eo(freeFile(fptr)); //if something bad happened, free everything

    return fptr;
}

_Bool storeIsEmpty()
{
    if (store.currNfiles == 0)
        return 1;
    return 0;
}

_Bool storeIsFull()
{
    if (store.currNfiles == store.maxNfiles)
        return 1;
    return 0;
}

/**
 * Tail insert
 * @returns 0 success, -1 internal error (ENOMEM)
 */
int storeInsert(fnode *fptr)
{
    errno = 0;

    queueEnqueue(store.files, fptr);
    eo(return -1);
    icl_hash_insert(store.fdict, fptr->path, store.files->tail);
    eo(queueDequeue(store.files); return -1;);

    store.currSize += fptr->size;
    store.currNfiles++;

    return 0;
}

/**
 * @param toRet can be NULL
 * @returns 0 success, 1 fptr not found, -1 internal error 
 */
int storeDelete(fnode *fptr, evictedFile **toRet)
{
    errno = 0;

    data *tmp = icl_hash_find(store.fdict, fptr->path);
    if (!tmp)
        return 1;

    queueRemove_node(store.files, tmp);
    icl_hash_delete(store.fdict, fptr->path, freeNothing, freeNothing);

    store.currSize -= fptr->size;
    store.currNfiles--;

    // We free fptr only if we are evicting it from the storage
    // (otherwise we wouldn't be able to re-insert it in case of LRU)
    if (toRet)
    {
        // NB: we copy lockersPending reference

        *toRet = copyFnode(fptr, 1);
        freeFile_LOCKERS(fptr);
        if (!toRet)
            return -1;
    }

    return 0;
}

/**
 * Finds fnode corresponding to the given path
 * @returns fnode* success, NULL not found (ENOENT), NULL LRU re-insertion error (ENOMEM)
 * @note if LRU then removes and re-enqueues the file. This might fail
 */
fnode *storeSearch(char *path)
{
    errno = 0;
    data *tmp = icl_hash_find(store.fdict, path);
    fnode *curr = NULL;
    if (!tmp)
        errno = ENOENT;
    else
    {
        curr = tmp->data;
        if (store.evictPolicy) // LRU
        {
            // We push the file again in the storage
            storeDelete(curr, NULL);
            if (storeInsert(curr) == -1)
                return NULL; // errno set
        }
    }

    return curr;
}

/**
 * Utilities for storeEviction
 * They both return -1 on error, but what actually matters is that errno gets set
 */
int isLocked(fnode *fptr)
{
    // ASSERT : lockstore
    ec_nz(LOCKORDERING, return -1);
    ec_nz(LOCKFILE, return -1);

    while (fptr->readers || fptr->writers) {
        ec_nz(WAITGO, { return -1; });
    }

    //fptr->writers++;
    //TODO forse non mi interessa nemmen fare la signal...

    UNLOCKORDERING;
    UNLOCKFILE;
    eo(return -1);
    return fptr->lockedBy;
}
int isEmpty(fnode *fptr)
{
    // ASSERT : lockstore
    ec_nz(LOCKORDERING, return -1);
    ec_nz(LOCKFILE, return -1);

    while (fptr->readers || fptr->writers) {
        ec_nz(WAITGO, { return -1; });
    }

    UNLOCKORDERING;
    UNLOCKFILE;
    eo(return -1);

    if (!fptr->size)
        return 1;
    return 0;
}

/**
 * Evicts one file from the storage
 * 
 * @param ignoreLocks 0 fails if O_LOCK, 1 evicts regardless of O_LOCK
 * @param pathToAvoid avoid deadlock if called by an appendToFile
 * @param ignoreEmpty 1 skips empty files
 * 
 * @returns NULL on failure (errno set), evictedFile on success 
 */
evictedFile *storeEviction(_Bool ignoreLocks, char *pathToAvoid, _Bool ignoreEmpty)
{
    errno = 0;
    fnode *fptr;
    evictedFile *tmpContent = NULL;

    // IF i'm evicting because i need size (aka pathToAvoid != NULL)
    // THEN i can avoid evicting empty files

    /**
    * I have to iterate until I find an evictable file.
    * LRU or FIFO doesn't matter 
    */
    data *curr = (store.files)->head;
    while (!errno && curr &&
               (cmpPath(curr->data, pathToAvoid) == 1) ||
           (isEmpty(curr->data) && ignoreEmpty) ||
           (isLocked(curr->data) && ignoreLocks))
    {
        curr = curr->next;
    }
    if (!errno && curr)
        storeDelete(curr->data, &tmpContent); // on failure, ret NULL && errno set

    // if no files were eligible for eviction, NULL is returned
    return tmpContent;
}

/**
 * Evicts files until the storage reaches 'sizeNeeded' free space.
 * Assumes sizeNeeded is a reachable size.
 * @returns evicted files, NULL on error (errno set)
 */
queue *storeCleaner(size_t sizeNeeded, char *pathToAvoid)
{
    // NB
    fnode *fptr;
    errno = 0;
    evictedFile *tmpContent;
    queue *toReturn = queueCreate(freeEvicted, cmpEvicted);
    if (!toReturn)
        return NULL;

    // First scan: avoid O_LOCK
    // ec_nz_f(LOCKSTORE, queueDestroy(toReturn); return NULL);
    while (!errno && (store.maxSize - store.currSize) < sizeNeeded)
    {
        tmpContent = storeEviction(0, pathToAvoid, 1);
        eok(queueEnqueue(toReturn, tmpContent));
    }
    // Second scan : doesn't care about O_LOCK
    while (!errno && (store.maxSize - store.currSize) < sizeNeeded)
    {
        tmpContent = storeEviction(1, pathToAvoid, 1);
        eok(queueEnqueue(toReturn, tmpContent));
    }
    // UNLOCKSTORE;
    eo(queueDestroy(toReturn); return NULL);
    return toReturn;
}

evictedFile *copyFnode(fnode *tmp, _Bool keepLockers)
{
    errno = 0;
    evictedFile *toRet;
    ec_z(toRet = malloc(sizeof(evictedFile)), return NULL;);
    ec_z(toRet->path = calloc(strnlen(tmp->path, PATH_LENGTH) + 1, sizeof(char)), free(toRet); return NULL;);
    ec_z(toRet->content = calloc(tmp->size, sizeof(char)), free(toRet->path); free(toRet); return NULL;);
    memcpy(toRet->path, tmp->path, strnlen(tmp->path, PATH_LENGTH) + 1);
    memcpy(toRet->content, tmp->content, tmp->size);
    toRet->size = tmp->size;
    if (keepLockers)
        toRet->notifyLockers = tmp->lockersPending;
    else
        toRet->notifyLockers = NULL;
    return toRet;
}

// I need to keep lockersPending queue in case of eviction
// I can't use more than a parameter, so i need two different function to achieve this

/**
 * Frees fnode* but keeps lockersPending
 */
void freeFile_LOCKERS(void *arg)
{
    errno = 0;
    fnode *fptr = (fnode *)arg;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->path);
    free(fptr->content);

    queueDestroy(fptr->openBy);

    eo(perror(E_DATA_INTERNAL));

    // Anything else ???
    // TODO is mutex_destroy required? NO, probably

    free(fptr);
    return;
}
/**
 * Frees fnode*
 */
void freeFile(void *arg)
{
    errno = 0;
    fnode *fptr = (fnode *)arg;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->path);
    free(fptr->content);

    queueDestroy(fptr->openBy);
    queueDestroy(fptr->lockersPending);

    eo(perror(E_DATA_INTERNAL));

    // Anything else ???
    // TODO is mutex_destroy required? NO, probably

    free(fptr);
    return;
}

void freeEvicted(void *p)
{
    evictedFile *arg = p;
    free(arg->content);
    free(arg->path);
    queueDestroy(arg->notifyLockers);
    arg->content = NULL;
    arg->path = NULL;
    arg->notifyLockers = NULL;
    free(arg);
}

/**
 * @returns -1 on error, 0 success
 */
int storeInit(size_t maxNfiles, size_t maxSize, size_t evictPolicy)
{
    // TODO safe assumere che venga chiamata una sola volta?
    errno = 0;
    store.fdict = NULL;

    pthread_mutex_init(&(store.lockStore), NULL);
    eo(storeDestroy(); return -1;); // if files or fdict is NULL it's not a problem
    store.files = queueCreate(freeFile, cmpFile);
    eo(storeDestroy(); return -1;);
    store.fdict = icl_hash_create(maxNfiles, NULL, cmpPathChar);
    eo(storeDestroy(); return -1;);
    store.maxNfiles = maxNfiles;
    store.maxSize = maxSize;
    store.currNfiles = 0;
    store.currSize = 0;
    store.evictPolicy = evictPolicy;

    return 0;
}

int storeDestroy()
{
    errno = 0;
    // These two fail only if INVARG
    icl_hash_destroy(store.fdict, freeNothing, freeNothing);
    queueDestroy(store.files);
    // TODO necessary?
    //ec_nz(pthread_mutex_destroy(&(store.lockStore)), return -1);
    return 0;
}

//TODO this is just some debugging stuff

void printFD(void *arg)
{
    Client *c = arg;
    printf("  FD: %d\n", c->fd);
    return;
}
void printEvicted(void *arg)
{
    evictedFile *c = arg;
    printf(ANSI_COLOR_CYAN "EVCTD -- PATH: %s | CONTENT: %.*s\n" ANSI_COLOR_RESET, c->path, (int)c->size, c->content);
    printf(" LOCKERS:\n");
    queueCallback(c->notifyLockers, printFD);

    return;
}

void printFnode(void *arg)
{
    fnode *c = arg;
    printf(ANSI_COLOR_GREEN "FNODE -- PATH: %s | CONTENT: %.*s\n" ANSI_COLOR_RESET, c->path, (int)c->size, c->content);
    printf(" LOCKERS:\n");
    queueCallback(c->lockersPending, printFD);
    printf(" OPENERS:\n");
    queueCallback(c->openBy, printFD);

    return;
}

// Compare functions
int cmpFd(void *a, void *b)
{
    if (((Client *)a)->fd == ((Client *)b)->fd)
        return 1;
    return 0;
}

int cmpFile(void *a, void *b)
{
    if (strncmp(((fnode *)a)->path, ((fnode *)b)->path, PATH_LENGTH) == 0)
        return 1;
    return 0;
}
int cmpEvicted(void *a, void *b)
{
    if (strncmp(((evictedFile *)a)->path, ((evictedFile *)b)->path, PATH_LENGTH) == 0)
        return 1;
    return 0;
}

int cmpPath(void *a, void *b)
{
    if (strncmp(((fnode *)a)->path, (char *)b, PATH_LENGTH) == 0)
        return 1;
    return 0;
}

int cmpPathChar(void *a, void *b)
{
    if (strncmp((char *)a, (char *)b, PATH_LENGTH) == 0)
        return 1;
    return 0;
}