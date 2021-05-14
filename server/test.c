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

int compare (void *a, void *b) {
    if (strcmp(((nodo*)a)->buf,((nodo*)b)->buf)) return 0;
    return 1;
}

// READ CONFIG FILE
#define LOGTEST 
#define LOG_TEST 16384

int main(void)
{
    int error = 0;
    queue *coda = queueCreate( libera, compare, &error);
    nodo *x, *j, *z;
    x = malloc(sizeof(nodo));
    j = malloc(sizeof(nodo));

    x->buf = calloc(1000,sizeof(char));
    x->paolo = calloc(10000,sizeof(char));
    j->buf = calloc(200,sizeof(char));
    j->paolo = calloc(30,sizeof(char));
    strcpy(x->buf, "pippo");
    strcpy(x->paolo, "pluto");
    strcpy(j->buf, "buf");

    printf("x->buf = %s\n",x->buf);

    queueEnqueue(coda, x, &error);
    queueEnqueue(coda, j, &error);

    nodo *tmp = NULL;
    tmp = queuePeek(coda, &error);
    if (tmp) printf("tmp1=%s\n",tmp->buf);


    z = malloc (sizeof(nodo));
    z->buf = malloc(200);
    z->paolo = malloc(30);

    strcpy(z->buf, "pippo");
    // printf("%s-%s\n",z->buf,x->buf);
    tmp = queueFind(coda,z, NULL, &error);
    if (tmp) puts("'pippo' Present");
    else puts ("'pippo' !present");

    tmp = NULL;

    tmp = queueRemove(coda,z, NULL, &error);
    if (tmp) libera(tmp);

    tmp = queuePeek(coda, &error);
    if (tmp) {
        printf("tmp2=%s\n",tmp->buf);}


    
    
    tmp = NULL;
    strcpy(z->buf, "pluto");
    tmp = queueFind(coda,z, NULL, &error);
    if (tmp) printf("tmp3=%s\n",tmp->buf);
    if (tmp) puts("'pluto' Present");
    else puts ("'pluto' !present");



    libera(z);

    // queueEnqueue(coda,&x);
    // printf("%d\n",isEmpty(coda));
    // tmp = queueDequeue(coda, &error);
    // libera(tmp);
    // tmp = queueDequeue(coda, &error);
    // libera(tmp);
    // printf("%d : %s\n",isEmpty(coda), j.buf);
    queueDestroy(coda, &error);

    queueDequeue(NULL,&error);
    printf("erro = %d\n",error);
    // TEST SERVER CONFIG
    ServerData s = readConfig("config.txt");

    printf("%ld %ld %ld %s\n", s.workers, s.nfiles, s.capacity, s.sockname);
    free(s.sockname);

    // TEST LOG
    #ifdef LOGTEST
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
    #endif
    return 0;
}