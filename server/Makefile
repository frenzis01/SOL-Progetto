CC		=  gcc
CFLAGS	        = -g -std=c99 -Wall -pedantic -Wno-unknown-pragmas
# gli includes sono nella directory include
INCDIR          = ./include
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
TARGETS	= 	bin/server

LIBS = -L ./lib -lqueue -lutils -lserver -llogger -lm -lfilesys -licl_hash -lconn -lpthread #-lm è per math.h
LIBS_SERVER = -L ./lib -lqueue -lutils -llogger -lfilesys -licl_hash -lconn -lpthread
LIBS_SERVER_SO = lib/libqueue.so lib/libutils.so lib/liblogger.so lib/libfilesys.so lib/libicl_hash.so lib/libconn.so
.PHONY: clean
.SUFFIXES: .c .h

#tests
test1: test/test1.sh bin/client bin/server
	rm -r -f test/read/* test/evicted/* sock
	test/test1.sh

test2: test/test2.sh bin/client bin/server
	rm -r -f test/read/* test/evicted/* sock
	test/test2.sh

test3: test/test3.sh bin/client bin/server
	rm -r -f test/read/* test/evicted/* sock
	test/test3.sh

bin/server: src/server.c lib/libserver.so $(INCDIR)/server.h
	rm -f sock
	rm -f log.txt
	$(CC) $(CFLAGS) -fno-stack-protector $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

bin/client: src/client.c lib/libclientApi.so lib/libclientUtils.so lib/libutils.so lib/libparser.so lib/libqueue.so $(INCDIR)/client.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lclientApi -lutils -lclientUtils -lparser -lqueue

#My own tests
bin/filesys: mytests/filesys.c lib/libfilesys.so lib/libqueue.so lib/libconn.so lib/liblogger.so lib/libicl_hash.so
	#filesys
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lfilesys -lqueue -lconn -llogger -lpthread -licl_hash

bin/mt_filesys: mytests/mt_filesys.c lib/libfilesys.so lib/libqueue.so lib/libconn.so lib/liblogger.so
	#filesys multithread
	rm -f log.txt
	$(CC) $(CFLAGS) -fno-stack-protector $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lfilesys -lqueue -lconn -llogger -licl_hash -lpthread

bin/parser:	mytests/parser.c lib/libparser.so lib/libqueue.so 
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lqueue -lparser -lutils

bin/clientApi: mytests/clientApi.c lib/libclientApi.so $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lclientApi -lutils -lclientUtils
	rm -r evicted/*

#shared libs
lib/libqueue.so: lib/queue.o $(INCDIR)/queue.h
	$(CC) -shared -o $@ $<

lib/libutils.so: lib/utils.o $(INCDIR)/utils.h
	$(CC) -shared -o $@ $<

lib/libicl_hash.so: lib/icl_hash.o $(INCDIR)/icl_hash.h
	$(CC) -shared -o $@ $<

#server libs
lib/liblogger.so: lib/logger.o lib/libutils.so $(INCDIR)/logger.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lutils -lpthread

lib/libfilesys.so: lib/filesys.o lib/libqueue.so lib/libicl_hash.so lib/libutils.so $(INCDIR)/filesys.h $(INCDIR)/conn.h 
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lqueue -licl_hash -lutils -lpthread

lib/libconn.so: lib/conn.o lib/libicl_hash.so lib/libutils.so $(INCDIR)/conn.h $(INCDIR)/protocol.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lutils -licl_hash

lib/libserver.so: lib/server.o $(INCDIR)/server.h $(LIBS_SERVER_SO)
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS_SERVER)

#client libs
lib/libparser.so: lib/parser.o lib/libqueue.so lib/libutils.so $(INCDIR)/parser.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lqueue -lutils

lib/libclientUtils.so: lib/clientUtils.o lib/libutils.so $(INCDIR)/clientUtils.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lutils

lib/libclientApi.so: lib/clientApi.o lib/libutils.so lib/libclientUtils.so $(INCDIR)/clientApi.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lutils -lclientUtils


#generic stuff
lib/%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC -o $@ $<

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

cleanall:
	rm -f $(TARGETS) lib/* bin/* log.txt
	rm -rf test/evicted/* test/read/*