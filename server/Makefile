ifeq ($(CC),)
	CC = ${CROSS_COMPILE}gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

all: default

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $^ -o $@ $(INCLUDES) $(LDFLAGS) $(CFLAGS)

clean:
	rm -f aesdsocket aesdsocket.o

.PHONY: clean
