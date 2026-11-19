/**
 * Project IFJ26 compiler
 * File: symtable.c
 * Description: Implementation of open addressing symbol table (TRP-izp) and scope stack
 * Author: <xstrelm00> Maksym Streltsov
 */

#include "symtable.h"
#include <stdlib.h>
#include <string.h>

#define SYMTABLE_DEFAULT_CAPACITY 31
#define SCOPE_STACK_INITIAL_CAP 16

static char *sym_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

/**
 * djb2 hash with XOR.
 */
static size_t hash1(const char *key) {
    size_t h = 5381;
    unsigned char c;
    while ((c = (unsigned char)*key++) != 0) {
        h = ((h << 5) + h) ^ c; // h * 33 ^ c
    }
    return h;
}

/**
 * Frees dynamically allocated fields inside a single occupied Symbol slot.
 */
static void free_symbol_data(Symbol *sym) {
    if (!sym) return;
    free(sym->key);
    sym->key = NULL;

    if (sym->param_lists) {
        for (size_t i = 0; i < sym->num_param_lists; i++) {
            free(sym->param_lists[i].types);
            if (sym->param_lists[i].names) {
                for (size_t j = 0; j < sym->param_lists[i].count; j++) {
                    free(sym->param_lists[i].names[j]);
                }
                free(sym->param_lists[i].names);
            }
        }
        free(sym->param_lists);
        sym->param_lists = NULL;
    }
    sym->num_param_lists = 0;
}

/**
 * Deep-copies a Symbol structure (key and param_lists).
 */
static int clone_symbol_data(Symbol *dst, const Symbol *src) {
    *dst = *src;
    dst->key = NULL;
    dst->param_lists = NULL;

    if (src->key) {
        dst->key = sym_strdup(src->key);
        if (!dst->key) return 99;
    }

    if (src->num_param_lists > 0 && src->param_lists) {
        dst->param_lists = (ParamList *)calloc(src->num_param_lists, sizeof(ParamList));
        if (!dst->param_lists) {
            free_symbol_data(dst);
            return 99;
        }
        for (size_t i = 0; i < src->num_param_lists; i++) {
            size_t cnt = src->param_lists[i].count;
            dst->param_lists[i].count = cnt;
            if (cnt > 0) {
                if (src->param_lists[i].types) {
                    dst->param_lists[i].types = (DataType *)malloc(cnt * sizeof(DataType));
                    if (!dst->param_lists[i].types) {
                        free_symbol_data(dst);
                        return 99;
                    }
                    memcpy(dst->param_lists[i].types, src->param_lists[i].types, cnt * sizeof(DataType));
                }
                if (src->param_lists[i].names) {
                    dst->param_lists[i].names = (char **)calloc(cnt, sizeof(char *));
                    if (!dst->param_lists[i].names) {
                        free_symbol_data(dst);
                        return 99;
                    }
                    for (size_t j = 0; j < cnt; j++) {
                        if (src->param_lists[i].names[j]) {
                            dst->param_lists[i].names[j] = sym_strdup(src->param_lists[i].names[j]);
                            if (!dst->param_lists[i].names[j]) {
                                free_symbol_data(dst);
                                return 99;
                            }
                        }
                    }
                }
            }
        }
    }

    dst->state = SLOT_OCCUPIED;
    return 0;
}

bool symtable_init_inplace(SymTable *t, size_t initial_capacity) {
    if (!t) return false;
    if (initial_capacity == 0) {
        initial_capacity = SYMTABLE_DEFAULT_CAPACITY;
    }
    t->slots = (Symbol *)calloc(initial_capacity, sizeof(Symbol));
    if (!t->slots) {
        t->capacity = 0;
        t->size = 0;
        return false;
    }
    t->capacity = initial_capacity;
    t->size = 0;
    return true;
}

SymTable *symtable_init(size_t initial_capacity) {
    SymTable *t = (SymTable *)malloc(sizeof(SymTable));
    if (!t) return NULL;
    if (!symtable_init_inplace(t, initial_capacity)) {
        free(t);
        return NULL;
    }
    return t;
}

Symbol *symtable_search(SymTable *t, const char *key) {
    if (!t || !t->slots || t->capacity == 0 || !key) {
        return NULL;
    }

    size_t h = hash1(key);
    for (size_t i = 0; i < t->capacity; i++) {
        size_t index = (h + i) % t->capacity;
        Symbol *slot = &t->slots[index];

        if (slot->state == SLOT_EMPTY) {
            return NULL;
        }
        if (slot->state == SLOT_OCCUPIED && strcmp(slot->key, key) == 0) {
            return slot;
        }
    }

    return NULL;
}

int symtable_rehash(SymTable *t, size_t new_capacity) {
    if (!t) return 99;
    if (new_capacity <= t->capacity) {
        new_capacity = t->capacity * 2 + 1;
    }

    Symbol *new_slots = (Symbol *)calloc(new_capacity, sizeof(Symbol));
    if (!new_slots) {
        return 99;
    }

    size_t new_size = 0;
    for (size_t i = 0; i < t->capacity; i++) {
        if (t->slots[i].state == SLOT_OCCUPIED) {
            size_t h = hash1(t->slots[i].key);
            for (size_t step = 0; step < new_capacity; step++) {
                size_t idx = (h + step) % new_capacity;
                if (new_slots[idx].state == SLOT_EMPTY) {
                    new_slots[idx] = t->slots[i];
                    new_slots[idx].state = SLOT_OCCUPIED;
                    new_size++;
                    break;
                }
            }
        }
    }

    free(t->slots);
    t->slots = new_slots;
    t->capacity = new_capacity;
    t->size = new_size;
    return 0;
}

