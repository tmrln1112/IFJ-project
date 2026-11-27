/**
 * Project IFJ26 compiler
 * File: codegen.c
 * Description: Generation of IFJcode26 target code
 * Author: <xalimzt00> Tamerlan Alimzhanov
 */

#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ---------- Variable names ---------- */
#define CG_MAX_VARS 1024

// One visible variable: name from source and its unique number
typedef struct {
  const char *name;
  int id;
} cg_var;

static cg_var cg_scope[CG_MAX_VARS]; // Stack of visible variables
static int cg_scope_top = 0;
static int cg_var_count = 0; // Numbers var/val in current function

// Makes a new variable visible, it hides older ones with the same name
static void cg_declare(const char *name) {
  if (cg_scope_top >= CG_MAX_VARS) {
    exit(99); // Internal error: too many variables
  }
  cg_scope[cg_scope_top].name = name;
  cg_scope[cg_scope_top].id = ++cg_var_count;
  cg_scope_top++;
}

// Prints the target name of a variable, the newest declaration wins
static void cg_print_var(const char *name) {
  for (int i = cg_scope_top - 1; i >= 0; i--) {
    if (strcmp(cg_scope[i].name, name) == 0) {
      printf("LF@%s$%d", name, cg_scope[i].id);
      return;
    }
  }
  printf("LF@%s", name); // Not declared by var/val, so it is a parameter
}

// Pops value from the stack into a variable
static void cg_pop_var(const char *name) {
  printf("POPS ");
  cg_print_var(name);
  printf("\n");
}

/* ---------- Operands ---------- */
// Prints a variable or literal, the form depends on its data type
void cg_print_operand(const ast_node *node) {
  switch (node->data_type) {
  case TYPE_ID:
    cg_print_var(node->value); // Local variable or parameter
    break;
  case TYPE_INT:
    cg_print_int(node->value);
    break;
  case TYPE_DOUBLE:
    cg_print_float(node->value);
    break;
  case TYPE_BOOL:
    printf("bool@%s", node->value);
    break;
  case TYPE_STRING:
    cg_print_string(node->value);
    break;
  default: // TYPE_UNIT and TYPE_NONE have no value
    printf("nil@nil");
    break;
  }
}

static void cg_expression(const ast_node *expr);

