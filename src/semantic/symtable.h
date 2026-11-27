/**
 * Project IFJ26 compiler
 * File: symtable.h
 * Description: symbol table and scope stack
 * Author: <xstrelm00> Maksym Streltsov
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "ast.h"
#include <stdbool.h>
#include <stddef.h>

struct ASTNode;

// base types come from ast.h, here we just add bool, any and unknown
typedef enum {
  DT_INT = TYPE_INT,
  DT_DOUBLE = TYPE_DOUBLE,
  DT_STRING = TYPE_STRING,
  DT_UNIT = TYPE_UNIT,
  TYPE_BOOLEAN = 100,
  TYPE_ANY = 101,
  TYPE_UNKNOWN = 102
} DataType;

// one (...) group of params for PARAMLISTS
typedef struct {
  size_t count;
  DataType *types;
  char **names;
} ParamList;

typedef enum { SYM_VAR = 0, SYM_FUNC } SymbolKind;

// used while inferring return types so we can catch recursion without ': Type'
typedef enum { INFER_NONE = 0, INFER_IN_PROGRESS, INFER_DONE } InferState;

typedef struct {
  char *key;
  SymbolKind kind;

  // for variables
  DataType type;
  bool is_mutable; // true only for var
  int unique_id;   // counter for shadowed vars

  // for functions
  DataType return_type;
  bool has_explicit_return;
  bool has_parens; // false for stuff like 'def answer = 42'
  size_t num_param_lists;
  ParamList *param_lists;

  bool is_main;
  InferState infer_state;
  struct ASTNode *ast_node; // kept around so we can infer return type on demand
} Symbol;

// opaque handle, internal layout stays in symtable.c
typedef struct SymTable SymTable;

typedef int (*symtable_iter_fn)(Symbol *sym, void *ctx);

typedef struct {
  SymTable **tables;
  size_t top;
  size_t capacity;
} ScopeStack;

SymTable *symtable_init(size_t initial_capacity);
Symbol *symtable_search(SymTable *t, const char *key);

// copies key and param_lists; returns 0, 4 on duplicate, 99 on alloc error
int symtable_insert(SymTable *t, Symbol new_sym);
int symtable_rehash(SymTable *t, size_t new_capacity);
int symtable_foreach(SymTable *t, symtable_iter_fn fn, void *ctx);

void symtable_free(SymTable *t);
void symtable_dispose(SymTable *t);

// scope stack for nested blocks
bool scope_stack_init(ScopeStack *s);
void scope_stack_free(ScopeStack *s);
int scope_push(ScopeStack *s);
void scope_pop(ScopeStack *s);

// inserts into current (top) scope only, returns 4 if redeclared in same block
int scope_declare_var(ScopeStack *s, const char *name, DataType type,
                      bool is_mutable, int unique_id, Symbol **out_sym);

// walks scopes from top to bottom
Symbol *scope_lookup_var(ScopeStack *s, const char *name);

const char *datatype_to_str(DataType type);

#endif
