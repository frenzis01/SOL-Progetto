#include <server.h>
#include <filesys.h>

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

// TODO queue dei client attivi
// TODO sincronizzare client->opened e client->locked con il filesys
// TODO SOLO PER IL PATH memcpy strlen+1 perchè memcpy non copia \0.

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
// TODO remove these cmpFunctions from .h ?
int cmpPathChar(void *a, void *b)
{
    if (strncmp((char *)a, (char *)b, PATH_LENGTH) == 0)
        return 1;
    return 0;
}

void freeNothing(void *arg){};

int readFile(char *path, evictedFile **toRet, Client *client, _Bool readN)
{
    errno = 0;
    fnode *fptr;
    int myerr;
    if (!readN) // se chiamo da readNfiles ho già la lock sullo store
        LOCKSTORE;

    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT; // meglio (semanticamente) settare errno prima di unlock
        if (!readN)
            UNLOCKSTORE;
        return -1; // file non trovato
    }

    LOCKORDERING;
    LOCKFILE;
    if (!readN)
        UNLOCKSTORE;

    // Wait per scrittori
    while (fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)),
              {
                  UNLOCKORDERING;
                  UNLOCKFILE;
                  return -1;
              });

    // Scrittori usciti, ora leggo

    fptr->readers++; // Readers count update

    UNLOCKORDERING; // avanti il prossimo
    UNLOCKFILE;

    // if (O_LOCK || !OPENED)
    if ((fptr->lockedBy && fptr->lockedBy != client->fd) || (!queueFind(fptr->openBy, client, cmpFd, &myerr) && !readN))
    {
        errno = EACCES; // File O_LOCK da un altro Client
    }

    // READ - BINARY
    // '\0' per terminare (?) (size + 1?)
    // TODO do I care about errors here?
    *toRet = copyFnode(fptr);

    // READ FINITO
    LOCKFILE;
    // TODO is this okay here? (controlla anche le altre funzioni)
    fptr->fdCanWrite = 0;
    fptr->readers--; // Readers count update
    if (fptr->readers == 0)
        pthread_cond_signal(&(fptr->go));
    UNLOCKFILE;

    if (errno)
        return -1; // questo errno becca eventuali ENOMEM in copyFnode
    return 0;      // success
}

