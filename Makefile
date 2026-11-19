CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0

.PHONY: all test bench clean
all: test_myalloc

test_myalloc: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o $@ myalloc.c test_myalloc.c

test: test_myalloc
	./test_myalloc

bench: myalloc.c bench.c myalloc.h
	$(CC) $(CFLAGS) -O2 -o bench myalloc.c bench.c
	./bench

clean:
	rm -f test_myalloc bench t_asan *.o
