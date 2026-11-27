/**
 * Project IFJ26 compiler
 * File: parser.c
 * Description: Syntax analysis: recursive descent and precedence analysis of
 * expressions Author: <xsmidja00> Jakub Šmíd
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "pstack.h"
#include "scanner.h"

#define STACK_CAPACITY 10

/* Current token received from scanner */
static Token current_token;

/* Token ahead of current token*/
static Token look_ahead_token;

/* Indicates validity of current_token */
static bool valid_token = false;

/* Indicates validity of look_ahead_token */
static bool valid_look_ahead = false;

/* Value used to stop reading new tokens */
static bool is_EOF = false;

/* Value used to store get_token return value*/
static int scanner_err = 0;
static bool consumed_dedent = false;

const precedence_type precedence_table[12][12] = {
    //  !      unary -  [*, /]  [+, -]  REL      EQ     &&      ||      ( ) ID $
    {P_PUSH, P_PUSH, P_POP, P_POP, P_POP, P_POP, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // !
    {P_PUSH, P_PUSH, P_POP, P_POP, P_POP, P_POP, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // unary -
    {P_PUSH, P_PUSH, P_POP, P_POP, P_POP, P_POP, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // [*, /]
    {P_PUSH, P_PUSH, P_PUSH, P_POP, P_POP, P_POP, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // [+, -]
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_ERR, P_POP, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // REL
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_ERR, P_POP, P_POP, P_PUSH, P_POP,
     P_PUSH, P_POP}, // EQ
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_POP, P_POP, P_PUSH,
     P_POP, P_PUSH, P_POP}, // &&
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_POP, P_PUSH,
     P_POP, P_PUSH, P_POP}, // ||
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH,
     P_EQ, P_PUSH, P_ERR}, // (
    {P_ERR, P_ERR, P_POP, P_POP, P_POP, P_POP, P_POP, P_POP, P_ERR, P_POP,
     P_ERR, P_POP}, // )
    {P_ERR, P_ERR, P_POP, P_POP, P_POP, P_POP, P_POP, P_POP, P_ERR, P_POP,
     P_ERR, P_POP}, // ID or literal
    {P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH, P_PUSH,
     P_ERR, P_PUSH, P_ERR}, // $
};

symbol_type_t token_to_symbol_type(Token_type type) {
  switch (type) {
  case T_MUL:
  case T_DIV:
    return S_MUL_DIV;
  case T_PLUS:
  case T_MINUS:
    return S_ADD_SUB;
  case T_LESS_T:
  case T_LESS_OR_EQ:
  case T_GREATER_T:
  case T_GREATER_OR_EQ:
    return S_REL_OP;
  case T_EQ:
  case T_NEQ:
    return S_EQ_OP;
  case T_LPAR:
    return S_LPAR;
  case T_RPAR:
    return S_RPAR;
  case T_ID:
  case T_LIT_INT:
  case T_LIT_DOUBLE:
  case T_LIT_STRING:
  case T_TRUE:
  case T_FALSE:
    return S_OPERAND;
  case T_NOT:
    return S_NOT;
  case T_AND:
    return S_AND;
  case T_OR:
    return S_OR;
  default:
    return S_END;
  }
}

int symbol_type_to_int(symbol_type_t *type) {
  switch (*type) {
  case S_NOT:
    return 0;
  case S_PREFIX:
    return 1;
  case S_MUL_DIV:
    return 2;
  case S_ADD_SUB:
    return 3;
  case S_REL_OP:
    return 4;
  case S_EQ_OP:
    return 5;
  case S_AND:
    return 6;
  case S_OR:
    return 7;
  case S_LPAR:
    return 8;
  case S_RPAR:
    return 9;
  case S_OPERAND:
    return 10;
  case S_END:
    return 11;
  default:
    return -1;
  }
}

char *op_token_to_str(Token_type type) {
  switch (type) {
  case T_MUL:
    return "*";
  case T_DIV:
    return "/";
  case T_PLUS:
    return "+";
  case T_MINUS:
    return "-";
  case T_LESS_T:
    return "<";
  case T_LESS_OR_EQ:
    return "<=";
  case T_GREATER_T:
    return ">";
  case T_GREATER_OR_EQ:
    return ">=";
  case T_EQ:
    return "==";
  case T_NEQ:
    return "!=";
  case T_NOT:
    return "!";
  case T_AND:
    return "&&";
  case T_OR:
    return "||";
  default:
    return NULL;
  }
}

