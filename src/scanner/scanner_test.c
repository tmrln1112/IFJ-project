/**
 * Project IFJ26 compiler
 * File: scanner_test.c
 * Description: Test driver for the scanner, prints tokens read from stdin
 * Author: <xkotliy00> Yaryna Kotliarenko
 */
#include "scanner.h"
#include <stdio.h>

const char *token_type_to_string(Token_type type) {
  switch (type) {
  case T_EOF:
    return "T_EOF";
  case T_EOL:
    return "T_EOL";
  case T_INDENT:
    return "T_INDENT";
  case T_DEDENT:
    return "T_DEDENT";
  case T_DEF:
    return "T_DEF";
  case T_DO:
    return "T_DO";
  case T_DOUBLE:
    return "T_DOUBLE";
  case T_ELSE:
    return "T_ELSE";
  case T_IF:
    return "T_IF";
  case T_IMPORT:
    return "T_IMPORT";
  case T_INT:
    return "T_INT";
  case T_RETURN:
    return "T_RETURN";
  case T_STRING:
    return "T_STRING";
  case T_THEN:
    return "T_THEN";
  case T_UNIT:
    return "T_UNIT";
  case T_VAL:
    return "T_VAL";
  case T_VAR:
    return "T_VAR";
  case T_WHILE:
    return "T_WHILE";
  case T_BOOLEAN:
    return "T_BOOLEAN";
  case T_TRUE:
    return "T_TRUE";
  case T_FALSE:
    return "T_FALSE";
  case T_CASE:
    return "T_CASE";
  case T_CLASS:
    return "T_CLASS";
  case T_EXTENSION:
    return "T_EXTENSION";
  case T_FOR:
    return "T_FOR";
  case T_LIST:
    return "T_LIST";
  case T_MATCH:
    return "T_MATCH";
  case T_TYPE:
    return "T_TYPE";
  case T_ID:
    return "T_ID";
  case T_LIT_INT:
    return "T_LIT_INT";
  case T_LIT_DOUBLE:
    return "T_LIT_DOUBLE";
  case T_LIT_STRING:
    return "T_LIT_STRING";
  case T_AT_MAIN:
    return "T_AT_MAIN";
  case T_PLUS:
    return "T_PLUS";
  case T_MINUS:
    return "T_MINUS";
  case T_MUL:
    return "T_MUL";
  case T_DIV:
    return "T_DIV";
  case T_ASSIGN:
    return "T_ASSIGN";
  case T_EQ:
    return "T_EQ";
  case T_NEQ:
    return "T_NEQ";
  case T_LESS_T:
    return "T_LESS_T";
  case T_LESS_OR_EQ:
    return "T_LESS_OR_EQ";
  case T_GREATER_T:
    return "T_GREATER_T";
  case T_GREATER_OR_EQ:
    return "T_GREATER_OR_EQ";
  case T_NOT:
    return "T_NOT";
  case T_AND:
    return "T_AND";
  case T_OR:
    return "T_OR";
  case T_ARROW_LEFT:
    return "T_ARROW_LEFT";
  case T_LPAR:
    return "T_LPAR";
  case T_RPAR:
    return "T_RPAR";
  case T_COLON:
    return "T_COLON";
  case T_SEMICOLON:
    return "T_SEMICOLON";
  case T_COMMA:
    return "T_COMMA";
  case T_DOT:
    return "T_DOT";
  case T_UNDERSCORE:
    return "T_UNDERSCORE";
  default:
    return "UNKNOWN_TOKEN";
  }
}

int main(void) {
  scanner_init(stdin);

  Token token;
  int result;

  while ((result = get_token(&token)) == 0) {
    printf("Line %d | Token: %-15s | Attr: %s%s\n", token.line,
           token_type_to_string(token.type),
           token.attribute ? token.attribute : "NULL",
           token.newline_before ? " | newline_before" : "");
    if (token.type == T_EOF) {
      free_token(&token);
      break;
    }
    free_token(&token);
  }
  scanner_free();

  if (result != 0) {
    fprintf(stderr, "LEXICAL ERROR\n");
    return 1;
  }
  return 0;
}
