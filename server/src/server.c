#define _POSIX_C_SOURCE 200809L
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <poll.h>
#include <sys/select.h>
#include <stdarg.h>

#include <filesys.h>
#include <logger.h>
#include <utils.h>
#include <icl_hash.h>
#include <queue.h>
#include <conn.h>
#include <protocol.h>

// Structs
typedef struct
{
    size_t workers;
    size_t capacity;
    size_t nfiles;
    size_t evictPolicy;
    char *sockname;
} ServerData; // init metadata
typedef struct
{
    size_t tid;
    pthread_t *sigHandler;
} workerArgs; // worker arguments

// Prototypes
void *worker(void *arg);
int readConfig(char *configPath, ServerData *new);
int parseRequest(char *str, Request *req);
void *dispatcher(void *arg);
int sendFileToClient(evictedFile *fptr, int fd, _Bool *W_hasExited);
int notifyPendingLockers(queue *lockers, int msg);
void *signalHandler(void *args);
int removeClient(int fd, queue **notifyLockers);
int manageExit(int fd);
int statsToLog(size_t maxNclients);

// Useful macros and constants
#pragma region
#define p(c)           \
    if (printsEnabled) \
    {                  \
        c;             \
    }

#define UPDATE_MAX(m, a) a > m ? m = a : m;

