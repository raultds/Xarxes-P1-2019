CC=gcc
CFLAGS=-ansi -pedantic -Wall -pthread

all:
	$(CC) $(CFLAGS) client.c -o client
	
clean:
	rm -f client
