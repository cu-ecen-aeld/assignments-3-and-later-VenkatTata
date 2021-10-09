CC=gcc
CROSS_COMPILE=

all: aesdsocket

clean:
		-rm -f *.o aesdsocket

%.o:%.c
	$(CROSS_COMPILE)$(CC) -c -o $@ $< -Wall -Werror
	
aesdsocket:aesdsocket.o
	$(CROSS_COMPILE)$(CC) -o $@ $^
	