/* ---------- Calls ---------- */
// Pushes arguments in order and calls the function, result stays on stack
void cg_fun_call(const ast_node *call) {
  const ast_node *list = call->left;
  if (list && list->node_type != NODE_LIST) {
    for (const ast_node *arg = list; arg; arg = arg->left)
      cg_expression(arg->right ? arg->right : arg);
  } else {
    for (; list; list = list->right)
      for (const ast_node *arg = list->left; arg; arg = arg->left)
        cg_expression(arg->right);
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

static void cg_param_lists_pop(ast_node *list) {
  if (!list)
    return;
  cg_param_lists_pop(list->right);
  cg_params_pop(list->left);
}

static void cg_if(const ast_node *stmt, bool keep_value);

/* ---------- Expressions ---------- */
// Prints stack instructions for one binary operator
static void cg_operator(const char *op) {
  if (strcmp(op, "+") == 0) {
    printf("CALL $$add\n"); // Number addition or string concatenation
  } else if (strcmp(op, "/") == 0) {
    printf("CALL $$div\n"); // IDIVS for Int, DIVS for Double
  } else {
    printf("CALL $$align\n"); // Int -> Double if the other one is Double
    if (strcmp(op, "-") == 0) {
      printf("SUBS\n");
    } else if (strcmp(op, "*") == 0) {
      printf("MULS\n");
    } else if (strcmp(op, "<") == 0) {
      printf("LTS\n");
    } else if (strcmp(op, ">") == 0) {
      printf("GTS\n");
    } else if (strcmp(op, "<=") == 0) {
      printf("GTS\nNOTS\n"); // a <= b is not (a > b)
    } else if (strcmp(op, ">=") == 0) {
      printf("LTS\nNOTS\n"); // a >= b is not (a < b)
    } else if (strcmp(op, "==") == 0) {
      printf("EQS\n");
    } else if (strcmp(op, "!=") == 0) {
      printf("EQS\nNOTS\n");
    }
  }
}

static void cg_expression_value(const ast_node *expr) {
  if (expr == NULL) {
    printf("PUSHS nil@nil\n"); // Missing value is Unit
    return;
  }
  if (expr->node_type == NODE_FUN_CALL) {
    cg_fun_call(expr); // Call leaves its result on the stack
    return;
  }
  if (expr->bare_call) {
    printf("CALL $%s\n", expr->value);
    return;
  }
  if (expr->node_type == NODE_IF) {
    cg_if(expr, true);
    return;
  }
  if (expr->node_type == NODE_MEMBER) {
    cg_expression(expr->left);
    if (strcmp(expr->value, "toString") == 0)
      printf("CALL $toStr\n");
    else if (strcmp(expr->value, "toInt") == 0)
      printf("CALL $toInt\n");
    else if (strcmp(expr->value, "length") == 0)
      printf("CALL $strLength\n");
    else
      printf("CALL $$floor\n");
    return;
  }
  if (expr->node_type != NODE_EXPRESSION && expr->node_type != NODE_CONDITION) {
    printf("PUSHS ");
    cg_print_operand(expr); // Leaf: variable or literal
    printf("\n");
    return;
  }
  if (expr->value == NULL) {
    cg_expression(expr->left); // Wrapper without operator
    return;
  }
  cg_expression(expr->left);
  if (strcmp(expr->value, "!") == 0) {
    printf("NOTS\n");
    return;
  }
  if (strcmp(expr->value, "&&") == 0 || strcmp(expr->value, "||") == 0) {
    int id = cg_new_label();
    const char *result = expr->value[0] == '&' ? "false" : "true";
    printf("PUSHS bool@%s\nJUMPIFEQS $$logic%d$short\n", result, id);
    cg_expression(expr->right);
    printf("JUMP $$logic%d$end\nLABEL $$logic%d$short\n", id, id);
    printf("PUSHS bool@%s\nLABEL $$logic%d$end\n", result, id);
    return;
  }
  if (expr->right == NULL && strcmp(expr->value, "-") == 0) {
    printf("PUSHS int@-1\n");
    cg_operator("*");
    return;
  }
  cg_expression(expr->right);
  cg_operator(expr->value);
}

// Generates an expression and leaves its result on the data stack.
static void cg_expression(const ast_node *expr) {
  cg_expression_value(expr);
  if (expr && expr->widen_to_double)
    printf("INT2FLOATS\n");
  if (expr && expr->discard_value)
    printf("POPS GF@$discard\nPUSHS nil@nil\n");
}

/* ---------- Statements ---------- */
static void cg_statement(const ast_node *stmt); // Used by if/while blocks
static void cg_body(const ast_node *body);

// Defines all local variables in advance, also inside if/while blocks,
// so DEFVAR is never repeated in a loop
static void cg_defvars(const ast_node *node) {
  while (node && node->node_type == NODE_BLOCK) {
    cg_defvars(node->left);
    node = node->right;
  }
  if (!node)
    return;
  if (node->node_type == NODE_VAR_DEF || node->node_type == NODE_VAL_DEF) {
    cg_defvars(node->right);
    printf("DEFVAR LF@%s$%d\n", node->value, ++cg_var_count);
    return;
  }
  if (node->node_type == NODE_ARGUMENT) {
    cg_defvars(node->right);
    cg_defvars(node->left);
    return;
  }
  if (node->node_type == NODE_FOR) {
    const ast_node *start = node->left;
    const ast_node *end = start->right;
    const ast_node *step = end->right;
    cg_defvars(start->left);
    cg_defvars(end->left);
    cg_defvars(step->left);
    int id = ++cg_var_count;
    printf("DEFVAR LF@%s$%d\n", node->value, id);
    printf("DEFVAR LF@$forEnd%d\nDEFVAR LF@$forStep%d\n", id, id);
    cg_defvars(step->right);
    cg_defvars(node->right);
    return;
  }
  cg_defvars(node->left);
  cg_defvars(node->right);
}

// Leaves the function, result must already be on the stack
static void cg_epilogue(void) {
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Pushes condition result (bool) on the stack
static void cg_condition(const ast_node *cond) { cg_expression(cond); }

// Generates all statements of a block (then/else branch or loop body)
static void cg_block(const ast_node *block) {
  int saved_top = cg_scope_top; // Variables of this block end with it
  for (; block != NULL && block->node_type == NODE_BLOCK;
       block = block->right) {
    if (block->left != NULL) {
      cg_statement(block->left);
    }
  }
  cg_scope_top = saved_top;
}

// Generates if-then-else using jumps, else part may be missing
static void cg_if(const ast_node *stmt, bool keep_value) {
  int id = cg_new_label();
  const ast_node *body = stmt->right; // NODE_IF_BODY
  int saved_top = cg_scope_top;

  cg_condition(stmt->left);
  printf("PUSHS bool@false\n");
  printf("JUMPIFEQS $$if%d$else\n", id); // Condition is false
  if (body != NULL) {
    if (keep_value)
      cg_body(body->left);
    else
      cg_block(body->left);
    cg_scope_top = saved_top;
  }
  printf("JUMP $$if%d$end\n", id);
  printf("LABEL $$if%d$else\n", id);
  if (body != NULL) {
    if (keep_value)
      cg_body(body->right);
    else
      cg_block(body->right);
    cg_scope_top = saved_top;
  }
  printf("LABEL $$if%d$end\n", id);
}

// Generates while loop, condition is checked before every pass
static void cg_while(const ast_node *stmt) {
  int id = cg_new_label();

  printf("LABEL $$while%d$start\n", id);
  cg_condition(stmt->left);
  printf("PUSHS bool@false\n");
  printf("JUMPIFEQS $$while%d$end\n", id); // Condition is false
  cg_block(stmt->right);
  printf("JUMP $$while%d$start\n", id);
  printf("LABEL $$while%d$end\n", id);
}

static void cg_for(const ast_node *stmt) {
  const ast_node *start = stmt->left;
  const ast_node *end = start->right;
  const ast_node *step = end->right;
  int saved_top = cg_scope_top;
  int label = cg_new_label();

  cg_expression(start->left);
  cg_expression(end->left);
  if (step->left)
    cg_expression(step->left);
  else
    printf("PUSHS int@1\n");

  cg_declare(stmt->value);
  int id = cg_var_count;
  printf("POPS LF@$forStep%d\nPOPS LF@$forEnd%d\n", id, id);
  cg_pop_var(stmt->value);
  // Step computed at runtime must not be zero, error 40
  printf("JUMPIFNEQ $$for%d$check LF@$forStep%d int@0\n", label, id);
  printf("EXIT int@40\n");

  printf("LABEL $$for%d$check\n", label);
  printf("PUSHS LF@$forStep%d\nPUSHS int@0\nLTS\nPUSHS bool@true\n", id);
  printf("JUMPIFEQS $$for%d$descending\n", label);
  printf("PUSHS LF@%s$%d\nPUSHS LF@$forEnd%d\n", stmt->value, id, id);
  printf("%s", stmt->inclusive_range ? "GTS\nNOTS\n" : "LTS\n");
  printf("JUMP $$for%d$bound\n", label);

  printf("LABEL $$for%d$descending\n", label);
  printf("PUSHS LF@%s$%d\nPUSHS LF@$forEnd%d\n", stmt->value, id, id);
  printf("%s", stmt->inclusive_range ? "LTS\nNOTS\n" : "GTS\n");
  printf("LABEL $$for%d$bound\n", label);
  printf("PUSHS bool@false\nJUMPIFEQS $$for%d$end\n", label);

  for (const ast_node *guard = step->right; guard; guard = guard->right) {
    cg_expression(guard->left);
    printf("PUSHS bool@false\nJUMPIFEQS $$for%d$next\n", label);
  }
  cg_block(stmt->right);

  printf("LABEL $$for%d$next\n", label);
  if (stmt->inclusive_range)
    printf("JUMPIFEQ $$for%d$end LF@%s$%d LF@$forEnd%d\n", label, stmt->value,
           id, id);
  printf("ADD LF@%s$%d LF@%s$%d LF@$forStep%d\n", stmt->value, id, stmt->value,
         id, id);
  printf("JUMP $$for%d$check\nLABEL $$for%d$end\n", label, label);
  cg_scope_top = saved_top;
}

// Generates one statement of a block
static void cg_statement(const ast_node *stmt) {
  switch (stmt->node_type) {
  case NODE_VAR_DEF:
  case NODE_VAL_DEF:
    cg_expression(stmt->right); // Value may still use an outer variable
    cg_declare(stmt->value);    // DEFVAR was done in cg_defvars
    cg_pop_var(stmt->value);
    break;
  case NODE_ASSIGNMENT:
    cg_expression(stmt->right);
    cg_pop_var(stmt->left->value);
    break;
  case NODE_FUN_CALL:
    cg_fun_call(stmt);
    printf("POPS GF@$discard\n"); // Result is not used
    break;
  case NODE_RETURN:
    cg_expression(stmt->left); // Call, expression or Unit
    cg_epilogue();
    break;
  case NODE_IF:
    cg_if(stmt, false);
    break;
  case NODE_FOR:
    cg_for(stmt);
    break;
  case NODE_WHILE:
    cg_while(stmt);
    break;
  case NODE_TERM:
  case NODE_MEMBER:
  case NODE_EXPRESSION:
    cg_expression(stmt);
    printf("POPS GF@$discard\n"); // Result is not used
    break;
  default: // Nothing to generate
    break;
  }
}

// Generates function body, the last call or expression is the result
static void cg_body(const ast_node *body) {
  if (body != NULL && body->node_type != NODE_BLOCK) {
    cg_expression(body); // def f() = g() or def f() = x + 1
    return;
  }
  for (; body != NULL && body->node_type == NODE_BLOCK; body = body->right) {
    const ast_node *stmt = body->left;
    if (stmt == NULL) {
      continue;
    }
    if (body->right == NULL && stmt->if_has_value) {
      cg_if(stmt, true);
      if (stmt->widen_to_double)
        printf("INT2FLOATS\n");
      if (stmt->discard_value)
        printf("POPS GF@$discard\nPUSHS nil@nil\n");
      return;
    }
    if (body->right == NULL &&
        (stmt->node_type == NODE_FUN_CALL ||
         stmt->node_type == NODE_EXPRESSION || stmt->node_type == NODE_TERM ||
         stmt->node_type == NODE_MEMBER)) {
      cg_expression(stmt); // Last call or expression gives the result
      return;
    }
    cg_statement(stmt);
  }
  printf("PUSHS nil@nil\n"); // Default result is Unit
}

// Generates one function: label, new frame, parameters, body, return
static void cg_function(ast_node *def) {
  const ast_node *body = def->right ? def->right->right : NULL;
  printf("\nLABEL $%s\n", def->value); // def->value is the function name
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  for (ast_node *list = def->left; list; list = list->right)
    cg_params_defvar(list->left);
  cg_var_count = 0;
  cg_defvars(body);
  cg_param_lists_pop(def->left);
  cg_var_count = 0; // Same numbers again while generating
  cg_scope_top = 0;
  cg_body(body);
  cg_epilogue();
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
  printf("JUMPIFEQ $toStr$bool LF@type string@bool\n");
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
  printf("JUMP $toStr$end\n");
  printf("LABEL $toStr$bool\n");
  printf("MOVE LF@result string@false\n");
  printf("JUMPIFEQ $toStr$end LF@value bool@false\n");
  printf("MOVE LF@result string@true\n");
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

/* ---------- Operator helpers ---------- */
// Converts Int to Double when the other operand is Double, keeps both on stack
static void cg_helper_align(void) {
  printf("\nLABEL $$align\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@a\n");
  printf("DEFVAR LF@b\n");
  printf("DEFVAR LF@ta\n");
  printf("DEFVAR LF@tb\n");
  printf("POPS LF@b\n"); // Right operand is on top
  printf("POPS LF@a\n");
  printf("TYPE LF@ta LF@a\n");
  printf("TYPE LF@tb LF@b\n");
  printf("JUMPIFEQ $$align$end LF@ta LF@tb\n"); // Same types, nothing to do
  printf("JUMPIFNEQ $$align$b LF@ta string@int\n");
  printf("JUMPIFNEQ $$align$end LF@tb string@float\n");
  printf("INT2FLOAT LF@a LF@a\n");
  printf("JUMP $$align$end\n");
  printf("LABEL $$align$b\n");
  printf("JUMPIFNEQ $$align$end LF@tb string@int\n");
  printf("JUMPIFNEQ $$align$end LF@ta string@float\n");
  printf("INT2FLOAT LF@b LF@b\n");
  printf("LABEL $$align$end\n");
  printf("PUSHS LF@a\n");
  printf("PUSHS LF@b\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Adds two numbers or concatenates two strings
static void cg_helper_add(void) {
  printf("\nLABEL $$add\n");
  printf("CALL $$align\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@a\n");
  printf("DEFVAR LF@b\n");
  printf("DEFVAR LF@type\n");
  printf("POPS LF@b\n");
  printf("POPS LF@a\n");
  printf("TYPE LF@type LF@a\n");
  printf("JUMPIFEQ $$add$str LF@type string@string\n");
  printf("PUSHS LF@a\n");
  printf("PUSHS LF@b\n");
  printf("ADDS\n");
  printf("JUMP $$add$end\n");
  printf("LABEL $$add$str\n");
  printf("CONCAT LF@a LF@a LF@b\n"); // CONCAT has no stack version
  printf("PUSHS LF@a\n");
  printf("LABEL $$add$end\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Divides two numbers, integer division for Int
static void cg_helper_div(void) {
  printf("\nLABEL $$div\n");
  printf("CALL $$align\n");
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
  printf("DEFVAR LF@a\n");
  printf("DEFVAR LF@b\n");
  printf("DEFVAR LF@type\n");
  printf("POPS LF@b\n");
  printf("POPS LF@a\n");
  printf("TYPE LF@type LF@a\n");
  printf("PUSHS LF@a\n");
  printf("PUSHS LF@b\n");
  printf("JUMPIFEQ $$div$int LF@type string@int\n");
  printf("DIVS\n");
  printf("JUMP $$div$end\n");
  printf("LABEL $$div$int\n");
  printf("IDIVS\n"); // Division by zero exits with 57
  printf("LABEL $$div$end\n");
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// Truncation differs from floor for negative nonintegral values.
static void cg_helper_floor(void) {
  printf("\nLABEL $$floor\nCREATEFRAME\nPUSHFRAME\n");
  printf("DEFVAR LF@value\nDEFVAR LF@rounded\nDEFVAR LF@check\n");
  printf("POPS LF@value\nMOVE LF@rounded LF@value\n");
  printf("LT LF@check LF@value float@0x1p+52\n");
  printf("JUMPIFEQ $$floor$end LF@check bool@false\n");
  printf("GT LF@check LF@value float@-0x1p+52\n");
  printf("JUMPIFEQ $$floor$end LF@check bool@false\n");
  printf("FLOAT2INT LF@rounded LF@value\nINT2FLOAT LF@rounded LF@rounded\n");
  printf("GT LF@check LF@rounded LF@value\n");
  printf("JUMPIFEQ $$floor$end LF@check bool@false\n");
  printf("SUB LF@rounded LF@rounded float@0x1p+0\n");
  printf("LABEL $$floor$end\nPUSHS LF@rounded\nPOPFRAME\nRETURN\n");
}

// Prints code of all built-in functions
static void cg_builtins(void) {
  cg_builtin_print();
  cg_builtin_read("readLine", "string", "string@");
  cg_builtin_read("readBoolean", "bool", "bool@false");
  cg_builtin_read("readInt", "int", "int@0");
  cg_builtin_read("readDouble", "float", "float@0x0p+0");
  cg_builtin_simple("toInt", "FLOAT2INT");
  cg_builtin_simple("strLength", "STRLEN");
  cg_builtin_simple("chr", "INT2CHAR");
  cg_builtin_tostr();
  cg_builtin_ord();
  cg_builtin_substr();
  cg_helper_align();
  cg_helper_add();
  cg_helper_div();
  cg_helper_floor();
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
  printf("DEFVAR GF@$discard\n"); // For unused call results
  printf("CALL $%s\n", main_name);
  printf("EXIT int@0\n"); // End program after @main returns

  for (ast_node *fn = root; fn != NULL; fn = fn->right) {
    cg_function(fn->left);
  }
  cg_builtins();
}