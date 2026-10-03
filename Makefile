# Makefile
CC = gcc
CFLAGS = -Wall -Wextra  -std=c11 -pedantic -g 

all :  scanner.o

scanner.o: scanner.c scanner.h
	$(CC) $(CFLAGS) -c scanner.c -o scanner.o

format:
	clang-format --style=LLVM -i *.c *.h
	
clean:
	rm -f scanner.o
