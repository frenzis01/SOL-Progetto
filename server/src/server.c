#include <server.h>

#pragma region
// output colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#pragma endregion

#define eo(c)                                                      \
    if (errno != 0)                                                \
    {                                                              \
        perror(ANSI_COLOR_RED "Server Internal" ANSI_COLOR_RESET); \
        c;                                                         \
    }
#define eo_af(c, f)                                                \
    c;                                                             \
    if (errno)                                                     \
    {                                                              \
        perror(ANSI_COLOR_RED "Server Internal" ANSI_COLOR_RESET); \
        f;                                                         \
    }

#define log(s, i, c)                                                      \
    res = (s);                                                            \
    errnosave = errno;                                                    \
    strerror_r(errno, errnoBuf, 200);                                     \
    if (res == -1)                                                        \
    {                                                                     \
        perror(ANSI_COLOR_CYAN "INTERNAL FATAL ERROR" ANSI_COLOR_RESET);  \
        c;                                                                \
    }                                                                     \
    snprintf(toLog, LOGBUF_LEN, "%ld__%d__" #s "__%s", i, res, errnoBuf); \
    eo_af(LoggerLog(toLog, strlen(toLog)), WK_DIE_ON_ERR);

#define PUSH_REQUEST(x)                                   \
    ec_nz(pthread_mutex_lock(&lockReq), {});              \
    /* // TODO okay to push an int? instead of a void* */ \
    eo_af(queueEnqueue(requests, x), /* handle error */); \
    ec_nz(pthread_cond_signal(&condReq), {});             \
    ec_nz(pthread_mutex_unlock(&lockReq), {});

typedef struct
{
    size_t tid;
    pthread_t *sigHandler;
} workerArgs;

#define CONFIG_PATH "config.txt"

// Prototypes
int sendResponseCode(int fd, int errnosave);
int sendFileToClient(evictedFile *fptr, int fd);
int notifyFailedLockers(queue *lockers, const short msg);
void *signalHandler(void *args);

// Client -> Dispatcher -> Manager -> FIFO request -> Workers (n)
//                              <-   pipe   <-
//

// Comunicazione Worker -> Manager
// La pipe va collegata alla select
#define MANAGER_READ mwPipe[0]
#define WORKER_WRITE mwPipe[1]
int mwPipe[2];

// Comunicazione signalHandler -> Manager
#define SIGNAL_READ sigPipe[0]
#define SIGNAL_WRITE sigPipe[1]
int sigPipe[2];

// Comunicazione richieste client Manager -> Worker
pthread_mutex_t lockReq = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condReq = PTHREAD_COND_INITIALIZER;
queue *requests;

// THREAD DISPATCHER
#define BUF 8192
#define UNIX_PATH_MAX 108

volatile sig_atomic_t myShutdown = 0;

#define LOG_PATH "log.txt"
#define STARTUP_MSG "SERVER STARTUP"
#define CLEANEXIT_MSG "SERVER SHUTDOWN"
#define H_BUCKETS 2048

#define HARSH_QUIT 2
#define HUP_QUIT 1

// Logger buf dim
#define LOGBUF_LEN 400
#define ERRNOBUF_LEN 200

void removeSocket()
{
    unlink(sockName);
    icl_hash_destroy(clients, NULL, NULL);
    storeDestroy();
}

int main(void)
{


    // char cwd[PATH_MAX];
    // if (getcwd(cwd, sizeof(cwd)) != NULL)
    // {
    //     printf("Current working dir: %s\n", cwd);
    // }
    // else
    // {
    //     perror("getcwd() error");
    //     return 1;
    // }

    // Signal handler setup
    sigset_t mask = initSigMask();
    ec_nz(pthread_sigmask(SIG_SETMASK, &mask, NULL), exit(EXIT_FAILURE));
    pthread_t sigHandler;
    ec_neg1(pthread_create(&sigHandler, NULL, signalHandler, NULL), exit(EXIT_FAILURE));

    // Server initialization
    ServerData meta;
    ec_neg1(readConfig(CONFIG_PATH, &meta), exit(EXIT_FAILURE));
    ec_neg1(storeInit(meta.nfiles, meta.capacity, meta.evictPolicy), exit(EXIT_FAILURE));
    ec_z(clients = icl_hash_create(H_BUCKETS,NULL,NULL), exit(EXIT_FAILURE));
    size_t poolSize = meta.workers;
    sockName = meta.sockname; // declared in conn.h
    
    atexit(removeSocket); // we do not care if they were uninitialized

    eo_af(LoggerCreate(LOG_PATH), storeDestroy(); exit(EXIT_FAILURE););
    eo_af(LoggerLog(STARTUP_MSG, strlen(STARTUP_MSG)), storeDestroy(); exit(EXIT_FAILURE););

    // Spawning threads
    pthread_t workers[poolSize], dispThread;

    // Create workers first, so there is no (less) risk that requests received (and 'signaled'!) before
    // the workers started waiting are never taken in charge
    workerArgs args[1];
    args->sigHandler = &sigHandler;
    for (size_t i = 0; i < poolSize; i++) // workers init
    {
        args->tid = i;
        ec_neg1(pthread_create(&workers[i], NULL, worker, (void *)(args)), /* handle error */);
    }

    ec_neg1(pthread_create(&dispThread, NULL, dispatcher, NULL), storeDestroy(); exit(EXIT_FAILURE));

    // cleanup
    ec_neg1(pthread_join(sigHandler, NULL), /* handle error */);
    ec_neg1(pthread_join(dispThread, NULL), /* handle error */);

    // send termination msg (NULL) to threads
    for (size_t i = 0; i < poolSize; i++)
    {
        PUSH_REQUEST(NULL);
    }

    for (size_t i = 0; i < poolSize; i++)
    {
        ec_neg1(pthread_join(workers[i], NULL), /* handle error */);
    }

    close(SIGNAL_WRITE);
    close(SIGNAL_READ);
    close(WORKER_WRITE);
    close(MANAGER_READ);

    return 0; // storeDestroy() and unlink are in 'atexit'
}

void *dispatcher(void *arg)
{
    // SERVER - GESTIONE SOCKET

    // fd_c == fd_client
    int fd_skt, fd_c, fd_hwm = 0, fd;
    char
        toLog[LOGBUF_LEN],
        buf[BUF], //read buffer
        fdBuf[INT_LEN],
        *fdTmp;
    fd_set set, read_set;
    ssize_t nread;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockName, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    // EXIT_FAILURE o return -1 ???
    ec_neg1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), {});
    ec_neg1(bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa)), {});
    ec_neg1(listen(fd_skt, SOMAXCONN), {});

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    FD_SET(MANAGER_READ, &set); // aggiungiamo la pipe fra M e W
    FD_SET(SIGNAL_READ, &set);  // aggiungiamo la pipe fra M e W

    int fdhwm = max(3, fd_skt, MANAGER_READ, SIGNAL_READ);

    int sel_ret;
    Client *requestor = NULL;

    // pthread_mutex_lock(&mutex);
    while (myShutdown != HARSH_QUIT)
    {
        // pthread_mutex_unlock(&mutex);
        read_set = set;
        ec_neg1((sel_ret = select(fd_hwm + 1, &read_set, NULL, NULL, NULL /*&timeout*/)), /* handle error */);
        /* if (sel_ret == 0)
        {
            continue; // skippi alla prossima iterazione del ciclo
        } */
        for (fd = 0; fd <= fd_hwm /*&& !sig_flag*/; fd++)
        {
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == fd_skt) // new connection
                {
                    ec_neg1(fd_c = accept(fd_skt, NULL, 0), {});
                    if (myShutdown) // If exiting, i don't accept new clients
                    {
                        ec_neg1(close(fd_c), exit(EXIT_FAILURE));
                    }
                    else
                    {
                        // activeConnections++;
                        ec_z(addClient(fd_c), /* handle error */);
                        ec_neg1(snprintf(toLog, LOGBUF_LEN, "CLIENT NEW: %d", fd_c), /* handle error */);
                        eo_af(LoggerLog(toLog, strlen(toLog)), /* handle error */);
                        FD_SET(fd_c, &set);
                        if (fd_c > fd_hwm)
                            fd_hwm = fd_c;
                    }
                }
                else if (fd == MANAGER_READ) // worker has done
                {
                    // Worker comunica quali fd dei client sono ancora attivi
                    // e dunque devono essere reinseriti nel set

                    // Se ci fossero più di 10 fd da leggere non è un problema,
                    // Rimarranno nella pipe e verranno letti alla prossima iterazione

                    int i = 0, *fdFromWorker = calloc(10, sizeof(int));
                    ec_neg1(readn(MANAGER_READ, fdFromWorker, 10 * sizeof(int)), /* handle error */);
                    while (fdFromWorker[i] != 0)
                    {
                        if (fdFromWorker[i] < 0)
                        { // a client left
                            fdFromWorker[i] *= -1;
                            ec_neg1(removeClient(fdFromWorker[i]), /* handle error */);
                            ec_neg1(snprintf(toLog, LOGBUF_LEN, "CLIENT LEFT: %d", fdFromWorker[i]), /* handle error */);
                            eo_af(LoggerLog(toLog, strlen(toLog)), /* handle error */);
                            if (NoMoreClients() && myShutdown == HUP_QUIT)
                            {
                                // done reading
                                goto dispatcher_exit;
                            }
                        }
                        else
                        {
                            FD_SET(fdFromWorker[i], &set);
                            if (fdFromWorker[i] > fd_hwm)
                                fd_hwm = fdFromWorker[i];
                        }
                        i++;
                    }
                }
                else if (fd == SIGNAL_READ) // Wake from signal handler
                {
                    puts("SIGNAL RECEIVED");
                    if (myShutdown == HARSH_QUIT)
                        break;
                }
                else // New request
                {
                    // instead of reading here, i push the fd in the buffer
                    FD_CLR(fd, &set);
                    if (fd == fd_hwm)
                        fd_hwm--;
                    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), /* handle error */);
                    ec_z(requestor = icl_hash_find(clients, fdBuf), /* handle error */);
                    PUSH_REQUEST(requestor);
                }
            }
        }
    }
