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

data_type_t token_type_to_node_type(Token_type type)
{
    switch (type) {
        case T_ID: return TYPE_ID;
        case T_DOUBLE: return TYPE_DOUBLE;
        case T_INT: return TYPE_INT;
        case T_STRING: return TYPE_STRING;
        case T_UNIT: return TYPE_UNIT;
        case T_LIT_DOUBLE: return TYPE_DOUBLE;
        case T_LIT_INT: return TYPE_INT;
        case T_LIT_STRING: return TYPE_STRING;
        default: return TYPE_NONE;
    }
}

/* Skips T_EOL tokens */
void skip_eol(void) 
{
    while (current_token.type == T_EOL) {
        advance_token();
    }
}

/* Skips T_EOL, T_INDENT, and T_DEDENT tokens */
void skip_whitespace(void) 
{
    while (current_token.type == T_EOL || 
            current_token.type == T_INDENT || 
            current_token.type == T_DEDENT) {
        advance_token();
    }
}

bool is_type_token(Token_type type) 
{
    return (type == T_INT || type == T_DOUBLE || type == T_STRING || type == T_UNIT);
}

bool is_operand_token(Token_type type) 
{
    return (type == T_ID || type == T_LIT_INT || type == T_LIT_DOUBLE || type == T_LIT_STRING);
}

bool start_of_function_definition() 
{
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
        advance_token();
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
    if (get_token(&look_ahead_token) != 0) {
        return 1;
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
        if (get_token(&look_ahead_token) != 0) {
            return 1;
        }
        if (look_ahead_token.type == T_EOF) { 
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
}

int parse_expression(ast_node **node)
{
    *node = NULL;
    while (valid_token && current_token.type != T_EOL && current_token.type != T_EOF) 
    {
        if (advance_token() != 0) { return 1; }
    }
    *node = ast_create_node(NODE_EXPRESSION, TYPE_NONE, NULL);
    if (!*node) { return 2; }
    return 0;
}

int parse_condition(ast_node **node)
{   
    *node = NULL;
    while (valid_token && current_token.type != T_THEN && current_token.type != T_DO) 
    {
        if (advance_token() != 0) { return 1; }
    }
    *node = ast_create_node(NODE_CONDITION, TYPE_NONE, NULL);
    if (!*node) { return 2; }
    return 0;
}

int parse_arguments(ast_node **node) 
{
    *node = NULL;

    // Empty
    if (valid_token && current_token.type == T_RPAR) {
        return 0;
    }

    // Single argument
    if (!valid_token || !is_operand_token(current_token.type) || !current_token.attribute) { return 1; }
    char *arg_id = strdup(current_token.attribute);
    if (!arg_id) { return 2; }

    data_type_t arg_type = token_type_to_node_type(current_token.type);
    *node = ast_create_node(NODE_ARGUMENT, arg_type, arg_id);
    if (!*node) {
        free(arg_id);
        return 2;
    }

    if (!is_operand_token(current_token.type))
    {   
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }
    advance_token();

    ast_node *last = *node;
    
    // Multiple arguments    
    while (valid_token && current_token.type == T_COMMA) 
    {
        advance_token();

        if (!valid_token || !is_operand_token(current_token.type) || !current_token.attribute) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 1;
        }

        arg_id = strdup(current_token.attribute);
        if (!arg_id) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 2;
        }

        arg_type = token_type_to_node_type(current_token.type);
        last->left = ast_create_node(NODE_ARGUMENT, arg_type, arg_id);
        if (!last->left) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 2;
        }

        if (!is_operand_token(current_token.type)) 
        {   
            ast_destroy(*node);
            *node = NULL;
            return 1;
        }
        advance_token();
        last = last->left;
    }
    return 0;
}

int parse_fun_call(ast_node **node) 
{
    *node = NULL;

    if (!valid_token || !current_token.attribute) { return 1; }
    char *fun_call_id = strdup(current_token.attribute);
    if (!fun_call_id) { return 2; }

    *node = ast_create_node(NODE_FUN_CALL, TYPE_ID, fun_call_id);
    if (!*node) 
    {
        free(fun_call_id);
        return 2;
    }
    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }
    if (!match_tokens(T_LPAR)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }

    int ret_val = parse_arguments(&(*node)->left);
    if (ret_val != 0) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return ret_val; 
    }

    if (!match_tokens(T_RPAR)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }
    return 0;
}

