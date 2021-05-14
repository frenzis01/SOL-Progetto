#include <server.h>

#define E_STR_LOCK 1
#define E_STR_COND 2
#define E_STR_INVARG 3
#define E_STR_QUEUE 4
#define E_STR_NOENT 5

#define LOCKFILE ec_nz(pthread_mutex_lock(&(fptr->mutex)), { myerr = E_STR_LOCK; });
#define UNLOCKFILE ec_nz(pthread_mutex_unlock(&(fptr->mutex)), { myerr = E_STR_LOCK; });
#define LOCKORDERING ec_nz(pthread_mutex_lock(&(fptr->ordering)), { myerr = E_STR_LOCK; });
#define UNLOCKORDERING ec_nz(pthread_mutex_unlock(&(fptr->ordering)), { myerr = E_STR_LOCK; });
#define LOCKSTORE ec_nz(pthread_mutex_lock(&(store.lockStore)), { myerr = E_STR_LOCK; });
#define UNLOCKSTORE ec_nz(pthread_mutex_unlock(&(store.lockStore)), { myerr = E_STR_LOCK; });
// #define READWAIT ec_nz(pthread_cond_wait(&(fptr->readGo),&(fptr->lockFile)),{});

// ec_nz(pthread_cond_wait(&(fptr->writeGo),&(fptr->lockFile)));

// OPERAZIONI STORAGE
// Operazioni chiamate dagli handler che controlleranno la concorrenza

// Utilizziamo soluzione Readers-Writers proposta nell'esercitazione su RW

int readFile(char *path, char **toRet, Client *client)
{
    errno = 0;
    fnode *fptr;
    int myerr;

    LOCKSTORE;

    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT; // meglio (semanticamente) settare errno prima di unlock
        UNLOCKSTORE;
        return -1; // file non trovato
    }

    LOCKORDERING;
    LOCKFILE;
    UNLOCKSTORE;

    // Wait per scrittori
    while (fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    // Scrittori usciti, ora leggo

    fptr->readers++; // Readers count update

    UNLOCKORDERING; // avanti il prossimo
    UNLOCKFILE;

    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd) || !queueFind(fptr->openBy, client, cmpFd, &myerr))
    {
        errno = EACCES; // File O_LOCK da un altro Client
    }

    // READ - BINARY
    // '\0' per terminare (?) (size + 1?)
    toRet = malloc(fptr->size);
    if (errno != ENOMEM)
        memcpy(*toRet, fptr->content, fptr->size);

    // READ FINITO
    LOCKFILE;
    fptr->readers--; // Readers count update
    if (fptr->readers == 0)
        pthread_cond_signal(&(fptr->go));
    UNLOCKFILE;

    return 0; // success
}

int appendToFile(char *path, char *content, size_t size, Client *client, queue **evicted, int writeFlag)
{
    // writeFlag distingue fra appendToFile e writeFile: cambia solo un controllo
    //  su client->fd e su fdCanWrite presente in ogni fnode
    errno = 0;
    fnode *fptr;
    int myerr;
    *evicted = NULL;

    LOCKSTORE;

    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1;
    }

    LOCKORDERING; // Blocchiamoci sul file
    LOCKFILE;
    UNLOCKSTORE;

    // ??? ok il controllo dopo la wait e la unlock?

    while (fptr->readers || fptr->writers) // Da rivedere le guardie sulle wait...
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING;
    UNLOCKFILE;

    // CHECK CAPACITY
    LOCKSTORE;
    if (store.currSize + size > store.maxSize)
    {
        *evicted = storeCleaner(size);
    }
    UNLOCKSTORE;

    int alreadyLocked = 0;
    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd) || !queueFind(fptr->openBy, client, cmpFd, &myerr))
    {
        errno = EACCES;
        alreadyLocked = 1;
    }
    else if (writeFlag && fptr->fdCanWrite != client->fd)
    {
        errno = EACCES;
        alreadyLocked = 2;
    }
    else
    {
        // TODO : test this
        fptr->content = realloc(fptr->content, fptr->size + size);
        memcpy(fptr->content + fptr->size, content, size);
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    return alreadyLocked;
}

