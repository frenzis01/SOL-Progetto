#include <logger.h>

// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// Logger *logger = NULL; // singleton

#define errnoCHK                                                       \
    if (errno)                                                         \
    {                                                                  \
        ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; }); \
        *error = myerr;                                                \
        return;                                                        \
    }

void LoggerCreate(int *error, char *path)
{
    int myerr = 0;
    ec_nz(pthread_mutex_lock(&mutex), { myerr = E_LOG_MUTEX; });
    // se il logger esiste già, fine
    if (logger)
    {
        ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; });
        return;
    }

    // else inizializziamo
    mye_z(logger = malloc(sizeof(Logger)), E_LOG_MALLOC);
    if (myerr)
    {
        *error = myerr;
        return;
    }

    mye_z(logger->buf = malloc(MAX_LOG_SIZE * sizeof(char)), E_LOG_MALLOC);
    // mye_n(logger->buf = calloc(MAX_LOG_SIZE, sizeof(char)), E_LOG_MALLOC);
    if (myerr)
    {
        *error = myerr;
        free(logger);
        return;
    }

    logger->bufLen = 0;
    (logger->buf)[0] = '\0'; // might be useful later

    mye_z(logger->file = fopen(path, "a"), E_LOG_FILE);
    if (myerr)
    {
        *error = myerr;
        free(logger->buf);
        free(logger);
        return;
    }

    ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; });
    return;
}

void LoggerDelete(int *error)
{
    int myerr = 0;
    ec_nz(pthread_mutex_lock(&mutex), { myerr = E_LOG_MUTEX; });
    if (!myerr)
    {
        ec_nz(fclose(logger->file), { myerr = E_LOG_FILE; }); // e se fallisse?
    }
    if (!myerr)
    {
        free(logger->buf);
        free(logger);
        logger = NULL;
        ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; });
    }
    *error = myerr;
    return;
}
void LoggerLog(char *buf, size_t len, int *error)
{
    errno = 0;
    time_t ltime; /* calendar time */
    int myerr = 0;
    char *asctime_buf = malloc(26);
    if (errno)
    {
        myerr = E_LOG_MALLOC;
        return;
    };
    struct tm *lcltime_res = malloc(sizeof(struct tm));
    if (errno)
    {
        free(asctime_buf);
        myerr = E_LOG_MALLOC;
        return;
    };

    ec_nz(pthread_mutex_lock(&mutex), { myerr = E_LOG_MUTEX; });
    if (!myerr)
    {
        // Aggiungiamo il timestamp e poi il buf
        ltime = time(NULL); /* get current cal time */
        errnoCHK;
        localtime_r(&ltime, lcltime_res);
        errnoCHK;
        char *ts = asctime_r(lcltime_res, asctime_buf);
        errnoCHK;
        size_t tsLen = strnlen(ts, INT_MAX);
        // strlen(ts) = 25, ma forse su alcune macchine è diverso
        ts[tsLen] = '\0';

        if (len + (logger->bufLen) + tsLen >= MAX_LOG_SIZE)
            LoggerFlush(&myerr);

        strncat(logger->buf, ts, tsLen + 1); // timestamp
        logger->bufLen += tsLen;
        (logger->buf)[logger->bufLen - 1] = '-'; // \space instead of \n
        (logger->buf)[logger->bufLen] = '\0';    // dovrebbe essere stato messo dalla strcat

        strncat((logger->buf), buf, len + 1); // actual logger message
        logger->bufLen += len;
        (logger->buf)[logger->bufLen] = '\n'; // add \n and termination
        (logger->buf)[++logger->bufLen] = '\0';

        // ts non va deallocato! "free(ts)" esplode
    }
    ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; });
    free(asctime_buf);
    free(lcltime_res);

    *error = myerr;
    return;
} // returns error

void LoggerFlush(int *error)
{
    int myerr = 0;
    ec_nz(pthread_mutex_lock(&mutex), { myerr = E_LOG_MUTEX; });
    if (!myerr)
    {
        fprintf(logger->file, "%s", logger->buf);
        *(logger->buf) = '\0';
        logger->bufLen = 0; // il '\0' non va contato nella lunghezza (?)

        ec_nz(pthread_mutex_unlock(&mutex), { myerr = E_LOG_MUTEX; });
    }
    *error = myerr;
    return;

} // returns error
