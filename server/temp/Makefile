CC		=  gcc
CCFLAGS	        += -g
TARGETS	= readFile
.PHONY: clean
.SUFFIXES: .c .h

%:	%.c
	$(CC) $(CCFLAGS) $< -o $@
	cp test.b test
	@valgrind ./$@
	cp test.b test
	@#ls -la $?

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