int build_stack_item(Token *token, symbol_type_t type, stack_item_t *item) {
  if (!token) {
    return 99;
  }

  item->type = type;
  item->token.type = token->type;
  item->token.attribute = NULL;
  item->node = NULL;

  if (token->attribute) {
    item->token.attribute = strdup(token->attribute);
    if (!item->token.attribute) {
      return 99;
    }
  }

  if (type == S_OPERAND) {
    const char *value = token->attribute;
    if (token->type == T_TRUE)
      value = "true";
    if (token->type == T_FALSE)
      value = "false";
    char *value_copy = NULL;
    if (value) {
      value_copy = strdup(value);
      if (!value_copy) {
        free(item->token.attribute);
        item->token.attribute = NULL;
        return 99;
      }
    }
    item->node = ast_create_node(
        NODE_TERM, token_type_to_node_type(token->type), value_copy);
    if (!item->node) {
      free(value_copy);
      free(item->token.attribute);
      item->token.attribute = NULL;
      return 99;
    }
  }

  return 0;
}

int expression_reduction(precedence_stack_t *stack) {
  if (!stack) {
    return 99;
  }

  stack_item_t first = {0}, second = {0}, third = {0};

  if (prec_stack_pop(stack, &first) != 0) {
    return 2;
  }

  ast_node *sub_tree = NULL;

  // Rule: i -> E
  if (first.type == S_OPERAND) {
    if (!first.node) {
      free(first.token.attribute);
      return 2;
    }

    sub_tree = first.node;
    first.node = NULL;
    free(first.token.attribute);
  }

  // Rule: (E) -> E
  else if (first.type == S_RPAR) {
    free(first.token.attribute);

    if (prec_stack_top(stack, &second) == 0 && second.type == S_LPAR) {
      prec_stack_pop(stack, &second);
      free(second.token.attribute);
      sub_tree = ast_create_node(NODE_TERM, TYPE_UNIT, NULL);
      if (!sub_tree)
        return 99;
    } else {
      if (prec_stack_pop(stack, &second) != 0 || second.type != S_EXP ||
          !second.node) {
        if (second.node) {
          ast_destroy(second.node);
        }
        free(second.token.attribute);
        return 2;
      }
      if (prec_stack_pop(stack, &third) != 0 || third.type != S_LPAR) {
        ast_destroy(second.node);
        if (third.node) {
          ast_destroy(third.node);
        }
        free(third.token.attribute);
        return 2;
      }
      free(third.token.attribute);

      sub_tree = second.node;
      second.node = NULL;
    }
  }

  // Rule: E operator E -> E
  else if (first.type == S_EXP) {
    if (!first.node) {
      return 2;
    }

    if (prec_stack_pop(stack, &second) != 0) {
      ast_destroy(first.node);
      return 2;
    }
    bool unary = (second.type == S_PREFIX || second.type == S_NOT);
    if (!unary && (prec_stack_pop(stack, &third) != 0 || third.type != S_EXP ||
                   !third.node)) {
      ast_destroy(first.node);
      ast_destroy(second.node);
      free(second.token.attribute);
      ast_destroy(third.node);
      free(third.token.attribute);
      return 2;
    }

    char *op_str = op_token_to_str(second.token.type);
    free(second.token.attribute);
    if (!op_str) {
      ast_destroy(first.node);
      ast_destroy(third.node);
      return 2;
    }

    sub_tree = ast_create_node(NODE_EXPRESSION, TYPE_NONE, op_str);
    if (!sub_tree) {
      ast_destroy(first.node);
      ast_destroy(third.node);
      return 99;
    }

    sub_tree->left = unary ? first.node : third.node;
    sub_tree->right = unary ? NULL : first.node;
  } else {
    if (first.node) {
      ast_destroy(first.node);
    }
    free(first.token.attribute);
    return 2;
  }

  // Adding sub_tree back to stack
  stack_item_t exp_item = {.type = S_EXP, .node = sub_tree};

  if (prec_stack_push(stack, exp_item) != 0) {
    ast_destroy(sub_tree);
    return 99;
  }

  return 0;
}

bool is_expression_stopper(Token_type type) {
  return (type == T_EOL || type == T_EOF || type == T_THEN || type == T_DO ||
          type == T_SEMICOLON || type == T_DEDENT || type == T_ELSE);
}

data_type_t token_type_to_node_type(Token_type type) {
  switch (type) {
  case T_ID:
    return TYPE_ID;
  case T_DOUBLE:
  case T_LIT_DOUBLE:
    return TYPE_DOUBLE;
  case T_INT:
  case T_LIT_INT:
    return TYPE_INT;
  case T_STRING:
  case T_LIT_STRING:
    return TYPE_STRING;
  case T_UNIT:
    return TYPE_UNIT;
  case T_BOOLEAN:
  case T_TRUE:
  case T_FALSE:
    return TYPE_BOOL;
  default:
    return TYPE_NONE;
  }
}

