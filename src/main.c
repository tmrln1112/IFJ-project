/**
 * Project IFJ26 compiler
 * File: main.c
 * Description: Compiler entry point (parser integration draft)
 * Author: <xstrelm00> Maksym Streltsov
 */
#include "parser.h"
#include "ast.h"
#include "scanner.h"
#include <stdio.h>

int main() {
  scanner_init(stdin);
  
  ast_node *root = NULL;
  int parser_return = 0;
  parser_return = parse_program(&root);

  if (parser_return != 0) 
  {
    ast_destroy(root);
    scanner_free();
    return parser_return;
  }



  ast_print_tree(root);
  ast_destroy(root);
  scanner_free();
  return 0;
}
