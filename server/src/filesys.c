#include <filesys.h>

#pragma region


// error messages
#define E_DATA_INTERNAL ANSI_COLOR_RED "INTERNAL ERROR: FILESYS FAILURE" ANSI_COLOR_RESET

#define eo(c)                    \
    if (errno != 0)              \
    {                            \
        perror(E_DATA_INTERNAL); \
        c;                       \
    }

// if NOTZERO then FAIL
#define ec_nz_f(s)                                  \
    if (s)                                          \
    {                                               \
        perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
        return -1;                                  \
    }

#define LOCKFILE pthread_mutex_lock(&(fptr->mutex))
#define UNLOCKFILE pthread_mutex_unlock(&(fptr->mutex))
#define LOCKORDERING pthread_mutex_lock(&(fptr->ordering))
#define UNLOCKORDERING pthread_mutex_unlock(&(fptr->ordering))
#define LOCKSTORE pthread_mutex_lock(&(store.lockStore))
#define UNLOCKSTORE pthread_mutex_unlock(&(store.lockStore))
#define WAITGO pthread_cond_wait(&(fptr->go), &(fptr->mutex))
#define SIGNALGO pthread_cond_signal(&(fptr->go))

#pragma endregion

#define UPDATE_MAX(m, a) a > m ? m = a : m;

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

void freeFile_LOCKERS(void *arg);
void printFD(void *arg);

/**
 * @param readN -- 1 if readFile was called by readNfiles, 0 otherwise
 * @returns 0 success, -1 internal error, 1 permission error (errno set)
 */
