/**
 * Project IFJ26 compiler
 * File: symtable.h
 * Description: Symbol table (open addressing hash table) and scope stack
 * Author: <xstrelm00> Maksym Streltsov
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include "../ast/ast.h"

struct ASTNode;

/**
 * Data types of the IFJ26 language (with BOOLTHEN extension).
 * Note: TYPE_INT, TYPE_DOUBLE, TYPE_STRING, TYPE_UNIT are already declared
 * in ast.h (data_type_t), so we map DataType seamlessly while adding
 * TYPE_BOOLEAN, TYPE_ANY, and TYPE_UNKNOWN.
 */
typedef enum {
    DT_INT = TYPE_INT,
    DT_DOUBLE = TYPE_DOUBLE,
    DT_STRING = TYPE_STRING,
    DT_UNIT = TYPE_UNIT,
    TYPE_BOOLEAN = 100,
    TYPE_ANY = 101,
    TYPE_UNKNOWN = 102
} DataType;

/**
 * Parameter list for a single pair of parentheses (PARAMLISTS extension).
 */
typedef struct {
    size_t count;       // Number of parameters inside one pair of parentheses
    DataType *types;    // Dynamic array of parameter types
    char **names;       // Dynamic array of parameter names
} ParamList;

/**
 * State of a hash table slot (open addressing TRP-izp).
 */
typedef enum {
    SLOT_EMPTY = 0,
    SLOT_OCCUPIED,
    SLOT_DELETED
} SlotState;

/**
 * Kind of symbol stored in the table.
 */
typedef enum {
    SYM_VAR = 0,
    SYM_FUNC
} SymbolKind;

/**
 * Return type inference state (protects against recursion without explicit return type).
 */
typedef enum {
    INFER_NONE = 0,
    INFER_IN_PROGRESS,
    INFER_DONE
} InferState;

/**
 * Symbol table entry.
 */
typedef struct {
    // Open addressing hash table service fields
    SlotState state;
    char *key;                  // Variable or function name
    SymbolKind kind;

    // Variable data (SYM_VAR)
    DataType type;              // Type of the variable
    bool is_mutable;            // true for 'var', false for 'val', function params, and 'for' loop vars
    int unique_id;              // Global counter (1, 2, 3...) to disambiguate shadowed variables in codegen

    // Function data (SYM_FUNC)
    DataType return_type;
    bool has_explicit_return;   // Whether ': Type' was specified in the function header
    bool has_parens;            // false for parameterless 'def answer: Int = 42' (PARAMLISTS)
    size_t num_param_lists;     // Number of parameter lists: 0 for no parens, >= 1 with parens
    ParamList *param_lists;     // Dynamic array of parameter lists

    bool is_main;               // Whether annotated with @main
    InferState infer_state;     // Return type inference progress state
    struct ASTNode *ast_node;   // Pointer to the function's AST node for on-demand return type inference
} Symbol;

/**
 * Open addressing hash table (TRP-izp).
 */
typedef struct {
    Symbol *slots;
    size_t capacity;
    size_t size;                // Number of OCCUPIED + DELETED slots
} SymTable;

/**
 * Stack of local variable scopes.
 */
typedef struct {
    SymTable **tables;
    size_t top;
    size_t capacity;
} ScopeStack;

/* --- SymTable operations --- */

/**
 * Allocates and initializes a symbol table with the given initial capacity.
 */
SymTable *symtable_init(size_t initial_capacity);

/**
 * Initializes an existing SymTable struct in-place.
 */
bool symtable_init_inplace(SymTable *t, size_t initial_capacity);

/**
 * Searches for a symbol by key. Returns pointer to Symbol if found, NULL otherwise.
 */
Symbol *symtable_search(SymTable *t, const char *key);

/**
 * Inserts a symbol into the table.
 * Makes deep copies of key and param_lists if present.
 * Returns 0 on success, 4 on duplicate key error, 99 on memory allocation failure.
 */
int symtable_insert(SymTable *t, Symbol new_sym);

/**
 * Rehashes the symbol table to a new capacity.
 * Returns 0 on success, 99 on allocation failure.
 */
int symtable_rehash(SymTable *t, size_t new_capacity);

/**
 * Frees all dynamically allocated memory inside the symbol table (and the table struct itself).
 */
void symtable_free(SymTable *t);

/**
 * Frees the internal buffers of a symbol table without freeing the SymTable pointer itself.
 */
void symtable_dispose(SymTable *t);

/* --- ScopeStack operations --- */

/**
 * Initializes an empty scope stack.
 */
bool scope_stack_init(ScopeStack *s);

/**
 * Frees all scopes in the stack and its internal array.
 */
void scope_stack_free(ScopeStack *s);

/**
 * Creates a new SymTable and pushes it onto the top of the scope stack.
 * Returns 0 on success, 99 on memory allocation error.
 */
int scope_push(ScopeStack *s);

/**
 * Frees the top SymTable and decrements top.
 */
void scope_pop(ScopeStack *s);

/**
 * Declares a variable in the topmost scope only.
 * Returns 0 on success, 4 if already declared in the current block, 99 on memory error.
 */
int scope_declare_var(ScopeStack *s, const char *name, DataType type, bool is_mutable, int unique_id, Symbol **out_sym);

/**
 * Looks up a variable by name from top scope down to 0.
 * Returns pointer to the first matching Symbol, or NULL if not found.
 */
Symbol *scope_lookup_var(ScopeStack *s, const char *name);

/* --- Helper functions --- */

/**
 * Converts DataType to its language string representation ("Int", "Double", "String", "Boolean", "Unit").
 */
const char *datatype_to_str(DataType type);

#endif // SYMTABLE_H