// used by workers
#define ec_neg1_SKT(s, c)                                                \
    do                                                                   \
    {                                                                    \
        if (!hasExited && (s) == (-1))                                   \
        {                                                                \
            if (errno == ECONNRESET || errno == EBADF || errno == EPIPE) \
            {                                                            \
                hasExited = 1;                                           \
                ec_neg1(manageExit(fd), c);                              \
            }                                                            \
            else                                                         \
            {                                                            \
                perror(#s);                                              \
                c;                                                       \
            }                                                            \
        }                                                                \
    } while (0);

#define log(s, c)                                                                                    \
    do                                                                                               \
    {                                                                                                \
        res = (s);                                                                                   \
        if (res == -1)                                                                               \
        {                                                                                            \
            perror(BCYN "INTERNAL FATAL ERROR" REG);                                                 \
            c;                                                                                       \
        }                                                                                            \
        if (res == 1)                                                                                \
        {                                                                                            \
            errnosave = errno;                                                                       \
        }                                                                                            \
        else                                                                                         \
            errnosave = 0;                                                                           \
        strerror_r(errno, errnoBuf, 200);                                                            \
        snprintf(toLog, REQstr_LEN + ERRNOBUF_LEN, "%ld__%s__%d__%s", myTid, reqStr, res, errnoBuf); \
        p(puts(toLog));                                                                              \
        ec_neg1(LoggerLog(toLog, strlen(toLog)), c);                                                 \
    } while (0);

#define logEvicted(evict, f, c)                                                                                                                                           \
    ec_neg(snprintf(toLog, REQstr_LEN + ERRNOBUF_LEN, "%ld__%d__%s to %d: %ld %s", myTid, req->op, evict ? "Evicting" : "Sending", fd, f->size, f->path), WK_DIE_ON_ERR); \
    p(puts(toLog));                                                                                                                                                       \
    ec_neg1(LoggerLog(toLog, strlen(toLog)), c);

#define PUSH_REQUEST(x, err_handle)                   \
    ec_nz(pthread_mutex_lock(&lockReq), err_handle);  \
    ec_neg1(queueEnqueue(requests, x), err_handle);   \
    ec_nz(pthread_cond_signal(&condReq), err_handle); \
    ec_nz(pthread_mutex_unlock(&lockReq), err_handle);

// Some constants
#define BUF 8192
#define LOG_PATH "log.txt"
#define STARTUP_MSG "SERVER STARTUP"
#define CLEANEXIT_MSG "SERVER SHUTDOWN "
#define H_BUCKETS 2048

#define HARSH_QUIT 2
#define HUP_QUIT 1
#pragma endregion

// COMMUNICATION BETWEEN THREADS

// Workers -> Dispatcher
// Used to put or remove FDs from the select's set
#define MANAGER_READ mwPipe[0]
#define WORKER_WRITE mwPipe[1]
int mwPipe[2];

// signalHandler -> Dispatcher
// Used to wake dispatcher when a signal is caught
#define SIGNAL_READ sigPipe[0]
#define SIGNAL_WRITE sigPipe[1]
int sigPipe[2];

// Dispatcher -> Workers
// Dispatcher pushes here a Client who sent a request
pthread_mutex_t lockReq = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condReq = PTHREAD_COND_INITIALIZER;
queue *requests;

// Some global variables
int myShutdown = 0;
char *sockName = "";
_Bool printsEnabled = 0;

// atexit
void removeSocket()
{
    unlink(sockName);
    free(sockName);
}

int main(int argc, char **argv)
{
    if (argc == 1 || argc > 3)
    {
        puts(BRED "No config.txt path specified. Exiting...");
        puts("Usage: config_file_path [ printsEnabled ]" REG);
        exit(EXIT_FAILURE);
    }
    char *config = argv[1];
    // If there's a second argument then prints are enabled, regardless of its content
    if (argc == 3)
        printsEnabled = 1;
    else
        printsEnabled = 0;

    atexit(removeSocket);

    // Signals masking
    sigset_t mask = initSigMask();

    // Signal handler setup
    myShutdown = 0;
    ec_neg1(pipe(sigPipe), exit(EXIT_FAILURE));
    ec_neg1(pipe(mwPipe), exit(EXIT_FAILURE));
    pthread_t sigHandThread;
    ec_neg1(pthread_create(&sigHandThread, NULL, signalHandler, &mask), exit(EXIT_FAILURE));

    // Server initialization
    ServerData meta;
    ec_neg1(readConfig(config, &meta), exit(EXIT_FAILURE));

    ec_neg1(storeInit(meta.nfiles, meta.capacity, meta.evictPolicy), exit(EXIT_FAILURE)); // STORAGE
    ec_nz(pthread_mutex_init(&lockClients, NULL), exit(EXIT_FAILURE));                    // CLIENTS
    ec_z(clients = icl_hash_create(H_BUCKETS, NULL, NULL), exit(EXIT_FAILURE));
    ec_z(requests = queueCreate(freeNothing, NULL), exit(EXIT_FAILURE)); // MW-FIFO
    size_t poolSize = meta.workers;                                      // META
    sockName = meta.sockname;                                            // declared in conn.h

    // Init Logger
    ec_neg1(LoggerCreate(LOG_PATH), storeDestroy(); exit(EXIT_FAILURE););
    ec_neg1(LoggerLog(STARTUP_MSG, strlen(STARTUP_MSG)), storeDestroy(); exit(EXIT_FAILURE););

    // Spawning threads
    pthread_t workers[poolSize], dispThread;

    // Workers
    workerArgs args[poolSize];
    for (size_t i = 0; i < poolSize; i++) // workers init
    {
        args[i].tid = i;
        args[i].sigHandler = &sigHandThread;
        ec_neg1(pthread_create(&workers[i], NULL, worker, (void *)(args + i)), exit(EXIT_FAILURE));
    }

    // Dispatcher/Manager
    ec_neg1(pthread_create(&dispThread, NULL, dispatcher, (void *)(&sigHandThread)), storeDestroy(); exit(EXIT_FAILURE));

    // FROM NOW ON, WE ARE MANAGING EXIT

    // Join dispatcher first
    ec_neg1(pthread_join(dispThread, NULL), exit(EXIT_FAILURE));

    // Send termination msg (NULL) to workers who might be waiting for requests
    for (size_t i = 0; i < poolSize; i++)
    {
        PUSH_REQUEST(NULL, exit(EXIT_FAILURE));
    }
    // Join workers
    for (size_t i = 0; i < poolSize; i++)
    {
        ec_neg1(pthread_join(workers[i], NULL), exit(EXIT_FAILURE));
    }

    ec_neg1(pthread_join(sigHandThread, NULL), exit(EXIT_FAILURE));

    puts("All threads joined");

    close(WORKER_WRITE);
    close(MANAGER_READ);

    icl_hash_destroy(clients, free, free);
    storeStats();
    storeDestroy();
    queueDestroy(requests);

    ec_neg1(LoggerLog(CLEANEXIT_MSG, strlen(STARTUP_MSG)), storeDestroy(); exit(EXIT_FAILURE););
    LoggerDelete();

    return 0; // unlink is in 'atexit'
}

// DISPATCHER (/manager)
#define DS_DEATH_MSG "DISPATCHER DIED"
#define DS_DIE_ON_ERR                                                   \
    do                                                                  \
    {                                                                   \
        perror(BRED "DISPATCHER DIED" REG);                             \
        LoggerLog(DS_DEATH_MSG, strlen(DS_DEATH_MSG));                  \
        ec_nz(pthread_kill(sigHandlerRef, SIGINT), exit(EXIT_FAILURE)); \
        goto dispatcher_cleanup;                                        \
    } while (0)

void *dispatcher(void *arg)
{
    p(puts(BYEL "Dispatcher startup" REG));

    pthread_t sigHandlerRef = *((pthread_t *)arg);
    size_t currNClient = 0;
    size_t maxNClient = 0;

    // fd_c == fd_client
    int fd_skt, fd_c, fd;
    char
        toLog[LOGBUF_LEN],
        // buf[BUF], //read buffer
        fdBuf[INT_LEN];
    // *fdTmp;
    fd_set set, read_set;

    struct sockaddr_un s_addr;
    memset(&s_addr, '0', sizeof(s_addr));
    strncpy(s_addr.sun_path, sockName, UNIX_PATH_MAX);
    s_addr.sun_family = AF_UNIX;

    ec_neg1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), DS_DIE_ON_ERR);
    ec_neg1(bind(fd_skt, (struct sockaddr *)&s_addr, sizeof(s_addr)), DS_DIE_ON_ERR);
    ec_neg1(listen(fd_skt, SOMAXCONN), DS_DIE_ON_ERR);

    FD_ZERO(&set);
    FD_ZERO(&read_set);

    FD_SET(fd_skt, &set);
    FD_SET(MANAGER_READ, &set);
    FD_SET(SIGNAL_READ, &set);

    int fd_hwm = max(3, fd_skt, MANAGER_READ, SIGNAL_READ);

    Client *requestor = NULL;

    while (myShutdown != HARSH_QUIT)
    {
        read_set = set;
        ec_neg1(select(fd_hwm + 1, &read_set, NULL, NULL, NULL), DS_DIE_ON_ERR);
        for (fd = 0; fd <= fd_hwm /*&& !sig_flag*/; fd++)
        {
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == fd_skt) // new connection
                {
                    ec_neg1(fd_c = accept(fd_skt, NULL, 0), {});
                    if (myShutdown) // If exiting, don't accept new clients
                    {
                        p(puts(BYEL "Dispatcher - New connection refused" REG));
                        ec_neg1(close(fd_c), DS_DIE_ON_ERR);
                    }
                    else
                    {
                        currNClient++;
                        UPDATE_MAX(maxNClient, currNClient);
                        Client *tmp = addClient(fd_c); // add new client to Clients' structure
                        ec_z(tmp, DS_DIE_ON_ERR);
                        p(printf(BYEL "Dispatcher - New connection accepted : %d - %ld\n" REG, tmp->fd, currNClient));
                        ec_neg(snprintf(toLog, LOGBUF_LEN, "CLIENT ADDED: %d - %ld", fd_c, currNClient), DS_DIE_ON_ERR;);
                        ec_neg1(LoggerLog(toLog, strlen(toLog)), DS_DIE_ON_ERR);
                        FD_SET(fd_c, &set); // listen to new fd
                        if (fd_c > fd_hwm)
                            fd_hwm = fd_c;
                    }
                }
                else if (fd == MANAGER_READ) // Worker has done
                {
                    int fdFromWorker;
                    ec_neg1(readn(MANAGER_READ, &fdFromWorker, sizeof(int)), DS_DIE_ON_ERR);
                    if (fdFromWorker < 0) // a client left
                    {                     // The worker who wrote has already removed the client from the filesys,
                                          //    but didn't close its fd
                        currNClient--;
                        fdFromWorker *= -1;
                        p(printf(BYEL "Dispatcher - Removing client %d - %ld\n" REG, fdFromWorker, currNClient));
                        ec_nz(close(fdFromWorker), DS_DIE_ON_ERR);
                        ec_neg1(snprintf(toLog, LOGBUF_LEN, "CLIENT LEFT: %d - %ld", fdFromWorker, currNClient), DS_DIE_ON_ERR);
                        ec_neg1(LoggerLog(toLog, strlen(toLog)), DS_DIE_ON_ERR);

                        if (NoMoreClients() && myShutdown == HUP_QUIT) // soft exiting
                        {
                            goto dispatcher_cleanup;
                        }
                    }
                    else
                    {
                        p(printf(BYEL "Dispatcher - Worker has done : %d\n" REG, fdFromWorker));
                        FD_SET(fdFromWorker, &set);
                        if (fdFromWorker > fd_hwm)
                            fd_hwm = fdFromWorker;
                    }
                }
                else if (fd == SIGNAL_READ) // Wake from signal handler
                {
                    p(puts(BYEL "Dispatcher - SIGNAL RECEIVED" REG));
                    if (myShutdown == HARSH_QUIT || (myShutdown == HUP_QUIT && NoMoreClients()))
                    {
                        p(puts("\t...EXITING..."));
                        goto dispatcher_cleanup;
                    }
                    else
                    {
                        int sig = 0;
                        readn(SIGNAL_READ, &sig, sizeof(int));
                        if (sig == SIGUSR1)
                        { // print Stats
                            ec_z(requestor = malloc(sizeof(Client)), DS_DIE_ON_ERR);
                            requestor->fd = -1;
                            PUSH_REQUEST(requestor, DS_DIE_ON_ERR);
                        }
                        else                           // HUP_EXITING but still some clients connected
                            FD_CLR(SIGNAL_READ, &set); // sigHandler returned, no reason to keep listening
                    }
                }
                else // New request
                {
                    p(printf(BYEL "Dispatcher - Request %d\n" REG, fd));
                    // instead of reading here, i push the fd in the buffer
                    FD_CLR(fd, &set);
                    if (fd == fd_hwm)
                        fd_hwm--;
                    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), DS_DIE_ON_ERR);
                    ec_nz(LOCKCLIENTS, DS_DIE_ON_ERR);
                    ec_z(requestor = icl_hash_find(clients, fdBuf), DS_DIE_ON_ERR);
                    ec_nz(UNLOCKCLIENTS, DS_DIE_ON_ERR);
                    PUSH_REQUEST(requestor, DS_DIE_ON_ERR);
                }
            }
        }
    }
