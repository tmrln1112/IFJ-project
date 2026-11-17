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

#endif // CODEGEN_H