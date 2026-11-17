#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "scanner.h"
#include <stdio.h>
#include <string.h>

#define MAX_INPUT 4096

// Reads whole stdin and prints it using the codegen function named in argv[1]
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s string|int|float|label|program\n", argv[0]);
    return 2;
  }
  // Runs the whole chain: scanner -> parser -> code generator
  if (strcmp(argv[1], "program") == 0) {
    scanner_init(stdin);
    ast_node *root = NULL;
    int ret = parse_program(&root);
    if (ret == 0) {
      cg_generate(root);
    }
    ast_destroy(root);
    scanner_free();
    return ret; // Parser error code, or 0 on success
  }
  char buf[MAX_INPUT + 1];
  size_t len = fread(buf, 1, MAX_INPUT, stdin);
  buf[len] = '\0';

  if (strcmp(argv[1], "string") == 0) {
    cg_print_string(buf);
  } else if (strcmp(argv[1], "int") == 0) {
    cg_print_int(buf);
  } else if (strcmp(argv[1], "float") == 0) {
    cg_print_float(buf);
  } else if (strcmp(argv[1], "label") == 0) {
    int a = cg_new_label();
    int b = cg_new_label();
    int c = cg_new_label();
    printf("%d %d %d", a, b, c);
  } else {
    fprintf(stderr, "unknown function: %s\n", argv[1]);
    return 2;
  }
  printf("\n");
  return 0;
}