int parse_assignment(ast_node **node) 
{
    *node = NULL;

    if (!valid_token || current_token.type != T_ID || !current_token.attribute) { return 1; }
    char *assign_id = strdup(current_token.attribute);
    if (!assign_id) { return 2; }

    *node = ast_create_node(NODE_ASSIGNMENT, TYPE_NONE, NULL);
    if (!*node)
    {
        free(assign_id);
        return 2;
    }

    (*node)->left = ast_create_node(NODE_TERM, TYPE_ID, assign_id);
    if (!(*node)->left)
    {
        ast_destroy(*node);
        *node = NULL;
        free(assign_id);
        return 2;
    }

    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }
    if (!match_tokens(T_ASSIGN)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1; 
    }
    if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR) 
    {
        int ret_val = parse_fun_call(&(*node)->right);
        if (ret_val != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return ret_val;
        }
    }

    else if (is_operand_token(current_token.type)) 
    {
        int ret_val = parse_expression(&(*node)->right);
        if (ret_val != 0) 
        {
            ast_destroy(*node);
            *node = NULL;
            return ret_val;
        }
    }
    else 
    {
        ast_destroy(*node);
        *node = NULL;
        return 1;
    }

    return 0;
}
int parse_var_val(ast_node **node)
{
    *node = NULL;

    if (current_token.type == T_VAL)
    {
        *node = ast_create_node(NODE_VAL_DEF, TYPE_ID, NULL);
        if (!*node) { return 2; }
    }
    else if (current_token.type == T_VAR)
    {
        *node = ast_create_node(NODE_VAR_DEF, TYPE_ID, NULL);
        if (!*node) { return 2; }
    }
    else { return 1; }

    advance_token();

    if (!valid_token || current_token.type != T_ID || !current_token.attribute) 
    {
        ast_destroy(*node);
        *node = NULL;
        return 1;
    }
    char *var_val = strdup(current_token.attribute);
    if (!var_val)
    {
        ast_destroy(*node);
        *node = NULL;
        return 2;
    }
    (*node)->value = var_val;

    if (!match_tokens(T_ID)) 
    {
        ast_destroy(*node);
        *node = NULL;
        return 1;
    }

    int ret_val = parse_type_opt(&((*node)->left));
    if (ret_val != 0) 
    {
        ast_destroy(*node);
        *node = NULL;
        return ret_val;
    }

    if (!match_tokens(T_ASSIGN)) 
    { 
        ast_destroy(*node);
        *node = NULL;
        return 1;
    }

    if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR)
    {
        ret_val = parse_fun_call(&((*node)->right));
        if (ret_val != 0)
        {
            ast_destroy(*node);
            *node = NULL;
            return ret_val;
        }
    }
    else
    {
        ret_val = parse_expression(&((*node)->right));
        if (ret_val != 0)
        {
            ast_destroy(*node);
            *node = NULL;
            return ret_val;
        }
    }
    
    return 0;
}

