CC		=  gcc
CFLAGS	        = -g -std=c99
# gli includes sono nella directory include
INCDIR          = ./include
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
TARGETS	= 	bin/filesys \
			bin/test
LIBS = -L ./lib -lqueue -lutils -lserver -llogger -lm -lfilesys -licl_hash -lpthread #-lm è per math.h
LIBS_SERVER = -L ./lib -lqueue -lutils -llogger -lfilesys -licl_hash -lpthread
LIBS_SERVER_SO = lib/libqueue.so lib/libutils.so lib/liblogger.so lib/libfilesys.so lib/libicl_hash.so
.PHONY: clean
.SUFFIXES: .c .h

bin/test:	test.c lib/libserver.so
	#per logging
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

bin/filesys: test/test_filesys.c lib/libfilesys.so lib/libserver.so
	#filesys
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

bin/mt_filesys: test/mt_filesys.c lib/libfilesys.so lib/libserver.so
	#filesys multithread
	rm -f log.txt
	$(CC) $(CFLAGS) -fno-stack-protector $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

lib/%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC -o $@ $<


lib/libqueue.so: lib/queue.o $(INCDIR)/queue.h
	$(CC) -shared -o $@ $<

lib/libutils.so: lib/utils.o $(INCDIR)/utils.h
	$(CC) -shared -o $@ $<

lib/liblogger.so: lib/logger.o $(INCDIR)/logger.h
	$(CC) -shared -o $@ $<

lib/libfilesys.so: lib/filesys.o lib/libqueue.so lib/libicl_hash.so $(INCDIR)/filesys.h 
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' -L ./lib -lqueue -licl_hash

lib/libicl_hash.so: lib/icl_hash.o $(INCDIR)/icl_hash.h
	$(CC) -shared -o $@ $<


lib/libserver.so: lib/server.o $(INCDIR)/server.h $(LIBS_SERVER_SO)
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS_SERVER)

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	rm -f $(TARGETS) lib/*