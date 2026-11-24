/**
 * Project IFJ26 compiler
 * File: main.c
 * Description: Compiler entry point
 * Author: <xstrelm00> Maksym Streltsov
 *         <xalimzt00> Tamerlan Alimzhanov 
 */
#include "parser.h"
#include "ast.h"
#include "scanner.h"
#include "codegen.h"
#include "semantic.h"
#include <stdio.h>


int main(void) {
  scanner_init(stdin);

  ast_node *root = NULL;
  int result = parse_program(&root);
  if (result == 0) {
    ast_print_tree(root); // remove
    result = semantic_analyze_parser_ast(root);
  }
  if (result == 0) {
    cg_generate(root);
  }

  ast_destroy(root);
  scanner_free();
  return result;
}