int symtable_insert(SymTable *t, Symbol new_sym) {
    if (!t || !new_sym.key) return 99;

    if (t->capacity == 0) {
        if (!symtable_init_inplace(t, SYMTABLE_DEFAULT_CAPACITY)) {
            return 99;
        }
    }

    if ((double)(t->size + 1) / (double)t->capacity > 0.7) {
        int err = symtable_rehash(t, t->capacity * 2 + 1);
        if (err != 0) return err;
    }

    size_t h = hash1(new_sym.key);
    Symbol *first_deleted = NULL;
    Symbol *target_slot = NULL;

    for (size_t i = 0; i < t->capacity; i++) {
        size_t index = (h + i) % t->capacity;
        Symbol *slot = &t->slots[index];

        if (slot->state == SLOT_EMPTY) {
            target_slot = first_deleted ? first_deleted : slot;
            break;
        } else if (slot->state == SLOT_DELETED) {
            if (!first_deleted) {
                first_deleted = slot;
            }
        } else if (slot->state == SLOT_OCCUPIED) {
            if (strcmp(slot->key, new_sym.key) == 0) {
                return 4; // Duplicate declaration error (error code 4)
            }
        }
    }

    if (!target_slot) {
        target_slot = first_deleted;
    }
    if (!target_slot) {
        return 99;
    }

    bool was_empty = (target_slot->state == SLOT_EMPTY);
    int err = clone_symbol_data(target_slot, &new_sym);
    if (err != 0) {
        return err;
    }

    if (was_empty) {
        t->size++;
    }
    return 0;
}

void symtable_dispose(SymTable *t) {
    if (!t || !t->slots) return;
    for (size_t i = 0; i < t->capacity; i++) {
        if (t->slots[i].state == SLOT_OCCUPIED) {
            free_symbol_data(&t->slots[i]);
        }
    }
    free(t->slots);
    t->slots = NULL;
    t->capacity = 0;
    t->size = 0;
}

void symtable_free(SymTable *t) {
    if (!t) return;
    symtable_dispose(t);
    free(t);
}

/* --- ScopeStack operations --- */

bool scope_stack_init(ScopeStack *s) {
    if (!s) return false;
    s->tables = (SymTable **)calloc(SCOPE_STACK_INITIAL_CAP, sizeof(SymTable *));
    if (!s->tables) {
        s->top = 0;
        s->capacity = 0;
        return false;
    }
    s->top = 0;
    s->capacity = SCOPE_STACK_INITIAL_CAP;
    return true;
}

void scope_stack_free(ScopeStack *s) {
    if (!s) return;
    while (s->top > 0) {
        scope_pop(s);
    }
    free(s->tables);
    s->tables = NULL;
    s->capacity = 0;
}

int scope_push(ScopeStack *s) {
    if (!s) return 99;
    if (s->capacity == 0) {
        if (!scope_stack_init(s)) return 99;
    }
    if (s->top >= s->capacity) {
        size_t new_cap = s->capacity * 2;
        SymTable **new_tables = (SymTable **)realloc(s->tables, new_cap * sizeof(SymTable *));
        if (!new_tables) return 99;
        s->tables = new_tables;
        s->capacity = new_cap;
    }

    SymTable *new_table = symtable_init(SYMTABLE_DEFAULT_CAPACITY);
    if (!new_table) return 99;

    s->tables[s->top++] = new_table;
    return 0;
}

void scope_pop(ScopeStack *s) {
    if (!s || s->top == 0) return;
    s->top--;
    symtable_free(s->tables[s->top]);
    s->tables[s->top] = NULL;
}

int scope_declare_var(ScopeStack *s, const char *name, DataType type, bool is_mutable, int unique_id, Symbol **out_sym) {
    if (!s || s->top == 0 || !name) return 99;

    Symbol sym;
    memset(&sym, 0, sizeof(Symbol));
    sym.state = SLOT_OCCUPIED;
    sym.key = (char *)name;
    sym.kind = SYM_VAR;
    sym.type = type;
    sym.is_mutable = is_mutable;
    sym.unique_id = unique_id;

    SymTable *top_table = s->tables[s->top - 1];
    int err = symtable_insert(top_table, sym);
    if (err != 0) {
        return err;
    }

    if (out_sym) {
        *out_sym = symtable_search(top_table, name);
    }
    return 0;
}

Symbol *scope_lookup_var(ScopeStack *s, const char *name) {
    if (!s || s->top == 0 || !name) return NULL;

    for (size_t i = s->top; i > 0; i--) {
        Symbol *found = symtable_search(s->tables[i - 1], name);
        if (found) {
            return found;
        }
    }
    return NULL;
}

const char *datatype_to_str(DataType type) {
    switch ((int)type) {
        case TYPE_INT:     return "Int";
        case TYPE_DOUBLE:  return "Double";
        case TYPE_STRING:  return "String";
        case TYPE_UNIT:    return "Unit";
        case TYPE_BOOLEAN: return "Boolean";
        case TYPE_ANY:     return "Any";
        default:           return "Unknown";
    }
}