dispatcher_cleanup:
    statsToLog(maxNClient);
    return NULL;
}

// SIGNAL HANDLER
void *signalHandler(void *args)
{
    p(puts(BBLU "Signal Handler startup" REG));
    sigset_t *set = args;
    while (1)
    {
        int r, sig;
        r = sigwait(set, &sig);
        ec_nz(r, return NULL; /*exit(EXIT_FAILURE)*/);

        switch (sig)
        {
        case SIGINT:
        case SIGQUIT:
            puts(BBLU "SIGINT or SIGQUIT RECEIVED" REG);
            myShutdown = HARSH_QUIT;
            close(SIGNAL_WRITE);
            return NULL;
        case SIGHUP:
            puts(BBLU "SIGHUP RECEIVED" REG);
            myShutdown = HUP_QUIT;
            close(SIGNAL_WRITE);
            return NULL;
        case SIGUSR1:
            puts(BBLU "STATS REQUEST RECEIVED" REG);
            write(SIGNAL_WRITE, &sig, sizeof(int));
            break; // back in the loop
        default:;
        }
    }
    return NULL;
}

#define WK_DIE_ON_ERR                                               \
    perror(BRED "WORKER DIED" REG);                                 \
    LoggerLog(deathMSG, strlen(deathMSG));                          \
    ec_nz(pthread_kill(sigHandlerRef, SIGINT), exit(EXIT_FAILURE)); \
    goto worker_cleanup;

