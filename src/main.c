/**
 * Project IFJ26 compiler
 * File: main.c
 * Description: Compiler entry point
 * Author: <xstrelm00> Maksym Streltsov
 *         <xalimzt00> Tamerlan Alimzhanov
 */
#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "scanner.h"
#include "semantic.h"
#include <stdio.h>

int main(void) {
  scanner_init(stdin);

  ast_node *root = NULL;
  int result = parse_program(&root);
  if (result == 0) {
    result = semantic_analyze_parser_ast(root);
  }
  if (result == 0) {
    cg_generate(root);
  }

  ast_destroy(root);
  scanner_free();
  return result;
}
