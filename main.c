#include <stdio.h>
#include "scanner.h"
#include "parser.h"
#include "ast.h"

int main(void) {
  scanner_init(stdin);
  
  ast_node *root = NULL;
  int p;
  p = parse_program(&root);
  if (p == 1) 
  {
    ast_destroy(root);
    scanner_free();
    return 2;
  }
  else if ( p == 2 ) 
  {
    ast_destroy(root);
    scanner_free();
    return 99;
  }
  else 
  {
    ast_print_tree(root);
    ast_destroy(root);
    scanner_free();
    return 0;
  }
} 
    