/**
 * Note:
 * When we accept a request from an fd (in the thread dispatcher) we remove it 
 * from the select's set, so a worker is the only one writing on a given fd. 
 */
void *worker(void *args)
{

    size_t myTid = ((workerArgs *)args)->tid;
    pthread_t sigHandlerRef = *(((workerArgs *)args)->sigHandler);
    p(printf(BGRN "Worker %ld startup\n" REG, myTid));

    Client *requestor = NULL;
    int fd, res = 0, errnosave = 0;
    char
        toLog[REQstr_LEN + ERRNOBUF_LEN],
        errnoBuf[ERRNOBUF_LEN],
        deathMSG[14 + sizeof(size_t)],
        *reqStr = NULL;
    Request *req = NULL;
    evictedFile *evicted = NULL;
    queue
        *evictedList = NULL;
    size_t nEvicted = 0;
    _Bool hasExited = 0;

    snprintf(deathMSG, 14 + sizeof(size_t), "WORKER DIED: %ld", myTid); // will be used in WK_DIE_ON_ERR

    while (myShutdown != HARSH_QUIT) // Slightly unnecessary, main() will send termination requests
    {
        hasExited = 0;
        // GET A REQUEST
        ec_nz(pthread_mutex_lock(&lockReq), {});

        while (queueIsEmpty(requests))
            ec_nz(pthread_cond_wait(&condReq, &lockReq), {}); // wait for requests
        requestor = queueDequeue(requests);
        ec_nz(pthread_mutex_unlock(&lockReq), {});

        if (!requestor) // TERMINATION REQUEST SENT BY main()
            goto worker_cleanup;
        if (requestor->fd == -1) // STORESTATS REQUEST
        {
            free(requestor);
            storeStats();
            continue;
        }

        // We have a request or a disconnection from a client
        fd = requestor->fd;
        req = getRequest(fd);

        if (!req && (errno == ENOTCONN || errno == ECONNRESET)) // client closed the socket
        {
            ec_neg1(manageExit(fd), WK_DIE_ON_ERR);
            continue;
        }
        else if (!req) // Something bad happened
        {
            WK_DIE_ON_ERR;
        }

        p(puts(BGRN "Request accepted" REG));
        ec_z(reqStr = reqToString(req, fd), WK_DIE_ON_ERR);
        p(puts(reqStr));
        switch (req->op)
        {
        case OPEN_FILE:
            log(openFile(req->path, req->o_creat, req->o_lock, req->client, &evicted), WK_DIE_ON_ERR);
            break;

        case READ_FILE:
            log(readFile(req->path, &evicted, req->client, 0), WK_DIE_ON_ERR);
            break;

        case READN_FILES:
            log(readNfiles(req->nfiles, &evictedList, req->client), WK_DIE_ON_ERR);
            break;

        case APPEND:
            log(appendToFile(req->path, req->append, req->appendLen, req->client, &evictedList, 0), WK_DIE_ON_ERR);
            break;

        case WRITE_FILE:
            log(appendToFile(req->path, req->append, req->appendLen, req->client, &evictedList, 1), WK_DIE_ON_ERR);
            break;

        case LOCK_FILE:
            log(lockFile(req->path, req->client), WK_DIE_ON_ERR);
            if (errnosave == EACCES) // in this case, the client has to wait
            {
                freeRequest(req);
                free(reqStr);
                reqStr = NULL;
                req = NULL;
                continue; // Worker can handle new requests in the meantime
            }
            break;

        case UNLOCK_FILE:
            log(unlockFile(req->path, req->client), WK_DIE_ON_ERR);
            if (res > 1) // notify new lock owner who didn't receive feedback in the first place
            {
                errnosave = 0;
                ec_neg1_SKT(writen(res, &errnosave, sizeof(int)), WK_DIE_ON_ERR);
                // put res(fd) back in select set if hasn't exited
                if (!hasExited)
                    ec_neg1(writen(WORKER_WRITE, &res, sizeof(int)), WK_DIE_ON_ERR);
                hasExited = 0;
            }
            break;

        case CLOSE_FILE:
            log(closeFile(req->path, req->client), WK_DIE_ON_ERR);
            break;

        case REMOVE_FILE:
            log(removeFile(req->path, req->client, &evicted), WK_DIE_ON_ERR);
            break;

        default:
            break;
        }
        // Internal error (res == -1) is handled in 'log' expansion,
        // from now on we can assume res != -1

        //SEND RESPONSE TO CLIENT
        if (req->op != READN_FILES) // READN_FILES => res == #read, we'll write it later
        {
            // reset errnosave in case of success
            errnosave = res == 0 ? res : errnosave;
            ec_neg1_SKT(writen(fd, &errnosave, sizeof(int)), WK_DIE_ON_ERR);
        }

        if ((!evicted && !evictedList) &&
            (req->op == OPEN_FILE ||
             req->op == APPEND ||
             req->op == WRITE_FILE))
        {
            // in this case, we have to notify the client he has not to expect
            // any evicted files
            nEvicted = 0;
            ec_neg1_SKT(writen(fd, &nEvicted, sizeof(size_t)), WK_DIE_ON_ERR);
        }
        if (evictedList) // Send back to client and notify pending lockers
        {
            // Can be capacityVictims or readNfiles target
            ec_neg1_SKT(writen(fd, &(evictedList->size), sizeof(size_t)), WK_DIE_ON_ERR);
            evictedFile *curr = queueDequeue(evictedList);
            while (curr)
            {
                if (!hasExited)
                {
                    ec_neg1(sendFileToClient(curr, fd, &hasExited), WK_DIE_ON_ERR);
                    if (!hasExited)
                        logEvicted(req->op != READN_FILES, curr, WK_DIE_ON_ERR);
                }
                if (req->op != READN_FILES) // if it is a victim
                {
                    ec_neg1(notifyPendingLockers(curr->notifyLockers, ENOENT), WK_DIE_ON_ERR);
                    curr->notifyLockers = NULL;
                }
                freeEvicted(curr);
                curr = queueDequeue(evictedList);
            }
            queueDestroy(evictedList);
        }
        if (evicted)
        {
            //Can be openVictim or readFile target or remove target
            if (!hasExited)
            {
                ec_neg1(sendFileToClient(evicted, fd, &hasExited), WK_DIE_ON_ERR);
                if (!hasExited)
                    logEvicted(req->op != READ_FILE, evicted, WK_DIE_ON_ERR);
            }
            if (req->op != READ_FILE) // if it is a victim or remove target
            {
                ec_neg1(notifyPendingLockers(evicted->notifyLockers, ENOENT), WK_DIE_ON_ERR);
                evicted->notifyLockers = NULL;
            }
            freeEvicted(evicted);
        }

        evicted = NULL;
        evictedList = NULL;

        // We need to put the ready fd back in the select's set
        if (!hasExited)
            ec_neg1(writen(WORKER_WRITE, &fd, sizeof(int)), WK_DIE_ON_ERR);
        freeRequest(req);
        free(reqStr);
        reqStr = NULL;
        req = NULL;
    }

worker_cleanup:
    if (req)
        freeRequest(req);
    free(reqStr);
    queueDestroy(evictedList);
    freeEvicted(evicted);
    evicted = NULL;
    evictedList = NULL;
    return NULL;
}

