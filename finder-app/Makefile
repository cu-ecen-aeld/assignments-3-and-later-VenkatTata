CC=gcc
CROSS_COMPILE=

all: writer

clean:
		-rm -f *.o writer

%.o:%.c
	$(CROSS_COMPILE)$(CC) -c -o $@ $< -Wall -Werror
	
writer:writer.o
	$(CROSS_COMPILE)$(CC) -o $@ $^
	
