#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "ast.h"

int skip_eol(void);

int skip_whitespace(void);

bool is_type_token(Token_type type);

bool start_of_function_definition(void); 

bool match_tokens(Token_type expected);

int advance_token(void);

int parser_init(void);

void parser_cleanup(void);

int parse_expression(ast_node **node);

int parse_condition(ast_node **node);

int parse_arguments(ast_node **node);

int parse_fun_call(ast_node **node);

int parse_assignment(ast_node **node);

int parse_var_val(ast_node **node);

int parse_block(ast_node **node);

int parse_fun_body(ast_node **node); 

int parse_type_opt(ast_node **node);

int parse_param(ast_node **node);

int parse_fun_def(ast_node **node);

int parse_function(ast_node **node);

int parse_prolog(void);

int parse_program(ast_node **node);

#endif