#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"

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

data_type_t token_type_to_node_type(Token_type type)
{
    switch (type) {
        case T_ID: return TYPE_ID;
        case T_DOUBLE: return TYPE_DOUBLE;
        case T_INT: return TYPE_INT;
        case T_STRING: return TYPE_STRING;
        case T_UNIT: return TYPE_UNIT;
        case T_TRUE:
        case T_FALSE: return TYPE_BOOL_LITERAL;
        case T_LIT_DOUBLE: return TYPE_DOUBLE;
        case T_LIT_INT: return TYPE_INT;
        case T_LIT_STRING: return TYPE_STRING;
        default: return TYPE_NONE;
    }
}

int parser_or_scanner_err(void)
{
    if (scanner_err != 0) {
        return scanner_err;
    }
    return 2;
}

/* Skips T_EOL tokens */
int skip_eol(void) 
{
    while (valid_token && current_token.type == T_EOL) {
        int err = advance_token();
        if (err != 0) {
            return err;
        }
    }
    return 0;
}

/* Skips T_EOL, T_INDENT, and T_DEDENT tokens */
int skip_whitespace(void) 
{
    while (valid_token &&
            (current_token.type == T_EOL || 
            current_token.type == T_INDENT || 
            current_token.type == T_DEDENT)) {
        int err = advance_token();
        if (err != 0) {
            return err;
        }
    }
    return 0;
}

bool is_type_token(Token_type type) 
{
    return (type == T_INT || type == T_DOUBLE || type == T_STRING || type == T_UNIT);
}

bool is_operand_token(Token_type type) 
{
    return (type == T_ID || type == T_LIT_INT || type == T_LIT_DOUBLE || type == T_LIT_STRING);
}

static bool starts_expression(Token_type type)
{
    return is_operand_token(type) || type == T_LPAR || type == T_MINUS ||
           type == T_NOT || type == T_TRUE || type == T_FALSE;
}

bool start_of_function_definition(void) 
{
    if (current_token.type == T_AT_MAIN || current_token.type == T_DEF) {
        return true;
    }
    return false;
}

/* Check if current and expected are the same Token_type
 * If yes advance to next token
 */
bool match_tokens(Token_type expected) 
{
    if (valid_token && current_token.type == expected) {
        if (advance_token() != 0) { return false; }
        return true;
    }
    return false;
}

/* Frees previous token
 * Reads next token
 * Returns 0 if success, 1 if fail
 */
