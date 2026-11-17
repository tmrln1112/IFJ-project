/**
 * Project IFJ26 compiler
 * File: codegen.c
 * Description: Generation of IFJcode26 target code
 * Author: <xalimzt00> Tamerlan Alimzhanov
 */

#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>

/* ---------- Constants ---------- */
// Prints a string constant, chars 0-32, # and \ as \xyz escape
void cg_print_string(const char *str) {
  printf("string@");
  for (size_t i = 0; str[i] != '\0'; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c <= 32 || c == 35 || c == 92) {
      printf("\\%03d", c); // Always 3 digits, e.g \032
    } else {
      putchar(c);
    }
  }
}

// Prints an int constant (scanner already removed underscores)
void cg_print_int(const char *text) { printf("int@%s", text); }

// Prints a float constant in hex format
void cg_print_float(const char *text) {
  printf("float@%a", strtod(text, NULL));
}

/* ---------- Labels ---------- */
// Returns a new unique label number
int cg_new_label(void) {
  static int last_label = 0; // Keeps value between calls
  return ++last_label;
}

/* ---------- Operands ---------- */
// Prints a variable or literal, the form depends on its data type
void cg_print_operand(const ast_node *node) {
  switch (node->data_type) {
  case TYPE_ID:
    printf("LF@%s", node->value); // Local variable of current function
    break;
  case TYPE_INT:
    cg_print_int(node->value);
    break;
  case TYPE_DOUBLE:
    cg_print_float(node->value);
    break;
  case TYPE_STRING:
    cg_print_string(node->value);
    break;
  default: // TYPE_UNIT and TYPE_NONE have no value
    printf("nil@nil");
    break;
  }
}

/* ---------- Functions ---------- */
// Defines a local variable for every parameter
static void cg_params_defvar(ast_node *param) {
  for (; param != NULL; param = param->left) {
    printf("DEFVAR LF@%s\n", param->value);
  }
}

// Pops arguments into parameters, last parameter first (top of stack)
static void cg_params_pop(ast_node *param) {
  if (param == NULL) {
    return;
  }
  cg_params_pop(param->left); // Go to the last parameter first
  printf("POPS LF@%s\n", param->value);
}

// Generates one function: label, new frame, parameters, return
static void cg_function(ast_node *def) {
  printf("\nLABEL $%s\n", def->value); // def->value is the function name
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  cg_params_defvar(def->left);
  cg_params_pop(def->left);
  printf("POPFRAME\n");
  printf("RETURN\n");
}

/* ---------- Program ---------- */
// Generates the whole program: calls @main, then prints all functions
void cg_generate(ast_node *root) {
  const char *main_name = NULL;
  for (ast_node *fn = root; fn != NULL; fn = fn->right) {
    if (fn->node_type == NODE_MAIN_FUNCTION) {
      main_name = fn->left->value;
    }
  }
  if (main_name == NULL) {
    return;
  }

  printf(".IFJcode26\n");
  printf("CALL $%s\n", main_name);
  printf("EXIT int@0\n"); // End program after @main returns

  for (ast_node *fn = root; fn != NULL; fn = fn->right) {
    cg_function(fn->left);
  }
}