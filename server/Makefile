CC		=  gcc
CFLAGS	        = -g 
# gli includes sono nella directory include
INCDIR          = ./include
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
TARGETS	= bin/test
LIBS = -L ./lib -lqueue -lutils -lserver -llogger -lm #-lm è per math.h
LIBS_SERVER = -L ./lib -lqueue -lutils -llogger
LIBS_SERVER_SO = lib/libqueue.so lib/libutils.so lib/liblogger.so
.PHONY: clean
.SUFFIXES: .c .h

bin/test:	test.c lib/libserver.so
	#per logging
	rm -f log.txt
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS)

lib/%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -fPIC -o $@ $<


lib/libqueue.so: lib/queue.o $(INCDIR)/queue.h
	$(CC) -shared -o $@ $<

lib/libutils.so: lib/utils.o $(INCDIR)/utils.h
	$(CC) -shared -o $@ $<

lib/liblogger.so: lib/logger.o $(INCDIR)/logger.h
	$(CC) -shared -o $@ $<

lib/libserver.so: lib/server.o $(INCDIR)/server.h $(LIBS_SERVER_SO)
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../lib' $(LIBS_SERVER)

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	rm -f $(TARGETS) lib/*