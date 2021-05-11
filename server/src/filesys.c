#include <server.h>

#define LOCKFILE ec_nz(pthread_mutex_lock(&(fptr->lockFile)),{});
#define UNLOCKFILE ec_nz(pthread_mutex_unlock(&(fptr->lockFile)),{});
#define LOCKSTORE ec_nz(pthread_mutex_lock(&(store.lockStore)),{});
#define UNLOCKSTORE ec_nz(pthread_mutex_unlock(&(store.lockStore)),{});
// #define READWAIT ec_nz(pthread_cond_wait(&(fptr->readGo),&(fptr->lockFile)),{});

// ec_nz(pthread_cond_wait(&(fptr->writeGo),&(fptr->lockFile)));

// OPERAZIONI STORAGE
// Operazioni chiamate dagli handler che controlleranno la concorrenza

// Utilizziamo paradigma per Readers-Writers descritto pag_123 cap_5.10
// !!! STARVATION

// ??? fdClient univoco?
int readFile(char *path, char **toRet, int fdClient)
{
    fnode *fptr;

    // Devo trovare il file nello store
    // Devo lockare lo store per evitare che il file venga aggiunto/eliminato mentre lo cerco
    //  Per non killare le performance
    //  potrei unlockare lo store appena trovo il file e a quel punto lockare il file
    ec_nz(pthread_mutex_lock(&(store.lockStore)),{}); // lock(store) per ricerca

    // Ma...ha senso cercare il file?
    // !!! OPENFILE dependant
    // Se ad ogni read/write corrisponde una open allora avrò trovato fnode* in quella
    //  e a questa funzione avrò passato fnode* non char *path.
    // In tal caso cambia la gestione delle lock (?)
    //  In realtà forse non molto ma vanno distribuite fra openFile e readFile
    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;                                  // meglio (semanticamente) settare errno prima di unlock
        ec_nz(pthread_mutex_unlock(&(store.lockStore)),{}); // sblocco lo store
        return -1;                                       // file non trovato
    }

    // VA FATTO? per la read forse no...
    // if (!fptr->open){
    //     errno = EBADF; // meglio (semanticamente) settare errno prima di unlock
    //     ec_nz(pthread_mutex_unlock(&(store.lockStore)));    // sblocco lo store
    //     return -1;  // file non trovato
    // }

    ec_nz(pthread_mutex_lock(&(fptr->lockFile)),{});    // Blocchiamoci sul file
    ec_nz(pthread_mutex_unlock(&(store.lockStore)),{}); // sblocco lo store
    // Posso sbloccare lo store perché se passasse il Deleter per liberare spazio,
    //  aspetterà di poter acquisire la lock sul file
    // Possiamo considerare il Deleter come un writer ?
    //  Ricorda che il Deleter entra in azione per colpa di un writer che non ha spazio per scrivere
    //  mmmh...

    if (fptr->lockedBy && fptr->lockedBy != fdClient)
    {                   // Check O_LOCK
        errno = EACCES; // File O_LOCK da un altro Client
        ec_nz(pthread_mutex_unlock(&(fptr->lockFile)),{});
        return -1;
    }

    // Wait per scrittori
    while (fptr->writer)
        ec_nz(pthread_cond_wait(&(fptr->readGo), &(fptr->lockFile)),{});

    // Scrittori usciti, ora leggo

    fptr->readers++; // Readers count update
    fptr->ref++;     // LFU references update

    // READ OPERATION
    // Può essere meglio passare direttamente i puntatori char* ?
    // *toRet = fptr->content;
    // Probab no... Chissà cosa succede al file, potrebbe diventare complicato
    toRet = malloc(fptr->size + 1);
    strncpy(*toRet, fptr->content, fptr->size);
    toRet[fptr->size] = '\0'; // man strncpy

    // Reader ha finito
    fptr->readers--; // Readers count update
    if (fptr->readers == 0 /* && waitingWriters > 0*/)
        pthread_cond_signal(&(fptr->writeGo));
    // Se non ci sono writer pazienza, il signal andrà perduto
    // Pazienza (?)

    ec_nz(pthread_mutex_unlock(&(fptr->lockFile)),{});
    return 0; // success
}

