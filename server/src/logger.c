#include <logger.h>


#define ec_nz_l(s, c)    \
    if ((s)) \
    {                  \
        c;             \
        perror(#s);    \
    }

#define ec_z_l(s, c)     \
    if (!(s)) \
    {                  \
        c;             \
        perror(#s);    \
    }

#define UNLOCK pthread_mutex_unlock(&mutex)
#define LOCK pthread_mutex_lock(&mutex)

int SimpleFlush();

int LoggerCreate(char *path)
{
    ec_z_l(path, errno = EINVAL; return -1 );

    ec_nz_l(LOCK, return -1 );
    if (logger)
    {
        UNLOCK;
        return 0;
    }

    // else inizializziamo
    ec_z_l(logger = malloc(sizeof(Logger)), UNLOCK; return -1;);

    ec_z_l(logger->buf = malloc(MAX_LOG_SIZE * sizeof(char)), free(logger); UNLOCK; return -1;);

    logger->bufLen = 0;
    (logger->buf)[0] = '\0'; // might be useful later

    ec_z_l(logger->file = fopen(path, "a"),
         free(logger->buf);
         free(logger);
         UNLOCK;
         return -1;);

    ec_nz_l(UNLOCK, return -1);
    return 0;
}

int LoggerDelete()
{
    ec_nz_l(LOCK, return -1);

    if (SimpleFlush() == -1) return -1;

    ec_nz_l(fclose(logger->file), return -1);
    free(logger->buf);
    free(logger);
    logger = NULL;

    ec_nz_l(UNLOCK, return -1);
    return 0;
}

#define log_cleanup do{perror("logger") ; UNLOCK; free(asctime_buf); free(lcltime_res); return -1;}while(0);
int LoggerLog(char *buf, size_t len)
{
    time_t ltime; /* calendar time */
    char *asctime_buf = NULL, *ts = NULL;
    struct tm *lcltime_res = NULL;

    ec_z_l(asctime_buf = malloc(26), return -1 );
    ec_z_l(lcltime_res = malloc(sizeof(struct tm)), free(asctime_buf); return -1;);
    ec_nz_l(LOCK, free(asctime_buf); free(lcltime_res); return -1;);

    // Add timestamp then buf
    ltime = time(NULL); // get current time
    if (ltime == (time_t)-1)
        log_cleanup;
    if (!localtime_r(&ltime, lcltime_res))
        log_cleanup;
    if ((ts = asctime_r(lcltime_res, asctime_buf)) == NULL)
        log_cleanup;
    // ts must not be freed

    size_t tsLen = strnlen(ts, INT_MAX);
    // strlen(ts) = 25
    ts[tsLen] = '\0';

    if (len + (logger->bufLen) + tsLen >= MAX_LOG_SIZE - 1){
        if (SimpleFlush() == -1) log_cleanup;
    }

    strncat(logger->buf, ts, tsLen + 1); // timestamp
    logger->bufLen += tsLen;
    (logger->buf)[logger->bufLen - 1] = '-'; // '-' instead of \n
    (logger->buf)[logger->bufLen] = '\0';

    strncat((logger->buf), buf, len + 1); // actual logger message
    logger->bufLen += len;
    (logger->buf)[logger->bufLen] = '\n'; // add \n and termination
    (logger->buf)[++logger->bufLen] = '\0';

    free(asctime_buf);
    free(lcltime_res);
    ec_nz_l(UNLOCK, return -1);

    return 0;
}

int LoggerFlush()
{
    ec_nz_l(LOCK, return -1;);

    if (fprintf(logger->file, "%s", logger->buf) < 0) return -1;
    *(logger->buf) = '\0';
    logger->bufLen = 0;

    ec_nz_l(UNLOCK, return -1);
    return 0;
}

/**
 * Assumes that the caller is the mutex owner
 * Used in LoggerLog to avoid deadlock 
 */
int SimpleFlush()
{
    if (fprintf(logger->file, "%s", logger->buf) < 0) return -1;
    *(logger->buf) = '\0';
    logger->bufLen = 0;
    return 0;
}
