ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -Wall -Werror -g 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -lrt -pthread
endif


all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o
	 $(CC) $(CFLAGS) -o aesdsocket aesdsocket.o $(LDFLAGS)
aesdsocket.o: aesdsocket.c
	 $(CC) $(CFLAGS) -c aesdsocket.c queue.h

clean: 
	 rm -f  *.o  \
	        aesdsocket