/**
 * Sends and evicted file to a client. Doens't notify the (pending) lockers
 */
int sendFileToClient(evictedFile *fptr, int fd, _Bool *W_hasExited)
{
    char *buf = NULL;
    int index = 0;
    _Bool hasExited = *W_hasExited;
    size_t len = sizeof(size_t) + strnlen(fptr->path, PATH_MAX) + sizeof(size_t) + fptr->size;
    ec_z(buf = malloc(len * sizeof(char)), return -1);
    // We'll write {pathLen,path,size,content}
    size_t pathLen = strnlen(fptr->path, PATH_MAX);
    memcpy(buf, &pathLen, sizeof(size_t));
    index += sizeof(size_t);
    memcpy(buf + index, fptr->path, pathLen);
    index += pathLen;
    memcpy(buf + index, &(fptr->size), sizeof(size_t));
    index += sizeof(size_t);
    memcpy(buf + index, fptr->content, fptr->size);

    // Write to the client
    ec_neg1_SKT(writen(fd, buf, len), free(buf); return -1;);
    *W_hasExited = hasExited;
    free(buf);
    return 0; // success
}

/**
 * Writes 'msg' to the pending lockers' fd. \n \n
 * Destroys 'lockers'
 * @param msg one the response codes
 * @returns 0 success, -1 error (errno set)
 */