dispatcher_exit:
    return NULL;
}

void *signalHandler(void *args)
{
    sigset_t *set = args;
    int r, sig;
    while (1)
    {
        r = sigwait(set, &sig);
        ec_nz(r, return NULL; /*exit(EXIT_FAILURE)*/);
    }
    switch (sig)
    {
    case SIGINT:
    case SIGQUIT:
        myShutdown = HARSH_QUIT;
        close(SIGNAL_WRITE);
        break;
    case SIGHUP:
        myShutdown = HUP_QUIT;
        close(SIGNAL_WRITE);
        break;
    default:
        break;
    }
    return NULL;
}

#define WK_DIE_ON_ERR                                               \
    perror(ANSI_COLOR_RED "WORKER DIED" ANSI_COLOR_RESET);          \
    LoggerLog(deathMSG, strlen(deathMSG));                          \
    ec_nz(pthread_kill(sigHandlerPtr, SIGINT), exit(EXIT_FAILURE)); \
    goto worker_cleanup;

/**
 * When we accept a request from an fd (in the thread dispatcher) we remove it 
 * from the select's set, so a worker is the only one writing on a given fd. 
 * 
 */
void *worker(void *args)
{
    size_t myTid = ((workerArgs *)args)->tid;
    pthread_t sigHandlerPtr = *(((workerArgs *)args)->sigHandler);

    Client *requestor = NULL;
    int fd, res = 0, errnosave = 0, specialMSG = 0;
    char
        toLog[LOGBUF_LEN],
        errnoBuf[ERRNOBUF_LEN],
        deathMSG[14 + sizeof(size_t)];
    Request *req = NULL;
    evictedFile *evicted = NULL;
    queue
        *evictedList = NULL,
        *notifyLockers = NULL;

    snprintf(deathMSG, 14 + sizeof(size_t), "WORKER DIED: %ld", myTid); // will be used in WK_DIE_ON_ERR

    while (!myShutdown)
    {
        // GET A REQUEST
        ec_nz(pthread_mutex_lock(&lockReq), {});

        ec_nz(pthread_cond_wait(&condReq, &lockReq), {}); // wait for requests
        eo_af(requestor = queueDequeue(requests), WK_DIE_ON_ERR);
        ec_nz(pthread_mutex_unlock(&lockReq), {});
        fd = requestor->fd;

        // Parsing Nightmare
        specialMSG = 0;
        req = getRequest(fd, &specialMSG);
        if (!req && specialMSG == 1) // client closed the socket
        {
            fd *= -1;
            ec_neg1(writen(WORKER_WRITE, &fd, sizeof(int)), WK_DIE_ON_ERR);
            /*
            ec_neg1(removeClient(fd), WK_DIE_ON_ERR);
            ec_neg1(snprintf(toLog, LOGBUF_LEN, "CLIENT LEFT: %d", fd), WK_DIE_ON_ERR);
            eo_af(LoggerLog(toLog, strlen(toLog)), WK_DIE_ON_ERR);
            // if there are no more clients and we are HUP-exiting, we must notify the dispatcher
            if (NoMoreClients() && myShutdown == HUP_QUIT) {
                // notify dispatcher
            }
            */
            continue;
        }
        else if (!req && specialMSG == 2)
        { // termination message
            break;
        }
        else if (!req)
        {
            WK_DIE_ON_ERR;
        }
        switch (req->op)
        {
        case OPEN_FILE:
            /* code */
            log(openFile(req->path, req->o_creat, req->o_lock, req->client, &evicted), myTid, WK_DIE_ON_ERR);
            break;
        case READ_FILE:
            log(readFile(req->path, &evicted, req->client, 0), myTid, WK_DIE_ON_ERR);
            break;
        case READN_FILES:
            log(readNfiles(req->nfiles, &evictedList, req->client), myTid, WK_DIE_ON_ERR);
            break;

        case APPEND:
            log(appendToFile(req->path, req->append, req->appendLen, req->client, &evictedList, 0), myTid, WK_DIE_ON_ERR);
            break;

        case WRITE_FILE:
            log(appendToFile(req->path, req->append, req->appendLen, req->client, &evictedList, 1), myTid, WK_DIE_ON_ERR);
            break;

        case LOCK_FILE:
            log(lockFile(req->path, req->client), myTid, WK_DIE_ON_ERR);
            if (errnosave == EACCES)
                continue; // in this case, the client has to wait
            break;

        case UNLOCK_FILE:
            log(unlockFile(req->path, req->client), myTid, WK_DIE_ON_ERR);
            if (res > 1) // notify new lock owner
            {
                ec_neg1(sendResponseCode(res, 0), WK_DIE_ON_ERR);
            }
            break;

        case CLOSE_FILE:
            log(closeFile(req->path, req->client), myTid, WK_DIE_ON_ERR);
            break;

        case REMOVE_FILE:
            log(removeFile(req->path, req->client, &evicted), myTid, WK_DIE_ON_ERR);
            break;

        default:
            break;
        }
        /* if (res == -1)
        {
            // perror?

            //free everything
            queueDestroy(evictedList);
            queueDestroy(notifyLockers);
            freeEvicted(evicted);
            evicted = NULL;
            evictedList = NULL;
            notifyLockers = NULL;

            //HANDLE INTERNAL ERROR

            break; // ?
        }
        else */
        // if (res != -1) shouldn't be necessary
        {
            //SEND RESPONSE TO CLIENT
            ec_neg1(sendResponseCode(fd, errnosave), WK_DIE_ON_ERR);
        }
        // Send back to client and notify pending lockers
        if (evictedList)
        {
            // Can be capacityVictims or readNfiles target
            evictedFile *curr = queueDequeue(evictedList);
            ec_neg1(writen(fd, &(evictedList->size), sizeof(size_t)), WK_DIE_ON_ERR);
            while (!errno && curr)
            {
                ec_neg1(sendFileToClient(curr, fd), WK_DIE_ON_ERR);
                if (req->op != READN_FILES) // if it is a victim
                {
                    ec_neg1(notifyFailedLockers(curr->notifyLockers, FILE_NOT_FOUND), WK_DIE_ON_ERR);
                    curr->notifyLockers = NULL;
                }
                freeEvicted(curr);
            }
            queueDestroy(evictedList);
        }
        if (evicted)
        {
            //Can be openVictim or readFile target
            ec_neg1(sendFileToClient(evicted, fd), WK_DIE_ON_ERR);
            if (req->op != READ_FILE) // if it is a victim
            {
                ec_neg1(notifyFailedLockers(evicted->notifyLockers, FILE_NOT_FOUND), WK_DIE_ON_ERR);
                evicted->notifyLockers = NULL;
            }
            freeEvicted(evicted);
        }
        if (notifyLockers)
        {
            ec_neg1(notifyFailedLockers(notifyLockers, FILE_NOT_FOUND), WK_DIE_ON_ERR);
        }

        evicted = NULL;
        evictedList = NULL;
        notifyLockers = NULL;

        // We need to put the ready fd back in the select's set
        ec_neg1(writen(WORKER_WRITE, &fd, sizeof(int)), WK_DIE_ON_ERR); //pipe Worker -> Manager
    }

worker_cleanup:
    queueDestroy(evictedList);
    queueDestroy(notifyLockers);
    freeEvicted(evicted);
    evicted = NULL;
    evictedList = NULL;
    notifyLockers = NULL;
    return NULL;
}