int parser_or_scanner_err(void) {
  if (scanner_err != 0) {
    return scanner_err;
  }
  return 2;
}

/* Skips T_EOL tokens */
int skip_eol(void) {
  while (valid_token && current_token.type == T_EOL) {
    int err = advance_token();
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

/* Skips T_EOL, T_INDENT, and T_DEDENT tokens */
int skip_whitespace(void) {
  while (valid_token &&
         (current_token.type == T_EOL || current_token.type == T_INDENT ||
          current_token.type == T_DEDENT)) {
    int err = advance_token();
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

bool is_type_token(Token_type type) {
  return (type == T_INT || type == T_DOUBLE || type == T_STRING ||
          type == T_UNIT || type == T_BOOLEAN);
}

bool is_operand_token(Token_type type) {
  return (type == T_ID || type == T_LIT_INT || type == T_LIT_DOUBLE ||
          type == T_LIT_STRING || type == T_TRUE || type == T_FALSE);
}

static bool starts_expression(Token_type type) {
  return is_operand_token(type) || type == T_LPAR || type == T_MINUS ||
         type == T_NOT || type == T_IF;
}

bool start_of_function_definition(void) {
  if (current_token.type == T_AT_MAIN || current_token.type == T_DEF) {
    return true;
  }
  return false;
}

/* Check if current and expected are the same Token_type
 * If yes advance to next token
 */
bool match_tokens(Token_type expected) {
  if (valid_token && current_token.type == expected) {
    if (advance_token() != 0) {
      return false;
    }
    return true;
  }
  return false;
}

/* Frees previous token
 * Reads next token
 * Returns 0 if success, 1 if fail
 */
int advance_token(void) {
  consumed_dedent = valid_token && current_token.type == T_DEDENT;
  // Free old current token if valid
  if (valid_token) {
    free_token(&current_token);
    valid_token = false;
  }

  // Move look_ahead_token into current_token if valid
  if (valid_look_ahead) {
    current_token = look_ahead_token;
    valid_token = true;
    valid_look_ahead = false;
  }

  // If we already hit EOF previously, don't query the scanner again
  if (is_EOF) {
    valid_look_ahead = true;
    return 0;
  }

  // Fetch the new token into look_ahead_token
  look_ahead_token.attribute = NULL;
  int err = get_token(&look_ahead_token);
  if (err != 0) {
    scanner_err = err;
    return err;
  }

  if (look_ahead_token.type == T_EOF) {
    is_EOF = true;
  }
  valid_look_ahead = true;

  // If current wasn't set yet (first load), grab another one
  if (!valid_token) {
    current_token = look_ahead_token;
    valid_token = true;
    valid_look_ahead = false;

    if (current_token.type == T_EOF) {
      is_EOF = true;
      return 0;
    }

    look_ahead_token.attribute = NULL;
    err = get_token(&look_ahead_token);
    if (err != 0) {
      scanner_err = err;
      return err;
    }
    if (look_ahead_token.type == T_EOF) {
      is_EOF = true;
    }
    valid_look_ahead = true;
  }

  return 0;
}

int parser_init(void) {
  current_token.attribute = NULL;
  look_ahead_token.attribute = NULL;
  valid_token = false;
  valid_look_ahead = false;
  is_EOF = false;
  scanner_err = 0;
  return advance_token();
}

void parser_cleanup(void) {
  if (valid_token) {
    free_token(&current_token);
    valid_token = false;
  }
  if (valid_look_ahead) {
    free_token(&look_ahead_token);
    valid_look_ahead = false;
  }
  is_EOF = false;
  scanner_err = 0;
  consumed_dedent = false;
}

static int parse_lists(ast_node **node, int (*parse_items)(ast_node **)) {
  while (valid_token && current_token.type == T_LPAR) {
    int err = advance_token();
    if (err != 0)
      return err;
    *node = ast_create_node(NODE_LIST, TYPE_NONE, NULL);
    if (!*node)
      return 99;
    err = parse_items(&(*node)->left);
    if (err != 0)
      return err;
    if (!match_tokens(T_RPAR))
      return parser_or_scanner_err();
    node = &(*node)->right;
  }
  return 0;
}

static int parse_member(ast_node **receiver) {
  int err = advance_token();
  if (err != 0)
    return err;
  if (!valid_token || current_token.type != T_ID)
    return parser_or_scanner_err();
  char *name = strdup(current_token.attribute);
  if (!name)
    return 99;
  ast_node *member = ast_create_node(NODE_MEMBER, TYPE_NONE, name);
  if (!member) {
    free(name);
    return 99;
  }
  member->left = *receiver;
  *receiver = member;
  err = advance_token();
  if (err != 0)
    return err;
  return parse_lists(&member->right, parse_arguments);
}

static int parse_if(ast_node **node);

static bool at_contextual_keyword(const char *word) {
  return valid_token && current_token.type == T_ID &&
         strcmp(current_token.attribute, word) == 0;
}

static int parse_expression_until(ast_node **node, bool range_header) {
  if (!node) {
    return 99;
  }
  *node = NULL;

  precedence_stack_t stack;
  if (prec_stack_init(&stack, STACK_CAPACITY) != 0) {
    return 99;
  }

  stack_item_t end_item;
  memset(&end_item, 0, sizeof(end_item));
  end_item.type = S_END;
  if (prec_stack_push(&stack, end_item) != 0) {
    prec_stack_free(&stack);
    return 99;
  }

  int paren_depth = 0;
  bool block_ended = false;
  while (true) {
    if (!valid_token) {
      prec_stack_free(&stack);
      return parser_or_scanner_err();
    }

    if (current_token.type == T_DOT) {
      stack_item_t *item = &stack.items[stack.top];
      if (item->type == S_RPAR) {
        int err = expression_reduction(&stack);
        if (err != 0) {
          prec_stack_free(&stack);
          return err;
        }
        item = &stack.items[stack.top];
      }
      if ((item->type != S_OPERAND && item->type != S_EXP) || !item->node) {
        prec_stack_free(&stack);
        return 2;
      }
      int err = parse_member(&item->node);
      if (err != 0) {
        prec_stack_free(&stack);
        return err;
      }
      continue;
    }

    bool range_stop =
        range_header && paren_depth == 0 &&
        (at_contextual_keyword("to") || at_contextual_keyword("until") ||
         at_contextual_keyword("by") || current_token.type == T_IF) &&
        (stack.items[stack.top].type == S_OPERAND ||
         stack.items[stack.top].type == S_EXP ||
         stack.items[stack.top].type == S_RPAR);
    bool is_stopper = block_ended || range_stop ||
                      is_expression_stopper(current_token.type) ||
                      current_token.type == T_COMMA ||
                      (current_token.type == T_RPAR && paren_depth == 0);
    symbol_type_t input_sym;
    if (is_stopper) {
      input_sym = S_END;
    } else {
      input_sym = current_token.type == T_IF
                      ? S_OPERAND
                      : token_to_symbol_type(current_token.type);
      if (current_token.type == T_MINUS &&
          stack.items[stack.top].type != S_EXP &&
          stack.items[stack.top].type != S_OPERAND &&
          stack.items[stack.top].type != S_RPAR) {
        input_sym = S_PREFIX;
      }
    }

    // If not a stopper and token_to_symbol_type fell back to S_END, it's an
    // invalid token for an expression
    if (!is_stopper && input_sym == S_END) {
      prec_stack_free(&stack);
      return 2;
    }

    stack_item_t top_term;
    if (prec_stack_top_terminal(&stack, &top_term) != 0) {
      prec_stack_free(&stack);
      return 2;
    }

    // End of expression reached and only $ (+ optional single S_EXP) remains on
    // stack
    if (is_stopper && top_term.type == S_END) {
      if (stack.top == 1 && stack.items[1].type == S_EXP &&
          stack.items[1].node != NULL) {
        *node = stack.items[1].node;
        stack.items[1].node = NULL;
        prec_stack_free(&stack);
        return 0;
      }
      prec_stack_free(&stack);
      return 2;
    }

    int on_top = symbol_type_to_int(&(top_term).type);
    int incoming = symbol_type_to_int(&input_sym);

    if (on_top == -1 || incoming == -1) {
      prec_stack_free(&stack);
      return 2;
    }

    precedence_type action = precedence_table[on_top][incoming];

    if (action == P_PUSH || action == P_EQ) {
      stack_item_t new_item = {0};
      int err;
      if (current_token.type == T_IF) {
        new_item.type = S_OPERAND;
        err = parse_if(&new_item.node);
        block_ended = paren_depth == 0 && consumed_dedent;
      } else if (current_token.type == T_ID && valid_look_ahead &&
                 look_ahead_token.type == T_LPAR) {
        new_item.type = S_OPERAND;
        err = parse_fun_call(&new_item.node);
      } else if (build_stack_item(&current_token, input_sym, &new_item) != 0) {
        err = 99;
      } else {
        if (input_sym == S_LPAR)
          paren_depth++;
        if (input_sym == S_RPAR)
          paren_depth--;
        err = advance_token();
      }
      if (err != 0) {
        ast_destroy(new_item.node);
        free(new_item.token.attribute);
        prec_stack_free(&stack);
        return err;
      }
      if (prec_stack_push(&stack, new_item) != 0) {
        if (new_item.node) {
          ast_destroy(new_item.node);
        }
        free(new_item.token.attribute);
        prec_stack_free(&stack);
        return 99;
      }
    } else if (action == P_POP) {
      int err = expression_reduction(&stack);
      if (err != 0) {
        prec_stack_free(&stack);
        return err;
      }
    } else {
      prec_stack_free(&stack);
      return 2;
    }
  }
}

int parse_expression(ast_node **node) {
  return parse_expression_until(node, false);
}

int parse_arguments(ast_node **node) {
  *node = NULL;
  if (valid_token && current_token.type == T_RPAR)
    return 0;

  ast_node **next = node;
  int err;
  while (true) {
    *next = ast_create_node(NODE_ARGUMENT, TYPE_NONE, NULL);
    if (!*next) {
      err = 99;
      break;
    }
    err = parse_expression(&(*next)->right);
    if (err != 0)
      break;

    if (!valid_token || current_token.type != T_COMMA)
      return 0;
    err = advance_token();
    if (err != 0)
      break;
    if (valid_token && current_token.type == T_RPAR &&
        current_token.newline_before)
      return 0;

    next = &(*next)->left;
  }
  ast_destroy(*node);
  *node = NULL;
  return err;
}

int parse_fun_call(ast_node **node) {
  *node = NULL;

  if (!valid_token || !current_token.attribute) {
    return parser_or_scanner_err();
  }

  char *fun_call_id = strdup(current_token.attribute);
  if (!fun_call_id) {
    return 99;
  }

  *node = ast_create_node(NODE_FUN_CALL, TYPE_ID, fun_call_id);
  if (!*node) {
    free(fun_call_id);
    return 99;
  }
  if (!match_tokens(T_ID)) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }
  if (!valid_token || current_token.type != T_LPAR) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }
  int err = parse_lists(&(*node)->left, parse_arguments);
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    return err;
  }
  return 0;
}

int parse_assignment(ast_node **node) {
  *node = NULL;

  if (!valid_token || current_token.type != T_ID || !current_token.attribute) {
    return parser_or_scanner_err();
  }

  char *assign_id = strdup(current_token.attribute);
  if (!assign_id) {
    return 99;
  }

  *node = ast_create_node(NODE_ASSIGNMENT, TYPE_NONE, NULL);
  if (!*node) {
    free(assign_id);
    return 99;
  }

  (*node)->left = ast_create_node(NODE_TERM, TYPE_ID, assign_id);
  if (!(*node)->left) {
    ast_destroy(*node);
    *node = NULL;
    free(assign_id);
    return 99;
  }

  if (!match_tokens(T_ID)) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }
  if (!match_tokens(T_ASSIGN)) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }

  if (starts_expression(current_token.type)) {
    int err = parse_expression(&(*node)->right);
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
  } else {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }

  return 0;
}
int parse_var_val(ast_node **node) {
  *node = NULL;

  if (current_token.type == T_VAL) {
    *node = ast_create_node(NODE_VAL_DEF, TYPE_ID, NULL);
    if (!*node) {
      return 99;
    }
  } else if (current_token.type == T_VAR) {
    *node = ast_create_node(NODE_VAR_DEF, TYPE_ID, NULL);
    if (!*node) {
      return 99;
    }
  } else {
    return parser_or_scanner_err();
  }

  int err = advance_token();
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    return err;
  }

  if (!valid_token || current_token.type != T_ID || !current_token.attribute) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }

  char *var_val = strdup(current_token.attribute);
  if (!var_val) {
    ast_destroy(*node);
    *node = NULL;
    return 99;
  }
  (*node)->value = var_val;

  if (!match_tokens(T_ID)) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }

  err = parse_type_opt(&((*node)->left));
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    return err;
  }

  if (!match_tokens(T_ASSIGN)) {
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
  }

  err = parse_expression(&((*node)->right));
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    return err;
  }

  return 0;
}

