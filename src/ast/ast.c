#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "ast.h"

ast_node *ast_create_node(node_type_t type, data_type_t data, char *value) 
{
    ast_node *ptr = (ast_node*)malloc(sizeof(ast_node));
    if (!ptr) {
        return NULL;
    }
    ptr->left = NULL;
    ptr->right = NULL;
    ptr->node_type = type;
    ptr->data_type = data;
    ptr->value = value;
    return ptr;
}

void ast_destroy(ast_node *ptr) 
{
    if (ptr == NULL) { return; }
    ast_node *tmp;
    ast_node *tmp_parent;
    while (ptr->left != NULL || ptr->right != NULL) 
    {
        tmp = ptr;
        while (tmp->left != NULL || tmp->right != NULL) {
            if (tmp->left != NULL) {
                tmp_parent = tmp;
                tmp = tmp->left;
            } 
            else if (tmp->right != NULL) {
                tmp_parent = tmp;
                tmp = tmp->right;
            }
        } //before free set parent node pionter to NULL
        if (tmp_parent->left == tmp) {
            tmp_parent->left = NULL;
        }    
        if (tmp_parent->right == tmp) {
            tmp_parent->right = NULL;
        }
        if (tmp->value != NULL)
        {
            free(tmp->value);
        }
        free(tmp);
    }
    if (ptr->value != NULL)
    {
        free(ptr->value);
    }
    free(ptr);
    return;
}

// Helper to convert node_type_t enum to a readable string
const char *node_type_to_string(node_type_t type) {
    switch (type) {
        case NODE_FUNCTION: return "FUNCTION";
        case NODE_MAIN_FUNCTION: return "MAIN_FUNCTION";
        case NODE_FUN_DEF: return "FUN_DEF";
        case NODE_PARAMETER: return "PARAMETER";
        case NODE_TYPE: return "TYPE";
        case NODE_BLOCK: return "BLOCK";
        case NODE_VAR_DEF: return "VAR_DEF";
        case NODE_VAL_DEF: return "VAL_DEF";
        case NODE_ASSIGNMENT: return "ASSIGNMENT";
        case NODE_RETURN: return "RETURN";
        case NODE_IF: return "IF";
        case NODE_IF_BODY: return "IF_BODY";
        case NODE_WHILE: return "WHILE";
        case NODE_FUN_CALL: return "FUN_CALL";
        case NODE_ARGUMENT: return "ARGUMENT";
        case NODE_TERM: return "TERM";
        case NODE_EXPRESSION: return "EXPRESSION";
        case NODE_CONDITION: return "CONDITION";
        default: return "UNKNOWN_NODE";
    }
}

// Helper to convert data_type_t enum to a readable string
const char *data_type_to_string(data_type_t type) {
    switch (type) {
        case TYPE_NONE: return "NONE";
        case TYPE_ID: return "ID";
        case TYPE_INT: return "INT";
        case TYPE_DOUBLE: return "DOUBLE";
        case TYPE_STRING: return "STRING";
        case TYPE_UNIT: return "UNIT";
        default: return "UNKNOWN_TYPE";
    }
}

// Recursive print function with indentation for hierarchy visualization
void ast_print_tree_rec(ast_node *node, int depth) {
    if (node == NULL) {
        return;
    }

    // Print indentation based on tree depth
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print node type and evaluated data type (omitting literal values)
    printf("Node: %s | DataType: %s | Value: %s\n", 
           node_type_to_string(node->node_type), 
           data_type_to_string(node->data_type), node->value);

    // Recursively print left and right subtrees
    ast_print_tree_rec(node->left, depth + 1);
    ast_print_tree_rec(node->right, depth + 1);
}

// Public wrapper function
void ast_print_tree(ast_node *root) {
    printf("--- AST STRUCTURE ---\n");
    if (root == NULL) {
        printf("(empty tree)\n");
    } else {
        ast_print_tree_rec(root, 0);
    }
    printf("---------------------\n");
}