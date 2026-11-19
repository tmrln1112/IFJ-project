#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "scanner.h"
#include "ast.h"
#include "pstack.h"

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

const precedence_type precedence_table[8][8] = {
  // [*, /]  [+, -]   REL   EQ    (      )      ID      $     
  {P_POP,  P_POP, P_POP, P_POP, P_PUSH, P_POP, P_PUSH, P_POP}, // [*, /]
  {P_PUSH,  P_POP, P_POP, P_POP, P_PUSH, P_POP, P_PUSH, P_POP}, // [+, -]
  {P_PUSH,  P_PUSH, P_ERR, P_POP, P_PUSH, P_POP, P_PUSH, P_POP}, // REL [<, >, <=, >=]
  {P_PUSH, P_PUSH, P_PUSH, P_ERR, P_PUSH, P_POP, P_PUSH, P_POP}, // EQ [!=, ==]
  {P_PUSH,  P_PUSH, P_PUSH, P_PUSH ,P_PUSH, P_EQ, P_PUSH, P_ERR}, // (
  {P_POP,  P_POP, P_POP, P_POP ,P_ERR, P_POP, P_ERR, P_POP}, // )
  {P_POP,  P_POP, P_POP, P_POP, P_EQ, P_POP, P_ERR, P_POP}, // ID or LITERAL
  {P_PUSH,  P_PUSH, P_PUSH, P_PUSH ,P_PUSH, P_ERR, P_PUSH, P_ERR}, // $
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
            return S_OPERAND;
        default:
            return S_END;
    }
}

int symbol_type_to_int(symbol_type_t *type)
{
    switch(*type) {
        case S_MUL_DIV: return 0;
        case S_ADD_SUB: return 1;
        case S_REL_OP: return 2;
        case S_EQ_OP: return 3;
        case S_LPAR: return 4;
        case S_RPAR: return 5;
        case S_OPERAND: return 6;
        case S_END: return 7;
        default: return -1;
    }
}

char *op_token_to_str(Token_type type)
{
    switch (type) {
        case T_MUL:             return "*";
        case T_DIV:             return "/";
        case T_PLUS:            return "+";
        case T_MINUS:           return "-";
        case T_LESS_T:          return "<";
        case T_LESS_OR_EQ:      return "<=";
        case T_GREATER_T:       return ">";
        case T_GREATER_OR_EQ:   return ">=";
        case T_EQ:              return "==";
        case T_NEQ:             return "!=";
        default:                return NULL;
    }
}

int build_stack_item(Token *token, symbol_type_t type, stack_item_t *item)
{
    if (!token) { return 99; }

    item->type = type;
    item->token.type = token->type;
    item->token.attribute = NULL;
    item->node = NULL;

    if (token->attribute) 
    {
        item->token.attribute = strdup(token->attribute);
        if (!item->token.attribute) { return 99; }
    }

    if (type == S_OPERAND) 
    {
        char *value_copy = NULL;
        if (token->attribute) {
            value_copy = strdup(token->attribute);
            if (!value_copy) {
                free(item->token.attribute);
                item->token.attribute = NULL;
                return 99;
            }
        }
        item->node = ast_create_node(NODE_TERM, token_type_to_node_type(token->type), value_copy);
        if (!item->node) {
            free(value_copy);
            free(item->token.attribute);
            item->token.attribute = NULL;
            return 99;
        }
    }

    return 0;
}

int expression_reduction(precedence_stack_t *stack)
{   
    if (!stack) { return 99; }

    stack_item_t first, second, third;

    if (prec_stack_pop(stack, &first) != 0) { return 2; }

    ast_node *sub_tree = NULL;

    // Rule: i -> E
    if (first.type == S_OPERAND) 
    {
        if (!first.node) 
        {
            free(first.token.attribute);
            return 2;
        }

        sub_tree = first.node;
        first.node = NULL;
        free(first.token.attribute);
    }

    // Rule: (E) -> E
    else if (first.type == S_RPAR) 
    {
        free(first.token.attribute);

        if (prec_stack_pop(stack, &second) != 0 || second.type != S_EXP || !second.node) 
        {
            if (second.node) { ast_destroy(second.node); }
            free(second.token.attribute);
            return 2;
        }
        if (prec_stack_pop(stack, &third) != 0 || third.type != S_LPAR) 
        {
            ast_destroy(second.node);
            if (third.node) { ast_destroy(third.node); }
            free(third.token.attribute);
            return 2;
        }
        free(third.token.attribute);

        sub_tree = second.node;
        second.node = NULL;
    }

    // Rule: E operator E -> E
    else if (first.type == S_EXP) 
    {
        if (!first.node) { return 2; }

        if (prec_stack_pop(stack, &second) != 0) 
        {
            ast_destroy(first.node);
            return 2;
        }
        if (prec_stack_pop(stack, &third) != 0 || third.type != S_EXP || !third.node) 
        {
            ast_destroy(first.node);
            if (second.node) { ast_destroy(second.node); }
            free(second.token.attribute);
            if (third.node) { ast_destroy(third.node); }
            return 2;
        }

        char *op_str = op_token_to_str(second.token.type);
        free(second.token.attribute);
        if (!op_str) 
        {
            ast_destroy(first.node);
            ast_destroy(third.node);
            return 2;
        }

        sub_tree = ast_create_node(NODE_EXPRESSION, TYPE_NONE, op_str);
        if (!sub_tree) 
        {
            free(op_str);
            ast_destroy(first.node);
            ast_destroy(third.node);
            return 99;
        }

        sub_tree->left = third.node;
        sub_tree->right = first.node;
    }
    else 
    {
        if (first.node) { ast_destroy(first.node); }
        free(first.token.attribute);
        return 2;
    }

    // Adding sub_tree back to stack
    stack_item_t exp_item;
    exp_item.type = S_EXP;
    exp_item.node = sub_tree;

    if (prec_stack_push(stack, exp_item) != 0) 
    {
        ast_destroy(sub_tree);
        return 99;
    }

    return 0;
}

