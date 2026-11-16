CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g

.PHONY: all clean format test

all: scanner_test

scanner_test: scanner_test.o scanner.o dynstr.o
	$(CC) $(CFLAGS) scanner_test.o scanner.o dynstr.o -o scanner_test

scanner_test.o: scanner_test.c scanner.h
	$(CC) $(CFLAGS) -c scanner_test.c -o scanner_test.o

scanner.o: scanner.c scanner.h dynstr.h
	$(CC) $(CFLAGS) -c scanner.c -o scanner.o

dynstr.o: dynstr.c dynstr.h
	$(CC) $(CFLAGS) -c dynstr.c -o dynstr.o

test: scanner_test
	sh tests/run_tests.sh ./scanner_test

format:
	clang-format --style=LLVM -i *.c *.h

clean:
	rm -f *.o scanner_test