int advance_token(void) 
{
    // Free old current token if valid
    if (valid_token) 
    {
        free_token(&current_token);
        valid_token = false;
    }

    // Move look_ahead_token into current_token if valid
    if (valid_look_ahead) 
    {
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
    if (err != 0) 
    {
        scanner_err = err;
        return err;
    }
    
    if (look_ahead_token.type == T_EOF) 
    { 
        is_EOF = true; 
    }
    valid_look_ahead = true;

    // If current wasn't set yet (first load), grab another one
    if (!valid_token) 
    {
        current_token = look_ahead_token;
        valid_token = true;
        valid_look_ahead = false;
        
        if (current_token.type == T_EOF) 
        {
            is_EOF = true;
            return 0;
        }

        look_ahead_token.attribute = NULL;
        err = get_token(&look_ahead_token);
        if (err != 0)
        {
            scanner_err = err;
            return err;
        }
        if (look_ahead_token.type == T_EOF) 
        { 
            is_EOF = true; 
        }
        valid_look_ahead = true;
    }

    return 0;
}

int parser_init(void)
{
    current_token.attribute = NULL;
    look_ahead_token.attribute = NULL;
    valid_token = false;
    valid_look_ahead = false;
    is_EOF = false;
    scanner_err = 0;
    return advance_token();
}

void parser_cleanup(void) 
{
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
}

static OperatorType binary_operator(Token_type token)
{
    switch (token) {
        case T_OR: return OP_OR;
        case T_AND: return OP_AND;
        case T_EQ: return OP_EQ;
        case T_NEQ: return OP_NEQ;
        case T_LESS_T: return OP_LT;
        case T_LESS_OR_EQ: return OP_LE;
        case T_GREATER_T: return OP_GT;
        case T_GREATER_OR_EQ: return OP_GE;
        case T_PLUS: return OP_ADD;
        case T_MINUS: return OP_SUB;
        case T_MUL: return OP_MUL;
        case T_DIV: return OP_DIV;
        default: return OP_NONE;
    }
}

static int operator_precedence(OperatorType op)
{
    switch (op) {
        case OP_OR: return 1;
        case OP_AND: return 2;
        case OP_EQ:
        case OP_NEQ: return 3;
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE: return 4;
        case OP_ADD:
        case OP_SUB: return 5;
        case OP_MUL:
        case OP_DIV: return 6;
        default: return 0;
    }
}

static int parse_binary_expression(ast_node **node, int min_precedence);

static int parse_expression_operand(ast_node **node)
{
    *node = NULL;
    if (!valid_token) return parser_or_scanner_err();

    Token_type type = current_token.type;
    if (type == T_LPAR) {
        int err = advance_token();
        if (err != 0) return err;

        if (valid_token && current_token.type == T_RPAR) {
            err = advance_token();
            if (err != 0) return err;
            *node = ast_create_node(NODE_TERM, TYPE_UNIT, NULL);
            return *node ? 0 : 99;
        }

        err = parse_binary_expression(node, 1);
        if (err != 0) return err;
        if (!match_tokens(T_RPAR)) {
            ast_destroy(*node);
            *node = NULL;
            return parser_or_scanner_err();
        }
        return 0;
    }

    if (type == T_MINUS || type == T_NOT) {
        int err = advance_token();
        if (err != 0) return err;

        ast_node *unary = ast_create_node(NODE_UNARY_OP, TYPE_NONE, NULL);
        if (!unary) return 99;
        unary->op = type == T_MINUS ? OP_NEG : OP_NOT;
        err = parse_expression_operand(&unary->left);
        if (err != 0) {
            ast_destroy(unary);
            return err;
        }
        *node = unary;
        return 0;
    }

    if (!is_operand_token(type) && type != T_TRUE && type != T_FALSE) {
        return parser_or_scanner_err();
    }

    const char *attribute = current_token.attribute;
    if (type == T_TRUE) attribute = "true";
    if (type == T_FALSE) attribute = "false";
    if (!attribute) return parser_or_scanner_err();
    char *value = strdup(attribute);
    if (!value) return 99;

    ast_node *term = ast_create_node(NODE_TERM, token_type_to_node_type(type), value);
    if (!term) {
        free(value);
        return 99;
    }
    int err = advance_token();
    if (err != 0) {
        ast_destroy(term);
        return err;
    }
    *node = term;
    return 0;
}

static int parse_binary_expression(ast_node **node, int min_precedence)
{
    ast_node *left = NULL;
    *node = NULL;
    int err = parse_expression_operand(&left);
    if (err != 0) return err;

    while (valid_token) {
        OperatorType op = binary_operator(current_token.type);
        int precedence = operator_precedence(op);
        if (precedence < min_precedence) break;

        ast_node *binary = ast_create_node(NODE_BINARY_OP, TYPE_NONE, NULL);
        if (!binary) {
            ast_destroy(left);
            return 99;
        }
        binary->op = op;
        binary->left = left;
        err = advance_token();
        if (err == 0) {
            err = parse_binary_expression(&binary->right, precedence + 1);
        }
        if (err != 0) {
            ast_destroy(binary);
            return err;
        }
        left = binary;
    }
    *node = left;
    return 0;
}

int parse_expression(ast_node **node)
{
    int err = parse_binary_expression(node, 1);
    if (err != 0) return err;
    if (valid_token && (current_token.type == T_EOL || current_token.type == T_EOF ||
                        current_token.type == T_SEMICOLON || current_token.type == T_DEDENT)) {
        return 0;
    }
    ast_destroy(*node);
    *node = NULL;
    return parser_or_scanner_err();
}

int parse_condition(ast_node **node)
{
    return parse_binary_expression(node, 1);
}

int parse_arguments(ast_node **node) 
{
    *node = NULL;

    // Empty
    if (valid_token && current_token.type == T_RPAR) { return 0; }

    // Single argument
    if (!valid_token || !is_operand_token(current_token.type) || !current_token.attribute) { return parser_or_scanner_err(); }
    char *arg_id = strdup(current_token.attribute);
    if (!arg_id) { return 99; }

    data_type_t arg_type = token_type_to_node_type(current_token.type);
    *node = ast_create_node(NODE_ARGUMENT, arg_type, arg_id);
    if (!*node) {
        free(arg_id);
        return 99;
    }

    int err = advance_token();
    if (err != 0)
    {
        ast_destroy(*node);
        *node = NULL;
        return err;
    }


    ast_node *last = *node;
    
    // Multiple arguments    
    while (valid_token && current_token.type == T_COMMA) 
    {
        err = advance_token();
        if (err != 0) {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
        if (valid_token && current_token.type == T_RPAR && current_token.newline_before) {
            break;
        }

        if (!valid_token || !is_operand_token(current_token.type) || !current_token.attribute) 
        {
            ast_destroy(*node);
            *node = NULL;
            return parser_or_scanner_err();
        }

        arg_id = strdup(current_token.attribute);
        if (!arg_id) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 99;
        }

        arg_type = token_type_to_node_type(current_token.type);
        last->left = ast_create_node(NODE_ARGUMENT, arg_type, arg_id);
        if (!last->left) 
        {
            free(arg_id);
            ast_destroy(*node);
            *node = NULL;
            return 99;
        }

        err = advance_token();
        if (err != 0) {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
        last = last->left;
    }
    return 0;
}

int parse_fun_call(ast_node **node) 
{
    *node = NULL;

    if (!valid_token || !current_token.attribute) { return  parser_or_scanner_err(); }

    char *fun_call_id = strdup(current_token.attribute);
    if (!fun_call_id) { return 99; }

    *node = ast_create_node(NODE_FUN_CALL, TYPE_ID, fun_call_id);
    if (!*node) 
    {
        free(fun_call_id);
        return 99;
    }
    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err(); 
    }
    if (!match_tokens(T_LPAR)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err(); 
    }

    int err = parse_arguments(&(*node)->left);
    if (err != 0) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return err; 
    }

    if (!match_tokens(T_RPAR)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err(); 
    }
    return 0;
}

