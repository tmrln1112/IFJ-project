#ifndef AST_H
#define AST_H

#include <stdbool.h>

typedef enum {
    TYPE_NONE,
    TYPE_ID,
    // Used for Keyword and Literal value
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_UNIT
} data_type_t;

typedef enum {
    NODE_FUNCTION,        // value: NULL; data_type: TYPE_NONE
                          // L: NODE_FUN_DEF
                          // R: NODE_FUNCTION | NODE_MAIN_FUNCTION | NULL

    NODE_MAIN_FUNCTION,   // value: NULL; data_type: TYPE_NONE
                          // L: NODE_FUN_DEF
                          // R: NODE_FUNCTION | NODE_MAIN_FUNCTION | NULL

    NODE_FUN_DEF,         // first: value: id of function; data_type: TYPE_ID
                          //        L: NODE_PARAMETER | NULL
                          //        R: second NODE_FUN_DEF

                          // second: value: NULL; data_type: TYPE_NONE
                          //        L: NODE_TYPE | NULL
                          //        R: NODE_BLOCK | NODE_FUN_CALL | NODE_EXPRESSION

    NODE_PARAMETER,       // value: id of parameter; data_type: explicit value type or TYPE_NONE
                          // L: NODE_PARAMETER | NULL; R: NULL

    NODE_TYPE,            // value: NULL; data_type: either funtion return type or variable/value type
                          // L: NULL; R: NULL

    NODE_BLOCK,           // value: NULL; data_type: TYPE_NONE
                          // L: NODE_VAR_DEF | NODE_VAL_DEF | NODE_RETURN | NODE_IF
                          //    | NODE_WHILE | NODE_ASSIGNMENT | NODE_FUN_CALL
                          //    | NODE_EXPRESSION | NULL
                          // R: NODE_BLOCK | NULL

    NODE_VAR_DEF,         // value: id of variable; data_type: TYPE_ID
                          // L: NODE_TYPE | NULL
                          // R: NODE_FUN_CALL | NODE_EXPRESSION | NULL

    NODE_VAL_DEF,         // value: id of value; data_type: TYPE_ID
                          // L: NODE_TYPE | NULL
                          // R: NODE_FUN_CALL | NODE_EXPRESSION | NULL

    NODE_ASSIGNMENT,      // value: NULL; data_type: TYPE_NONE
                          // L: NODE_TERM (always ID) <--- assigning to this value
                          // R: NODE_FUN_CALL | NODE_EXPRESSION | NULL

    NODE_RETURN,          // value: NULL; data_type: TYPE_NONE
                          // L: NODE_FUN_CALL | NODE_EXPRESSION | NULL; R: NULL

    NODE_IF,              // value: NULL; data_type: TYPE_NONE
                          // L: NODE_CONDITION; R: NODE_IF_BODY | NULL

    NODE_IF_BODY,         // value: NULL; data_type: TYPE_NONE
                          // L: NODE_BLOCK ; R: NODE_BLOCK | NODE_IF_BODY

    NODE_WHILE,           // value: NULL; data_type: TYPE_NONE
                          // L: NODE_CONDITION; R: NODE_BLOCK

    NODE_FUN_CALL,        // value: id of function; data_type: TYPE_ID
                          // L: NODE_ARGUMENT | NULL; R: NULL

    NODE_ARGUMENT,        // value: id or literal; data_type: type of value
                          // L: NODE_ARGUMENT | NULL; R: NULL

    NODE_TERM,            // value: id or literal | NULL; data_type: type of value
                          // L: NULL; R: NULL

    NODE_EXPRESSION,      // value: operator; L: operand; R: second operand or NULL
    NODE_CONDITION,       // 
} node_type_t;

typedef struct ast_node {
    node_type_t node_type;
    data_type_t data_type;
    char *value;
    struct ast_node *left;
    struct ast_node *right;
    bool widen_to_double;
    bool discard_value;
    bool if_has_value;
} ast_node;

ast_node *ast_create_node(node_type_t type, data_type_t data, char *value);

void ast_destroy(ast_node *ptr);

const char *node_type_to_string(node_type_t type);

const char *data_type_to_string(data_type_t type);

void ast_print_tree_rec(ast_node *node, int depth);

void ast_print_tree(ast_node *root);

#endif