int readFile(char *path, evictedFile **toRet, Client *client, _Bool readN)
{
    errno = 0;
    int errnobk = 0;
    fnode *fptr;
    if (!readN)
    { // If readFile was called from readNfiles, it already owns lockStore
        ec_nz_f(LOCKSTORE);
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

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    if (!readN)
    {
        ec_nz_f(UNLOCKSTORE);
    }

    while (fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->readers++; // Readers count update

    ec_nz_f(UNLOCKORDERING); // avanti il prossimo
    ec_nz_f(UNLOCKFILE);

    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd) || (!queueFind(fptr->openBy, client, cmpFd) && !readN))
    {
        errnobk = errno = EACCES;
    }
    else
    {
        // READ - BINARY
        ec_z_f(*toRet = copyFnode(fptr, 0), /* errno set. perror has already been called */);
    }

    // READ DONE

    ec_nz_f(LOCKFILE);
    fptr->fdCanWrite = 0;
    fptr->readers--;
    if (fptr->readers == 0)
    {
        ec_nz_f(SIGNALGO);
    }
    ec_nz_f(UNLOCKFILE);

    errnobk ? errno = errnobk : errnobk;
    if (errno == EACCES)
        return 1;
    // if (errno)
    //     return -1;
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
    evictedFile *qnode = NULL;
    data *curr = NULL; //we'll need this in case of FIFO evictPolicy

    // Holds STORE for the entire process to ensure that the files read are distinguished
    ec_nz_f(LOCKSTORE);

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
            // No need to add curr != NULL in while(guard):
            // !curr => nread = currNfiles = 0

            // We need to iterate through the list 'manually'
            if (!curr)
                curr = (store.files)->head;
            else
                curr = curr->next;

            if (!curr)
                break; // will ever catch this? No...

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
    ec_nz_f(UNLOCKSTORE);
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
    int errnobk = errno = 0;
    fnode *fptr;
    *evicted = NULL;

    ec_nz_f(LOCKSTORE);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz_f(UNLOCKSTORE);
        return 1;
    }

    // the append content by itself might exceed maxSize
    if (size > store.maxSize)
    {
        errno = EFBIG;
        ec_nz(UNLOCKSTORE, return -1);
        return 1;
    }

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    // ec_nz_f(UNLOCKSTORE);
    /**
     * We can't UNLOCKSTORE here and re-acquire later to check store's capacity,
     * because it might lead to a deadlock if another thread calls readNfiles between
     * the two locks
     */

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

    int failure = 0;

    if (size + fptr->size > store.maxSize)
    {
        errnobk = errno = EFBIG;
        failure = 1;
    }
    else
    {
        // if (O_LOCK || !OPENED)
        if ((fptr->lockedBy && fptr->lockedBy != client->fd) || !queueFind(fptr->openBy, client, cmpFd))
        {
            errnobk = errno = EACCES;
            failure = 1;
        }
        else if (writeFlag && fptr->fdCanWrite != client->fd)
        {
            errnobk = errno = EACCES;
            failure = 2;
        }
        else
        {
            // CHECK CAPACITY
            // ('if' slightly unnecessary, only avoids some alloc/free)
            if (store.currSize + size > store.maxSize)
            {
                ec_z(*evicted = storeCleaner(size, path), return -1);
            }
            store.currSize += size;
            UPDATE_MAX(store.maxSizeReached, store.currSize);

            // PERFORM OPERATION
            ec_z(fptr->content = realloc(fptr->content, fptr->size + size), return -1);
            memcpy(fptr->content + fptr->size, content, size);
            fptr->size += size;
        }
    }
    ec_nz_f(UNLOCKSTORE);

    ec_nz_f(LOCKFILE);
    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz_f(SIGNALGO);
    ec_nz_f(UNLOCKFILE);

    errnobk ? errno = errnobk : errnobk;
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
    int errnobk = errno = 0;
    fnode *fptr;
    evictedFile *victim = NULL;

    ec_nz_f(LOCKSTORE);

    fptr = storeSearch(path);
    if (!fptr && errno == ENOMEM)
        return -1; // LRU re-insertion failed

    if (!createF && !fptr)
    {
        errno = ENOENT;
        ec_nz_f(UNLOCKSTORE);
        return 1;
    }
    if (createF && fptr)
    {
        errno = EADDRINUSE;
        ec_nz_f(UNLOCKSTORE);
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
        eo(freeFile(fptr); return -1;);
        if (lockF)
        {
            fptr->lockedBy = client->fd;
            fptr->fdCanWrite = client->fd; // next OP might be writeFile
        }
        else
        {
            fptr->fdCanWrite = 0;
        }
        ec_neg1(queueEnqueue(fptr->openBy, client), freeFile(fptr); return -1);

        ec_nz(LOCKFILE, freeFile(fptr); return -1);
        // store already locked

        ec_neg1(storeInsert(fptr), freeFile(fptr); return -1);
        ec_nz_f(UNLOCKSTORE);
        ec_nz_f(SIGNALGO);
        ec_nz_f(UNLOCKFILE);

        return 0; //success
    }

    // In this case we have to open an existing file

    // openFile acts as a Writer

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    ec_nz_f(UNLOCKSTORE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

    // O_LOCK check
    if ((fptr->lockedBy && fptr->lockedBy != client->fd))
    {
        errnobk = errno = EACCES;
    }
    else
    {
        if (!queueFind(fptr->openBy, client, cmpFd)) // No duplicates allowed
            queueEnqueue(fptr->openBy, client);

        if (lockF) // If we got here, fptr's O_LOCK isn't currently owned by anyone
            fptr->lockedBy = client->fd;
    }

    ec_nz_f(LOCKFILE);

    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz_f(SIGNALGO);

    ec_nz_f(UNLOCKFILE);

    errnobk ? errno = errnobk : errnobk;
    if (errno == EACCES)
        return 1;
    // else if (errno)
    //     return -1;

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
    int errnobk = errno = 0;
    fnode *fptr;

    ec_nz_f(LOCKSTORE);

    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz_f(UNLOCKSTORE);
        return 1;
    }

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    ec_nz_f(UNLOCKSTORE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

    // if (O_LOCK)      !OPENED non ci interessa
    if (fptr->lockedBy && fptr->lockedBy != client->fd)
    {
        queueEnqueue(fptr->lockersPending, client);
        if (!errno)
            errnobk = errno = EACCES;
    }
    else
    {
        fptr->lockedBy = client->fd;
    }

    ec_nz_f(LOCKFILE);
    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz_f(SIGNALGO);

    ec_nz_f(UNLOCKFILE);

    errnobk ? errno = errnobk : errnobk;
    if (errno == EACCES)
        return 1;
    return 0;
}

/**
 * @note 'return 1', isn't a real error
 * @returns [fd] of new lock owner,0 success && !lockersPending, -1 internal error,
 *  1 client doesn't own O_LOCK or file not found (EACCES | ENOENT) 
 */