// ??? fdClient univoco?
// SERVE GESTIONE SIZE , CAPACITY ECC.
// Verrà fatta nella open ? NO! La open non sa quanto sia grosso quello va scritto
int writeFile(char *path, char *content, Client client)
{
    fnode *fptr;

    size_t size = strlen(content);
    LOCKSTORE;
    // CHECK CAPACITY (non è nella open)
    if (store.currSize + size > store.maxSize)
    {
        UNLOCKSTORE;
        // slaughterFile(size, client);
        LOCKSTORE;
    }

    // Il file esiste? Se sì, lo sovrascriviamo o scriviamo in append?
    // !!! Lasciamo da parte l'append

    if ((fptr = storeSearch(path)) == NULL)
    {
        // la openFile controllerà se abbiamo raggiunto il tetto max di file contenuti nello store
        int openf = openFile(path, 1, 0, client, &fptr);
        if (openf == -1)
        { 
            // file locked da un !=client
            // errno = EACCES è stato settato da openFile
            UNLOCKSTORE;
            return -1;
        }
        else if (openf == -2)
        {
            // non entro mai qui, giusto? S N S I
        }
        //else : openFile ha avuto success
    }

    LOCKFILE; // Blocchiamoci sul file
    UNLOCKSTORE;

    while (fptr->readers /* && fptr->writer ???*/) // Da rivedere le guardie sulle wait...
        ec_nz(pthread_cond_wait(&(fptr->writeGo), &(fptr->lockFile)),{});
    
    // BIVIO in base a openFile
    //  Non si può fare prima della wait perchè qualcuno potrebbe chiudere il file
    // 1st approach
    //  if (!fptr->open) fail;
    // 2nd approach
    // chiamo comunque openFile perchè il file deve essere aperto SOLO dal client
    //  che ha richiesto questa WRITE

    // se è locked non posso writare
    if (fptr->lockedBy && fptr->lockedBy != client.fd)
    {
        errno = EACCES;
        UNLOCKFILE;
        return -1; // partial failure
        // RICORDA: la chiamata lock del client NON deve terminare finchè
        //  il file non viene lockato
    }

    fptr->writer++;
    fptr->ref++;

    // la size di fptr è strlen(content)? e se non avesse lo '\0' alla fine?
    fptr->size = strlen(content); // senza +1 (?)
    fptr->content = malloc(fptr->size + 1);
    strncpy(fptr->content, content, fptr->size); // non becca il +1 (?) (da testare)
    fptr->content[fptr->size] = '\0';            // man strncpy

    LOCKSTORE;
    // storeInsert(fptr);
    UNLOCKSTORE;

    UNLOCKFILE;
    return 0; // success
}

int openFile(char *path, int createF, int lockF, Client client, fnode **toRet)
{
    fnode *fptr;
    LOCKSTORE;

    *toRet == NULL;
    // O_CREATE not set e il file non esiste nello storage
    if (!createF && !storeSearch(path))
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1;
    }
    // O_CREATE set e il file esiste già nello storage
    if (createF && storeSearch(path))
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
        // UNLOCKSTORE
        fptr = initFile(path);
        if (lockF)
            fptr->lockedBy = client.pid;
        // LOCKSTORE
        LOCKFILE
        storeInsert(fptr); // store già locked
        UNLOCKSTORE
        *toRet = fptr;
        UNLOCKFILE
        return 0; //success
    }
    // IL FILE ESISTE GIÀ NELLO STORAGE
    // 'else'
    fptr = storeSearch(path);

    LOCKFILE;
    while (fptr->readers /* && fptr->writer ???*/) // Da rivedere le guardie sulle wait...
        ec_nz(pthread_cond_wait(&(fptr->writeGo), &(fptr->lockFile)),{});
    
    if (fptr->lockedBy && fptr->lockedBy != client.fd)
    {
        errno = EACCES;
        UNLOCKFILE;
        return -1;
    }
    *toRet = fptr;
    UNLOCKSTORE;

    // vero casino
    // E ora? casino. Cosa ritorniamo?
    // Stiamo gestendo i lockersPending in una queue sul file
    // Il file esisteva già ed era lockato da un altro client => la open NON ha avuto successo
    // O meglio, non ancora.
    // Che fare?
    // Il problema si risolve, perchè quando chi ha lockato il file chiamerà l'unlockFile
    // Nella unlockFile scriveremo sul fd di (pop(lockersPending)) per dirgli che
    //  la sua lock è andata a buon fine
    // In questo modo nessun thread deve fermarsi ad aspettare
    if (lockF)
    {
        // RICORDA, abbiamo lockato il file

        if (fptr->lockedBy && fptr->lockedBy != client.fd)
        {
            enqueue(fptr->lockersPending, client.fd);
            UNLOCKFILE;
            return -2; // partial failure
            // RICORDA: la chiamata lock del client NON deve terminare finchè
            //  il file non viene lockato
        }
        else
        {
            fptr->lockedBy = client.fd;
            UNLOCKFILE;
            return 0; //success
        }
    }
    // dovremmo spostare questa roba nella lockfile e guardare solo il val di ritorno?
    return 0;
}

