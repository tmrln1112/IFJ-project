#include "dynstr.h"
#include <stdio.h>
#include <stdlib.h>
static void fatal_oom(void) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(99);
}
void dynstr_init(DynStr *s) {
    s->capacity = 32;
    s->length = 0;
    s->data = malloc(s->capacity);
    if (!s->data) {
        fatal_oom();
    }
    s->data[0] = '\0';
}
void dynstr_append(DynStr *s, char c) {
    if (s->length + 2 > s->capacity) {
        size_t new_cap = s->capacity * 2;
        char *new_data = realloc(s->data, new_cap);
        if (!new_data) {
            fatal_oom();
        }
        s->data = new_data;
        s->capacity = new_cap;
    }
    s->data[s->length] = c;
    s->length++;
    s->data[s->length] = '\0';
}
char *dynstr_take(DynStr *s) {
    char *result = s->data;
    s->data = NULL;
    s->length = 0;
    s->capacity = 0;
    return result;
}
void dynstr_free(DynStr *s) {
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }
    s->length = 0;
    s->capacity = 0;
}