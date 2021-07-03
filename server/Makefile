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

bin/server: src/server.c lib/libserver.so $(INCDIR)/server.h
	rm -f sock
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

#My own tests
bin/filesys: test/filesys.c lib/libfilesys.so lib/libqueue.so lib/libconn.so lib/liblogger.so lib/libicl_hash.so
	#filesys
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lfilesys -lqueue -lconn -llogger -lpthread -licl_hash

bin/mt_filesys: test/mt_filesys.c lib/libfilesys.so lib/libqueue.so lib/libconn.so lib/liblogger.so
	#filesys multithread
	rm -f log.txt
	$(CC) $(CFLAGS) -fno-stack-protector $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lfilesys -lqueue -lconn -llogger -licl_hash -lpthread

bin/parser:	test/parser.c lib/libparser.so lib/libqueue.so
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lqueue -lparser

bin/clientApi: test/clientApi.c lib/libclientApi.so $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lclientApi -lutils

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

lib/libclientApi.so: lib/clientApi.o lib/libutils.so $(INCDIR)/clientApi.h
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lutils


#generic stuff
lib/%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC -o $@ $<

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	rm -f $(TARGETS) lib/* bin/* log.txt