int notifyPendingLockers(queue *lockers, int msg)
{
    errno = 0;
    Client *curr = queueDequeue(lockers);
    int fd;
    _Bool hasExited = 0;
    while (!errno && curr)
    {
        hasExited = 0;
        fd = curr->fd;
        ec_neg1_SKT(writen(fd, &msg, sizeof(int)), return -1;);
        if (!hasExited)
            ec_n(writen(WORKER_WRITE, &(fd), sizeof(int)), sizeof(int), return -1;);
        curr = queueDequeue(lockers);
    }
    queueDestroy(lockers);
    return 0; // success
}

/**
 * Reads "workers" "nfiles" "capacity" "sockname" "evictPolicy" from a config file
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
        new->evictPolicy = 1; // LRU

    fclose(conf);

    return 0;
}

/**
 * Put here and not in conn.h to avoid circular dependencies with filesys.
 * Note that doesn't close(fd), it will be done later by dispatcher
 */
int removeClient(int fd, queue **notifyLockers)
{
    // close connection
    char fdBuf[INT_LEN];
    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), return -1);
    // remove from storage
    Client *toRemove = icl_hash_find(clients, fdBuf);
    ec_z(toRemove, return -1);
    ec_z(*notifyLockers = storeRemoveClient(toRemove), return -1);
    ec_neg1(icl_hash_delete(clients, fdBuf, free, free), return -1);
    return 0;
}

int manageExit(int fd)
{
    queue *notifyLockers = NULL;
    ec_nz(LOCKCLIENTS, return -1);
    ec_neg1(removeClient(fd, &notifyLockers), UNLOCKCLIENTS; return -1);
    ec_nz(UNLOCKCLIENTS, return -1);
    // if there was at least one pending locker on a file O_LOCKed by
    // the removed client, we must notify it of the successful lockFile()
    ec_neg1(notifyPendingLockers(notifyLockers, SUCCESS), queueDestroy(notifyLockers); return -1);
    notifyLockers = NULL;

    // if there are no more clients and we are HUP-exiting,
    // dispatcher will manage exit (and log client's disconnection)
    fd *= -1;
    ec_neg1(writen(WORKER_WRITE, &fd, sizeof(int)), return -1);
    return 0;
}

int statsToLog(size_t maxNclients)
{
    char toLog[500];
    snprintf(toLog, 1000, "STATS: %ld %.2lf %ld %ld",
             store.maxNfilesReached,
             (double)store.maxSizeReached / (double)1000000,
             store.nEviction,
             maxNclients);
    return LoggerLog(toLog, strnlen(toLog, 500));
}