bool is_expression_stopper(Token_type type) 
{
    return (type == T_EOL || type == T_EOF || type == T_THEN || type == T_DO);
}

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

int parse_expression(ast_node **node)
{
    if (!node) { return 99; }
    *node = NULL;

    precedence_stack_t stack;
    if (prec_stack_init(&stack, STACK_CAPACITY) != 0) { return 99; }

    stack_item_t end_item;
    memset(&end_item, 0, sizeof(end_item));
    end_item.type = S_END;
    if (prec_stack_push(&stack, end_item) != 0) 
    {
        prec_stack_free(&stack);
        return 99;
    }

    while (true) 
    {
        if (!valid_token) 
        {
            prec_stack_free(&stack);
            return parser_or_scanner_err();
        }

        bool is_stopper = is_expression_stopper(current_token.type);
        symbol_type_t input_sym;
        if (is_stopper) 
        {
            input_sym = S_END;
        }
        else 
        {
            input_sym = token_to_symbol_type(current_token.type);
        }

        // If not a stopper and token_to_symbol_type fell back to S_END, it's an invalid token for an expression
        if (!is_stopper && input_sym == S_END) 
        {
            prec_stack_free(&stack);
            return 2;
        }

        stack_item_t top_term;
        if (prec_stack_top_terminal(&stack, &top_term) != 0) 
        {
            prec_stack_free(&stack);
            return 2;
        }

        // End of expression reached and only $ (+ optional single S_EXP) remains on stack
        if (is_stopper && top_term.type == S_END) 
        {
            if (stack.top == 1 && stack.items[1].type == S_EXP && stack.items[1].node != NULL) 
            {
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

        if (on_top == -1 || incoming == -1) 
        {
            prec_stack_free(&stack);
            return 2;
        }

        precedence_type action = precedence_table[on_top][incoming];

        if (action == P_PUSH || action == P_EQ) 
        {
            stack_item_t new_item;
            if (build_stack_item(&current_token, input_sym, &new_item) != 0) 
            {
                prec_stack_free(&stack);
                return 99;
            }
            if (prec_stack_push(&stack, new_item) != 0) 
            {
                if (new_item.node) { ast_destroy(new_item.node); }
                free(new_item.token.attribute);
                prec_stack_free(&stack);
                return 99;
            }

            int err = advance_token();
            if (err != 0) 
            {
                prec_stack_free(&stack);
                return err;
            }
        }
        else if (action == P_POP) 
        {
            int err = expression_reduction(&stack);
            if (err != 0) 
            {
                prec_stack_free(&stack);
                return err;
            }
        }
        else 
        {
            prec_stack_free(&stack);
            return 2;
        }
    }
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
    if (!*node) 
    {
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

    else if (is_operand_token(current_token.type) || current_token.type == T_LPAR) 
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

    // Empty block
    if (valid_token && current_token.type == T_LPAR) 
    {
        err = advance_token();
        if (err != 0) { return err; }
        if (!match_tokens(T_RPAR)) { return parser_or_scanner_err(); }

        *node = ast_create_node(NODE_BLOCK, TYPE_NONE, NULL);
        if (!*node) { return 99; }

        err = skip_eol();
        if (err != 0)
        {
            ast_destroy(*node);
            *node = NULL;
            return err;
        }
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

            if (valid_token && current_token.type != T_EOL) 
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

            err = parse_expression(&(*last)->left->left);
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

            err = parse_expression(&(*last)->left->left);
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
        else if (valid_token && is_operand_token(current_token.type)) 
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
        err = skip_eol();
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
    else if (is_operand_token(current_token.type) || current_token.type == T_LPAR) 
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