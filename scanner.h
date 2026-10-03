/** 
*Project IFJ26 compiler
*File: scanner.h
*Description: Interface of the lexical analyzer 
*Author <xkotliy00> Yaryna Kotliarenko
*/

#ifndef SCANNER_H
#define SCANNER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


    typedef enum{
    T_EOF, // end of file
    T_EOL, // end of line
    T_ERROR, // error
    T_INDENT, // indentation
    T_DEDENT, // dedentation

    T_SEMICOLON, // semicolon ;
    T_COLON, // colon :
    T_COMMA, // comma ,
    T_DOT, // dot .
    T_LPAR, // left parenthesis (
    T_RPAR, // right parenthesis )

    T_ID, // identifier
    T_LIT_INT, // literal integer
    T_LIT_DOUBLE, // literal double
    T_LIT_STRING, // literal string

    T_AT_MAIN, // @main


    //keywords
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

    //operators
    T_PLUS, // plus
    T_MINUS, // minus
    T_MUL, // multiply
    T_DIV, // divide
    T_ASSIGN, // assignment
    T_EQ, // equal
    T_NEQ, // not equal
    T_LESS_T, // less than
    T_LESS_OR_EQ, // less than or equal to
    T_GREATER_T, // greater than
    T_GREATER_OR_EQ, // greater than or equal to
    
} Token_type;

    typedef struct {
    Token_type type; // type of the token
    char *attribute; // attribute of the token
    int line; // line number in the source file
} Token;

//initial scanner with input source file
void scanner_init(FILE *source);

// gets the next token from the input source file
int get_token(Token *token);

// frees the memory allocated for token attribute
void free_token(Token *token);

#endif //SCANNER_H