/**
 * Sends response code
 */
int sendResponseCode(int fd, int errnosave)
{
    char buf[1];
    // TODO okay to use buf[1] like this?
    switch (errnosave)
    {
    case ENOENT:
        *buf = FILE_NOT_FOUND;
        ec_n(writen(fd, buf, 1), 1, { /* handle error*/ return -1; });
        break;
    case EACCES:
        *buf = NO_ACCESS;
        ec_n(writen(fd, buf, 1), 1, { /* handle error*/ return -1; });
        break;
    case EFBIG:
        *buf = TOO_BIG;
        ec_n(writen(fd, buf, 1), 1, { /* handle error*/ return -1; });
        break;
    case EADDRINUSE:
        *buf = ALREADY_EXISTS;
        ec_n(writen(fd, buf, 1), 1, { /* handle error*/ return -1; });
        break;
    case 0:
        *buf = SUCCESS;
        ec_n(writen(fd, buf, 1), 1, { /* handle error*/ return -1; });
        break;
    default:
        break;
    }
    return 0;
}

#define BUFSIZE MSGLEN_DIM + strlen(fptr->path) + MSGLEN_DIM + fptr->size + 1
/**
 * Sends and evicted file to a client. Doens't notify the (pending) lockers
 */
int sendFileToClient(evictedFile *fptr, int fd)
{
    char *buf = NULL;
    // TODO '+1' ok? Sì, perchè buf sarà stringa e servirà '\0'
    ec_z(buf = calloc(BUFSIZE, 1), return -1);
    // {10B_pathLen,path,10B_size,content}
    // path + size
    ec_n(snprintf(buf, BUFSIZE - fptr->size, "%010ld%s%010ld", strlen(fptr->path), fptr->path, fptr->size),
         BUFSIZE - fptr->size,
         {
             free(buf);
             return -1;
         });
    // content
    memcpy(buf + strlen(buf), fptr->content, fptr->size);

    // write to the client
    ec_n(writen(fd, buf, BUFSIZE), BUFSIZE, free(buf); return -1;);
    return 0; // success
}

