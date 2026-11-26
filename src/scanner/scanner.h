/**
 *Project IFJ26 compiler
 *File: scanner.h
 *Description: Interface of the lexical analyzer
 *Author <xkotliy00> Yaryna Kotliarenko
 */
#ifndef SCANNER_H
#define SCANNER_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  T_EOF,    // end of file
  T_EOL,    // end of line
  T_INDENT, // indentation
  T_DEDENT, // dedentation

  T_SEMICOLON,  // semicolon ;
  T_COLON,      // colon :
  T_COMMA,      // comma ,
  T_DOT,        // dot .
  T_UNDERSCORE, // lone underscore _
  T_LPAR,       // left parenthesis (
  T_RPAR,       // right parenthesis )
  T_LBRACKET,   // left bracket [
  T_RBRACKET,   // right bracket ]
  T_PIPE,       // single pipe |

  T_ID,         // identifier
  T_LIT_INT,    // literal integer
  T_LIT_DOUBLE, // literal double
  T_LIT_STRING, // literal string

  T_AT_MAIN, // @main

  // keywords
  T_DEF,
  T_DO,
  T_DOUBLE,
  T_ELSE,
  T_IF,
  T_IMPORT,
  T_INT,
  T_RETURN,
  T_STRING,
  T_THEN,
  T_UNIT,
  T_VAL,
  T_VAR,
  T_WHILE,

  // keywords of the language extensions
  T_BOOLEAN,
  T_TRUE,
  T_FALSE,
  T_CASE,
  T_CLASS,
  T_EXTENSION,
  T_FOR,
  T_LIST, // list type name
  T_MATCH,
  T_TYPE,

  // operators
  T_PLUS,          // plus
  T_MINUS,         // minus
  T_MUL,           // multiply
  T_DIV,           // divide
  T_ASSIGN,        // assignment
  T_EQ,            // equal
  T_NEQ,           // not equal
  T_LESS_T,        // less than
  T_LESS_OR_EQ,    // less than or equal to
  T_GREATER_T,     // greater than
  T_GREATER_OR_EQ, // greater than or equal to
  T_NOT,           // logical not !
  T_AND,           // logical and &&
  T_OR,            // logical or ||
  T_ARROW_LEFT,    // generator arrow <-

} Token_type;

typedef struct {
  Token_type type;     // type of the token
  char *attribute;     // attribute of the token
  int line;            // line number in the source file
  bool newline_before; // an LF inside parentheses was skipped right before
                       // this token (needed for a trailing comma)
} Token;
/**
 * @brief Initializes the scanner module with input source file.
 * @param source Pointer to open source file stream.
 */
void scanner_init(FILE *source);
/**
 * @brief Frees memory owned by the scanner (indentation stack).
 * @note Call once after the last get_token(), also after a lexical error.
 */
void scanner_free(void);
/**
 * @brief Reads the next token from the input stream.
 * @param token Pointer to the Token structure to be filled.
 * @return 0 on success, 1 on lexical error.
 * @note Dynamically allocated attribute memory must be freed using
 * free_token().
 */
int get_token(Token *token);
/**
 * @brief Frees memory allocated for token attribute string.
 * @param token Pointer to token structure.
 */
void free_token(Token *token);
#endif // SCANNER_H