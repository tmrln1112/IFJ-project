#include "../../src/codegen/codegen.h"
#include <stdio.h>

#define MAX_INPUT 4096

/* Reads whole stdin and prints it as an IFJcode26 string constant. */
int main(void) {
  char buf[MAX_INPUT + 1];
  size_t len = fread(buf, 1, MAX_INPUT, stdin);
  buf[len] = '\0';
  cg_print_string(buf);
  printf("\n");
  return 0;
}