/**
 * Writes 'msg' on the pending lockers' fd. \n \n
 * Destroys 'lockers'
 * @param msg one the response codes
 * @returns 0 success, -1 error (errno set)
 */
int notifyFailedLockers(queue *lockers, const short msg)
{
    errno = 0;
    Client *curr = queueDequeue(lockers);
    while (!errno && curr)
    {
        // errno might get dirty here
        char buf[1];
        buf[1] = msg; // notify client
        ec_n(writen(curr->fd, buf, 1), 1, queueDestroy(lockers); return -1;);
        // notify dispatcher to put fd back into select's set
        ec_n(writen(WORKER_WRITE, &(curr->fd), sizeof(int)), sizeof(int), queueDestroy(lockers); return -1;);

        curr = curr = queueDequeue(lockers);
    }
    queueDestroy(lockers);
    return 0; // success
}

// leggi configurazione server
/**
 * Reads "workers" "nfiles" "capacity" "sockname" "evictPolicy from a config file"
 * @returns -1 on error, 0 success
 */
int readConfig(char *configPath, ServerData *new)
{
    // INIT STATS

    FILE *conf;
    ec(conf = fopen(configPath, "r"), NULL, return -1;);

    ec_neg1(conf_sizet(conf, "workers", &(new->workers)), return -1;);
    ec_neg1(fseek(conf, 0, SEEK_SET), return -1;);
    ec_neg1(conf_sizet(conf, "nfiles", &(new->nfiles)), return -1;);
    ec_neg1(fseek(conf, 0, SEEK_SET), return -1;);
    ec_neg1(conf_sizet(conf, "capacity", &(new->capacity)), return -1;);
    ec_neg1(fseek(conf, 0, SEEK_SET), return -1;);
    ec_z(new->sockname = conf_string(conf, "sockname"), return -1);
    ec_neg1(fseek(conf, 0, SEEK_SET), free(new->sockname); return -1;);
    ec_neg1(conf_sizet(conf, "evictPolicy", &(new->evictPolicy)), free(new->sockname); return -1;);

    if (new->evictPolicy)
        new->evictPolicy = 1;

    /* code */

    fclose(conf);

    return 0;
}