int openFile(char *path, int createF, int lockF, Client *client, fnode **toRet)
{
    // openFile non restituisce il file evicted
    errno = 0;
    fnode *fptr;
    int myerr;
    LOCKSTORE;
    fptr = storeSearch(path);

    *toRet == NULL;
    // O_CREATE not set e il file non esiste nello storage
    if (!createF && !fptr)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1;
    }
    // O_CREATE set e il file esiste già nello storage
    if (createF && fptr)
    {
        errno = EADDRINUSE;
        UNLOCKSTORE;
        return -1;
    }
    if (createF)
    {
        if (storeIsFull()) // #files == maxNfiles
        {
            // remove 1 file
        }

        fptr = initFile(path);
        if (lockF)
        {
            fptr->lockedBy = client->fd;
            fptr->fdCanWrite = client->fd; // next OP might be writeFile
        }
        queueEnqueue(fptr->openBy, client, &myerr);

        LOCKFILE; // store già locked
        storeInsert(fptr);
        UNLOCKSTORE;
        queueEnqueue(fptr->openBy, client, &myerr);
        *toRet = fptr;
        UNLOCKFILE;
        return 0; //success
    }
    // IL FILE ESISTE GIÀ NELLO STORAGE
    // fptr punta al file

    // openFile agisce come un Writer

    LOCKORDERING;
    LOCKFILE;
    UNLOCKSTORE;

    while (fptr->readers || fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING;
    UNLOCKFILE;

    int alreadyLocked = 0;

    // ??? è possibile controllare se il file era già lockato prima di acquisirlo?
    //   ...No

    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd))
    {
        errno = EACCES;
        alreadyLocked = 1;
    }
    else
    {
        fptr->fdCanWrite = 0; // va fatto per ogni operazione

        if (!queueFind(fptr->openBy, client, cmpFd, &myerr))
            queueEnqueue(fptr->openBy, client, &myerr);

        if (lockF) // se siamo arrivati fin qui il file non era già lockato
            fptr->lockedBy = client->fd;
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;

    return alreadyLocked;
}

int lockFile(char *path, Client *client)
{
    // lockFile non necessita di una open prima
    errno = 0;
    fnode *fptr;
    int myerr;
    queue *evicted = NULL;

    LOCKSTORE;

    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1;
    }

    LOCKORDERING; // Blocchiamoci sul file
    LOCKFILE;
    UNLOCKSTORE;

    // ??? ok il controllo dopo la wait e la unlock?

    while (fptr->readers || fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING;
    UNLOCKFILE;

    // if (O_LOCK || !OPENED)
    if (fptr->lockedBy && fptr->lockedBy != client->fd)
        queueEnqueue(fptr->lockersPending, client, &myerr);
    else
        fptr->lockedBy = client->fd;

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    return 0; // success
}