// Branches share a block representation in the semantic pass and code
// generator.
static int parse_branch(ast_node **node) {
  if (valid_token && current_token.type == T_EOL) {
    if (!match_tokens(T_EOL) || !match_tokens(T_INDENT))
      return parser_or_scanner_err();
    int err = parse_block(node);
    if (err != 0)
      return err;
    return match_tokens(T_DEDENT) ? 0 : parser_or_scanner_err();
  }

  *node = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
  if (!*node)
    return 99;
  if (valid_token && current_token.type == T_IF)
    return parse_if(&(*node)->left);
  return parse_expression(&(*node)->left);
}

static int parse_if(ast_node **node) {
  *node = ast_create_node(NODE_IF, TYPE_NONE, NULL);
  if (!*node)
    return 99;

  int err = advance_token();
  if (err == 0)
    err = parse_expression(&(*node)->left);
  if (err == 0 && !match_tokens(T_THEN))
    err = parser_or_scanner_err();
  if (err == 0) {
    (*node)->right = ast_create_node(NODE_IF_BODY, TYPE_NONE, NULL);
    if (!(*node)->right)
      err = 99;
  }
  if (err == 0)
    err = parse_branch(&(*node)->right->left);
  if (err == 0 && valid_token && current_token.type == T_ELSE) {
    err = advance_token();
    if (err == 0)
      err = parse_branch(&(*node)->right->right);
  }
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
  }
  return err;
}

