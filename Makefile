CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g
TARGET = ifj26

# sources are split between these dirs
VPATH = src:src/scanner:src/parser:src/ast:src/codegen
CPPFLAGS = -Isrc -Isrc/scanner -Isrc/parser -Isrc/ast -Isrc/codegen

SRCS = main.c scanner.c dynstr.c \
       ast.c parser.c codegen.c  

OBJS = $(addprefix build/,$(SRCS:.c=.o))
DEPS = $(OBJS:.o=.d) build/scanner_test.d

.PHONY: all clean rebuild format test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# .d files track included headers
build/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# scanner can be tested separately

scanner_test: build/scanner_test.o build/scanner.o build/dynstr.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: scanner_test
	sh tests/scanner/run_tests.sh ./scanner_test

format:
	find src -type f \( -name '*.c' -o -name '*.h' \) -exec clang-format --style=LLVM -i {} +

clean:
	$(RM) -r build
	$(RM) $(TARGET) scanner_test

# separate calls so clean does not race the build with -j
rebuild:
	$(MAKE) clean
	$(MAKE) all

-include $(DEPS)
