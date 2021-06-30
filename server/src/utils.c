#include <utils.h>

#include <stdarg.h>

/** Read "n" bytes from a descriptor 
 * @returns bytes read (n - nleft), -1 on error
*/
ssize_t 
readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount read so far */
        }
        else if (nread == 0)
        {
            puts("eof");
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    puts("fineciclo");
    return (n - nleft); /* return >= 0  (bytes read) */
}

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount written so far */
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); /* return >= 0 */
}

// Non rimuove newline \n
char *readLineFromFILEn(char *inputfer, unsigned int len, FILE *fp)
{
    char *res = fgets(inputfer, len, fp);

    if (res == NULL)
        return res;

    return inputfer;
}

// Rimuove il newline \n
char *readLineFromFILE(char *buffer, unsigned int len, FILE *fp)
{
    char *res = fgets(buffer, len, fp);

    if (res == NULL)
        return res;

    /* remove the useless newline character */
    char *newline = strchr(buffer, '\n');
    if (newline)
    {
        *newline = '\0';
    }
    return buffer;
}

// Mette il content del file in buf
// !!! if file is bigger than bufSize, it does nothing
int myRead(const char *path, char *buf, size_t bufSize)
{
    // path assoluto richiesto
    int fd, bytesRead;
    ec_neg1(fd = open(path, O_RDONLY),return -1); // e se venisse interrotta da un segnale?
    // if (fd = open(path,O_RDONLY)) return 0;
    ec_neg1(bytesRead = readn(fd, buf, bufSize),return -1);
    ec_neg1(close(fd),return -1); // e se venisse interrotta da un segnale?
    return bytesRead;
}


// READ CONFIG FILE
// TEST OK

/**
 * Finds a char* value after a "[string]="
 * @returns NULL on error, desired string on success
 */
char *conf_string(FILE *file, char const *desired_name) { 
    char name[128],
     val[128],
    *toRet = calloc(128, sizeof(char));

    while (fscanf(file, "%127[^=]=%127[^\n]%*c", name, val) == 2) {
        if (0 == strcmp(name, desired_name)) {
            // return strdup(val);
            strncpy(toRet,val,128);
            return toRet;
        }
    }
    return NULL;
}


/**
 * Finds a size_t value after a "[string]="
 * @returns -1 on error, desired string on success
 */
int conf_sizet(FILE *file, char const *desired_name, size_t *ret) {
    char *temp = conf_string(file, desired_name);
    if (!temp) return -1;

    char *stop;
    *ret = strtol(temp, &stop, 10);
    int ret_val = stop == temp;
    free(temp);
    return ret_val;
}

/**
 * Find maximum between two or more integer variables
 * @param args Total number of integers
 * @param ... List of integer variables to find maximum
 * @return Maximum among all integers passed
 */
int max(int args, ...){
    int i, max, cur;
    va_list valist;
    va_start(valist, args);

    max = INT_MIN;

    for(i=0; i<args; i++){
        cur = va_arg(valist, int); // Get next elements in the list
        if(max < cur)
            max = cur;
    }

    va_end(valist); // Clean memory assigned by valist

    return max;
}

/**
 * Checks if 's' is an integer and stores the corresponding value in 'n'
 * @param n where the result will be stored
 * @returns 1 success, 0 's' not an integer
 */
int isInteger(const char* s, int* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN){
        // overflow/underflow
        return 0;
    }

    if (errno==0 && e != NULL && e != s){
        *n = (int) val;
        // è un numero valido
        return 1;
    }

    // non è un numero
    return 0;
}