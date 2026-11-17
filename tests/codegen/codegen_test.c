#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "scanner.h"
#include <stdio.h>
#include <string.h>

#define MAX_INPUT 4096

// Splits a "<type> <value>" line, returns the type and sets *value
static data_type_t split_operand(char *line, char **value) {
  char *space = strchr(line, ' ');
  if (space != NULL) {
    *space = '\0';
    *value = space + 1;
  } else {
    *value = line + strlen(line); // No value -> empty string
  }
  if (strcmp(line, "id") == 0) {
    return TYPE_ID;
  } else if (strcmp(line, "int") == 0) {
    return TYPE_INT;
  } else if (strcmp(line, "float") == 0) {
    return TYPE_DOUBLE;
  } else if (strcmp(line, "string") == 0) {
    return TYPE_STRING;
  }
  return TYPE_UNIT;
}

// Reads whole stdin and prints it using the codegen function named in argv[1]
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s string|int|float|label|program|operand|call\n",
            argv[0]);
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
  } else if (strcmp(argv[1], "operand") == 0) {
    char *value;
    data_type_t type = split_operand(buf, &value);
    ast_node node = {NODE_TERM, type, value, NULL, NULL};
    cg_print_operand(&node);
  } else if (strcmp(argv[1], "call") == 0) {
    // Input: first line is the function name, then one argument per line
    ast_node args[16];
    int count = 0;
    char *name = strtok(buf, "\n");
    char *line;
    while (count < 16 && (line = strtok(NULL, "\n")) != NULL) {
      char *value;
      data_type_t type = split_operand(line, &value);
      args[count] = (ast_node){NODE_ARGUMENT, type, value, NULL, NULL};
      if (count > 0) {
        args[count - 1].left = &args[count]; // Link previous argument to this
      }
      count++;
    }
    ast_node call = {NODE_FUN_CALL, TYPE_ID, name, NULL, NULL};
    if (count > 0) {
      call.left = &args[0];
    }
    cg_fun_call(&call);
    return 0; // Output already ends with a newline
  } else {
    fprintf(stderr, "unknown function: %s\n", argv[1]);
    return 2;
  }
  printf("\n");
  return 0;
}