// Each header item owns the expression on its left and the next item on its
// right.
static int parse_range_item(ast_node **item) {
  *item = ast_create_node(NODE_LIST, TYPE_NONE, NULL);
  if (!*item)
    return 99;
  return parse_expression_until(&(*item)->left, true);
}

static int parse_for(ast_node **node) {
  int err = advance_token();
  if (err != 0)
    return err;

  // Multi-line header: generator and guards are on indented lines
  // and 'do' is aligned with 'for' (CONTROL extension)
  bool multiline = valid_token && current_token.type == T_EOL;
  if (multiline && (!match_tokens(T_EOL) || !match_tokens(T_INDENT)))
    return parser_or_scanner_err();

  if (!valid_token || current_token.type != T_ID)
    return parser_or_scanner_err();
  char *name = strdup(current_token.attribute);
  if (!name)
    return 99;
  *node = ast_create_node(NODE_FOR, TYPE_NONE, name);
  if (!*node) {
    free(name);
    return 99;
  }

  if (!match_tokens(T_ID) || !match_tokens(T_ARROW_LEFT))
    return parser_or_scanner_err();
  err = parse_range_item(&(*node)->left);
  if (err != 0)
    return err;
  if (!at_contextual_keyword("to") && !at_contextual_keyword("until"))
    return parser_or_scanner_err();

  (*node)->inclusive_range = at_contextual_keyword("to");
  err = advance_token();
  ast_node **end = &(*node)->left->right;
  if (err == 0)
    err = parse_range_item(end);
  if (err != 0)
    return err;

  // Keep a slot for the optional step before the guard list.
  ast_node *step = ast_create_node(NODE_LIST, TYPE_NONE, NULL);
  if (!step)
    return 99;
  (*end)->right = step;
  if (at_contextual_keyword("by")) {
    err = advance_token();
    if (err == 0)
      err = parse_expression_until(&step->left, true);
    if (err != 0)
      return err;
  }

  ast_node **guard = &step->right;
  while (true) {
    // In a multi-line header each guard may start on its own line
    if (multiline && valid_token && current_token.type == T_EOL &&
        valid_look_ahead && look_ahead_token.type == T_IF) {
      err = advance_token();
      if (err != 0)
        return err;
    }
    if (!valid_token || current_token.type != T_IF)
      break;
    err = advance_token();
    if (err == 0)
      err = parse_range_item(guard);
    if (err != 0)
      return err;
    guard = &(*guard)->right;
  }
  if (multiline && (!match_tokens(T_EOL) || !match_tokens(T_DEDENT)))
    return parser_or_scanner_err();
  if (!match_tokens(T_DO))
    return parser_or_scanner_err();
  return parse_branch(&(*node)->right);
}

