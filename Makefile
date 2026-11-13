CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g

.PHONY: all clean format test

all: ifj26

ifj26: main.o scanner.o dynstr.o
	$(CC) $(CFLAGS) main.o scanner.o dynstr.o -o ifj26

main.o: main.c scanner.h
	$(CC) $(CFLAGS) -c main.c -o main.o

scanner.o: scanner.c scanner.h dynstr.h
	$(CC) $(CFLAGS) -c scanner.c -o scanner.o

dynstr.o: dynstr.c dynstr.h
	$(CC) $(CFLAGS) -c dynstr.c -o dynstr.o

test: ifj26
	sh tests/run_tests.sh ./ifj26

format:
	clang-format --style=LLVM -i *.c *.h

clean:
	rm -f *.o ifj26