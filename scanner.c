/**
 * Project IFJ26 compiler
 * File: scanner.c
 * Description: Implementation of the lexical analyzer (DFA)
 * Author: <xkotliy00> Yaryna Kotliarenko
 */

#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Dynamic expandable string buffer for tokens of unknown length
typedef struct
{
    char *data;      // Pointer to the character array
    size_t length;   // Current character count
    size_t capacity; // Total allocated buffer capacity
} DynStr;

// Initializes an empty string buffer
static bool dynstr_init(DynStr *s)
{
    s->capacity = 32;
    s->length = 0;
    s->data = malloc(s->capacity);
    if (!s->data)
    {
        s->capacity = 0;
        return false;
    }
    s->data[0] = '\0';
    return true;
}

// Appends a character to the buffer, doubling capacity if needed
static bool dynstr_append(DynStr *s, char c)
{
    if (s->length + 2 > s->capacity)
    {
        size_t new_cap = s->capacity * 2;
        char *new_data = realloc(s->data, new_cap);
        if (!new_data)
            return false;
        s->data = new_data;
        s->capacity = new_cap;
    }
    s->data[s->length] = c;
    s->length++;
    s->data[s->length] = '\0';
    return true;
}

// Transfers ownership of the string buffer to the token attribute and resets buffer
static char *dynstr_take(DynStr *s)
{
    char *result = s->data;
    s->data = NULL;
    s->length = 0;
    s->capacity = 0;
    return result;
}

// Frees heap memory used by the dynamic string buffer
static void dynstr_free(DynStr *s)
{
    if (s->data)
    {
        free(s->data);
        s->data = NULL;
    }
    s->length = 0;
    s->capacity = 0;
}

static FILE *source_file = NULL;
static int current_line = 1;

void scanner_init(FILE *source)
{
    source_file = source;
    current_line = 1;
}

void free_token(Token *token)
{
    if (token && token->attribute)
    {
        free(token->attribute);
        token->attribute = NULL;
    }
}

// Keyword lookup matching IFJ26 specification (Section 1.1)
static Token_type check_keyword(const char *str)
{
    if (strcmp(str, "def") == 0)
        return T_DEF;
    if (strcmp(str, "do") == 0)
        return T_DO;
    if (strcmp(str, "Double") == 0)
        return T_DOUBLE;
    if (strcmp(str, "else") == 0)
        return T_ELSE;
    if (strcmp(str, "if") == 0)
        return T_IF;
    if (strcmp(str, "import") == 0)
        return T_IMPORT;
    if (strcmp(str, "Int") == 0)
        return T_INT;
    if (strcmp(str, "return") == 0)
        return T_RETURN;
    if (strcmp(str, "String") == 0)
        return T_STRING;
    if (strcmp(str, "then") == 0)
        return T_THEN;
    if (strcmp(str, "Unit") == 0)
        return T_UNIT;
    if (strcmp(str, "val") == 0)
        return T_VAL;
    if (strcmp(str, "var") == 0)
        return T_VAR;
    if (strcmp(str, "while") == 0)
        return T_WHILE;

    return T_ID; // Return general identifier type if not in keyword table
}

typedef enum
{
    STATE_START,
    STATE_ID,
    STATE_UNDERSCORE,
    STATE_ZERO,
    STATE_INT,
    STATE_INT_UNDERSCORE
} State;

// Helper function to initialize token fields and return success code (0)
static int make_token(Token *token, Token_type type, char *attr)
{
    token->type = type;
    token->attribute = attr; // NULL if no attribute
    token->line = current_line;
    return 0; // Success
}