int parse_assignment(ast_node **node) 
{
    *node = NULL;

    if (!valid_token || current_token.type != T_ID || !current_token.attribute) { return parser_or_scanner_err(); }

    char *assign_id = strdup(current_token.attribute);
    if (!assign_id) { return 99; }

    *node = ast_create_node(NODE_ASSIGNMENT, TYPE_NONE, NULL);
    if (!*node)
    {
        free(assign_id);
        return 99;
    }

    (*node)->left = ast_create_node(NODE_TERM, TYPE_ID, assign_id);
    if (!(*node)->left)
    {
        ast_destroy(*node);
        *node = NULL;
        free(assign_id);
        return 99;
    }

    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err(); 
    }
    if (!match_tokens(T_ASSIGN)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err(); 
    }
    if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR) 
    {
        int err = parse_fun_call(&(*node)->right);
        if (err != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
    }

    else if (starts_expression(current_token.type))
    {
        int err = parse_expression(&(*node)->right);
        if (err != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
    }
    else 
    {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
    }

    return 0;
}
int parse_var_val(ast_node **node)
{
    *node = NULL;

    if (current_token.type == T_VAL)
    {
        *node = ast_create_node(NODE_VAL_DEF, TYPE_ID, NULL);
        if (!*node) { return 99; }
    }
    else if (current_token.type == T_VAR)
    {
        *node = ast_create_node(NODE_VAR_DEF, TYPE_ID, NULL);
        if (!*node) { return 99; }
    }
    else { return parser_or_scanner_err(); }

    int err = advance_token();
    if (err != 0)
    {
        ast_destroy(*node);
        *node = NULL;
        return err;
    }

    if (!valid_token || current_token.type != T_ID || !current_token.attribute) 
    {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
    }

    char *var_val = strdup(current_token.attribute);
    if (!var_val)
    {
        ast_destroy(*node);
        *node = NULL;
        return 99;
    }
    (*node)->value = var_val;

    if (!match_tokens(T_ID)) 
    {
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
    }

    err = parse_type_opt(&((*node)->left));
    if (err != 0) 
    {
        ast_destroy(*node);
        *node = NULL;
        return err;
    }

    if (!match_tokens(T_ASSIGN)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return parser_or_scanner_err();
    }

    if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR)
    {
        err = parse_fun_call(&((*node)->right));
        if (err != 0)
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
    }
    else
    {
        err = parse_expression(&((*node)->right));
        if (err != 0)
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
    }
    
    return 0;
}

