/**
 * Project IFJ26 compiler
 * File: semantic.h
 * Description: Semantic analyzer interface and AST structures supporting BOOLTHEN, FUNEXP,
 *              CONTROL, VALMEM and PARAMLISTS
 * Author: <xstrelm00> Maksym Streltsov
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include "symtable.h"
#include "../ast/ast.h"

/**
 * Semantic error codes according to IFJ26 specification:
 *  0 - No error
 *  3 - Undefined function/variable or missing @main
 *  4 - Redefinition of variable/function, duplicate parameter name, overriding built-in VALMEM, multiple @main
 *  5 - Wrong number/types of arguments or wrong parentheses usage in function call or @main header
 *  6 - Type incompatibility in expressions/statements/return, missing return type in recursive function or function with return
 *  7 - Assignment to immutable variable (val, parameter, for-loop variable)
 * 10 - Other semantic error (e.g., for-loop step literal is 0)
 * 99 - Internal compiler error (memory allocation failure)
 */
#define SEM_OK              0
#define SEM_ERR_UNDEF       3
#define SEM_ERR_REDEF       4
#define SEM_ERR_CALL        5
#define SEM_ERR_TYPE        6
#define SEM_ERR_IMMUTABLE   7
#define SEM_ERR_OTHER       10
#define SEM_ERR_INTERNAL    99

/**
 * Operator types for unary and binary expressions (including BOOLTHEN).
 */
typedef enum {
    OP_NONE = 0,
    // Unary operators
    OP_NEG,         // -x
    OP_NOT,         // !x (BOOLTHEN)
    // Arithmetic & String binary operators
    OP_ADD,         // + (Int/Double addition or String concatenation)
    OP_SUB,         // -
    OP_MUL,         // *
    OP_DIV,         // /
    // Relational operators
    OP_LT,          // <
    OP_LE,          // <=
    OP_GT,          // >
    OP_GE,          // >=
    // Equality operators
    OP_EQ,          // ==
    OP_NEQ,         // !=
    // Logical operators (BOOLTHEN)
    OP_AND,         // &&
    OP_OR           // ||
} OperatorType;

/**
 * High-level semantic AST node kinds covering core IFJ26 and supported extensions.
 */
typedef enum {
    AST_PROGRAM = 0,
    AST_FUNC_DEF,       // function definition
    AST_BLOCK,          // Sequence of statements/expressions
    AST_VAR_DECL,       // val / var declaration
    AST_ASSIGN,         // id = expr
    AST_IDENTIFIER,     // Variable reference OR parameterless function call (PARAMLISTS)
    AST_FUNC_CALL,      // Function call with 1+ argument lists: id(args1)(args2)...
    AST_MEMBER_ACCESS,  // expr.member (VALMEM)
    AST_IF,             // if cond then ... [else ...] (BOOLTHEN + CONTROL)
    AST_WHILE,          // while cond do ...
    AST_FOR,            // for i <- from to/until end [by step] [if guard]... do body (CONTROL)
    AST_RETURN,         // return [expr]
    AST_BINOP,          // left op right
    AST_UNOP,           // op operand
    AST_LITERAL         // Int, Double, String, Boolean, Unit literal
} ASTNodeKind;

/**
 * Argument list for a single pair of parentheses in a call (FUNEXP + PARAMLISTS).
 */
typedef struct {
    size_t count;
    struct ASTNode **args;
} ArgList;

/**
 * Rich semantic ASTNode supporting all fields described in the semantic specification.
 */
