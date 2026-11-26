/**
 * Project IFJ26 compiler
 * File: pstack.h
 * Description: Stack for the precedence analysis of expressions
 * Author: <xsmidja00> Jakub Šmíd
 */

#ifndef PSTACK_H
#define PSTACK_H

#include "scanner.h"
#include "ast.h"

typedef enum {
  // Terminals
  S_NOT,        // !
  S_PREFIX,     // unary -
  S_MUL_DIV,    // *, /
  S_ADD_SUB,    // +, -
  S_REL_OP,     // <, >, <=, >=
  S_EQ_OP,      // ==, !=
  S_AND,        // &&
  S_OR,         // ||
  S_LPAR,       // (
  S_RPAR,       // )
  S_OPERAND,    // id or literal
  S_END,        // $ (stopper)

  // Non terminal
  S_EXP,

} symbol_type_t;

typedef struct stack_item {
    symbol_type_t type;
    Token token;
    ast_node *node;
} stack_item_t;

typedef struct {
    stack_item_t *items;
    int top;
    int capacity;
} precedence_stack_t;

int prec_stack_init(precedence_stack_t *stack, int capacity);

int prec_stack_push(precedence_stack_t *stack, stack_item_t item);

int prec_stack_pop(precedence_stack_t *stack, stack_item_t *item);

int prec_stack_top(precedence_stack_t *stack, stack_item_t *item);

int prec_stack_top_terminal(precedence_stack_t *stack, stack_item_t *item);

void prec_stack_free(precedence_stack_t *stack);

#endif