int unlockFile(char *path, Client *client)
{
    errno = 0;
    fnode *fptr;
    int myerr;

    LOCKSTORE;
    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1; // il file non esiste nello storage
    }

    LOCKORDERING; // Blocchiamoci sul file
    LOCKFILE;
    UNLOCKSTORE;

    while (fptr->readers || fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING;
    UNLOCKFILE;

    int alreadyLocked = 0;

    if (fptr->lockedBy && fptr->lockedBy != client->fd)
    {
        errno = EACCES;
        alreadyLocked = 1;
    }
    else
    {
        fptr->lockedBy = dequeue(fptr->lockersPending, &myerr);
        // INFORMA IL CLIENT DELL'AVVENUTA LOCK
        // Write su lockedBy_fd
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
}

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

int cmpPath(void *a, void *b)
{
    if (strncmp(((fnode *)a)->path, (char *)b, PATH_LENGTH) == 0)
        return 1;
    return 0;
}

fnode *initFile(char *path)
{
    errno = 0;
    int myerr;

    fnode *fptr = malloc(sizeof(fnode));
    if (!fptr)
        return NULL;
    fptr->path = path;
    fptr->content = NULL;
    fptr->size = 0;
    fptr->lockersPending = queueCreate(free, cmpFd, &myerr);
    fptr->openBy = queueCreate(free, cmpFd, &myerr);
    fptr->readers = 0;
    fptr->writers = 0;
    // è corretto NULL qui ???
    ec_nz(pthread_cond_init(&(fptr->go), NULL), {myerr = E_STR_COND; return NULL; });
    ec_nz(pthread_mutex_init(&(fptr->mutex), NULL), {myerr = E_STR_LOCK; return NULL; });
    ec_nz(pthread_mutex_init(&(fptr->ordering), NULL), {myerr = E_STR_LOCK; return NULL; });

    // fptr->prev = NULL;
    // fptr->next = NULL;

    return fptr;
}

int storeInit(size_t maxNfiles, size_t maxSize)
{
    int myerr;

    store.files = queueCreate(freeFile, cmpFile, &myerr);
    store.maxNfiles = maxNfiles;
    store.maxSize = maxSize;

    return myerr;
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

// Inserisce sempre in coda
int storeInsert(fnode *fptr)
{
    // ec_nz(pthread_mutex_lock(&(store.lockStore)));
    int myerr = 0;

    if (!fptr)
        return E_STR_INVARG;
    if (!store.files)
        return E_STR_QUEUE;

    queueEnqueue(store.files, fptr, &myerr);

    store.currSize += fptr->size;
    store.currNfiles++;

    return 0;
}

int storeDelete(fnode *fptr, char **toRet)
{

    int myerr = 0;

    if (!fptr)
        return E_STR_INVARG;
    if (!store.files)
        return E_STR_QUEUE;

    fnode *tmp = queueRemove(store.files, fptr, cmpFile, &myerr);
    // if (myerr) return E_STR_QUEUE;

    if (!tmp)
        return E_STR_NOENT;

    store.currSize -= fptr->size;
    store.currNfiles--;

    // Se il file viene espulso dallo store, libero il ptr ma
    //  prima restituisco il contenuto del file

    // altrimenti no
    if (toRet)
    {
        // SALVIAMO IN toRet il content del file eliminato
        *toRet = malloc(fptr->size); // char ? binary ?
        memcpy(*toRet, fptr->content, fptr->size);
        freeFile(fptr);
    }

    return 0;
}

fnode *storeSearch(char *path)
{
    int myerr = 0;
    fnode *curr = queueFind(store.files, path, cmpPath, &myerr);

    // Se curr non è in storage
    if (!curr)
        errno = ENOENT;
    else
    {
        myerr = storeDelete(curr, 0);
        myerr = storeInsert(curr); // LRU : riposizioniamo il file in fondo alla coda
    }

    return curr; // file non trovato
}

// TODO
// I FILE ESPULSI VANNO RESTITUITI!
queue *storeCleaner(size_t sizeNeeded)
{
    // NB
    fnode *fptr;
    int myerr = 0;
    char *tmpContent;
    queue *toReturn = queueCreate(free, strcmp, &myerr); // is strcmp ok here?
    if (myerr)
    {
        errno = ENOMEM;
        return NULL;
    }
    // il chiamante deve controllare che sizeNeeded < store.maxSize

    // Prima passata : evito le lock
    LOCKSTORE;
    while ((store.maxSize - store.currSize) < sizeNeeded)
    {
        fptr = queuePeek(store.files, &myerr);
        LOCKORDERING;
        LOCKFILE;

        while (fptr->readers || fptr->writers)
            ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

        fptr->writers++;

        UNLOCKORDERING;
        UNLOCKFILE;

        if (fptr->lockedBy == 0)
        {
            storeDelete(fptr, &tmpContent);
            queueEnqueue(toReturn, tmpContent, &myerr);
        }

        LOCKFILE;
        fptr->writers--;
        ec_nz(pthread_cond_signal(&(fptr->go)), {});
        UNLOCKFILE;
    }
    // Seconda passata : elimino indipendentemente dai O_LOCK
    while ((store.maxSize - store.currSize) < sizeNeeded)
    {
        fptr = queuePeek(store.files, &myerr);
        LOCKORDERING;
        LOCKFILE;

        while (fptr->readers || fptr->writers)
            ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

        fptr->writers++;

        UNLOCKORDERING;
        UNLOCKFILE;

        storeDelete(fptr, &tmpContent);
        queueEnqueue(toReturn, tmpContent, &myerr);

        LOCKFILE;
        fptr->writers--;
        ec_nz(pthread_cond_signal(&(fptr->go)), {});
        UNLOCKFILE;
    }
    UNLOCKSTORE;
}

int freeFile(fnode *fptr)
{
    int myerr;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->content);
    destroyQueue(&(fptr->lockersPending), &myerr);
    destroyQueue(&(fptr->openBy), &myerr);

    // Anything else ???

    free(fptr);
    return 0;
}