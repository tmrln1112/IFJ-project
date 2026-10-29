#ifndef DYNSTR_H
#define DYNSTR_H

#include <stddef.h>
#include <stdbool.h>

// Dynamic expandable string buffer for storing text of arbitrary length
typedef struct {
    char *data;      // Pointer to the dynamically allocated character array
    size_t length;   // Current number of characters stored (excluding '\0')
    size_t capacity; // Total allocated memory capacity in bytes
} DynStr;

// Initializes an empty string buffer
void dynstr_init(DynStr *s);

// Appends one character to the end, doubling capacity if needed
void dynstr_append(DynStr *s, char c);

// Hands the collected text over to the caller and resets the buffer
char *dynstr_take(DynStr *s);

// Releases the memory used by the buffer
void dynstr_free(DynStr *s);

#endif // DYNSTR_H