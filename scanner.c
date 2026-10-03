/** 
*Project IFJ26 compiler
*File: scanner.c
*Description: Implementation of the lexical analyzer 
*Author <xkotliy00> Yaryna Kotliarenko
*/
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
// Dynamic, expandable buffer for text of unknown length
typedef struct {
    char *data; // pointer to the character array
    size_t length; // number of characters currently stored
    size_t capacity; // total allocated capacity of the buffer
} DynStr;
// Initializes an empty string buffer
static bool dynstr_init(DynStr *s) {
    s->capacity = 32; // Initial capacity 
    s->length = 0; // No characters yet
    s->data = malloc(s->capacity); // Allocate memory on the heap
    if (!s->data){
        s->capacity = 0; // Reset capacity on failure 
    return false;
    }  
    s->data[0] = '\0'; //Ensure valid empty string
    return true;
}
// Append a character to the string buffer, expanding if necessary
static bool dynstr_append(DynStr *s, char c) {
    // Requires space for the new character and null terminator
    if (s->length + 2 > s->capacity) {
        size_t new_cap = s->capacity * 2; // Double the capacity to achieve amortized O(1) complexity
        char *new_data = realloc(s->data, new_cap);
        if (!new_data) return false; // Allocation failed— preserve original s->data to avoid memory leak
        s->data = new_data; //Switch pointer to resized memory block
        s->capacity = new_cap;
    }
    s->data[s->length] = c; //Store character at current end
    s->length++; //Increment character count
    s->data[s->length] = '\0'; //Keep string null-terminated
    return true;
}

// Transfer buffer ownership to token attribute and reset the buffer
static char *dynstr_take(DynStr *s) {
    char *result = s->data; //Extract pointer to the character array
    s->data = NULL; // Prevents double free when freeing the buffer
    s->length = 0;
    s->capacity = 0;
    return result;
}
// Free the memory allocated for the string buffer
static void dynstr_free(DynStr *s) {
    if (s->data) {
        free(s->data); //Release allocated memory back to OS
        s->data = NULL; // Avoid dangling pointer and use-after-free 
    }
    s->length = 0;
    s->capacity = 0;
}

static FILE *source_file = NULL; 
static int current_line = 1; // Track the current line number for error reporting

void scanner_init(FILE *source) {
    source_file = source; // Store the file pointer for tokenization
    current_line = 1; // Reset line counter at initialization
}

void free_token(Token *token) {
    if (token && token->attribute) {
        free(token->attribute); // Free the dynamically allocated attribute string
        token->attribute = NULL; // Avoid dangling pointer
    }
}

int get_token(Token *token) {
    if (!source_file || !token) {
        return 1; 
    }
    // TODO: Implement the actual tokenization logic here.
    token->type = T_EOF;
    token->attribute = NULL;
    token->line = current_line;
    return 0;
}
