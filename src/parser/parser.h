#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "ast.h"
#include "pstack.h"

typedef enum {
    P_POP, // >
    P_PUSH, // <
    P_EQ, // =
    P_ERR, 
} precedence_type;

symbol_type_t check_add_sub_arity(Token_type type, precedence_stack_t *stack);

symbol_type_t token_to_symbol_type(Token_type type);

int symbol_type_to_int(symbol_type_t *type);

char *op_token_to_str(Token_type type);

int build_stack_item(Token *token, symbol_type_t type, stack_item_t *item);

int expression_reduction(precedence_stack_t *stack);

int parser_or_scanner_err(void);

data_type_t token_type_to_node_type(Token_type type);

int parser_or_scanner_err(void);

int skip_eol(void);

int skip_whitespace(void);

bool is_type_token(Token_type type);

bool start_of_function_definition(void); 

bool match_tokens(Token_type expected);

int advance_token(void);

int parser_init(void);

void parser_cleanup(void);

int parse_expression(ast_node **node);

int parse_arguments(ast_node **node);

int parse_fun_call(ast_node **node);

int parse_assignment(ast_node **node);

int parse_var_val(ast_node **node);

int parse_else(ast_node **node);

int parse_block(ast_node **node);

int parse_fun_body(ast_node **node); 

int parse_type_opt(ast_node **node);

int parse_param(ast_node **node);

int parse_fun_def(ast_node **node);

int parse_function(ast_node **node);

int parse_prolog(void);

int parse_program(ast_node **node);

#endif