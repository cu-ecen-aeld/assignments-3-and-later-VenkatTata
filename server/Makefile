CC=gcc
CROSS_COMPILE=

all: aesdsocket

clean:
		-rm -f *.o aesdsocket
	
aesdsocket:aesdsocket.o
	$(CROSS_COMPILE)$(CC) -Wall -Werror -g -o aesdsocket aesdsocket.o -lrt -lpthread

aesdsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) -Wall -Werror -g -c aesdsocket.c queue.h
	
