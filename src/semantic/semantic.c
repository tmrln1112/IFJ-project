/**
 * Project IFJ26 compiler
 * File: semantic.c
 * Description: two-pass semantic analysis
 * Author: <xstrelm00> Maksym Streltsov
 */

#include "semantic.h"
#include <stdlib.h>
#include <string.h>

static char *sem_strdup(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s) + 1;
  char *copy = (char *)malloc(len);
  if (copy) {
    memcpy(copy, s, len);
  }
  return copy;
}

bool semantic_context_init(SemanticContext *ctx) {
  if (!ctx)
    return false;
  ctx->global_funcs = symtable_init(61);
  if (!ctx->global_funcs) {
    return false;
  }
  if (!scope_stack_init(&ctx->local_scopes)) {
    symtable_free(ctx->global_funcs);
    ctx->global_funcs = NULL;
    return false;
  }
  ctx->current_func = NULL;
  ctx->next_var_id = 1;
  return true;
}

void semantic_context_free(SemanticContext *ctx) {
  if (!ctx)
    return;
  scope_stack_free(&ctx->local_scopes);
  symtable_free(ctx->global_funcs);
  ctx->global_funcs = NULL;
  ctx->current_func = NULL;
}

// implicit Int -> Double widening gets flagged directly on parser node for
// codegen
static void mark_widening(ASTNode *node) {
  if (!node)
    return;
  node->conv_to_double = true;
  if (node->parser_node)
    node->parser_node->widen_to_double = true;
}

bool check_type_compat(DataType expected, DataType actual,
                       ASTNode *actual_node) {
  if (expected == TYPE_ANY && actual != TYPE_UNKNOWN)
    return true;
  if (expected == actual)
    return true;
  if (expected == (DataType)TYPE_DOUBLE && actual == (DataType)TYPE_INT) {
    mark_widening(actual_node);
    return true;
  }
  return false;
}

// builtins

static int add_builtin_func(SymTable *table, const char *name,
                            DataType ret_type, size_t param_count,
                            const DataType *param_types) {
  Symbol sym;
  memset(&sym, 0, sizeof(Symbol));
  sym.key = (char *)name;
  sym.kind = SYM_FUNC;
  sym.return_type = ret_type;
  sym.has_explicit_return = true;
  sym.has_parens = true;
  sym.num_param_lists = 1;
  sym.infer_state = INFER_DONE;

  ParamList plist;
  plist.count = param_count;
  plist.types = (DataType *)param_types;
  plist.names = NULL;
  sym.param_lists = &plist;

  return symtable_insert(table, sym);
}

int semantic_register_builtins(SemanticContext *ctx) {
  if (!ctx || !ctx->global_funcs)
    return SEM_ERR_INTERNAL;

  int err = 0;
  const DataType p_any[] = {TYPE_ANY};
  const DataType p_dbl[] = {(DataType)TYPE_DOUBLE};
  const DataType p_str[] = {(DataType)TYPE_STRING};
  const DataType p_int[] = {(DataType)TYPE_INT};
  const DataType p_substr[] = {(DataType)TYPE_STRING, (DataType)TYPE_INT,
                               (DataType)TYPE_INT};
  const DataType p_ord[] = {(DataType)TYPE_STRING, (DataType)TYPE_INT};

  if ((err = add_builtin_func(ctx->global_funcs, "print", (DataType)TYPE_UNIT,
                              1, p_any)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "readLine",
                              (DataType)TYPE_STRING, 0, NULL)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "readInt", (DataType)TYPE_INT,
                              0, NULL)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "readDouble",
                              (DataType)TYPE_DOUBLE, 0, NULL)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "readBoolean", TYPE_BOOLEAN, 0,
                              NULL)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "toStr", (DataType)TYPE_STRING,
                              1, p_any)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "toInt", (DataType)TYPE_INT, 1,
                              p_dbl)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "strLength",
                              (DataType)TYPE_INT, 1, p_str)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "subStr",
                              (DataType)TYPE_STRING, 3, p_substr)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "ord", (DataType)TYPE_INT, 2,
                              p_ord)) != 0)
    return err;
  if ((err = add_builtin_func(ctx->global_funcs, "chr", (DataType)TYPE_STRING,
                              1, p_int)) != 0)
    return err;

  return SEM_OK;
}

// used to tell error 6 (wrong receiver type) apart from error 3 (unknown
// member)
static bool is_known_valmem_name(const char *member_name) {
  if (!member_name)
    return false;
  return (strcmp(member_name, "toString") == 0 ||
          strcmp(member_name, "toInt") == 0 ||
          strcmp(member_name, "floor") == 0 ||
          strcmp(member_name, "length") == 0);
}

// param names must be unique across all (...) lists, otherwise err 4
static int check_unique_params(const ASTNode *fn_node) {
  for (size_t i1 = 0; i1 < fn_node->num_param_lists; i1++) {
    for (size_t j1 = 0; j1 < fn_node->param_lists[i1].count; j1++) {
      const char *name1 = fn_node->param_lists[i1].names
                              ? fn_node->param_lists[i1].names[j1]
                              : NULL;
      if (!name1)
        continue;

      for (size_t i2 = i1; i2 < fn_node->num_param_lists; i2++) {
        size_t start_j2 = (i2 == i1) ? (j1 + 1) : 0;
        for (size_t j2 = start_j2; j2 < fn_node->param_lists[i2].count; j2++) {
          const char *name2 = fn_node->param_lists[i2].names
                                  ? fn_node->param_lists[i2].names[j2]
                                  : NULL;
          if (name2 && strcmp(name1, name2) == 0) {
            return SEM_ERR_REDEF;
          }
        }
      }
    }
  }

  return SEM_OK;
}