int parse_block(ast_node **node) {
  *node = NULL;

  int err = skip_eol();
  if (err != 0) {
    return err;
  }

  ast_node **last = node;

  while (valid_token && current_token.type != T_DEDENT) {
    *last = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
    if (!*last) {
      ast_destroy(*node);
      *node = NULL;
      return 99;
    }
    // VARiable and VALue definition
    if (valid_token &&
        (current_token.type == T_VAL || current_token.type == T_VAR)) {
      err = parse_var_val(&((*last)->left));
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
    }

    // RETURN statement
    else if (valid_token && current_token.type == T_RETURN) {
      err = advance_token();
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }

      (*last)->left = ast_create_node(NODE_RETURN, TYPE_NONE, NULL);
      if (!(*last)->left) {
        ast_destroy(*node);
        *node = NULL;
        return 99;
      }

      if (valid_token && !is_expression_stopper(current_token.type)) {
        err = parse_expression(&(*last)->left->left);
        if (err != 0) {
          ast_destroy(*node);
          *node = NULL;
          return err;
        }
      }
    }

    else if (valid_token && current_token.type == T_FOR) {
      err = parse_for(&(*last)->left);
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
    }

    // IF statement
    else if (valid_token && current_token.type == T_IF) {
      err = parse_if(&(*last)->left);
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
    }

    // WHILE loop
    else if (valid_token && current_token.type == T_WHILE) {
      err = advance_token();
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }

      (*last)->left = ast_create_node(NODE_WHILE, TYPE_NONE, NULL);
      if (!(*last)->left) {
        ast_destroy(*node);
        *node = NULL;
        return 99;
      }

      err = parse_expression(&(*last)->left->left);
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
      if (!match_tokens(T_DO)) {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
      }
      if (!match_tokens(T_EOL)) {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
      }
      if (!match_tokens(T_INDENT)) {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
      }

      err = parse_block(&(*last)->left->right);
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
      if (!match_tokens(T_DEDENT)) {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
      }
    }

    // ID + look_ahead
    else if (valid_token && current_token.type == T_ID) {

      // ASSIGNMENT
      if (valid_look_ahead && look_ahead_token.type == T_ASSIGN) {
        err = parse_assignment(&((*last)->left));
        if (err != 0) {
          ast_destroy(*node);
          *node = NULL;
          return err;
        }
      }

      else {
        err = parse_expression(&((*last)->left));
        if (err != 0) {
          ast_destroy(*node);
          *node = NULL;
          return err;
        }
      }

    }

    // EXPRESSION
    else if (valid_token && starts_expression(current_token.type)) {
      err = parse_expression(&((*last)->left));
      if (err != 0) {
        ast_destroy(*node);
        *node = NULL;
        return err;
      }
    }

    else {
      ast_destroy(*node);
      *node = NULL;
      return parser_or_scanner_err();
    }

    last = &((*last)->right);
    if (valid_token && current_token.type == T_SEMICOLON) {
      err = advance_token();
    }
    if (err == 0)
      err = skip_eol();
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
  }
  return 0;
}

