/**
 * Project IFJ26 compiler
 * File: codegen.c
 * Description: Generation of IFJcode26 target code
 * Author: <xalimzt00> Tamerlan Alimzhanov
 */

#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>

void cg_print_string(const char *s) {
  printf("string@");
  for (size_t i = 0; s[i] != '\0'; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c <= 32 || c == 35 || c == 92) {
      printf("\\%03d", c);
    } else {
      putchar(c);
    }
  }
}

void cg_print_int(const char *text) { printf("int@%s", text); }

void cg_print_float(const char *text) {
  printf("float@%a", strtod(text, NULL));
}