// pass 1: put top-level function into global table
static int register_top_level_func(SemanticContext *ctx, ASTNode *fn_node,
                                   int *main_count) {
  if (!fn_node || !fn_node->name)
    return SEM_ERR_INTERNAL;

  int err = check_unique_params(fn_node);
  if (err != SEM_OK) {
    return err;
  }

  if (fn_node->is_main) {
    (*main_count)++;
    if (*main_count > 1) {
      return SEM_ERR_REDEF;
    }
    // @main only takes ()
    if (!fn_node->has_parens || fn_node->num_param_lists != 1 ||
        !fn_node->param_lists || fn_node->param_lists[0].count != 0) {
      return SEM_ERR_CALL;
    }
    if (fn_node->has_explicit_return &&
        fn_node->declared_type != (DataType)TYPE_UNIT) {
      return SEM_ERR_TYPE;
    }
  }

  Symbol sym;
  memset(&sym, 0, sizeof(Symbol));
  sym.key = fn_node->name;
  sym.kind = SYM_FUNC;
  sym.has_explicit_return = fn_node->has_explicit_return;
  sym.return_type =
      fn_node->has_explicit_return ? fn_node->declared_type : TYPE_UNKNOWN;
  sym.has_parens = fn_node->has_parens;
  sym.num_param_lists = fn_node->num_param_lists;
  sym.param_lists = fn_node->param_lists;
  sym.is_main = fn_node->is_main;
  sym.infer_state = INFER_NONE;
  sym.ast_node = fn_node;

  return symtable_insert(ctx->global_funcs, sym);
}

static bool guarantees_return(ASTNode *node) {
  if (!node)
    return false;
  switch (node->kind) {
  case AST_RETURN:
    return true;
  case AST_BLOCK:
    for (size_t i = 0; i < node->item_count; i++) {
      if (guarantees_return(node->items[i])) {
        return true;
      }
    }
    return false;
  case AST_IF:
    if (node->then_branch && node->else_branch) {
      return guarantees_return(node->then_branch) &&
             guarantees_return(node->else_branch);
    }
    return false;
  default:
    return false;
  }
}

// grabs the trailing expr of a body so we can attach widening/discard flags to
// it
static ASTNode *get_last_expr_node(ASTNode *body) {
  if (!body)
    return NULL;
  if (body->kind == AST_BLOCK) {
    if (body->item_count == 0)
      return NULL;
    return get_last_expr_node(body->items[body->item_count - 1]);
  }
  return body;
}

int analyze_function(SemanticContext *ctx, Symbol *fn) {
  if (!ctx || !fn)
    return SEM_ERR_INTERNAL;

  if (fn->infer_state == INFER_DONE) {
    return SEM_OK;
  }
  if (fn->infer_state == INFER_IN_PROGRESS) {
    // hit recursion while still inferring return type -> needs explicit ':
    // Type'
    if (!fn->has_explicit_return) {
      return SEM_ERR_TYPE;
    }
    return SEM_OK;
  }

  fn->infer_state = INFER_IN_PROGRESS;

  // save current func + scopes since type inference can jump into another func
  Symbol *prev_func = ctx->current_func;
  ScopeStack prev_scopes = ctx->local_scopes;

  ctx->current_func = fn;
  if (!scope_stack_init(&ctx->local_scopes)) {
    ctx->current_func = prev_func;
    ctx->local_scopes = prev_scopes;
    return SEM_ERR_INTERNAL;
  }

  int err = scope_push(&ctx->local_scopes);
  if (err != SEM_OK) {
    scope_stack_free(&ctx->local_scopes);
    ctx->current_func = prev_func;
    ctx->local_scopes = prev_scopes;
    return err;
  }

  // bind params (all immutable)
  for (size_t i = 0; i < fn->num_param_lists; i++) {
    for (size_t j = 0; j < fn->param_lists[i].count; j++) {
      const char *pname =
          fn->param_lists[i].names ? fn->param_lists[i].names[j] : NULL;
      DataType ptype = fn->param_lists[i].types[j];
      if (pname) {
        err = scope_declare_var(&ctx->local_scopes, pname, ptype, false,
                                ctx->next_var_id++, NULL);
        if (err != SEM_OK) {
          scope_stack_free(&ctx->local_scopes);
          ctx->current_func = prev_func;
          ctx->local_scopes = prev_scopes;
          return err;
        }
      }
    }
  }

  // trailing if/else acts as return value unless func explicitly returns Unit
  ASTNode *body_node = fn->ast_node ? fn->ast_node->body : NULL;
  if (body_node) {
    ASTNode *last_node = get_last_expr_node(body_node);
    if (last_node && last_node->kind == AST_IF && last_node->else_branch) {
      if (!fn->has_explicit_return || fn->return_type != (DataType)TYPE_UNIT) {
        last_node->is_expression = true;
      }
    }
  }

  DataType body_type = analyze_block(ctx, body_node, &err);
  if (err != SEM_OK) {
    scope_stack_free(&ctx->local_scopes);
    ctx->current_func = prev_func;
    ctx->local_scopes = prev_scopes;
    return err;
  }

  if (fn->has_explicit_return) {
    if (fn->return_type == (DataType)TYPE_UNIT) {
      ASTNode *last = get_last_expr_node(body_node);
      if (body_type != (DataType)TYPE_UNIT && last && last->parser_node) {
        last->parser_node->discard_value = true;
      }
    } else {
      if (!guarantees_return(body_node)) {
        ASTNode *last_expr = get_last_expr_node(body_node);
        if (!check_type_compat(fn->return_type, body_type, last_expr)) {
          scope_stack_free(&ctx->local_scopes);
          ctx->current_func = prev_func;
          ctx->local_scopes = prev_scopes;
          return SEM_ERR_TYPE;
        }
      }
    }
  } else {
    fn->return_type = body_type;
    if (fn->is_main && body_type != (DataType)TYPE_UNIT) {
      scope_stack_free(&ctx->local_scopes);
      ctx->current_func = prev_func;
      ctx->local_scopes = prev_scopes;
      return SEM_ERR_TYPE;
    }
  }

  if (fn->ast_node) {
    fn->ast_node->sem_type = fn->return_type;
  }

  scope_pop(&ctx->local_scopes);
  scope_stack_free(&ctx->local_scopes);
  ctx->current_func = prev_func;
  ctx->local_scopes = prev_scopes;

  fn->infer_state = INFER_DONE;
  return SEM_OK;
}