int parse_fun_body(ast_node **node) {
  *node = NULL;
  if (valid_token && current_token.type == T_EOL) {
    int err = advance_token();
    if (err != 0) {
      return err;
    }

    if (!match_tokens(T_INDENT)) {
      return parser_or_scanner_err();
    }

    err = parse_block(node);
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }

    if (!match_tokens(T_DEDENT)) {
      ast_destroy(*node);
      *node = NULL;
      return parser_or_scanner_err();
    }
  } else if (starts_expression(current_token.type)) {
    int err = parse_expression(node);
    if (err != 0) {
      return err;
    }
  } else {
    return parser_or_scanner_err();
  }
  return 0;
}

int parse_type_opt(ast_node **node) {
  *node = NULL;

  if (valid_token && current_token.type == T_ASSIGN) {
    return 0;
  }

  if (valid_token && current_token.type == T_COLON) {
    int err = advance_token();
    if (err != 0) {
      return err;
    }

    if (!is_type_token(current_token.type)) {
      return parser_or_scanner_err();
    }

    data_type_t type_tmp = token_type_to_node_type(current_token.type);
    *node = ast_create_node(NODE_TYPE, type_tmp, NULL);
    if (!*node) {
      return 99;
    }

    err = advance_token();
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
  }
  return 0;
}

int parse_param(ast_node **node) {
  *node = NULL;

  // Empty
  if (valid_token && current_token.type == T_RPAR) {
    return 0;
  }

  // Single parameter
  if (!valid_token || !current_token.attribute) {
    return parser_or_scanner_err();
  }
  char *param_id = strdup(current_token.attribute);
  if (!param_id) {
    return 99;
  }

  if (!match_tokens(T_ID)) {
    free(param_id);
    return parser_or_scanner_err();
  }
  if (!match_tokens(T_COLON)) {
    free(param_id);
    return parser_or_scanner_err();
  }
  if (!is_type_token(current_token.type)) {
    free(param_id);
    return parser_or_scanner_err();
  }

  data_type_t param_type = token_type_to_node_type(current_token.type);
  *node = ast_create_node(NODE_PARAMETER, param_type, param_id);
  if (!*node) {
    free(param_id);
    return 99;
  }

  ast_node *last = *node;
  int err = advance_token();
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    return err;
  }

  // Multiple parameters
  while (valid_token && current_token.type == T_COMMA) {
    err = advance_token();
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
    if (valid_token && current_token.type == T_RPAR &&
        current_token.newline_before) {
      break;
    }

    if (!valid_token || !current_token.attribute) {
      ast_destroy(*node);
      *node = NULL;
      return parser_or_scanner_err();
    }

    param_id = strdup(current_token.attribute);
    if (!param_id) {
      ast_destroy(*node);
      *node = NULL;
      return 99;
    }

    if (!match_tokens(T_ID)) {
      ast_destroy(*node);
      *node = NULL;
      free(param_id);
      return parser_or_scanner_err();
    }
    if (!match_tokens(T_COLON)) {
      ast_destroy(*node);
      *node = NULL;
      free(param_id);
      return parser_or_scanner_err();
    }
    if (!is_type_token(current_token.type)) {
      ast_destroy(*node);
      *node = NULL;
      free(param_id);
      return parser_or_scanner_err();
    }

    param_type = token_type_to_node_type(current_token.type);
    last->left = ast_create_node(NODE_PARAMETER, param_type, param_id);
    if (!last->left) {
      ast_destroy(*node);
      *node = NULL;
      free(param_id);
      return 99;
    }

    last = last->left;
    err = advance_token();
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
  }
  return 0;
}

