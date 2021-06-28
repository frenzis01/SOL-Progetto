#include <logger.h>

#define ec_z_se(s, e, c)   \
    if (!errno && !(s)) \
    {                   \
        errno = e;      \
        c;              \
        perror(#s);     \
    }

#define eo(c)                 \
    if (errno != 0)                \
    {                         \
        c;                    \
        perror("Some error"); \
    }

#define ec_nz_l(s, c)    \
    if (!errno && (s)) \
    {                  \
        c;             \
        perror(#s);    \
    }

#define ec_z_l(s, c)     \
    if (!errno && !(s)) \
    {                  \
        c;             \
        perror(#s);    \
    }

#define UNLOCK pthread_mutex_unlock(&mutex)
#define LOCK pthread_mutex_lock(&mutex)

void SimpleFlush();

void LoggerCreate(char *path)
{
    errno = 0;
    ec_z_se(path, EINVAL, return );

    ec_nz_l(LOCK, return );
    // se il logger esiste già, fine
    if (logger)
    {
        UNLOCK;
        return;
    }

    // else inizializziamo
    ec_z_l(logger = malloc(sizeof(Logger)), UNLOCK; return;);

    ec_z_l(logger->buf = malloc(MAX_LOG_SIZE * sizeof(char)), free(logger); UNLOCK; return;);

    logger->bufLen = 0;
    (logger->buf)[0] = '\0'; // might be useful later

    ec_z_l(logger->file = fopen(path, "a"),
         free(logger->buf);
         free(logger);
         UNLOCK;
         return;);

    UNLOCK;
    return;
}

void LoggerDelete()
{
    errno = 0;
    ec_nz_l(LOCK, return );

    ec_nz_l(fclose(logger->file), return );
    free(logger->buf);
    free(logger);
    logger = NULL;

    UNLOCK;
    return;
}

#define errno_EXIT eo(UNLOCK; free(asctime_buf); free(lcltime_res); return;);
void LoggerLog(char *buf, size_t len)
{
    errno = 0;
    time_t ltime; /* calendar time */
    char *asctime_buf, *ts = NULL;
    struct tm *lcltime_res;

    ec_z_l(asctime_buf = malloc(26), return );
    ec_z_l(lcltime_res = malloc(sizeof(struct tm)), free(asctime_buf); return;);
    ec_nz_l(LOCK, free(asctime_buf); free(lcltime_res); return;);

    // Aggiungiamo il timestamp e poi il buf
    ltime = time(NULL); /* get current cal time */
    errno_EXIT;
    localtime_r(&ltime, lcltime_res);
    errno_EXIT;
    ts = asctime_r(lcltime_res, asctime_buf);
    errno_EXIT;
    // ts non va deallocato! "free(ts)" esplode

    size_t tsLen = strnlen(ts, INT_MAX);
    // strlen(ts) = 25, ma forse su alcune macchine è diverso
    ts[tsLen] = '\0';

    if (len + (logger->bufLen) + tsLen >= MAX_LOG_SIZE - 1)
        SimpleFlush();

    errno_EXIT; // LoggerFlush might fail
    strncat(logger->buf, ts, tsLen + 1); // timestamp
    logger->bufLen += tsLen;
    (logger->buf)[logger->bufLen - 1] = '-'; // '-' instead of \n
    (logger->buf)[logger->bufLen] = '\0';    // dovrebbe essere stato messo dalla strcat

    strncat((logger->buf), buf, len + 1); // actual logger message
    logger->bufLen += len;
    (logger->buf)[logger->bufLen] = '\n'; // add \n and termination
    (logger->buf)[++logger->bufLen] = '\0';

    free(asctime_buf);
    free(lcltime_res);
    UNLOCK;

    return;
} // returns error

void LoggerFlush()
{
    errno = 0;
    ec_nz_l(LOCK, return;);

    fprintf(logger->file, "%s", logger->buf);
    *(logger->buf) = '\0';
    logger->bufLen = 0; // il '\0' non va contato nella lunghezza (?)

    UNLOCK;
    return;

}

/**
 * Assumes that the caller is the mutex owner
 * Used in LoggerLog to avoid deadlock 
 */
void SimpleFlush()
{
    errno = 0;

    fprintf(logger->file, "%s", logger->buf);
    *(logger->buf) = '\0';
    logger->bufLen = 0;
    return;

}
