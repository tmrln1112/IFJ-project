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

/* ---------- Calls ---------- */
// Pushes arguments in order and calls the function, result stays on stack
void cg_fun_call(const ast_node *call) {
  for (const ast_node *arg = call->left; arg != NULL; arg = arg->left) {
    printf("PUSHS ");
    cg_print_operand(arg);
    printf("\n");
  }
  printf("CALL $%s\n", call->value);
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
  printf("PUSHS nil@nil\n"); // Default result is Unit
  printf("POPFRAME\n");
  printf("RETURN\n");
}

/* ---------- Built-in functions ---------- */
// Prints any value, Unit is printed as ()
static void cg_builtin_print(void) {
  printf("\nLABEL $print\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@value\n");
  printf("POPS LF@value\n");
  printf("JUMPIFNEQ $print$write LF@value nil@nil\n");
  printf("WRITE string@()\n"); // Unit has no text form in WRITE
  printf("JUMP $print$end\n");
  printf("LABEL $print$write\n");
  printf("WRITE LF@value\n");
  printf("LABEL $print$end\n");
  printf("PUSHS nil@nil\n"); // print returns Unit
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Reads one line of given type, on EOF or bad input uses fallback value
static void cg_builtin_read(const char *name, const char *type,
                            const char *fallback) {
  printf("\nLABEL $%s\n", name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@result\n");
  printf("READ LF@result %s\n", type);
  printf("JUMPIFNEQ $%s$end LF@result nil@nil\n", name);
  printf("MOVE LF@result %s\n", fallback); // READ gave nil
  printf("LABEL $%s$end\n", name);
  printf("PUSHS LF@result\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Built-in with one argument that is done by one instruction
static void cg_builtin_simple(const char *name, const char *instr) {
  printf("\nLABEL $%s\n", name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@result\n");
  printf("POPS LF@value\n");
  printf("%s LF@result LF@value\n", instr);
  printf("PUSHS LF@result\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Converts any value to string, type is checked at runtime
static void cg_builtin_tostr(void) {
  printf("\nLABEL $toStr\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@type\n");
  printf("DEFVAR LF@result\n");
  printf("POPS LF@value\n");
  printf("TYPE LF@type LF@value\n");
  printf("JUMPIFEQ $toStr$int LF@type string@int\n");
  printf("JUMPIFEQ $toStr$float LF@type string@float\n");
  printf("JUMPIFEQ $toStr$unit LF@type string@nil\n");
  printf("MOVE LF@result LF@value\n"); // Already a string
  printf("JUMP $toStr$end\n");
  printf("LABEL $toStr$int\n");
  printf("INT2STR LF@result LF@value\n");
  printf("JUMP $toStr$end\n");
  printf("LABEL $toStr$float\n");
  printf("FLOAT2STR LF@result LF@value\n"); // Always %.2f
  printf("JUMP $toStr$end\n");
  printf("LABEL $toStr$unit\n");
  printf("MOVE LF@result string@()\n");
  printf("LABEL $toStr$end\n");
  printf("PUSHS LF@result\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Returns ASCII code of char at index, -1 if index is out of string
static void cg_builtin_ord(void) {
  printf("\nLABEL $ord\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@index\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@check\n");
  printf("DEFVAR LF@result\n");
  printf("POPS LF@index\n"); // Last argument is on top
  printf("POPS LF@value\n");
  printf("STRLEN LF@len LF@value\n");
  printf("LT LF@check LF@index int@0\n");
  printf("JUMPIFEQ $ord$out LF@check bool@true\n");
  printf("LT LF@check LF@index LF@len\n");
  printf("JUMPIFEQ $ord$out LF@check bool@false\n"); // Also empty string
  printf("STRI2INT LF@result LF@value LF@index\n");
  printf("JUMP $ord$end\n");
  printf("LABEL $ord$out\n");
  printf("MOVE LF@result int@-1\n");
  printf("LABEL $ord$end\n");
  printf("PUSHS LF@result\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Returns part of string from start to end (end not included), error 40
// on bad indexes
static void cg_builtin_substr(void) {
  printf("\nLABEL $subStr\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@start\n");
  printf("DEFVAR LF@end\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@check\n");
  printf("DEFVAR LF@char\n");
  printf("DEFVAR LF@result\n");
  printf("POPS LF@end\n"); // Arguments come back in reverse order
  printf("POPS LF@start\n");
  printf("POPS LF@value\n");
  printf("STRLEN LF@len LF@value\n");
  printf("LT LF@check LF@start int@0\n");
  printf("JUMPIFEQ $subStr$error LF@check bool@true\n");
  printf("LT LF@check LF@end int@0\n");
  printf("JUMPIFEQ $subStr$error LF@check bool@true\n");
  printf("GT LF@check LF@start LF@end\n");
  printf("JUMPIFEQ $subStr$error LF@check bool@true\n");
  printf("LT LF@check LF@start LF@len\n");
  printf("JUMPIFEQ $subStr$error LF@check bool@false\n"); // start >= len
  printf("GT LF@check LF@end LF@len\n");
  printf("JUMPIFEQ $subStr$error LF@check bool@true\n");
  printf("MOVE LF@result string@\n");
  printf("LABEL $subStr$loop\n"); // Add one char per step
  printf("JUMPIFEQ $subStr$end LF@start LF@end\n");
  printf("GETCHAR LF@char LF@value LF@start\n");
  printf("CONCAT LF@result LF@result LF@char\n");
  printf("ADD LF@start LF@start int@1\n");
  printf("JUMP $subStr$loop\n");
  printf("LABEL $subStr$error\n");
  printf("EXIT int@40\n");
  printf("LABEL $subStr$end\n");
  printf("PUSHS LF@result\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Prints code of all built-in functions
static void cg_builtins(void) {
  cg_builtin_print();
  cg_builtin_read("readLine", "string", "string@");
  cg_builtin_read("readInt", "int", "int@0");
  cg_builtin_read("readDouble", "float", "float@0x0p+0");
  cg_builtin_simple("toInt", "FLOAT2INT");
  cg_builtin_simple("strLength", "STRLEN");
  cg_builtin_simple("chr", "INT2CHAR");
  cg_builtin_tostr();
  cg_builtin_ord();
  cg_builtin_substr();
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
  cg_builtins();
}