int parse_block(ast_node **node) 
{
    *node = NULL;

    int err = skip_eol();
    if (err != 0) { return err; }

    ast_node **last = node;

    while (valid_token && current_token.type != T_DEDENT) 
    {
        *last = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
        if (!*last)
        {
            ast_destroy(*node);
            *node = NULL;
            return 99;
        }
        // VARiable and VALue definition
        if (valid_token && (current_token.type == T_VAL || current_token.type == T_VAR)) 
        {
            err = parse_var_val(&((*last)->left));
            if (err != 0)
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
        }

        // RETURN statement
        else if (valid_token && current_token.type == T_RETURN) 
        {
            err = advance_token();
            if (err != 0) {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }

            (*last)->left = ast_create_node(NODE_RETURN, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 99;
            }

            if (valid_token && current_token.type != T_EOL &&
                current_token.type != T_SEMICOLON && current_token.type != T_DEDENT &&
                current_token.type != T_EOF)
            {
                if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR)
                {
                    err = parse_fun_call(&(*last)->left->left);
                    if (err != 0) 
                    {
                        ast_destroy(*node);
                        *node = NULL;
                        return err;
                    }
                }
                else
                {
                    err = parse_expression(&(*last)->left->left);
                    if (err != 0) 
                    {
                        ast_destroy(*node);
                        *node = NULL;
                        return err;
                    }
                }
            }
        }

        // IF statement
        else if (valid_token && current_token.type == T_IF) 
        {
            err = advance_token();
            if (err != 0) {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }

            (*last)->left = ast_create_node(NODE_IF, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 99;
            }

            err = parse_condition(&(*last)->left->left);
            if (err != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
            if (!match_tokens(T_THEN)) 
            { 
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_EOL)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }

            (*last)->left->right = ast_create_node(NODE_IF_BODY, TYPE_NONE, NULL);
            if (!(*last)->left->right) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 99;
            }

            err = parse_block(&(*last)->left->right->left);
            if (err != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_ELSE)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_EOL))
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }

            err = parse_block(&(*last)->left->right->right);
            if (err != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
        }

        // WHILE loop
        else if (valid_token && current_token.type == T_WHILE) 
        {
            err = advance_token();
            if (err != 0) {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }

            (*last)->left = ast_create_node(NODE_WHILE, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 99;
            }

            err = parse_condition(&(*last)->left->left);
            if (err != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
            if (!match_tokens(T_DO)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_EOL)) 
            { 
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }

            err = parse_block(&(*last)->left->right);
            if (err != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return parser_or_scanner_err();
            }
        }

        // ID + look_ahead
        else if (valid_token && current_token.type == T_ID) 
        {

            // ASSIGNMENT
            if (valid_look_ahead && look_ahead_token.type == T_ASSIGN) 
            {
                err = parse_assignment(&((*last)->left));
                if (err != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return err;
                }
            } 

            // FUN_CALL
            else if (valid_look_ahead && look_ahead_token.type == T_LPAR) 
            {
                err = parse_fun_call(&((*last)->left));
                if (err != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return err;
                }
            }

            // EXPRESION
            else 
            {
                err = parse_expression(&((*last)->left));
                if (err != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return err;
                }
            }

        }

        // EXPRESSION
        else if (valid_token && starts_expression(current_token.type))
        {
            err = parse_expression(&((*last)->left));
            if (err != 0)
            {
                ast_destroy(*node);
                *node = NULL;
                return err;
            }
        }

        else 
        {
            ast_destroy(*node);
            *node = NULL;
            return parser_or_scanner_err();
        }

        last = &((*last)->right);
        if (valid_token && current_token.type == T_SEMICOLON) {
            err = advance_token();
        }
        if (err == 0) err = skip_eol();
        if (err != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
        
    }
    return 0;
}