int parse_block(ast_node **node) 
{
    *node = NULL;

    skip_eol();

        // Empty block
    if (valid_token && current_token.type == T_LPAR) 
    {
        advance_token();
        if (!match_tokens(T_RPAR)) { return 1;}

        *node = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
        if (!*node) { return 2; }

        skip_eol();
        return 0;
    }

    ast_node **last = node;

    while (valid_token && current_token.type != T_DEDENT) 
    {
        *last = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
        if (!*last)
        {
            ast_destroy(*node);
            *node = NULL;
            return 2;
        }
        // VARiable and VALue definition
        if (valid_token && (current_token.type == T_VAL || current_token.type == T_VAR)) 
        {
            int ret_val = parse_var_val(&((*last)->left));
            if (ret_val != 0)
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
        }

        // RETURN statement
        else if (valid_token && current_token.type == T_RETURN) 
        {
            advance_token();
            (*last)->left = ast_create_node(NODE_RETURN, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 2;
            }

            if (valid_token && current_token.type != T_EOL) 
            {
                if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR)
                {
                    int ret_val = parse_fun_call(&(*last)->left->left);
                    if (ret_val != 0) 
                    {
                        ast_destroy(*node);
                        *node = NULL;
                        return ret_val;
                    }
                }
                else
                {
                    int ret_val = parse_expression(&(*last)->left->left);
                    if (ret_val != 0) 
                    {
                        ast_destroy(*node);
                        *node = NULL;
                        return ret_val;
                    }
                }
            }
        }

        // IF statement
        else if (valid_token && current_token.type == T_IF) 
        {
            advance_token();
            (*last)->left = ast_create_node(NODE_IF, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 2;
            }

            int ret_val = parse_condition(&(*last)->left->left);
            if (ret_val != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
            if (!match_tokens(T_THEN)) 
            { 
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_EOL)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }

            (*last)->left->right = ast_create_node(NODE_IF_BODY, TYPE_NONE, NULL);
            if (!(*last)->left->right) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 2;
            }

            ret_val = parse_block(&(*last)->left->right->left);
            if (ret_val != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_ELSE)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_EOL))
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }

            ret_val = parse_block(&(*last)->left->right->right);
            if (ret_val != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
        }

        // WHILE loop
        else if (valid_token && current_token.type == T_WHILE) 
        {
            advance_token();
            (*last)->left = ast_create_node(NODE_WHILE, TYPE_NONE, NULL);
            if (!(*last)->left) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 2;
            }

            int ret_val = parse_condition(&(*last)->left->left);
            if (ret_val != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
            if (!match_tokens(T_DO)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_EOL)) 
            { 
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
            if (!match_tokens(T_INDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }

            ret_val = parse_block(&(*last)->left->right);
            if (ret_val != 0) 
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
            if (!match_tokens(T_DEDENT)) 
            {
                ast_destroy(*node);
                *node = NULL;
                return 1;
            }
        }

        // ID + look_ahead
        else if (valid_token && current_token.type == T_ID) 
        {

            // ASSIGNMENT
            if (valid_look_ahead && look_ahead_token.type == T_ASSIGN) 
            {
                int ret_val = parse_assignment(&((*last)->left));
                if (ret_val != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return ret_val;
                }
            } 

            // FUN_CALL
            else if (valid_look_ahead && look_ahead_token.type == T_LPAR) 
            {
                int ret_val = parse_fun_call(&((*last)->left));
                if (ret_val != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return ret_val;
                }
            }

            // EXPRESION
            else 
            {
                int ret_val = parse_expression(&((*last)->left));
                if (ret_val != 0)
                {
                    ast_destroy(*node);
                    *node = NULL;
                    return ret_val;
                }
            }

        }

        // EXPRESSION
        else if (valid_token && is_operand_token(current_token.type)) 
        {
            int ret_val = parse_expression(&((*last)->left));
            if (ret_val != 0)
            {
                ast_destroy(*node);
                *node = NULL;
                return ret_val;
            }
        }

        else 
        {
            ast_destroy(*node);
            *node = NULL;
            return 1;
        }

        last = &((*last)->right);
        skip_eol();
        
    }
    return 0;
}

int parse_fun_body(ast_node **node) 
{
    *node = NULL;
    if (valid_token && current_token.type == T_EOL) 
    {
        advance_token();
        if (!match_tokens(T_INDENT)) { return 1; }

        int ret_val = parse_block(node);
        if (ret_val != 0)
        { 
            ast_destroy(*node);
            *node = NULL;
            return ret_val;
        }

        if (!match_tokens(T_DEDENT)) 
        { 
            ast_destroy(*node);
            *node = NULL;
            return 1; 
        }
    } 
    else if (valid_token && valid_look_ahead && current_token.type == T_ID && look_ahead_token.type == T_LPAR) 
    {
        int ret_val = parse_fun_call(node);  
        if (ret_val != 0) { return ret_val; } 
    }
    else if (is_operand_token(current_token.type)) 
    {
        int ret_val = parse_expression(node);
        if (ret_val != 0) { return ret_val; }
    }
    else
    {
        return 1;
    }
    return 0;
}

