#include "../../src/codegen/codegen.h"
#include <stdio.h>
#include <string.h>

#define MAX_INPUT 4096

/* Reads whole stdin and prints it using the codegen function named in argv[1]*/
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s string|int\n", argv[0]);
    return 2;
  }
  char buf[MAX_INPUT + 1];
  size_t len = fread(buf, 1, MAX_INPUT, stdin);
  buf[len] = '\0';

  if (strcmp(argv[1], "string") == 0) {
    cg_print_string(buf);
  } else if (strcmp(argv[1], "int") == 0) {
    cg_print_int(buf);
  } else {
    fprintf(stderr, "unknow function: %s\n", argv[1]);
    return 2;
  }
  printf("\n");
  return 0;
}
