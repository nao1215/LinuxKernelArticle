CC     = gcc
CFLAGS = -Wall -O2

all:test

test:test.c
	$(CC) $(CFLAGS) -o test test.c

.PHONY: clean
clean:
	rm -f test *.o
