/**
 * Project IFJ26 compiler
 * File: scanner.c
 * Description: Implementation of the lexical analyzer (DFA)
 * Author: <xkotliy00> Yaryna Kotliarenko
 */

#include "scanner.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dynamic expandable string buffer for storing token attributes of arbitrary length
typedef struct {
    char *data;      // Pointer to the dynamically allocated character array
    size_t length;   // Current number of characters stored (excluding '\0')
    size_t capacity; // Total allocated memory capacity in bytes
} DynStr;

//  Creates an empty buffer. Returns false if malloc fails.
static bool dynstr_init(DynStr *s) {
    s->capacity = 32;
    s->length = 0;
    s->data = malloc(s->capacity);
    if (!s->data) {
        s->capacity = 0;
        return false;
    }
    s->data[0] = '\0';
    return true;
}

// Adds one character to the end. When the buffer is full, it doubles in size.
static bool dynstr_append(DynStr *s, char c) {
    if (s->length + 2 > s->capacity) { // reserve extra space for null terminator
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

/// Hands the collected text over to the caller (we use it as token->attribute).
// After this the buffer is empty and the caller is responsible for free().

static char *dynstr_take(DynStr *s) {
    char *result = s->data;
    s->data = NULL;
    s->length = 0;
    s->capacity = 0;
    return result;
}

// Releases the buffer. Safe to call even if it was already taken or freed.
static void dynstr_free(DynStr *s) {
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }
    s->length = 0;
    s->capacity = 0;
}
/* ---------- Scanner state ---------- */
static FILE *source_file = NULL; // where we read the source code from
static int current_line = 1;  // line we are on now, used in error messages

// initializes the scanner module with input stream reference
void scanner_init(FILE *source) {
    source_file = source;
    current_line = 1;
}

// frees the text stored in a token
void free_token(Token *token) {
    if (token && token->attribute) {
        free(token->attribute);
        token->attribute = NULL;
    }
}
/* ---------- Helpers ---------- */

// checks whether a word is a reserved keyword
// returns the keyword's token type, or T_ID
static Token_type check_keyword(const char *str) {
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

    return T_ID; 
}

// reports a lexical error to stderr, cleans up and returns error code 1.
static int lex_error(DynStr *buf) {
    dynstr_free(buf);
    fprintf(stderr, "Lexical error at line %d\n", current_line);
    return 1;
}

// fills in the token and returns 0 (success), so callers can write
static int make_token(Token *token, Token_type type, char *attr) {
    token->type = type;
    token->attribute = attr; // NULL when the token carries no text
    token->line = current_line;
    return 0; 
}
/* ---------- States of the automaton ---------- */

typedef enum {
    STATE_START,             // initial state / whitespace skipping
    STATE_ID,                // processing identifier or keyword
    STATE_UNDERSCORE,        // read a leading '_', needs one more character
    STATE_ZERO,              // read a '0'
    STATE_INT,               // inside an integer that started with 1-9
    STATE_INT_UNDERSCORE,    // read '_' in an integer, a digit must follow
    STATE_DOT,               // read '.', at least one digit must follow
    STATE_DOUBLE,            // processing Double fractional digits
    STATE_DOUBLE_UNDERSCORE, // read '_' in Double fractional part
    STATE_EXP,               // read 'e' or 'E', expecting a sign or a digit
    STATE_EXP_SIGN,          // read the sign of the exponent, digit must follow
    STATE_EXP_DIGIT,         // processing exponent digits
    STATE_EXP_UNDERSCORE     // read '_' in the exponent
} State;

/* ---------- Main function ---------- */
// Reads characters one by one and moves between states until a full token
// is recognized. Returns 0 - success, 1 - lexical error and 99 when
// memory allocation fails.
//
// Note: '_' separators and the 'd'/'D' suffix are NOT stored in the attribute,
// so the text of a number can go straight to strtol()/strtod().

int get_token(Token *token) {
    if (!source_file || !token) {
        return 1; 
    }
    token->attribute = NULL;

    State state = STATE_START;
    int c;

    DynStr buffer;
    if (!dynstr_init(&buffer)) {
        return 99; // out of memory
    }

    while (1) {
        c = fgetc(source_file);

        switch (state) {
        case STATE_START:
            //  skip whitespaces between tokens
            if (c == ' ' || c == '\t' || c == '\r') {
                break;
            }

            //  track newlines and return T_EOL token
            if (c == '\n') {
                dynstr_free(&buffer);
                int result = make_token(token, T_EOL, NULL);
                current_line++; // only after the token got its own line number
                return result;
            }

            //  end of file token
            if (c == EOF) {
                dynstr_free(&buffer);
                return make_token(token, T_EOF, NULL);
            }

            //  identifiers or keywords starting with letters
            if (isalpha(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_ID;
                break;
            }

            //  identifier starting with '_'
            if (c == '_') {
                dynstr_append(&buffer, (char)c);
                state = STATE_UNDERSCORE;
                break;
            }

            //  zero literal or Double starting with '0'
            if (c == '0') {
                dynstr_append(&buffer, (char)c);
                state = STATE_ZERO;
                break;
            }

            //  non-zero integer literal ('1'-'9')
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_INT;
                break;
            }

            //  a Double may start with a dot, e.g. ".5"
            if (c == '.') {
                dynstr_append(&buffer, (char)c);
                state = STATE_DOT;
                break;
            }

            //  single-character operators and delimiters
            switch (c) {
            case '+':
                dynstr_free(&buffer);
                return make_token(token, T_PLUS, NULL);
            case '-':
                dynstr_free(&buffer);
                return make_token(token, T_MINUS, NULL);
            case '*':
                dynstr_free(&buffer);
                return make_token(token, T_MUL, NULL);
            case '(':
                dynstr_free(&buffer);
                return make_token(token, T_LPAR, NULL);
            case ')':
                dynstr_free(&buffer);
                return make_token(token, T_RPAR, NULL);
            case ',':
                dynstr_free(&buffer);
                return make_token(token, T_COMMA, NULL);
            case ':':
                dynstr_free(&buffer);
                return make_token(token, T_COLON, NULL);
            case ';':
                dynstr_free(&buffer);
                return make_token(token, T_SEMICOLON, NULL);
            default:
                // a character that doesn't belong to the language
                return lex_error(&buffer);
            }

        case STATE_UNDERSCORE:
            // standalone '_' is forbidden. Must be followed by alnum or another '_'
            if (isalnum(c) || c == '_') {
                dynstr_append(&buffer, (char)c);
                state = STATE_ID;
            } else {
                return lex_error(&buffer);
            }
            break;

        case STATE_ID:
            // accumulate characters for identifiers/keywords
            if (isalnum(c) || c == '_') {
                dynstr_append(&buffer, (char)c);
            } else {
                 // The word ended. The character we just read belongs to the
                // next token, so we put it back.
                ungetc(c, source_file);

                char *attr_str = dynstr_take(&buffer);
                Token_type kw_type = check_keyword(attr_str);

                if (kw_type == T_ID) {
                    return make_token(token, T_ID, attr_str); // keep the name
                } else {
                    // keywords do not store string attributes
                    free(attr_str);
                    return make_token(token, kw_type, NULL);
                }
            }
            break;

        case STATE_ZERO:
            if (c == '.') {
                dynstr_append(&buffer, (char)c);
                state = STATE_DOT;
            } else if (c == 'e' || c == 'E') {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP;
            } else if (isdigit(c) || c == '_') {
                // "05" or "0_1": integers must not start with zero
                return lex_error(&buffer);
            } else {
                // return simple "0" integer token
                ungetc(c, source_file);
                return make_token(token, T_LIT_INT, dynstr_take(&buffer));
            }
            break;

        case STATE_INT:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
            } else if (c == '_') {
                 // we don't store the '_', only remember that one was read
                state = STATE_INT_UNDERSCORE;
            } else if (c == '.') {
                dynstr_append(&buffer, (char)c);
                state = STATE_DOT;
            } else if (c == 'e' || c == 'E') {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP;
            } else {
                // end of the integer, give the extra character back
                ungetc(c, source_file);
                return make_token(token, T_LIT_INT, dynstr_take(&buffer));
            }
            break;

        case STATE_INT_UNDERSCORE:
         // '_' is only allowed between two digits
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_INT;
            } else {
                return lex_error(&buffer);
            }
            break;

        case STATE_DOT:
        // "1." and "." alone are errors, we need a digit after the dot
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_DOUBLE;
            } else {
                return lex_error(&buffer);
            }
            break;

        case STATE_DOUBLE:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
            } else if (c == '_') {
                state = STATE_DOUBLE_UNDERSCORE;
            } else if (c == 'e' || c == 'E') {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP;
            } else if (c == 'd' || c == 'D') {
               // optional suffix: it belongs to the number, so nothing to put back
                return make_token(token, T_LIT_DOUBLE, dynstr_take(&buffer));
            } else {
                ungetc(c, source_file);
                return make_token(token, T_LIT_DOUBLE, dynstr_take(&buffer));
            }
            break;

        case STATE_DOUBLE_UNDERSCORE:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_DOUBLE;
            } else {
                return lex_error(&buffer);
            }
            break;

        case STATE_EXP:
        // 'e'/'E' without sign or digits is invalid
            if (c == '+' || c == '-') {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP_SIGN;
            } else if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP_DIGIT;
            } else {
                return lex_error(&buffer);// e.g. "1e"
            }
            break;

        case STATE_EXP_SIGN:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP_DIGIT;
            } else {
                return lex_error(&buffer); // e.g. "1e+"
            }
            break;

        case STATE_EXP_DIGIT:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
            } else if (c == '_') {
                // skip appending '_' inside exponent digits
                state = STATE_EXP_UNDERSCORE;
            } else if (c == 'd' || c == 'D') {
               // optional suffix, same as in STATE_DOUBLE
                return make_token(token, T_LIT_DOUBLE, dynstr_take(&buffer));
            } else {
                ungetc(c, source_file);
                return make_token(token, T_LIT_DOUBLE, dynstr_take(&buffer));
            }
            break;

        case STATE_EXP_UNDERSCORE:
            if (isdigit(c)) {
                dynstr_append(&buffer, (char)c);
                state = STATE_EXP_DIGIT;
            } else {
                // Trailing underscore after exponent digits is invalid
                return lex_error(&buffer);
            }
            break;
        }
    }
}