typedef struct ASTNode {
    ast_node *parser_node;          // Borrowed parser node for code generation annotations
    ASTNodeKind kind;

    // Statements / Program items list
    struct ASTNode **items;
    size_t item_count;

    // Function definition (AST_FUNC_DEF)
    char *name;                     // Function, variable, or identifier name
    bool is_main;                   // @main annotation
    bool has_parens;                // true if declared/called with parentheses
    size_t num_param_lists;         // Number of parameter lists (PARAMLISTS)
    ParamList *param_lists;         // Array of parameter lists
    bool has_explicit_return;       // true if ': ReturnType' is present
    DataType declared_type;         // Explicit return type or variable type annotation
    struct ASTNode *body;           // Function body or loop body

    // Variable declaration (AST_VAR_DECL) & Assignment (AST_ASSIGN)
    bool is_mutable;                // true for 'var', false for 'val'
    bool has_explicit_type;         // true if ': T' was specified on val/var
    struct ASTNode *init_expr;      // Initializer expression for val/var
    struct ASTNode *rhs;            // Right-hand side expression for assignment

    // Function call (AST_FUNC_CALL) & Member access/call (AST_MEMBER_ACCESS)
    size_t num_arg_lists;           // Number of parenthesized argument lists
    ArgList *arg_lists;             // Array of argument lists
    struct ASTNode *receiver;       // Left of '.' in expr.member
    char *member_name;              // Right of '.' in expr.member

    // Conditional (AST_IF) & Loops (AST_WHILE, AST_FOR)
    struct ASTNode *cond;           // Condition in if / while
    struct ASTNode *then_branch;    // Then block/expr
    struct ASTNode *else_branch;    // Else block/expr (NULL if omitted)
    bool is_expression;             // true if 'if' is evaluated as an expression

    // For loop (AST_FOR - CONTROL)
    char *iter_name;                // Loop variable 'i'
    struct ASTNode *from_expr;      // Start bound
    struct ASTNode *end_expr;       // End bound
    bool is_until;                  // true for 'until', false for 'to'
    struct ASTNode *step_expr;      // Optional 'by step' (NULL if omitted)
    struct ASTNode **guards;        // Optional 'if guard' expressions
    size_t guard_count;

    // Return statement (AST_RETURN)
    struct ASTNode *return_expr;    // Optional return expression (NULL if bare 'return')

    // Operators (AST_BINOP, AST_UNOP)
    OperatorType op;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *operand;

    // Literal value (AST_LITERAL)
    DataType literal_type;
    long long int_val;
    double double_val;
    char *str_val;
    bool bool_val;
    bool is_int_literal;            // Used to detect literal 0 in 'for ... by 0'

    // Annotations populated by Semantic Analysis for Code Generation
    DataType sem_type;              // Inferred/verified semantic type of this node
    int unique_var_id;              // Resolved unique variable ID (for shadowing)
    bool is_func_call;              // Marks an identifier node that resolves to a parameterless function call
    bool conv_to_double;            // Marks implicit Int -> Double conversion on this node
} ASTNode;

/**
 * Context state for the two-pass semantic analyzer.
 */
typedef struct {
    SymTable *global_funcs;         // Global symbol table for functions
    ScopeStack local_scopes;        // Stack of local variable scopes for the active function
    Symbol *current_func;           // Function currently being analyzed (for 'return' validation)
    int next_var_id;                // Global counter (1, 2, 3...) for unique variable IDs
} SemanticContext;

/* --- Core Semantic Analysis API --- */

/**
 * Initializes the semantic context and creates global_funcs and local_scopes.
 */
bool semantic_context_init(SemanticContext *ctx);

/**
 * Frees all tables and scopes inside the semantic context.
 */
void semantic_context_free(SemanticContext *ctx);

/**
 * Registers all 11 built-in IFJ26 functions into ctx->global_funcs.
 */
int semantic_register_builtins(SemanticContext *ctx);

/**
 * Runs complete 2-pass semantic analysis on an ASTNode tree.
 * Returns 0 (SEM_OK) on success, or an IFJ26 semantic error code (3, 4, 5, 6, 7, 10, 99).
 */
int semantic_analyze(ASTNode *root);

/**
 * Bridge entry point: converts the current parser's ast_node tree into ASTNode
 * and runs semantic_analyze on it.
 */
int semantic_analyze_parser_ast(ast_node *parser_root);

/**
 * Analyzes a single function symbol (Pass 2), performing return type inference
 * and recursion checking.
 */
int analyze_function(SemanticContext *ctx, Symbol *fn);

/**
 * Checks type compatibility (including TYPE_ANY and implicit Int -> Double promotion).
 * If expected == TYPE_DOUBLE and actual == TYPE_INT, sets actual_node->conv_to_double = true.
 */
bool check_type_compat(DataType expected, DataType actual, ASTNode *actual_node);

/**
 * Analyzes a block node and returns the resulting DataType of the block.
 */
DataType analyze_block(SemanticContext *ctx, ASTNode *block_node, int *err);

/**
 * Analyzes an expression node and returns its DataType.
 */
DataType analyze_expr(SemanticContext *ctx, ASTNode *node, int *err);

/**
 * Analyzes a statement or general AST node and returns its DataType.
 */
DataType analyze_node(SemanticContext *ctx, ASTNode *node, int *err);

/**
 * Helper to free a semantic ASTNode tree.
 */
void semantic_ast_free(ASTNode *node);

#endif // SEMANTIC_H
