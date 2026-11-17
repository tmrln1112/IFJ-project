/**
 * Project IFJ26 compiler
 * File: codegen.h
 * Description: Interface of IFJcode26 target code generator
 * Author: <xalimzt00> Tamerlan Alimzhanov
 */

#ifndef CODEGEN_H
#define CODEGEN_H
#include "ast.h"

/**
 * Prints string s as an IFJcode26 string constant (string@...) to stdout.
 * Characters with ASCII code 0-32, 35 and 92 are written as \xyz escape.
 */
void cg_print_string(const char *s);

/**
 * Prints an integer constant in IFJcode26 format (int@...) to stdout.
 * text: decimal number from the scanner, without underscores (e.g. "42").
 */
void cg_print_int(const char *text);

/**
 * Prints a floating-point constant in IFJcode26 format (float@..., printf %a)
 * to stdout.
 * text: decimal literal from the scanner, without underscores (e.g. "1.5").
 */
void cg_print_float(const char *text);

/**
 * Returns a new unique number for IFJcode26 labels (1, 2,3, ...).
 * Used to make labels of if/while unique, e.g "while$3$start".
 */
int cg_new_label(void);

/**
 * Generates the whole IFJcode26 program for the AST to stdout.
 * root: first NODE_FUNCTION / NODE_MAIN_FUNCTION node from parse_program.
 */
void cg_generate(ast_node *root);

#endif // CODEGEN_H
