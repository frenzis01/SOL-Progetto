#include <clientApi.h>
#include <protocol.h>


// Simple test used during clientApi development

#define SOCKNAME "sock"

int main () {
    struct timespec abstime;
    abstime.tv_sec = time(NULL) + 200;
    openConnection(SOCKNAME, 1000, abstime);
    openFile("/testdir/fileTest1",3);
    openFile("/testdir/fileTest2",3);
    openFile("/testdir/fileTest3",3);
    char buf[] = "Content test";
    appendToFile("/testdir/fileTest1",buf,strlen(buf), NULL);
    appendToFile("/testdir/fileTest2",buf,strlen(buf), NULL);
    appendToFile("/testdir/fileTest2",buf,strlen(buf), NULL);
    lockFile("/testdir/fileTest1");
    void *tmp = NULL;
    size_t size;
    readFile("/testdir/fileTest1",&tmp,&size);
    readNFiles(-4,"evicted/dir1");
    unlockFile("/testdir/fileTest1");
    lockFile("/testdir/fileTest1");
    readNFiles(-4,"evicted/dir2");
    removeFile("/testdir/fileTest1");

    free(tmp);
    sleep(2);
    closeConnection(SOCKNAME);
    return 0;
}