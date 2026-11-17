/**
 * Project IFJ26 compiler
 * File: codegen.h
 * Description: Interface of IFJcode26 target code generator
 * Author: <xalimzt00> Tamerlan Alimzhanov
 */

#ifndef CODEGEN_H
#define CODEGEN_H

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

#endif // CODEGEN_H
