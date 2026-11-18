/**
 * Project IFJ26 compiler
 * File: main.c
 * Description: Compiler entry point (parser integration draft)
 * Author: <xstrelm00> Maksym Streltsov
 *         <xalimzt00> Tamerlan Alimzhanov 
 */
#include "parser.h"
#include "ast.h"
#include "scanner.h"
#include "codegen.h"
#include <stdio.h>


int main(void) {
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

  cg_generate(root); 
  ast_destroy(root);
  scanner_free();
  return 0;
}