int readNfiles(int n, queue **toRet, Client *client)
{
    // returns nleft

    int myerr = 0;
    int nread = 0;
    ec_z(*toRet = queueCreate(freeEvicted, NULL, &myerr), return -1);
    char *content;
    evictedFile *qnode;

    LOCKSTORE; // MANTENIAMO LA STORELOCK per garantire che i file siano distinti
    if (n <= 0 || n > store.currNfiles)
        n = store.currNfiles;
    while (n-- && nread != store.currNfiles)
    {
        fnode *tmp = queuePeek(store.files, &myerr);
        // Abbiamo LOCKSTORE quindi siamo certi di trovare tmp->path in readFile
        if (readFile(tmp->path, &qnode, client, 1) == -1)
            n++;
        else
            queueEnqueue(*toRet, qnode, &myerr);
        nread++;
    }
    UNLOCKSTORE;
    return nread;
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

    // CHECK se la size da aggiungere è > maxSize
    if (size > store.maxSize)
    {
        errno = EFBIG;
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

    int alreadyLocked = 0;

    // CHECK se la size totale è > maxSize
    if (size + fptr->size > store.maxSize)
    {
        errno = EFBIG;
        alreadyLocked = -1;
    }
    else
    {
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
            // CHECK CAPACITY
            LOCKSTORE;
            if (!errno && store.currSize + size > store.maxSize)
            {
                *evicted = storeCleaner(size, path);
            }
            // è necessario metterlo qui
            store.currSize += size;
            UNLOCKSTORE;
            
            fptr->fdCanWrite = 0;
            // TODO errcheck
            fptr->content = realloc(fptr->content, fptr->size + size);
            memcpy(fptr->content + fptr->size, content, size);
            fptr->size += size;
            // non facciamo memcpy(...,size + 1) perchè maneggiamo file binari
            // dunque non abbiamo il char \0 alla fine. Sarà compito del chiamante
            // passare strlen+1 come size
        }
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    return alreadyLocked;
}

int openFile(char *path, int createF, int lockF, Client *client, /*fnode **toRet,*/ evictedFile **evicted)
{
    // openFile non restituisce il file evicted
    errno = 0;
    fnode *fptr;
    int myerr;
    LOCKSTORE;
    fptr = storeSearch(path);

    // *toRet == NULL;
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
        // MANTENGO LA LOCKSTORE
        int ignoreLocks = 0;
        while (storeIsFull()) // #files == maxNfiles
        {
            // viene eseguito due volte, la seconda ignora le lock
            *evicted = storeEviction(ignoreLocks, NULL);
            ignoreLocks++;
        }

        fptr = initFile(path);
        if (lockF)
        {
            fptr->lockedBy = client->fd;
            fptr->fdCanWrite = client->fd; // next OP might be writeFile
        }
        else
        {
            fptr->fdCanWrite = 0;
        }
        queueEnqueue(fptr->openBy, client, &myerr);

        LOCKFILE; // store già locked
        
        storeInsert(fptr);
        UNLOCKSTORE;

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

    // if O_LOCK la open fallisce, non inserisce il client nei pendingLockers
    // if (O_LOCK || !OPENED)

    if ((fptr->lockedBy && fptr->lockedBy != client->fd))
    {
        errno = EACCES;
        alreadyLocked = 1;
    }
    else
    {
        fptr->fdCanWrite = 0; // va fatto per ogni operazione

        if (!queueFind(fptr->openBy, client, cmpFd, &myerr)) // Non voglio duplicati
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

    _Bool alreadyLocked = 0;

    // if (O_LOCK) !OPENED non ci interessa
    if (fptr->lockedBy && fptr->lockedBy != client->fd)
    {
        queueEnqueue(fptr->lockersPending, client, &myerr);
        alreadyLocked = 1;
    }
    else
    {
        fptr->fdCanWrite = 0;
        fptr->lockedBy = client->fd;
    }

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    return alreadyLocked; // success
}

int unlockFile(char *path, Client *client)
{
    // returns -1 on error, 1 if locked by someone else,
    //   fd!=0 if fd acquired the lock, 0 if there were no lockers pending
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

    int lockedAcquiredBy = 0;

    if (fptr->lockedBy != client->fd)
    {
        errno = EACCES;
        lockedAcquiredBy = 1;
    }
    else
    {
        fptr->fdCanWrite = 0;
        // TODO okay?
        if (!queueIsEmpty(fptr->lockersPending, &myerr))
        {
            Client *tmp = queueDequeue(fptr->lockersPending, &myerr);
            fptr->lockedBy = (tmp)->fd;
            lockedAcquiredBy = (tmp)->fd;
        }
        else
        {
            fptr->lockedBy = 0;
        }
        // INFORMA IL CLIENT DELL'AVVENUTA LOCK
        // Write su lockedBy_fd
    }
    // TODO delete this printf
    // printf("LockedAcquiredBy = %d\n", fptr->lockedBy);

    LOCKFILE;

    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    return lockedAcquiredBy;
}

int closeFile(char *path, Client *client)
{
    // se il file non era aperto dal client non fallisce
    errno = 0;
    fnode *fptr;
    int myerr;

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

    // TODO non ci interessa che il file sia lockato, se era aperto
    //  lo devo poter chiudere comunque

    // // if (O_LOCK)
    // if (fptr->lockedBy != client->fd)
    // {
    //     errno = EACCES;
    // }
    // else

    // se il client non era fra gli openers va bene comunque
    // ma lo segnalo ritornando -1
    if (queueRemove(fptr->openBy, client, cmpFd, &myerr) == NULL)
    {
        // puts("debug");
        errno = EACCES;
    }

    fptr->fdCanWrite = 0;

    LOCKFILE;
    fptr->writers--;
    ec_nz(pthread_cond_signal(&(fptr->go)), {});

    UNLOCKFILE;
    if (errno)
        return -1;
    return 0; // success
}

int removeFile(char *path, Client *client, evictedFile **evicted)
{
    // se il file non era aperto dal client non fallisce
    errno = 0;
    fnode *fptr;
    int myerr;

    LOCKSTORE;

    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1;
    }

    LOCKORDERING; // Blocchiamoci sul file
    LOCKFILE;
    // UNLOCKSTORE; //NON SBLOCCHIAMO LO STORE

    while (fptr->readers || fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING; // Superflue, non abbiamo sbloccato lo store
    UNLOCKFILE;     // TODO RICORDA! NON SUPERFLUE, ALTRIMENTI QUANDO DEALLOCO
                    // IL FILE, DISTRUGGO MUTEX SU CUI C'ERA UNA LOCK ATTIVA

    // Non ci interessa la
    // if (!O_LOCK_byClient)
    if (fptr->lockedBy != client->fd)
    {
        errno = EACCES;
    }
    else
    {
        // puts("\tFILE rimosso");
        storeDelete(fptr, evicted);
    }

    // NON devo lockare il file, perchè è stato rimosso

    // LOCKFILE;
    // fptr->writers--;
    // ec_nz(pthread_cond_signal(&(fptr->go)), {});

    // UNLOCKFILE;

    UNLOCKSTORE;
    return 0; // success
}

fnode *initFile(char *path)
{
    errno = 0;
    int myerr;

    fnode *fptr = malloc(sizeof(fnode));
    if (!fptr)
        return NULL;
    // TODO use PATH_LENGTH_MAX?
    ec_z(fptr->path = calloc(strnlen(path, INT_MAX) + 1, sizeof(char)), return NULL);
    memcpy(fptr->path, path, strnlen(path, INT_MAX) + 1);
    fptr->content = NULL;

    fptr->size = 0;
    // TODO should it free the queues? NO, se tengo una struttura con tutti i client
    fptr->lockersPending = queueCreate(freeNothing, cmpFd, &myerr);
    fptr->openBy = queueCreate(freeNothing, cmpFd, &myerr);
    fptr->lockedBy = 0;
    fptr->readers = 0;
    fptr->writers = 0;
    // è corretto NULL qui ???
    ec_nz(pthread_cond_init(&(fptr->go), NULL),
          {
              myerr = E_STR_COND;
              return NULL;
          });
    ec_nz(pthread_mutex_init(&(fptr->mutex), NULL),
          {
              myerr = E_STR_LOCK;
              return NULL;
          });
    ec_nz(pthread_mutex_init(&(fptr->ordering), NULL),
          {
              myerr = E_STR_LOCK;
              return NULL;
          });

    // fptr->prev = NULL;
    // fptr->next = NULL;

    return fptr;
}

int storeInit(size_t maxNfiles, size_t maxSize)
{
    // TODO safe assumere che venga chiamata una sola volta?
    int myerr;

    store.files = queueCreate(freeFile, cmpFile, &myerr);
    store.fdict = icl_hash_create(maxNfiles, NULL, cmpPathChar);
    store.maxNfiles = maxNfiles;
    store.maxSize = maxSize;
    store.currNfiles = 0;
    store.currSize = 0;

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
    icl_hash_insert(store.fdict, fptr->path, store.files->tail);

    store.currSize += fptr->size;
    store.currNfiles++;

    return 0;
}

int storeDelete(fnode *fptr, evictedFile **toRet)
{

    int myerr = 0;

    if (!fptr)
        return E_STR_INVARG;
    if (!store.files)
        return E_STR_QUEUE;

    // tmp è un nodo di queue il cui campo 'data' è fptr
    data *tmp = icl_hash_find(store.fdict, fptr->path);
    if (!tmp)
        return E_STR_NOENT;

    queueRemove_node(store.files, tmp, &myerr);
    icl_hash_delete(store.fdict, fptr->path, freeNothing, freeNothing);

    // if (myerr) return E_STR_QUEUE;

    store.currSize -= fptr->size;
    store.currNfiles--;

    // Se il file viene espulso dallo store, libero il ptr ma
    //  prima restituisco il contenuto del file

    // Se vogliamo reinserire il file alla fine della coda non deallochiamo
    if (toRet)
    {

        // 1st APPROACH -> Don't copy anything, re-assign path and content pointers

        // SALVIAMO IN toRet il content del file eliminato
        // ec_z(*toRet = malloc(sizeof(evictedFile)), return -1);
        // (*toRet)->content = fptr->content;
        // (*toRet)->path = fptr->path;
        // (*toRet)->size = fptr->size;
        // queueDestroy((fptr->lockersPending), &myerr);
        // queueDestroy((fptr->openBy), &myerr);
        // free(fptr);

        // 2nd APPROACH -> copia tutto e freefile
        //  NB: non uso storeEviction perchè fa una peek
        *toRet = copyFnode(fptr);

        freeFile(fptr);
    }

    return 0;
}

fnode *storeSearch(char *path)
{
    int myerr = 0;
    // fnode *curr = queueFind(store.files, path, cmpPath, &myerr);
    data *tmp = icl_hash_find(store.fdict, path);
    fnode *curr = NULL;
    // Se curr non è in storage
    if (!tmp)
        errno = ENOENT;
    else
    {
        curr = tmp->data;
        myerr = storeDelete(curr, NULL);
        myerr = storeInsert(curr); // LRU : riposizioniamo il file in fondo alla coda
    }

    return curr;
}

evictedFile *storeEviction(_Bool ignoreLocks, char *pathToAvoid)
{
    /*
    Perché pathToAvoid?



    */
    // Assumiamo di avere la lock sullo store per tutta la funzione
    fnode *fptr;
    int myerr = 0;
    evictedFile *tmpContent = NULL;

    fptr = queuePeek(store.files, &myerr);
    // TODO o forse no?
    // IF i'm evicting because i need size (aka pathToAvoid != NULL)
    // THEN i can avoid evicting empty files
    if (pathToAvoid && cmpPath(fptr, pathToAvoid) == 1)
    {
        // shouldn't encounter it ever again
        // Se il file da solo eccedeva la size non arrivo a questo punto
        storeSearch(fptr->path);
        fptr = queuePeek(store.files, &myerr);
    }
    LOCKORDERING; // Sono l'unico qui, perché se ci fosse stato qualcun altro,
                  // questi non avrebbe rilasciato la store che ho acquisito prima
    LOCKFILE;
    // NB: non sblocchiamo lo store qui

    while (fptr->readers || fptr->writers)
        ec_nz(pthread_cond_wait(&(fptr->go), &(fptr->mutex)), {});

    fptr->writers++;

    UNLOCKORDERING; // Superflue perché tanto sono l'unico (?)
    UNLOCKFILE;     // Di norma servirebbero per permettere ad altri r/w di acquisirle
                    // e mettersi in coda sulla 'go', ma avendo la LOCKSTORE non c'è nessuno

    if (fptr->lockedBy == 0 || ignoreLocks)
    {
        storeDelete(fptr, &tmpContent); //TODO check return value
    }
    else
    {
        // storesearch serve solo per spostare il file in fondo alla queue
        // altrimenti alla prossima iterazione la peek restituisce
        // il file locked e rimane nel loop all'infinito
        fptr = storeSearch(fptr->path);

        // aggiorniamo fptr solo se non rimuoviamo il file
        LOCKFILE;
        fptr->writers--;
        ec_nz(pthread_cond_signal(&(fptr->go)), {});
        UNLOCKFILE;
    }

    return tmpContent;
}

queue *storeCleaner(size_t sizeNeeded, char *pathToAvoid)
{
    // NB
    fnode *fptr;
    int myerr = 0;
    evictedFile *tmpContent;
    queue *toReturn = queueCreate(free, cmpEvicted, &myerr); // is strcmp ok here?
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
        tmpContent = storeEviction(0, pathToAvoid);
        queueEnqueue(toReturn, tmpContent, &myerr);
    }
    // Seconda passata : elimino indipendentemente dai O_LOCK
    while ((store.maxSize - store.currSize) < sizeNeeded)
    {
        tmpContent = storeEviction(1, pathToAvoid);
        queueEnqueue(toReturn, tmpContent, &myerr);
    }
    UNLOCKSTORE;
    return toReturn;
}

evictedFile *copyFnode(fnode *tmp)
{
    // TODO freestuff
    evictedFile *toRet;
    ec_z(toRet = malloc(sizeof(evictedFile)), return NULL;);
    ec_z(toRet->path = calloc(strnlen(tmp->path, PATH_LENGTH) + 1, sizeof(char)), return NULL;);
    ec_z(toRet->content = calloc(tmp->size, sizeof(char)), return NULL;);
    memcpy(toRet->path, tmp->path, strnlen(tmp->path, PATH_LENGTH) + 1);
    memcpy(toRet->content, tmp->content, tmp->size);
    toRet->size = tmp->size;
    return toRet;
}

void freeFile(void *arg)
{
    fnode *fptr = (fnode *)arg;
    int myerr;
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->path);
    free(fptr->content);
    queueDestroy((fptr->lockersPending), &myerr);
    queueDestroy((fptr->openBy), &myerr);

    // Anything else ???

    free(fptr);
    return;
}

void freeEvicted(void *p)
{
    evictedFile *arg = p;
    free(arg->content);
    free(arg->path);
    free(arg);
}

int storeDestroy()
{
    int myerr = 0;
    icl_hash_destroy(store.fdict, freeNothing, freeNothing);
    queueDestroy(store.files, &myerr);
    ec_nz(myerr, return -1);
    ec_nz(pthread_mutex_destroy(&(store.lockStore)), return -1);
    return 0;
}

//TODO this is just some debugging stuff

void printFD(void *arg)
{
    Client *c = arg;
    printf("FD: %d\n", c->fd);
    return;
}
void printEvicted(void *arg)
{
    evictedFile *c = arg;
    printf("PATH: %s | CONTENT: %.*s\n", c->path, (int)c->size, c->content);
    return;
}

void printFnode(void *arg)
{
    fnode *c = arg;
    printf("PATH: %s | CONTENT: %.*s\n", c->path, (int)c->size, c->content);
    return;
}