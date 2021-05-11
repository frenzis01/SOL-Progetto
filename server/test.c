#include <stdio.h>
#include <queue.h>
#include <string.h>
#include <utils.h>
#include <server.h>

#include <math.h>   // calculate size_t->char[] len

typedef struct gino
{
    char *buf;
    char *paolo;
} nodo;

void libera(void *arg)
{
    nodo *toFree = (nodo *)arg;
    free(toFree->buf);
    free(toFree->paolo);
    free(toFree);
}

// READ CONFIG FILE

#define LOG_TEST 16384

int main(void)
{
    queue *coda = createQueue(sizeof(nodo), libera);
    nodo x, j;
    x.buf = malloc(1000);
    x.paolo = malloc(10000);
    j.buf = malloc(200);
    j.paolo = malloc(30);
    strcpy(x.buf, "gino");
    enqueue(coda, &x);
    enqueue(coda, &j);

    // enqueue(coda,&x);
    // printf("%d\n",isEmpty(coda));
    nodo *tmp;
    tmp = dequeue(coda);
    libera(tmp);
    tmp = dequeue(coda);
    libera(tmp);
    // printf("%d : %s\n",isEmpty(coda), j.buf);
    destroyQueue(coda);

    // TEST SERVER CONFIG
    ServerData s = readConfig("config.txt");

    printf("%ld %ld %ld %s\n", s.workers, s.nfiles, s.capacity, s.sockname);
    free(s.sockname);

    // TEST LOG
    int myerr = 0;
    LoggerCreate(&myerr,"log.txt");
    LoggerLog("FIRST LINE ",strlen("FIRST LINE "),&myerr);
    LoggerFlush(&myerr);

    for (size_t i = 1; i < LOG_TEST; i++)
    {
        
        char buf[2000] = "Some stuff ";
        char nbuf[2000];
        sprintf(nbuf,"%ld",i);
        strcat(buf,nbuf);

        LoggerLog(buf,strlen(buf),&myerr);
    }
    LoggerFlush(&myerr);

    
    LoggerDelete(&myerr);
    return 0;
}