int parse_fun_def(ast_node **node) {
  *node = NULL;
  ast_node *definition = NULL;

  if (valid_token && current_token.type == T_AT_MAIN) {
    int err = advance_token();
    if (err != 0) {
      return err;
    }
    err = skip_eol();
    if (err != 0) {
      return err;
    }
  }
  if (!match_tokens(T_DEF)) {
    return parser_or_scanner_err();
  }
  if (!valid_token || current_token.type != T_ID) {
    return parser_or_scanner_err();
  }

  if (!current_token.attribute) {
    return parser_or_scanner_err();
  }
  char *fun_id = strdup(current_token.attribute);
  if (!fun_id) {
    return 99;
  }

  definition = ast_create_node(NODE_FUN_DEF, TYPE_ID, fun_id);
  if (!definition) {
    free(fun_id);
    return 99;
  }

  if (!match_tokens(T_ID)) {
    ast_destroy(definition);
    return parser_or_scanner_err();
  }

  int err = parse_lists(&definition->left, parse_param);
  if (err != 0) {
    ast_destroy(definition);
    return err;
  }

  definition->right = ast_create_node(NODE_FUN_DEF, TYPE_NONE, NULL);
  if (!definition->right) {
    ast_destroy(definition);
    return 99;
  }

  err = parse_type_opt(&definition->right->left);
  if (err != 0) {
    ast_destroy(definition);
    return err;
  }

  if (!match_tokens(T_ASSIGN)) {
    ast_destroy(definition);
    return parser_or_scanner_err();
  }

  err = parse_fun_body(&definition->right->right);
  if (err != 0) {
    ast_destroy(definition);
    return err;
  }

  *node = definition;
  return 0;
}

int parse_function(ast_node **node) {
  *node = NULL;
  ast_node **last = node;
  ast_node *tmp = NULL;
  while (start_of_function_definition()) {
    if (current_token.type == T_AT_MAIN) {
      *last = ast_create_node(NODE_MAIN_FUNCTION, TYPE_NONE, NULL);
    } else {
      *last = ast_create_node(NODE_FUNCTION, TYPE_NONE, NULL);
    }
    if (!*last) {
      ast_destroy(*node);
      *node = NULL;
      return 99;
    }

    int err = parse_fun_def(&tmp);
    if (err != 0) {
      ast_destroy(tmp);
      ast_destroy(*last);
      *last = NULL;
      return err;
    }
    (*last)->left = tmp;
    last = &((*last)->right);
    tmp = NULL;

    err = skip_eol();
    if (err != 0) {
      ast_destroy(*node);
      *node = NULL;
      return err;
    }
  }
  return 0;
}

int parse_prolog(void) {
  if (!match_tokens(T_IMPORT)) {
    return parser_or_scanner_err();
  }

  if (!current_token.attribute) {
    return parser_or_scanner_err();
  }
  if (current_token.type != T_ID ||
      strcmp(current_token.attribute, "ifj26") != 0) {
    return parser_or_scanner_err();
  }
  int err = advance_token();
  if (err != 0) {
    return err;
  }

  if (!match_tokens(T_DOT)) {
    return parser_or_scanner_err();
  }

  if (!match_tokens(T_MUL)) {
    return parser_or_scanner_err();
  }

  return 0;
}

int parse_program(ast_node **node) {
  *node = NULL;

  int err = parser_init();
  if (err != 0) {
    parser_cleanup();
    return err;
  }

  err = parse_prolog();
  if (err != 0) {
    parser_cleanup();
    return err;
  }

  err = skip_whitespace();
  if (err != 0) {
    parser_cleanup();
    return err;
  }

  err = parse_function(node);
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    parser_cleanup();
    return err;
  }

  err = skip_whitespace();
  if (err != 0) {
    ast_destroy(*node);
    *node = NULL;
    parser_cleanup();
    return err;
  }

  if (!match_tokens(T_EOF)) {
    ast_destroy(*node);
    *node = NULL;
    parser_cleanup();
    return parser_or_scanner_err();
  }

  parser_cleanup();
  return 0;
}