int unlockFile(char *path, Client *client)
{
    int errnobk = errno = 0;
    fnode *fptr;

    ec_nz_f(LOCKSTORE);
    if ((fptr = storeSearch(path)) == NULL)
    {
        if (errno == ENOMEM)
        {
            UNLOCKSTORE;
            return -1;
        }
        ec_nz_f(UNLOCKSTORE);
        return 1;
    }

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    ec_nz_f(UNLOCKSTORE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

    int toRet = 0;

    if (fptr->lockedBy != client->fd)
    {
        errnobk = errno = EACCES;
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

    ec_nz_f(LOCKFILE);
    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz_f(SIGNALGO);

    ec_nz_f(UNLOCKFILE);
    errnobk ? errno = errnobk : errnobk;
    // if (errno && errno != EACCES)
    //     return -1;
    return toRet;
}

/**
 * @note closeFile works regardless of O_LOCK flags
 * @returns 0 success, -1 internal error, 1 client isn't an opener 
 * or file not found (EACCES | ENOENT)
 */
int closeFile(char *path, Client *client)
{
    int errnobk = errno = 0;
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

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    ec_nz_f(UNLOCKSTORE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz(WAITGO, return -1);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

    if (queueRemove(fptr->openBy, client, cmpFd) == NULL)
    {
        // The requesting client not being an opener isn't a big deal
        //  but I should report it anyway
        errnobk = errno = EACCES;
    }

    ec_nz_f(LOCKFILE);
    fptr->fdCanWrite = 0;
    fptr->writers--;
    ec_nz_f(SIGNALGO);
    ec_nz_f(UNLOCKFILE);

    errnobk ? errno = errnobk : errnobk;
    if (errno == EACCES)
        return 1;
    return 0; // success
}

/**
 * Removes one file from the storage. \n 
 * A client must be the O_LOCK owner, but it doesn't have to be an opener
 * @returns 0 success, -1 internal error, 1 client isn't O_LOCK owner
 */
int removeFile(char *path, Client *client, evictedFile **evicted)
{
    int errnobk = errno = 0;
    fnode *fptr;

    ec_nz_f(LOCKSTORE);

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

    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);
    // UNLOCKSTORE;  // we must be sure to the only thread active when we remove the file

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    fptr->writers++;

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);
    // if I don't unlock these two, when I remove the file i'll free a file with a thread (myself)
    // owning these two mutexes 

    // if (!O_LOCK_byClient)
    if (fptr->lockedBy != client->fd)
    {
        errno = EACCES;
        ec_nz_f(LOCKFILE);
        fptr->fdCanWrite = 0;
        fptr->writers--;
        ec_nz(SIGNALGO, return -1;);
        ec_nz_f(UNLOCKFILE);
    }
    else
    {
        ec_neg1(storeDelete(fptr, evicted), return -1);
        errnobk = errno; // might be EACCES
    }

    // NON devo lockare il file, perchè è stato rimosso

    ec_nz_f(UNLOCKSTORE);
    errnobk ? errno = errnobk : errnobk;
    if (errno == EACCES)
        return 1;
    return 0; // success
}

/**
 * Scans the whole storage to remove a client from openBy e lockedBy/lockersPending
 * @returns NULL on error, on success -> list of clients who successfully acquired O_LOCK after the removal of
 *  the previous owner. Might be empty.
 */
queue *storeRemoveClient(Client *client)
{
    queue *notifyLockers;
    ec_z(notifyLockers = queueCreate(freeNothing, cmpFd), return NULL);
    ec_nz(LOCKSTORE, goto rmvclnt_cleanup);

    data *curr = store.files->head;
    fnode *fptr = NULL;

    while (!errno && curr)
    {
        fptr = curr->data;

        ec_nz(LOCKORDERING, goto rmvclnt_cleanup);
        ec_nz(LOCKFILE, goto rmvclnt_cleanup);

        while (fptr->readers || fptr->writers)
            ec_nz(WAITGO, goto rmvclnt_cleanup);

        ec_nz(UNLOCKORDERING, goto rmvclnt_cleanup);
        ec_nz(UNLOCKFILE, goto rmvclnt_cleanup);

        queueRemove(fptr->openBy, client, NULL); // not a problem if not present

        if (fptr->lockedBy == client->fd)
        {
            Client *tmp = queueDequeue(fptr->lockersPending);
            if (tmp)
            {
                fptr->lockedBy = (tmp)->fd;
                ec_neg1(queueEnqueue(notifyLockers, tmp), goto rmvclnt_cleanup);
            }
            else
                fptr->lockedBy = 0;
        }
        else
            queueRemove(fptr->lockersPending, client, NULL);

        curr = curr->next;
    }

    ec_nz(UNLOCKSTORE, goto rmvclnt_cleanup);
    return notifyLockers;

rmvclnt_cleanup:
    queueDestroy(notifyLockers);
    return NULL;
}

fnode *initFile(char *path)
{
    errno = 0;

    fnode *fptr;
    ec_z(fptr = malloc(sizeof(fnode)), return NULL);

    ec_z(fptr->path = calloc(strnlen(path, PATH_MAX) + 1, sizeof(char)), free(fptr); return NULL;);
    memcpy(fptr->path, path, strnlen(path, PATH_MAX) + 1);

    fptr->content = NULL;
    fptr->size = 0;
    ec_z(fptr->lockersPending = queueCreate(freeNothing, cmpFd), freeFile(fptr); return NULL);
    ec_z(fptr->openBy = queueCreate(freeNothing, cmpFd), freeFile(fptr); return NULL);
    fptr->lockedBy = 0;
    fptr->readers = 0;
    fptr->writers = 0;
    ec_nz(pthread_cond_init(&(fptr->go), NULL), freeFile(fptr); return NULL);
    ec_nz(pthread_mutex_init(&(fptr->mutex), NULL), freeFile(fptr); return NULL);
    ec_nz(pthread_mutex_init(&(fptr->ordering), NULL), freeFile(fptr); return NULL);

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

    ec_neg1(queueEnqueue(store.files, fptr), return -1);
    ec_z(icl_hash_insert(store.fdict, fptr->path, store.files->tail), queueDequeue(store.files); return -1;);

    store.currSize += fptr->size;
    store.currNfiles++;

    UPDATE_MAX(store.maxNfilesReached, store.currNfiles);

    return 0;
}

/**
 * @param toRet NULL if we are not evicting
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
    fnode *curr = NULL;
    data *tmp = icl_hash_find(store.fdict, path);
    if (!tmp)
        errno = ENOENT;
    else
    {
        curr = tmp->data;
        if (store.evictPolicy) // LRU
        {
            // We push the file again in the storage
            // we are sure to find it

            ec_neg1(storeDelete(curr, NULL), return NULL);
            ec_neg1(storeInsert(curr), return NULL);
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
    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz_f(WAITGO);
    }

    // no need to SIGNALGO, when this function gets called
    // no one is waiting on the condition variable

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);
    return fptr->lockedBy;
}
int isEmpty(fnode *fptr)
{
    // ASSERT : lockstore
    ec_nz_f(LOCKORDERING);
    ec_nz_f(LOCKFILE);

    while (fptr->readers || fptr->writers)
    {
        ec_nz(WAITGO, return -1);
    }

    ec_nz_f(UNLOCKORDERING);
    ec_nz_f(UNLOCKFILE);

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
 * @returns NULL on failure (errno set) or no files are eligible for eviction,
 *  evictedFile on success 
 */
evictedFile *storeEviction(_Bool ignoreLocks, char *pathToAvoid, _Bool ignoreEmpty)
{
    errno = 0;
    evictedFile *tmpContent = NULL;

    // IF i'm evicting because i need size (aka pathToAvoid != NULL)
    // THEN i can avoid evicting empty files

    /**
    * I have to iterate until I find an evictable file.
    * LRU or FIFO doesn't matter 
    */
    data *curr = (store.files)->head;
    while (!errno && curr && ((cmpPath(curr->data, pathToAvoid) == 1) || (isEmpty(curr->data) && ignoreEmpty) || (isLocked(curr->data) && ignoreLocks)))
    {
        curr = curr->next;
    }
    if (!errno && curr)
    {
        storeDelete(curr->data, &tmpContent); // on failure, ret NULL && errno set
        eok(store.nEviction++);
    }

    // if no files were eligible for eviction, NULL is returned
    return tmpContent;
}

/**
 * Evicts files until the storage reaches 'sizeNeeded' free space.
 * Assumes sizeNeeded is a reachable size.
 * @returns evicted files (might be empty), NULL on error (errno set)
 */
queue *storeCleaner(size_t sizeNeeded, char *pathToAvoid)
{
    errno = 0;
    evictedFile *tmpContent;
    queue *toReturn = queueCreate(freeEvicted, cmpEvicted);
    if (!toReturn)
        return NULL;

    // First scan: avoid O_LOCK
    while ((store.maxSize - store.currSize) < sizeNeeded)
    {
        tmpContent = storeEviction(0, pathToAvoid, 1);
        if (!tmpContent && errno)
            goto strcln_cleanup;
        ec_neg1(queueEnqueue(toReturn, tmpContent), goto strcln_cleanup);
    }
    // Second scan : doesn't care about O_LOCK
    while ((store.maxSize - store.currSize) < sizeNeeded)
    {
        tmpContent = storeEviction(1, pathToAvoid, 1);
        if (!tmpContent && errno)
            goto strcln_cleanup;
        ec_neg1(queueEnqueue(toReturn, tmpContent), goto strcln_cleanup);
    }

    return toReturn;

strcln_cleanup:
    freeEvicted(tmpContent);
    queueDestroy(toReturn);
    return NULL;
}

int storeStats()
{
    puts(ANSI_COLOR_CYAN "-----STORE STATISTICS:" ANSI_COLOR_RESET);
    printf(
        ANSI_COLOR_BLUE "Max #files reached: %ld\n" ANSI_COLOR_GREEN "Max size reached: %ld\n" ANSI_COLOR_MAGENTA "#evicted: %ld\n" ANSI_COLOR_YELLOW "Files currently in the storage:\n" ANSI_COLOR_RESET,
        store.maxNfilesReached,
        store.maxSizeReached,
        store.nEviction);
    ec_neg1(queueCallback(store.files, printPath),return -1);
    puts(ANSI_COLOR_CYAN "-----END" ANSI_COLOR_RESET);
    return 0;
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
    if (!arg)
        return;

    errno = 0;
    fnode *fptr = (fnode *)arg;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->path);
    free(fptr->content);

    queueDestroy(fptr->openBy);

    free(fptr);
    return;
}
/**
 * Frees fnode*
 */