static int check_call_arg_lists(SemanticContext *ctx, Symbol *fn,
                                ASTNode *call_node) {
  if (call_node->num_arg_lists != fn->num_param_lists) {
    return SEM_ERR_CALL;
  }

  for (size_t i = 0; i < fn->num_param_lists; i++) {
    if (call_node->arg_lists[i].count != fn->param_lists[i].count) {
      return SEM_ERR_CALL;
    }
    for (size_t j = 0; j < fn->param_lists[i].count; j++) {
      ASTNode *arg_node = call_node->arg_lists[i].args[j];
      int err = SEM_OK;
      DataType arg_type = analyze_expr(ctx, arg_node, &err);
      if (err != SEM_OK) {
        return err;
      }
      if (!check_type_compat(fn->param_lists[i].types[j], arg_type, arg_node)) {
        return SEM_ERR_CALL;
      }
    }
  }

  return SEM_OK;
}

DataType analyze_block(SemanticContext *ctx, ASTNode *block_node, int *err) {
  if (!block_node) {
    return (DataType)TYPE_UNIT;
  }
  if (block_node->kind != AST_BLOCK) {
    return analyze_node(ctx, block_node, err);
  }

  if (block_node->item_count == 0) {
    block_node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  DataType last_type = (DataType)TYPE_UNIT;
  for (size_t i = 0; i < block_node->item_count; i++) {
    last_type = analyze_node(ctx, block_node->items[i], err);
    if (*err != SEM_OK) {
      return TYPE_UNKNOWN;
    }
  }

  block_node->sem_type = last_type;
  return last_type;
}

DataType analyze_expr(SemanticContext *ctx, ASTNode *node, int *err) {
  if (!node) {
    *err = SEM_ERR_TYPE;
    return TYPE_UNKNOWN;
  }
  if (node->kind == AST_IF) {
    node->is_expression = true;
  }
  return analyze_node(ctx, node, err);
}

DataType analyze_node(SemanticContext *ctx, ASTNode *node, int *err) {
  if (!ctx || !node || !err) {
    if (err)
      *err = SEM_ERR_INTERNAL;
    return TYPE_UNKNOWN;
  }
  if (*err != SEM_OK) {
    return TYPE_UNKNOWN;
  }

  switch (node->kind) {
  case AST_BLOCK: {
    int push_err = scope_push(&ctx->local_scopes);
    if (push_err != SEM_OK) {
      *err = push_err;
      return TYPE_UNKNOWN;
    }
    DataType btype = analyze_block(ctx, node, err);
    scope_pop(&ctx->local_scopes);
    return btype;
  }

  case AST_VAR_DECL: {
    DataType rhs_type = analyze_expr(ctx, node->init_expr, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    if (rhs_type == TYPE_UNKNOWN) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    DataType final_type = rhs_type;
    if (node->has_explicit_type) {
      if (!check_type_compat(node->declared_type, rhs_type, node->init_expr)) {
        *err = SEM_ERR_TYPE;
        return TYPE_UNKNOWN;
      }
      final_type = node->declared_type;
    }

    int uid = ctx->next_var_id++;
    int decl_err = scope_declare_var(&ctx->local_scopes, node->name, final_type,
                                     node->is_mutable, uid, NULL);
    if (decl_err != SEM_OK) {
      *err = decl_err; // duplicate in same block -> 4
      return TYPE_UNKNOWN;
    }

    node->unique_var_id = uid;
    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_ASSIGN: {
    Symbol *sym = scope_lookup_var(&ctx->local_scopes, node->name);
    if (!sym) {
      *err = SEM_ERR_UNDEF;
      return TYPE_UNKNOWN;
    }
    if (!sym->is_mutable) {
      *err = SEM_ERR_IMMUTABLE;
      return TYPE_UNKNOWN;
    }

    DataType rhs_type = analyze_expr(ctx, node->rhs, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    if (!check_type_compat(sym->type, rhs_type, node->rhs)) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    node->unique_var_id = sym->unique_id;
    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_IDENTIFIER: {
    Symbol *sym = scope_lookup_var(&ctx->local_scopes, node->name);
    if (sym) {
      node->sem_type = sym->type;
      node->unique_var_id = sym->unique_id;
      node->is_func_call = false;
      return sym->type;
    }

    Symbol *fn = symtable_search(ctx->global_funcs, node->name);
    if (!fn) {
      *err = SEM_ERR_UNDEF;
      return TYPE_UNKNOWN;
    }

    // defined with parens, called without them -> 5
    if (fn->has_parens) {
      *err = SEM_ERR_CALL;
      return TYPE_UNKNOWN;
    }

    // bare call of parameterless func (PARAMLISTS)
    if (fn->return_type == TYPE_UNKNOWN) {
      int fn_err = analyze_function(ctx, fn);
      if (fn_err != SEM_OK) {
        *err = fn_err;
        return TYPE_UNKNOWN;
      }
    }

    if (node->parser_node)
      node->parser_node->bare_call = true;
    node->is_func_call = true;
    node->sem_type = fn->return_type;
    return fn->return_type;
  }

  case AST_FUNC_CALL: {
    Symbol *fn = symtable_search(ctx->global_funcs, node->name);
    if (!fn) {
      *err = SEM_ERR_UNDEF;
      return TYPE_UNKNOWN;
    }

    // defined without parens (def x = 42), called as x() -> 5
    if (!fn->has_parens) {
      *err = SEM_ERR_CALL;
      return TYPE_UNKNOWN;
    }

    int call_err = check_call_arg_lists(ctx, fn, node);
    if (call_err != SEM_OK) {
      *err = call_err;
      return TYPE_UNKNOWN;
    }

    if (fn->return_type == TYPE_UNKNOWN) {
      int fn_err = analyze_function(ctx, fn);
      if (fn_err != SEM_OK) {
        *err = fn_err;
        return TYPE_UNKNOWN;
      }
    }

    node->is_func_call = true;
    node->sem_type = fn->return_type;
    return fn->return_type;
  }

  case AST_MEMBER_ACCESS: {
    DataType rec_type = analyze_expr(ctx, node->receiver, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    const char *mname = node->member_name;
    if (!mname) {
      *err = SEM_ERR_INTERNAL;
      return TYPE_UNKNOWN;
    }

    // valmem members; Int works for toInt/floor too via Int -> Double widening
    bool is_num =
        (rec_type == (DataType)TYPE_DOUBLE || rec_type == (DataType)TYPE_INT);
    bool is_tostring =
        (strcmp(mname, "toString") == 0 && rec_type != TYPE_UNKNOWN);
    bool is_num_toint = (is_num && strcmp(mname, "toInt") == 0);
    bool is_num_floor = (is_num && strcmp(mname, "floor") == 0);
    bool is_str_len =
        (rec_type == (DataType)TYPE_STRING && strcmp(mname, "length") == 0);

    if (is_tostring || is_num_toint || is_num_floor || is_str_len) {
      if (node->has_parens) {
        // no parens allowed on valmem properties
        *err = SEM_ERR_CALL;
        return TYPE_UNKNOWN;
      }
      if ((is_num_toint || is_num_floor) && rec_type == (DataType)TYPE_INT) {
        mark_widening(node->receiver);
      }
      DataType res_type = (DataType)TYPE_STRING;
      if (is_num_toint || is_str_len) {
        res_type = (DataType)TYPE_INT;
      } else if (is_num_floor) {
        res_type = (DataType)TYPE_DOUBLE;
      }
      node->sem_type = res_type;
      return res_type;
    }

    *err = is_known_valmem_name(mname) ? SEM_ERR_TYPE : SEM_ERR_UNDEF;
    return TYPE_UNKNOWN;
  }

  case AST_IF: {
    DataType cond_type = analyze_expr(ctx, node->cond, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;
    if (cond_type != TYPE_BOOLEAN) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    int p_err = scope_push(&ctx->local_scopes);
    if (p_err != SEM_OK) {
      *err = p_err;
      return TYPE_UNKNOWN;
    }
    if (node->is_expression && node->then_branch) {
      ASTNode *last_then = get_last_expr_node(node->then_branch);
      if (last_then && last_then->kind == AST_IF)
        last_then->is_expression = true;
    }
    DataType then_type = analyze_block(ctx, node->then_branch, err);
    scope_pop(&ctx->local_scopes);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    DataType else_type = (DataType)TYPE_UNIT;
    if (node->else_branch) {
      p_err = scope_push(&ctx->local_scopes);
      if (p_err != SEM_OK) {
        *err = p_err;
        return TYPE_UNKNOWN;
      }
      if (node->is_expression) {
        ASTNode *last_else = get_last_expr_node(node->else_branch);
        if (last_else && last_else->kind == AST_IF)
          last_else->is_expression = true;
      }
      else_type = analyze_block(ctx, node->else_branch, err);
      scope_pop(&ctx->local_scopes);
      if (*err != SEM_OK)
        return TYPE_UNKNOWN;
    }

    if (node->is_expression) {
      if (node->parser_node)
        node->parser_node->if_has_value = true;
      if (!node->else_branch) {
        *err = SEM_ERR_TYPE; // if-expr needs else
        return TYPE_UNKNOWN;
      }
      if (then_type == else_type) {
        node->sem_type = then_type;
        return then_type;
      }
      if (then_type == (DataType)TYPE_INT &&
          else_type == (DataType)TYPE_DOUBLE) {
        ASTNode *then_last = get_last_expr_node(node->then_branch);
        mark_widening(then_last);
        node->sem_type = (DataType)TYPE_DOUBLE;
        return (DataType)TYPE_DOUBLE;
      }
      if (then_type == (DataType)TYPE_DOUBLE &&
          else_type == (DataType)TYPE_INT) {
        ASTNode *else_last = get_last_expr_node(node->else_branch);
        mark_widening(else_last);
        node->sem_type = (DataType)TYPE_DOUBLE;
        return (DataType)TYPE_DOUBLE;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    // statement form
    if (!node->else_branch) {
      node->sem_type = (DataType)TYPE_UNIT;
      return (DataType)TYPE_UNIT;
    }
    if (then_type == else_type) {
      node->sem_type = then_type;
      return then_type;
    }
    if (then_type == (DataType)TYPE_INT && else_type == (DataType)TYPE_DOUBLE) {
      ASTNode *then_last = get_last_expr_node(node->then_branch);
      mark_widening(then_last);
      node->sem_type = (DataType)TYPE_DOUBLE;
      return (DataType)TYPE_DOUBLE;
    }
    if (then_type == (DataType)TYPE_DOUBLE && else_type == (DataType)TYPE_INT) {
      ASTNode *else_last = get_last_expr_node(node->else_branch);
      mark_widening(else_last);
      node->sem_type = (DataType)TYPE_DOUBLE;
      return (DataType)TYPE_DOUBLE;
    }
    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_WHILE: {
    DataType cond_type = analyze_expr(ctx, node->cond, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;
    if (cond_type != TYPE_BOOLEAN) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    int p_err = scope_push(&ctx->local_scopes);
    if (p_err != SEM_OK) {
      *err = p_err;
      return TYPE_UNKNOWN;
    }
    (void)analyze_block(ctx, node->body, err);
    scope_pop(&ctx->local_scopes);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_FOR: {
    DataType from_type = analyze_expr(ctx, node->from_expr, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;
    DataType end_type = analyze_expr(ctx, node->end_expr, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    if (from_type != (DataType)TYPE_INT || end_type != (DataType)TYPE_INT) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    if (node->step_expr) {
      DataType step_type = analyze_expr(ctx, node->step_expr, err);
      if (*err != SEM_OK)
        return TYPE_UNKNOWN;
      if (step_type != (DataType)TYPE_INT) {
        *err = SEM_ERR_TYPE;
        return TYPE_UNKNOWN;
      }
      // literal 0 or -0 in 'by' is error 10
      ASTNode *s = node->step_expr;
      if (s->kind == AST_LITERAL && s->is_int_literal && s->int_val == 0) {
        *err = SEM_ERR_OTHER;
        return TYPE_UNKNOWN;
      }
      if (s->kind == AST_UNOP && s->op == OP_NEG && s->operand &&
          s->operand->kind == AST_LITERAL && s->operand->is_int_literal &&
          s->operand->int_val == 0) {
        *err = SEM_ERR_OTHER;
        return TYPE_UNKNOWN;
      }
    }

    int p_err = scope_push(&ctx->local_scopes);
    if (p_err != SEM_OK) {
      *err = p_err;
      return TYPE_UNKNOWN;
    }

    int uid = ctx->next_var_id++;
    int decl_err = scope_declare_var(&ctx->local_scopes, node->iter_name,
                                     (DataType)TYPE_INT, false, uid, NULL);
    if (decl_err != SEM_OK) {
      scope_pop(&ctx->local_scopes);
      *err = decl_err;
      return TYPE_UNKNOWN;
    }
    node->unique_var_id = uid;

    for (size_t i = 0; i < node->guard_count; i++) {
      DataType g_type = analyze_expr(ctx, node->guards[i], err);
      if (*err != SEM_OK) {
        scope_pop(&ctx->local_scopes);
        return TYPE_UNKNOWN;
      }
      if (g_type != TYPE_BOOLEAN) {
        scope_pop(&ctx->local_scopes);
        *err = SEM_ERR_TYPE;
        return TYPE_UNKNOWN;
      }
    }

    (void)analyze_block(ctx, node->body, err);
    scope_pop(&ctx->local_scopes);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_RETURN: {
    // using 'return' requires explicit ': Type' on the function
    if (!ctx->current_func || !ctx->current_func->has_explicit_return) {
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    if (!node->return_expr) {
      if (ctx->current_func->return_type != (DataType)TYPE_UNIT) {
        *err = SEM_ERR_TYPE;
        return TYPE_UNKNOWN;
      }
    } else {
      DataType ret_type = analyze_expr(ctx, node->return_expr, err);
      if (*err != SEM_OK)
        return TYPE_UNKNOWN;

      if (ctx->current_func->return_type == (DataType)TYPE_UNIT) {
        if (ret_type != (DataType)TYPE_UNIT && node->return_expr->parser_node) {
          node->return_expr->parser_node->discard_value = true;
        }
      } else {
        if (!check_type_compat(ctx->current_func->return_type, ret_type,
                               node->return_expr)) {
          *err = SEM_ERR_TYPE;
          return TYPE_UNKNOWN;
        }
      }
    }

    node->sem_type = (DataType)TYPE_UNIT;
    return (DataType)TYPE_UNIT;
  }

  case AST_UNOP: {
    DataType op_type = analyze_expr(ctx, node->operand, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    if (node->op == OP_NEG) {
      if (op_type == (DataType)TYPE_INT || op_type == (DataType)TYPE_DOUBLE) {
        node->sem_type = op_type;
        return op_type;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    } else if (node->op == OP_NOT) {
      if (op_type == TYPE_BOOLEAN) {
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    *err = SEM_ERR_TYPE;
    return TYPE_UNKNOWN;
  }

  case AST_BINOP: {
    DataType ltype = analyze_expr(ctx, node->left, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;
    DataType rtype = analyze_expr(ctx, node->right, err);
    if (*err != SEM_OK)
      return TYPE_UNKNOWN;

    bool l_num =
        (ltype == (DataType)TYPE_INT || ltype == (DataType)TYPE_DOUBLE);
    bool r_num =
        (rtype == (DataType)TYPE_INT || rtype == (DataType)TYPE_DOUBLE);

    switch (node->op) {
    case OP_SUB:
    case OP_MUL:
    case OP_DIV: {
      if (!l_num || !r_num) {
        *err = SEM_ERR_TYPE;
        return TYPE_UNKNOWN;
      }
      if (ltype == (DataType)TYPE_INT && rtype == (DataType)TYPE_INT) {
        node->sem_type = (DataType)TYPE_INT;
        return (DataType)TYPE_INT;
      }
      if (ltype == (DataType)TYPE_INT)
        mark_widening(node->left);
      if (rtype == (DataType)TYPE_INT)
        mark_widening(node->right);
      node->sem_type = (DataType)TYPE_DOUBLE;
      return (DataType)TYPE_DOUBLE;
    }

    case OP_ADD: {
      if (ltype == (DataType)TYPE_STRING && rtype == (DataType)TYPE_STRING) {
        node->sem_type = (DataType)TYPE_STRING;
        return (DataType)TYPE_STRING;
      }
      if (l_num && r_num) {
        if (ltype == (DataType)TYPE_INT && rtype == (DataType)TYPE_INT) {
          node->sem_type = (DataType)TYPE_INT;
          return (DataType)TYPE_INT;
        }
        if (ltype == (DataType)TYPE_INT)
          mark_widening(node->left);
        if (rtype == (DataType)TYPE_INT)
          mark_widening(node->right);
        node->sem_type = (DataType)TYPE_DOUBLE;
        return (DataType)TYPE_DOUBLE;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE: {
      if (l_num && r_num) {
        if (ltype == (DataType)TYPE_DOUBLE && rtype == (DataType)TYPE_INT) {
          mark_widening(node->right);
        } else if (ltype == (DataType)TYPE_INT &&
                   rtype == (DataType)TYPE_DOUBLE) {
          mark_widening(node->left);
        }
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      if (ltype == (DataType)TYPE_STRING && rtype == (DataType)TYPE_STRING) {
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    case OP_EQ:
    case OP_NEQ: {
      if (l_num && r_num) {
        if (ltype == (DataType)TYPE_DOUBLE && rtype == (DataType)TYPE_INT) {
          mark_widening(node->right);
        } else if (ltype == (DataType)TYPE_INT &&
                   rtype == (DataType)TYPE_DOUBLE) {
          mark_widening(node->left);
        }
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      if (ltype == rtype &&
          (ltype == (DataType)TYPE_STRING || ltype == TYPE_BOOLEAN ||
           ltype == (DataType)TYPE_UNIT)) {
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    case OP_AND:
    case OP_OR: {
      if (ltype == TYPE_BOOLEAN && rtype == TYPE_BOOLEAN) {
        node->sem_type = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
      }
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }

    default:
      *err = SEM_ERR_TYPE;
      return TYPE_UNKNOWN;
    }
  }

  case AST_LITERAL: {
    node->sem_type = node->literal_type;
    return node->literal_type;
  }

  default:
    *err = SEM_ERR_INTERNAL;
    return TYPE_UNKNOWN;
  }
}

static int check_func_symbol(Symbol *sym, void *user_data) {
  if (sym->kind == SYM_FUNC && sym->ast_node != NULL) {
    return analyze_function((SemanticContext *)user_data, sym);
  }
  return SEM_OK;
}

int semantic_analyze(ASTNode *root) {
  if (!root)
    return SEM_ERR_UNDEF;

  SemanticContext ctx;
  if (!semantic_context_init(&ctx)) {
    return SEM_ERR_INTERNAL;
  }

  int err = semantic_register_builtins(&ctx);
  if (err != SEM_OK) {
    semantic_context_free(&ctx);
    return err;
  }

  // pass 1: collect all function headers
  int main_count = 0;
  for (size_t i = 0; i < root->item_count; i++) {
    ASTNode *item = root->items[i];
    if (!item)
      continue;

    if (item->kind == AST_FUNC_DEF) {
      err = register_top_level_func(&ctx, item, &main_count);
      if (err != SEM_OK) {
        semantic_context_free(&ctx);
        return err;
      }
    }
  }

  if (main_count == 0) {
    semantic_context_free(&ctx);
    return SEM_ERR_UNDEF;
  }

  // pass 2: check bodies
  err = symtable_foreach(ctx.global_funcs, check_func_symbol, &ctx);
  if (err != SEM_OK) {
    semantic_context_free(&ctx);
    return err;
  }

  semantic_context_free(&ctx);
  return SEM_OK;
}

// ast cleanup + bridge from parser ast_node

void semantic_ast_free(ASTNode *node) {
  if (!node)
    return;

  for (size_t i = 0; i < node->item_count; i++) {
    semantic_ast_free(node->items[i]);
  }
  free(node->items);

  free(node->name);
  free(node->member_name);
  free(node->iter_name);
  free(node->str_val);

  if (node->param_lists) {
    for (size_t i = 0; i < node->num_param_lists; i++) {
      free(node->param_lists[i].types);
      if (node->param_lists[i].names) {
        for (size_t j = 0; j < node->param_lists[i].count; j++) {
          free(node->param_lists[i].names[j]);
        }
        free(node->param_lists[i].names);
      }
    }
    free(node->param_lists);
  }

  if (node->arg_lists) {
    for (size_t i = 0; i < node->num_arg_lists; i++) {
      for (size_t j = 0; j < node->arg_lists[i].count; j++) {
        semantic_ast_free(node->arg_lists[i].args[j]);
      }
      free(node->arg_lists[i].args);
    }
    free(node->arg_lists);
  }

  semantic_ast_free(node->body);
  semantic_ast_free(node->init_expr);
  semantic_ast_free(node->rhs);
  semantic_ast_free(node->receiver);
  semantic_ast_free(node->cond);
  semantic_ast_free(node->then_branch);
  semantic_ast_free(node->else_branch);
  semantic_ast_free(node->from_expr);
  semantic_ast_free(node->end_expr);
  semantic_ast_free(node->step_expr);

  for (size_t i = 0; i < node->guard_count; i++) {
    semantic_ast_free(node->guards[i]);
  }
  free(node->guards);

  semantic_ast_free(node->return_expr);
  semantic_ast_free(node->left);
  semantic_ast_free(node->right);
  semantic_ast_free(node->operand);

  free(node);
}

static ASTNode *alloc_sem_node(ASTNodeKind kind) {
  ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
  if (n) {
    n->kind = kind;
    n->sem_type = TYPE_UNKNOWN;
  }
  return n;
}

static DataType map_parser_dtype(data_type_t dt) {
  switch (dt) {
  case TYPE_INT:
    return (DataType)TYPE_INT;
  case TYPE_DOUBLE:
    return (DataType)TYPE_DOUBLE;
  case TYPE_STRING:
    return (DataType)TYPE_STRING;
  case TYPE_BOOL:
    return TYPE_BOOLEAN;
  case TYPE_UNIT:
    return (DataType)TYPE_UNIT;
  default:
    return TYPE_UNKNOWN;
  }
}

static ASTNode *convert_parser_node(ast_node *pnode);

static ASTNode *convert_parser_node_impl(ast_node *pnode) {
  if (!pnode)
    return NULL;

  switch (pnode->node_type) {
  case NODE_BLOCK: {
    ASTNode *blk = alloc_sem_node(AST_BLOCK);
    if (!blk)
      return NULL;

    size_t count = 0;
    for (ast_node *cur = pnode; cur != NULL; cur = cur->right) {
      if (cur->left)
        count++;
    }
    if (count > 0) {
      blk->items = (ASTNode **)calloc(count, sizeof(ASTNode *));
      if (!blk->items) {
        semantic_ast_free(blk);
        return NULL;
      }
      size_t idx = 0;
      for (ast_node *cur = pnode; cur != NULL; cur = cur->right) {
        if (cur->left) {
          blk->items[idx++] = convert_parser_node(cur->left);
        }
      }
      blk->item_count = idx;
    }
    return blk;
  }

  case NODE_VAR_DEF:
  case NODE_VAL_DEF: {
    ASTNode *decl = alloc_sem_node(AST_VAR_DECL);
    if (!decl)
      return NULL;
    decl->name = sem_strdup(pnode->value);
    decl->is_mutable = (pnode->node_type == NODE_VAR_DEF);
    if (pnode->left && pnode->left->node_type == NODE_TYPE) {
      decl->has_explicit_type = true;
      decl->declared_type = map_parser_dtype(pnode->left->data_type);
    }
    decl->init_expr = convert_parser_node(pnode->right);
    return decl;
  }

  case NODE_ASSIGNMENT: {
    ASTNode *asgn = alloc_sem_node(AST_ASSIGN);
    if (!asgn)
      return NULL;
    if (pnode->left) {
      asgn->name = sem_strdup(pnode->left->value);
    }
    asgn->rhs = convert_parser_node(pnode->right);
    return asgn;
  }

  case NODE_RETURN: {
    ASTNode *ret = alloc_sem_node(AST_RETURN);
    if (!ret)
      return NULL;
    ret->return_expr = convert_parser_node(pnode->left);
    return ret;
  }

  case NODE_IF: {
    ASTNode *ifn = alloc_sem_node(AST_IF);
    if (!ifn)
      return NULL;
    ifn->cond = convert_parser_node(pnode->left);
    if (pnode->right && pnode->right->node_type == NODE_IF_BODY) {
      ifn->then_branch = convert_parser_node(pnode->right->left);
      ifn->else_branch = convert_parser_node(pnode->right->right);
    }
    return ifn;
  }

  case NODE_FOR: {
    ASTNode *loop = alloc_sem_node(AST_FOR);
    if (!loop)
      return NULL;
    ast_node *start = pnode->left;
    ast_node *end = start->right;
    ast_node *step = end->right;
    loop->iter_name = sem_strdup(pnode->value);
    loop->from_expr = convert_parser_node(start->left);
    loop->end_expr = convert_parser_node(end->left);
    loop->step_expr = convert_parser_node(step->left);
    loop->is_until = !pnode->inclusive_range;
    loop->body = convert_parser_node(pnode->right);
    size_t count = 0;
    for (ast_node *g = step->right; g; g = g->right)
      count++;
    if (count) {
      loop->guards = calloc(count, sizeof(ASTNode *));
      if (!loop->guards) {
        semantic_ast_free(loop);
        return NULL;
      }
      for (ast_node *g = step->right; g; g = g->right)
        loop->guards[loop->guard_count++] = convert_parser_node(g->left);
    }
    return loop;
  }

  case NODE_WHILE: {
    ASTNode *wh = alloc_sem_node(AST_WHILE);
    if (!wh)
      return NULL;
    wh->cond = convert_parser_node(pnode->left);
    wh->body = convert_parser_node(pnode->right);
    return wh;
  }

  case NODE_FUN_CALL: {
    ASTNode *call = alloc_sem_node(AST_FUNC_CALL);
    if (!call)
      return NULL;
    call->name = sem_strdup(pnode->value);
    call->has_parens = true;
    for (ast_node *list = pnode->left; list; list = list->right)
      call->num_arg_lists++;
    call->arg_lists = calloc(call->num_arg_lists, sizeof(ArgList));
    if (!call->arg_lists) {
      semantic_ast_free(call);
      return NULL;
    }

    size_t group = 0;
    for (ast_node *list = pnode->left; list; list = list->right, group++) {
      ArgList *args = &call->arg_lists[group];
      for (ast_node *a = list->left; a; a = a->left)
        args->count++;
      if (!args->count)
        continue;
      args->args = calloc(args->count, sizeof(ASTNode *));
      if (!args->args) {
        args->count = 0;
        semantic_ast_free(call);
        return NULL;
      }
      size_t idx = 0;
      for (ast_node *a = list->left; a; a = a->left)
        args->args[idx++] = convert_parser_node(a->right);
    }
    return call;
  }

  case NODE_MEMBER: {
    ASTNode *member = alloc_sem_node(AST_MEMBER_ACCESS);
    if (!member)
      return NULL;
    member->receiver = convert_parser_node(pnode->left);
    member->member_name = sem_strdup(pnode->value);
    member->has_parens = pnode->right != NULL;
    return member;
  }

  case NODE_ARGUMENT:
  case NODE_TERM: {
    if (pnode->data_type == TYPE_ID) {
      ASTNode *id = alloc_sem_node(AST_IDENTIFIER);
      if (id)
        id->name = sem_strdup(pnode->value);
      return id;
    } else {
      ASTNode *lit = alloc_sem_node(AST_LITERAL);
      if (!lit)
        return NULL;
      lit->literal_type = map_parser_dtype(pnode->data_type);
      if (pnode->data_type == TYPE_INT && pnode->value) {
        lit->is_int_literal = true;
        lit->int_val = strtoll(pnode->value, NULL, 10);
      }
      return lit;
    }
  }

  case NODE_EXPRESSION:
  case NODE_CONDITION: {
    if (!pnode->value)
      return convert_parser_node(pnode->left);

    if (!pnode->right &&
        (strcmp(pnode->value, "-") == 0 || strcmp(pnode->value, "!") == 0)) {
      ASTNode *unary = alloc_sem_node(AST_UNOP);
      if (!unary)
        return NULL;
      unary->op = pnode->value[0] == '!' ? OP_NOT : OP_NEG;
      unary->operand = convert_parser_node(pnode->left);
      return unary;
    }

    OperatorType op = OP_NONE;
    const char *operators[] = {
        [OP_ADD] = "+", [OP_SUB] = "-",  [OP_MUL] = "*",  [OP_DIV] = "/",
        [OP_LT] = "<",  [OP_LE] = "<=",  [OP_GT] = ">",   [OP_GE] = ">=",
        [OP_EQ] = "==", [OP_NEQ] = "!=", [OP_AND] = "&&", [OP_OR] = "||"};
    for (int i = OP_ADD; i <= OP_OR; i++) {
      if (strcmp(pnode->value, operators[i]) == 0) {
        op = (OperatorType)i;
        break;
      }
    }

    ASTNode *binary = alloc_sem_node(AST_BINOP);
    if (!binary)
      return NULL;
    binary->op = op;
    binary->left = convert_parser_node(pnode->left);
    binary->right = convert_parser_node(pnode->right);
    return binary;
  }

  default:
    return NULL;
  }
}

static ASTNode *convert_parser_node(ast_node *pnode) {
  ASTNode *node = convert_parser_node_impl(pnode);
  if (node && !node->parser_node)
    node->parser_node = pnode;
  return node;
}

int semantic_analyze_parser_ast(ast_node *parser_root) {
  if (!parser_root)
    return SEM_ERR_UNDEF;

  ASTNode *prog = alloc_sem_node(AST_PROGRAM);
  if (!prog)
    return SEM_ERR_INTERNAL;

  size_t fn_count = 0;
  for (ast_node *cur = parser_root; cur != NULL; cur = cur->right) {
    if (cur->left && cur->left->node_type == NODE_FUN_DEF)
      fn_count++;
  }

  if (fn_count > 0) {
    prog->items = (ASTNode **)calloc(fn_count, sizeof(ASTNode *));
    if (!prog->items) {
      semantic_ast_free(prog);
      return SEM_ERR_INTERNAL;
    }
  }

  size_t idx = 0;
  for (ast_node *cur = parser_root; cur != NULL; cur = cur->right) {
    ast_node *fdef = cur->left;
    if (!fdef || fdef->node_type != NODE_FUN_DEF)
      continue;

    ASTNode *fn = alloc_sem_node(AST_FUNC_DEF);
    if (!fn) {
      semantic_ast_free(prog);
      return SEM_ERR_INTERNAL;
    }
    fn->name = sem_strdup(fdef->value);
    fn->is_main = (cur->node_type == NODE_MAIN_FUNCTION);
    prog->items[idx++] = fn;
    prog->item_count = idx;
    fn->has_parens = fdef->left != NULL;
    for (ast_node *list = fdef->left; list; list = list->right)
      fn->num_param_lists++;
    if (fn->has_parens) {
      fn->param_lists = calloc(fn->num_param_lists, sizeof(ParamList));
      if (!fn->param_lists) {
        semantic_ast_free(prog);
        return SEM_ERR_INTERNAL;
      }
    }
    size_t group = 0;
    for (ast_node *list = fdef->left; list; list = list->right, group++) {
      ParamList *params = &fn->param_lists[group];
      for (ast_node *p = list->left; p; p = p->left)
        params->count++;
      if (!params->count)
        continue;
      params->types = calloc(params->count, sizeof(DataType));
      params->names = calloc(params->count, sizeof(char *));
      if (!params->types || !params->names) {
        semantic_ast_free(prog);
        return SEM_ERR_INTERNAL;
      }
      size_t index = 0;
      for (ast_node *p = list->left; p; p = p->left, index++) {
        params->types[index] = map_parser_dtype(p->data_type);
        params->names[index] = sem_strdup(p->value);
        if (!params->names[index]) {
          semantic_ast_free(prog);
          return SEM_ERR_INTERNAL;
        }
      }
    }

    if (fdef->right) {
      if (fdef->right->left && fdef->right->left->node_type == NODE_TYPE) {
        fn->has_explicit_return = true;
        fn->declared_type = map_parser_dtype(fdef->right->left->data_type);
      }
      fn->body = convert_parser_node(fdef->right->right);
    }
  }

  int res = semantic_analyze(prog);
  semantic_ast_free(prog);
  return res;
}
