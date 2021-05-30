#include <server.h>

// Client -> Dispatcher -> Manager -> FIFO request -> Workers (n)
//                              <-   pipe   <-
//

// Comunicazione Worker -> Manager
// La pipe va collegata alla select
#define MANAGER_READ mwPipe[0]
#define WORKER_WRITE mwPipe[1]
pthread_mutex_t lockPipe = PTHREAD_MUTEX_INITIALIZER;
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

int myShutdown = 0;

#define HARSH_QUIT 2

void *dispatcher(void *arg)
{
    // SERVER - GESTIONE SOCKET

    // fd_c == fd_client
    int fd_skt, fd_c, fd_hwm = 0, fd;
    char buf[BUF]; //read buffer
    char *res;     // capitalized buffer --> In realtà è superfluo (vedi implementazione di getResult)
    // --> Solo per leggibilità
    fd_set set, read_set;
    ssize_t nread;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, serverData.sockname, UNIX_PATH_MAX);
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

    // gestione timeout select per quando riceviamo un segnale
    // struct timeval timeout;
    // timeout.tv_sec = 1;
    // timeout.tv_usec = 0;

    int sel_ret;
    Request *currRequest = malloc(sizeof(Request));

    int myerr = 0;

    // pthread_mutex_lock(&mutex);
    while (1)
    {
        // pthread_mutex_unlock(&mutex);
        read_set = set;
        ec_neg1((sel_ret = select(fd_hwm + 1, &read_set, NULL, NULL, NULL /*&timeout*/)), {});
        /* if (sel_ret == 0)
        {
            continue; // skippi alla prossima iterazione del ciclo
        } */
        for (fd = 0; fd <= fd_hwm /*&& !sig_flag*/; fd++)
        {
            // Socket comunicazione client
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == fd_skt && myShutdown == 0)
                {
                    ec_neg1(fd_c = accept(fd_skt, NULL, 0), {});
                    // activeConnections++;
                    FD_SET(fd_c, &set);
                    if (fd_c > fd_hwm)
                        fd_hwm = fd_c;
                }
                else if (fd == MANAGER_READ && myShutdown != HARSH_QUIT)
                {
                    // Worker comunica quali fd dei client sono ancora attivi
                    // e dunque devono essere reinseriti nel set

                    // Se ci fossero più di 10 fd da leggere non è un problema,
                    // Rimarranno nella pipe e verranno letti alla prossima iterazione

                    int i, *msgFromWorker = calloc(10, sizeof(int));
                    read(MANAGER_READ, msgFromWorker, 10 * sizeof(int));
                    while (msgFromWorker[i] != 0)
                    {
                        FD_SET(msgFromWorker[i], &set);
                        if (msgFromWorker[i] > fd_hwm)
                            fd_hwm = msgFromWorker[i];
                        i++;
                    }
                }
                else if (fd == SIGNAL_READ)
                {
                    puts("SIGNAL RECEIVED");
                    if (myShutdown == HARSH_QUIT)
                        break;
                }
                else
                {
                    // invece di leggere, passo ai worker il fd
                    FD_CLR(fd, &set);
                    if (fd == fd_hwm)
                        fd_hwm--;

                    ec_nz(pthread_mutex_lock(&lockReq), {});
                    // TODO queueEnqueue(requests, fd, &myerr);
                    ec_nz(pthread_cond_signal(&condReq), {});
                    ec_nz(pthread_mutex_unlock(&lockReq), {});
                }
            }
        }
    }
    // pthread_mutex_unlock(&mutex);
}

void *signalHandler(void *args)
{
    //...
}

void *worker(void *args)
{
    int fd, myerr = 0;
    while (!myShutdown)
    {
        ec_nz(pthread_mutex_lock(&lockReq), {});

        ec_nz(pthread_cond_wait(&condReq, &lockReq), {}); // wait for requests
        // TODO fd = queueDequeue(requests, &myerr);
        ec_nz(pthread_mutex_unlock(&lockReq), {});

        // Parsing Nightmare

        // Serve request

        ec_nz(pthread_mutex_lock(&lockPipe),{});
        // write(WORKER_WRITE,fd,sizeof(int)); //pipe Worker -> Manager
        ec_nz(pthread_mutex_unlock(&lockPipe),{});
    }
}

// leggi configurazione server
ServerData readConfig(char *configPath)
{
    ServerData new;
    // INIT STATS

    FILE *conf;
    ec(conf = fopen(configPath, "r"), NULL, {});

    conf_sizet(conf, "workers", &new.workers);
    fseek(conf, 0, SEEK_SET);
    conf_sizet(conf, "nfiles", &new.nfiles);
    fseek(conf, 0, SEEK_SET);
    conf_sizet(conf, "capacity", &new.capacity);
    fseek(conf, 0, SEEK_SET);
    new.sockname = conf_string(conf, "sockname");

    /* code */

    fclose(conf);

    return new;
}