int parse_fun_body(ast_node **node) 
{
    *node = NULL;
    if (valid_token && current_token.type == T_EOL) 
    {
        int err = advance_token();
        if (err != 0) { return err; }

        if (!match_tokens(T_INDENT)) { return parser_or_scanner_err(); }

        err = parse_block(node);
        if (err != 0)
        { 
            ast_destroy(*node);
            *node = NULL;
            return err;
        }

        if (!match_tokens(T_DEDENT)) 
        { 
            ast_destroy(*node);
            *node = NULL;
            return parser_or_scanner_err(); 
        }
    } 
    else if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR) 
    {
        int err = parse_fun_call(node);  
        if (err != 0) { return err; } 
    }
    else if (starts_expression(current_token.type))
    {
        int err = parse_expression(node);
        if (err != 0) { return err; }
    }
    else
    {
        return parser_or_scanner_err();
    }
    return 0;
}

int parse_type_opt(ast_node **node) 
{
    *node = NULL;

    if (valid_token && current_token.type == T_ASSIGN) { return 0; }

    if (valid_token && current_token.type == T_COLON) {
        int err = advance_token();
        if (err != 0) { return err; }

        if (!is_type_token(current_token.type)) { return parser_or_scanner_err(); }

        data_type_t type_tmp = token_type_to_node_type(current_token.type);
        *node = ast_create_node(NODE_TYPE, type_tmp, NULL);
        if(!*node) { return 99; }

        err = advance_token();
        if (err != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
    }
    return 0;
}

int parse_param(ast_node **node) 
{
    *node = NULL;

    // Empty
    if (valid_token && current_token.type == T_RPAR) {
        return 0;
    }

    // Single parameter
    if (!valid_token || !current_token.attribute) { return parser_or_scanner_err(); }
    char *param_id = strdup(current_token.attribute);
    if (!param_id) { return 99; }

    if (!match_tokens(T_ID)) 
    {   
        free(param_id);
        return parser_or_scanner_err(); 
    }
    if (!match_tokens(T_COLON)) 
    {
        free(param_id); 
        return parser_or_scanner_err();
    }
    if (!is_type_token(current_token.type)) 
    { 
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
    while (valid_token && current_token.type == T_COMMA) 
    {
        err = advance_token();
        if (err != 0) {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
        if (valid_token && current_token.type == T_RPAR && current_token.newline_before) {
            break;
        }

        if (!valid_token || !current_token.attribute) 
        {
            ast_destroy(*node);
            *node = NULL;
            return parser_or_scanner_err();
        }

        param_id = strdup(current_token.attribute);
        if (!param_id) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 99;
        }

        if (!match_tokens(T_ID)) 
        {   
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return parser_or_scanner_err();
        }
        if (!match_tokens(T_COLON))
        {   
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return parser_or_scanner_err();
        }
        if (!is_type_token(current_token.type))
        {
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return parser_or_scanner_err();
        }

        param_type = token_type_to_node_type(current_token.type);
        last->left = ast_create_node(NODE_PARAMETER, param_type, param_id);
        if (!last->left) 
        {
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

int parse_fun_def(ast_node **node) 
{
    *node = NULL;
    ast_node *definition = NULL;

    if (valid_token && current_token.type == T_AT_MAIN) {
        int err = advance_token();
        if (err != 0) { return err; }
        err = skip_eol();
        if (err != 0) { return err; }
    }
    if (!match_tokens(T_DEF)) { return parser_or_scanner_err(); }
    if (!valid_token || current_token.type != T_ID) { return parser_or_scanner_err();}

    if (!current_token.attribute) { return parser_or_scanner_err(); }
    char *fun_id = strdup(current_token.attribute);
    if (!fun_id) { return 99; }

    definition = ast_create_node(NODE_FUN_DEF, TYPE_ID, fun_id);
    if (!definition) {
        free(fun_id);
        return 99;
    }

    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(definition);
        return parser_or_scanner_err(); 
    }

    if (!match_tokens(T_LPAR)) 
    {
        ast_destroy(definition);
        return parser_or_scanner_err(); 
    }

    int err = parse_param(&definition->left); 
    if (err != 0)
    { 
        ast_destroy(definition);
        return err; 
    }

    if (!match_tokens(T_RPAR)) 
    { 
        ast_destroy(definition);
        return parser_or_scanner_err(); 
    }

    definition->right = ast_create_node(NODE_FUN_DEF, TYPE_NONE, NULL);
    if (!definition->right) 
    {   
        ast_destroy(definition);
        return 99; 
    }

    err = parse_type_opt(&definition->right->left);
    if (err != 0)
    {   
        ast_destroy(definition);
        return err; 
    }
    
    if (!match_tokens(T_ASSIGN)) 
    {   
        ast_destroy(definition); 
        return parser_or_scanner_err(); 
    }

    err = parse_fun_body(&definition->right->right);
    if (err != 0)
    {
        ast_destroy(definition);
        return err;
    }

    *node = definition;
    return 0;
}

int parse_function(ast_node **node) 
{
    *node = NULL;
    ast_node **last = node;
    ast_node *tmp = NULL;
    while (start_of_function_definition()) 
    {   
        if (current_token.type == T_AT_MAIN) 
        {
            *last = ast_create_node(NODE_MAIN_FUNCTION, TYPE_NONE, NULL);
        }
        else 
        {
            *last = ast_create_node(NODE_FUNCTION, TYPE_NONE, NULL);
        }
        if (!*last)
        { 
            ast_destroy(*node);
            *node = NULL;
            return 99; 
        }

        int err = parse_fun_def(&tmp);
        if (err != 0) 
        {
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

int parse_prolog(void) 
{
    if(!match_tokens(T_IMPORT)) { return parser_or_scanner_err(); }

    if (!current_token.attribute) { return parser_or_scanner_err();}
    if(current_token.type != T_ID || strcmp(current_token.attribute, "ifj26") != 0) { return parser_or_scanner_err(); }
    int err = advance_token();
    if (err != 0) { return err;}

    if(!match_tokens(T_DOT)) { return parser_or_scanner_err(); }

    if(!match_tokens(T_MUL)) { return parser_or_scanner_err(); }

    return 0;
}

int parse_program(ast_node **node)
{   
    *node = NULL;

    int err = parser_init();
    if (err != 0) 
    {
        parser_cleanup();
        return err;
    }

    err = parse_prolog();
    if (err != 0)
    {
        parser_cleanup();
        return err;
    }

    err = skip_whitespace();
    if (err != 0) {
        parser_cleanup();
        return err;
    }

    err = parse_function(node);
    if (err != 0)
    {
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

    if (!match_tokens(T_EOF)) 
    {
        ast_destroy(*node);
        *node = NULL;
        parser_cleanup();
        return parser_or_scanner_err();
    }

    parser_cleanup();
    return 0;
}