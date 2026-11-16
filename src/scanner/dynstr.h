/**
 * Project IFJ26 compiler
 * File: dynstr.h
 * Description: Interface for dynamic string buffer
 * Author: <xkotliy00> Yaryna Kotliarenko
 */
#ifndef DYNSTR_H
#define DYNSTR_H

#include <stdbool.h>
#include <stddef.h>

// Dynamic expandable string buffer for storing text of arbitrary length
typedef struct {
  char *data;      // Pointer to the dynamically allocated character array
  size_t length;   // Current number of characters stored (excluding '\0')
  size_t capacity; // Total allocated memory capacity in bytes
} DynStr;
/**
 * @brief Initializes an empty string buffer.
 * @param s Pointer to the DynStr structure to initialize.
 */
void dynstr_init(DynStr *s);
/**
 * @brief Appends one character to the end, doubling capacity if needed.
 * @param s Pointer to the string buffer.
 * @param c Character to append.
 */
void dynstr_append(DynStr *s, char c);
/**
 * @brief Hands the collected text over to the caller and resets the buffer.
 * @param s Pointer to the string buffer.
 * @return Pointer to the allocated C-string (caller must free it), or NULL on
 * failure.
 */
char *dynstr_take(DynStr *s);
/**
 * @brief Releases the memory used by the buffer.
 * @param s Pointer to the string buffer to free.
 */
void dynstr_free(DynStr *s);
#endif // DYNSTR_H