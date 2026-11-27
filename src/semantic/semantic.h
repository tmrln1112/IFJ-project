/**
 * Project IFJ26 compiler
 * File: semantic.h
 * Description: semantic analysis interface and internal ast
 * Author: <xstrelm00> Maksym Streltsov
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtable.h"
#include <stdbool.h>
#include <stddef.h>

// return codes from spec
#define SEM_OK 0
#define SEM_ERR_UNDEF 3     // undefined var/func or missing @main
#define SEM_ERR_REDEF 4     // redefinition, duplicate param, >1 @main
#define SEM_ERR_CALL 5      // bad arg count/types, bad parens
#define SEM_ERR_TYPE 6      // type mismatch, missing return type when needed
#define SEM_ERR_IMMUTABLE 7 // reassigning val / param / loop var
#define SEM_ERR_OTHER 10    // e.g. for loop with literal 0 step
#define SEM_ERR_INTERNAL 99

typedef enum {
  OP_NONE = 0,
  OP_NEG, // -x
  OP_NOT, // !x
  OP_ADD, // + (numbers or string concat)
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_EQ,
  OP_NEQ,
  OP_AND,
  OP_OR
} OperatorType;

typedef enum {
  AST_PROGRAM = 0,
  AST_FUNC_DEF,
  AST_BLOCK,
  AST_VAR_DECL,
  AST_ASSIGN,
  AST_IDENTIFIER, // var ref or bare call without parens
  AST_FUNC_CALL,
  AST_MEMBER_ACCESS,
  AST_IF,
  AST_WHILE,
  AST_FOR,
  AST_RETURN,
  AST_BINOP,
  AST_UNOP,
  AST_LITERAL
} ASTNodeKind;

typedef struct {
  size_t count;
  struct ASTNode **args;
} ArgList;

// internal ast used by semantic pass, keeps a pointer back to parser node
// so we can set flags for codegen (widening, discard, bare_call, etc)
typedef struct ASTNode {
  ast_node *parser_node;
  ASTNodeKind kind;

  struct ASTNode **items;
  size_t item_count;

  // func def
  char *name;
  bool is_main;
  bool has_parens;
  size_t num_param_lists;
  ParamList *param_lists;
  bool has_explicit_return;
  DataType declared_type;
  struct ASTNode *body;

  // var/val & assign
  bool is_mutable;
  bool has_explicit_type;
  struct ASTNode *init_expr;
  struct ASTNode *rhs;

  // calls & member access
  size_t num_arg_lists;
  ArgList *arg_lists;
  struct ASTNode *receiver;
  char *member_name;

  // if / while
  struct ASTNode *cond;
  struct ASTNode *then_branch;
  struct ASTNode *else_branch;
  bool is_expression;

  // for loop
  char *iter_name;
  struct ASTNode *from_expr;
  struct ASTNode *end_expr;
  bool is_until;
  struct ASTNode *step_expr;
  struct ASTNode **guards;
  size_t guard_count;

  struct ASTNode *return_expr;

  // unary / binary ops
  OperatorType op;
  struct ASTNode *left;
  struct ASTNode *right;
  struct ASTNode *operand;

  // literals
  DataType literal_type;
  long long int_val;
  double double_val;
  char *str_val;
  bool bool_val;
  bool is_int_literal; // needed to spot literal 0 in 'by 0'

  // filled during analysis
  DataType sem_type;
  int unique_var_id;
  bool is_func_call;
  bool conv_to_double;
} ASTNode;

typedef struct {
  SymTable *global_funcs;
  ScopeStack local_scopes;
  Symbol *current_func;
  int next_var_id;
} SemanticContext;

bool semantic_context_init(SemanticContext *ctx);
void semantic_context_free(SemanticContext *ctx);
int semantic_register_builtins(SemanticContext *ctx);

int semantic_analyze(ASTNode *root);
int semantic_analyze_parser_ast(ast_node *parser_root);

int analyze_function(SemanticContext *ctx, Symbol *fn);
bool check_type_compat(DataType expected, DataType actual,
                       ASTNode *actual_node);

DataType analyze_block(SemanticContext *ctx, ASTNode *block_node, int *err);
DataType analyze_expr(SemanticContext *ctx, ASTNode *node, int *err);
DataType analyze_node(SemanticContext *ctx, ASTNode *node, int *err);

void semantic_ast_free(ASTNode *node);

#endif
