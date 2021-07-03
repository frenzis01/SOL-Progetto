#include <clientApi.h>
#include <protocol.h>


// Simple test used during clientApi development

#define SOCKNAME "sock"

int main () {
    struct timespec abstime;
    abstime.tv_sec = time(NULL) + 200;
    openConnection(SOCKNAME, 1000, abstime);
    openFile("fileTest1",3);
    openFile("fileTest2",3);
    openFile("fileTest3",3);
    char buf[] = "Content test";
    appendToFile("fileTest1",buf,strlen(buf), NULL);
    appendToFile("fileTest2",buf,strlen(buf), NULL);
    appendToFile("fileTest2",buf,strlen(buf), NULL);
    lockFile("fileTest1");
    void *tmp = NULL;
    size_t size;
    readFile("fileTest1",&tmp,&size);
    readNFiles(-4,"unused");
    unlockFile("fileTest1");
    lockFile("fileTest1");
    readNFiles(-4,"unused");
    removeFile("fileTest1");

    free(tmp);
    sleep(2);
    closeConnection(SOCKNAME);
    return 0;
}