int get_token(Token *token)
{
    if (!source_file || !token)
    {
        return 1;
    }
    token->attribute = NULL;

    State state = STATE_START;
    int c;

    DynStr buffer;
    if (!dynstr_init(&buffer))
    {
        return 99; // Memory allocation failure (internal scanner error)
    }

    while (1)
    {
        c = fgetc(source_file);

        switch (state)
        {
        case STATE_START:
            // 1. Skip spaces, tabs, and carriage returns (S -> S loop)
            if (c == ' ' || c == '\t' || c == '\r')
            {
                break;
            }

            // 2. EOL token (newline \n)
            if (c == '\n')
            {
                dynstr_free(&buffer);
                int result = make_token(token, T_EOL, NULL);
                current_line++;
                return result;
            }

            // 3. EOF token (end of file)
            if (c == EOF)
            {
                dynstr_free(&buffer);
                return make_token(token, T_EOF, NULL);
            }

            // 4. Identifiers
            if (isalpha(c))
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_ID;
                break;
            }

            if (c == '_')
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_UNDERSCORE;
                break;
            }

            // 5. Zero integer literal ('0') -> STATE_ZERO
            if (c == '0')
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_ZERO;
                break;
            }
            // 6. Non-zero integer literal ('1'-'9') -> STATE_INT
            if (isdigit(c))
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_INT;
                break;
            }

            // 7. Single-character operators
         switch (c)
            {
            case '+': dynstr_free(&buffer); return make_token(token, T_PLUS, NULL);
            case '-': dynstr_free(&buffer); return make_token(token, T_MINUS, NULL);
            case '*': dynstr_free(&buffer); return make_token(token, T_MUL, NULL);
            case '(': dynstr_free(&buffer); return make_token(token, T_LPAR, NULL);
            case ')': dynstr_free(&buffer); return make_token(token, T_RPAR, NULL);
            case ',': dynstr_free(&buffer); return make_token(token, T_COMMA, NULL);
            case ':': dynstr_free(&buffer); return make_token(token, T_COLON, NULL);
            case ';': dynstr_free(&buffer); return make_token(token, T_SEMICOLON, NULL);
            default:  dynstr_free(&buffer); return 1;
            } 
                

    

        case STATE_UNDERSCORE:
            // Standalone '_' is forbidden. Must be followed by [a-zA-Z0-9_]
            if (isalnum(c) || c == '_')
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_ID;
            }
            else
            {
                dynstr_free(&buffer);
                return 1; // Lexical error code 1
            }
            break;

        case STATE_ID:
            if (isalnum(c) || c == '_')
            {
                dynstr_append(&buffer, (char)c);
            }
            else
            {
                // Push back excess non-identifier character into stream
                ungetc(c, source_file);

                char *attr_str = dynstr_take(&buffer);
                Token_type kw_type = check_keyword(attr_str);

                if (kw_type == T_ID)
                {
                    return make_token(token, T_ID, attr_str);
                }
                else
                {
                    free(attr_str);
                    return make_token(token, kw_type, NULL);
                }
            }
            break;

        case STATE_ZERO:
            if (isdigit(c) || c == '_')
            {
                // Leading zeros in non-zero integers (e.g. 05, 0_1) are illegal in IFJ26
                dynstr_free(&buffer);
                return 1; // Lexical error code 1
            }
            else
            {
                // Push back excess character and finish T_LIT_INT "0"
                ungetc(c, source_file);
                return make_token(token, T_LIT_INT, dynstr_take(&buffer));
            }
            
        case STATE_INT:
            if (isdigit(c))
            {
                dynstr_append(&buffer, (char)c);
            }
            else if (c == '_')
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_INT_UNDERSCORE;
            }
            else
            {
                // Push back excess character and finish T_LIT_INT
                ungetc(c, source_file);
                return make_token(token, T_LIT_INT, dynstr_take(&buffer));
            }
            break;

        case STATE_INT_UNDERSCORE:
            if (isdigit(c))
            {
                dynstr_append(&buffer, (char)c);
                state = STATE_INT;
            }
            else
            {
                // Underscore must be strictly between digits (e.g., 1_0 is OK, 1_ or 1__0 is error)
                dynstr_free(&buffer);
                return 1; // Lexical error code 1
            }
            break;
        }
    }
}