void freeFile(void *arg)
{
    if (!arg)
        return;
    errno = 0;
    fnode *fptr = (fnode *)arg;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->path);
    free(fptr->content);

    queueDestroy(fptr->openBy);
    queueDestroy(fptr->lockersPending);

    free(fptr);
    return;
}

void freeEvicted(void *p)
{
    if (!p)
        return;
    evictedFile *arg = p;
    free(arg->content);
    free(arg->path);
    queueDestroy(arg->notifyLockers);
    arg->content = NULL;
    arg->path = NULL;
    arg->notifyLockers = NULL;
    free(arg);
    return;
}

/**
 * @returns -1 on error, 0 success
 */
int storeInit(size_t maxNfiles, size_t maxSize, size_t evictPolicy)
{
    errno = 0;
    store.fdict = NULL;

    // files or fdict being NULL during storeDestroy is not a problem
    ec_nz(pthread_mutex_init(&(store.lockStore), NULL), storeDestroy(); return -1;);
    ec_z(store.files = queueCreate(freeFile, cmpFile), storeDestroy(); return -1;);
    ec_z(store.fdict = icl_hash_create(maxNfiles, NULL, cmpPathChar), storeDestroy(); return -1;);
    store.maxNfiles = maxNfiles;
    store.maxSize = maxSize;
    store.currNfiles = 0;
    store.currSize = 0;
    store.evictPolicy = evictPolicy;

    store.nEviction = 0;
    store.maxNfilesReached = 0;
    store.maxSizeReached = 0;

    return 0;
}