int lockFile(char *path, Client client)
{
    fnode *fptr;
    LOCKSTORE;
    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1; // il file non esiste nello storage
    }
    // else : il file c'è
    LOCKFILE;
    UNLOCKSTORE;

    while (fptr->readers /* && fptr->writer ???*/) // Da rivedere le guardie sulle wait...
        ec_nz(pthread_cond_wait(&(fptr->writeGo), &(fptr->lockFile)),{});
    

    // è stato lockato da un altro client?
    if (fptr->lockedBy && fptr->lockedBy != client.fd)
    {
        enqueue(fptr->lockersPending, client.fd);
        UNLOCKFILE;
        return -2; // partial failure
        // RICORDA: la chiamata lock del client NON deve terminare finchè
        //  il file non viene lockato
    }
    // 'else' : è stato già lockato dal client o non è stato lockato,

    fptr->lockedBy = client.fd;
    UNLOCKFILE;
    return 0; //success
}

int unlockFile (char *path, Client client) {
    fnode *fptr;
    LOCKSTORE;
    if ((fptr = storeSearch(path)) == NULL)
    {
        errno = ENOENT;
        UNLOCKSTORE;
        return -1; // il file non esiste nello storage
    }
    LOCKFILE;
    UNLOCKSTORE;
    while (fptr->readers /* && fptr->writer ???*/) // Da rivedere le guardie sulle wait...
        ec_nz(pthread_cond_wait(&(fptr->writeGo), &(fptr->lockFile)),{});
    

    if (fptr->lockedBy && fptr->lockedBy != client.fd)
    {
        errno = EACCES;
        UNLOCKFILE;
        return -1;
    }

    fptr->lockedBy = dequeue(fptr->lockersPending);
    // INFORMA IL CLIENT DELL'AVVENUTA LOCK
    // Write su lockedBy_fd


    UNLOCKFILE;
}

fnode *initFile(char *path)
{
    fnode *fptr = malloc(sizeof(fnode));
    fptr->path = path;
    fptr->content = NULL;
    fptr->size = 0;
    createQueue(fptr->lockersPending, freeFile);
    fptr->readers = 0;
    fptr->writer = 0;
    // è corretto NULL qui ???
    pthread_cond_init(&(fptr->writeGo), NULL);
    pthread_cond_init(&(fptr->readGo), NULL);
    pthread_mutex_init(&(fptr->lockFile), NULL);
    fptr->open = 1;
    fptr->ref = 0;
    fptr->prev = NULL;
    fptr->next = NULL;

    return fptr;
}

_Bool storeIsEmpty()
{
    if (store.head /* || store.currNfiles == 0 */)
        return 0;
    return 1;
}

_Bool storeIsFull()
{
    if (store.currNfiles == store.maxNfiles)
        return 0;
    return 1;
}

// Inserisce sempre in coda
int storeInsert(fnode *fptr)
{
    // ec_nz(pthread_mutex_lock(&(store.lockStore)));

    // if (!store || !fptr) return -1;

    fptr->next = NULL;
    fptr->prev = store.tail;

    // Inserimento in testa
    if (!store.head)
    {
        store.head = fptr;
        store.tail = fptr;
    }
    // inserimento in coda
    else
    {
        store.tail->next = fptr;
    }

    store.currNfiles++;
    store.currSize += sizeof(fptr->content);
}

int storeDelete(fnode *fptr)
{

    if (!fptr->prev)
    { // fptr testa
        store.head = NULL;
    }
    else
    {
        fptr->prev->next = fptr->next;
    }
    if (!fptr->next)
    { // fptr coda
        store.tail = NULL;
    }
    else
    {
        fptr->next->prev = fptr->prev;
    }

    //

    freeFile(fptr);

    return 0;
}

fnode *storeSearch(char *path)
{
    fnode *curr = store.head;
    // Scorro finchè non trovo il file desiderato o arrivo in fondo
    while (curr && strncmp(curr->path, path, strlen(path)))
    {
        curr = curr->next;
    }

    // Se curr non è in storage
    if (!curr)
        errno = ENOENT;

    
    return curr; // file non trovato
}

int freeFile(fnode *fptr)
{
    // Assumiamo che il caller abbia sistemato mutex e cond
    free(fptr->content);
    destroyQueue(&(fptr->lockersPending));

    // Anything else ???

    free(fptr);
    return 0;
}