int parse_type_opt(ast_node **node) 
{
    *node = NULL;

    if (valid_token && current_token.type == T_ASSIGN) { return 0; }

    if (valid_token && current_token.type == T_COLON) {
        advance_token();

        if (!is_type_token(current_token.type)) { return 1; }

        data_type_t type_tmp = token_type_to_node_type(current_token.type);
        *node = ast_create_node(NODE_TYPE, type_tmp, NULL);
        if(!*node) { return 2; }

        advance_token(); 
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
    if (!valid_token || !current_token.attribute) { return 1; }
    char *param_id = strdup(current_token.attribute);
    if (!param_id) { return 2; }

    if (!match_tokens(T_ID)) 
    {   
        free(param_id);
        return 1; 
    }
    if (!match_tokens(T_COLON)) 
    {
        free(param_id); 
        return 1;
    }
    if (!is_type_token(current_token.type)) 
    { 
        free(param_id);
        return 1; 
    }

    data_type_t param_type = token_type_to_node_type(current_token.type);
    *node = ast_create_node(NODE_PARAMETER, param_type, param_id);
    if (!*node) {
        free(param_id);
        return 2;
    }

    ast_node *last = *node;
    advance_token();
    
    // Multiple parameters    
    while (valid_token && current_token.type == T_COMMA) 
    {
        advance_token();

        if (!valid_token || !current_token.attribute) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 1;
        }

        param_id = strdup(current_token.attribute);
        if (!param_id) 
        {
            ast_destroy(*node);
            *node = NULL;
            return 2;
        }

        if (!match_tokens(T_ID)) 
        {   
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return 1;
        }
        if (!match_tokens(T_COLON))
        {   
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return 1;
        }
        if (!is_type_token(current_token.type))
        {
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return 1;
        }

        param_type = token_type_to_node_type(current_token.type);
        last->left = ast_create_node(NODE_PARAMETER, param_type, param_id);
        if (!last->left) 
        {
            ast_destroy(*node);
            *node = NULL;
            free(param_id);
            return 2;
        }

        last = last->left;
        advance_token();
    }
    return 0;
}

int parse_fun_def(ast_node **node) 
{
    *node = NULL;
    ast_node *definition = NULL;

    if (valid_token && current_token.type == T_AT_MAIN) {
        advance_token();
        skip_eol();
    }
    if (!match_tokens(T_DEF)) { return 1; }
    if (!valid_token || current_token.type != T_ID) { return 1;}

    if (!current_token.attribute) { return 1; }
    char *fun_id = strdup(current_token.attribute);
    if (!fun_id) { return 2; }

    definition = ast_create_node(NODE_FUN_DEF, TYPE_ID, fun_id);
    if (!definition) {
        free(fun_id);
        return 2;
    }

    if (!match_tokens(T_ID)) 
    { 
        ast_destroy(definition);
        return 1; 
    }

    if (!match_tokens(T_LPAR)) 
    {
        ast_destroy(definition);
        return 1; 
    }

    int ret_val = parse_param(&definition->left); 
    if (ret_val != 0)
    { 
        ast_destroy(definition);
        return ret_val; 
    }

    if (!match_tokens(T_RPAR)) 
    { 
        ast_destroy(definition);
        return 1; 
    }

    definition->right = ast_create_node(NODE_FUN_DEF, TYPE_NONE, NULL);
    if (!definition->right) 
    {   
        ast_destroy(definition);
        return 2; 
    }

    ret_val = parse_type_opt(&definition->right->left);
    if (ret_val != 0)
    {   
        ast_destroy(definition);
        return ret_val; 
    }
    
    if (!match_tokens(T_ASSIGN)) 
    {   
        ast_destroy(definition); 
        return 1; 
    }

    ret_val = parse_fun_body(&definition->right->right);
    if (ret_val != 0)
    {
        ast_destroy(definition);
        return ret_val;
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
        if (!*last) { return 2; }

        int ret_val = parse_fun_def(&tmp);
        if (ret_val != 0) 
        {
            ast_destroy(tmp);
            ast_destroy(*last);
            *last = NULL;
            return ret_val;
        }
        (*last)->left = tmp;
        last = &((*last)->right);
        tmp = NULL;
        
        skip_eol();
    }
    return 0;
}

int parse_prolog(void) 
{
    if(!match_tokens(T_IMPORT)) { return 1; }

    if (!current_token.attribute) { return 1;}
    if(current_token.type != T_ID || strcmp(current_token.attribute, "ifj26") != 0) { return 1; }
    advance_token();

    advance_token(); // skipping UNKNOWN TOKEN FOR NOW
    // if(!match_tokens(T_DOT)) { return 1; } add back after fixing scanner dot detection in PROLOG

    if(!match_tokens(T_MUL)) { return 1; }

    return 0;
}

int parse_program(ast_node **node)
{   
    *node = NULL;

    if (parser_init() != 0) 
    {
        parser_cleanup();
        return 1;
    }

    if (parse_prolog() != 0) 
    {
        parser_cleanup();
        return 1;
    }

    skip_whitespace();

    int ret_val = parse_function(node);
    if (ret_val != 0)
    {
        ast_destroy(*node);
        *node = NULL;
        parser_cleanup();
        return ret_val;
    }

    skip_whitespace();

    if (!match_tokens(T_EOF)) 
    {
        ast_destroy(*node);
        *node = NULL;
        parser_cleanup();
        return 1;
    }

    parser_cleanup();
    return 0;
}