int storeDestroy()
{
    errno = 0;
    // These two fail only if INVARG
    icl_hash_destroy(store.fdict, freeNothing, freeNothing);
    queueDestroy(store.files);
    return 0;
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

// These are just some utilities

void printPath(void *arg)
{
    fnode *fptr = arg;
    printf("%s\n", fptr->path);
    return;
}

void printFD(void *arg)
{
    Client *c = arg;
    printf("  FD: %d\n", c->fd);
    return;
}

void printString(const char *str, size_t len){
    for (size_t i = 0; i < len; i++)
    {
        printf("%c", str[i]);
    }
}
void printEvicted(void *arg)
{
    evictedFile *c = arg;
    printf(ANSI_COLOR_MAGENTA "EVCTD -- PATH: %s | CONTENT: ", c->path);
    printString(c->content,c->size);
    printf(ANSI_COLOR_RESET "\n");
    printf(" LOCKERS:\n");
    queueCallback(c->notifyLockers, printFD);

    return;
}

void printFnode(void *arg)
{
    fnode *c = arg;
    printf(ANSI_COLOR_GREEN "FNODE -- PATH: %s | CONTENT: ", c->path);
    printString(c->content,c->size);
    printf(ANSI_COLOR_RESET "\n");
    printf(" LOCKERS:\n");
    queueCallback(c->lockersPending, printFD);
    printf(" OPENERS:\n");
    queueCallback(c->openBy, printFD);

    return;
}