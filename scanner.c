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

static void fatal_oom(void) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(99);
}

//  Creates an empty buffer. Returns false if malloc fails.
static void dynstr_init(DynStr *s) {
    s->capacity = 32;
    s->length = 0;
    s->data = malloc(s->capacity);
    if (!s->data) {
        fatal_oom();
    }
    s->data[0] = '\0';
}

// Adds one character to the end. When the buffer is full, it doubles in size.
static void dynstr_append(DynStr *s, char c) {
    if (s->length + 2 > s->capacity) { // reserve extra space for null terminator
        size_t new_cap = s->capacity * 2;
        char *new_data = realloc(s->data, new_cap);
        if (!new_data){
           fatal_oom();
        }
        s->data = new_data;
        s->capacity = new_cap;
    }
    s->data[s->length] = c;
    s->length++;
    s->data[s->length] = '\0';
}

// Hands the collected text over to the caller (we use it as token->attribute).
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
/* ---------- Parentheses nesting ---------- */
static int paren_depth = 0;      // nesting level of parentheses ()

/* ---------- Indentation handling (INDENT / DEDENT) ---------- */
#define MAX_INDENT_LEVELS 100
static int indent_stack[MAX_INDENT_LEVELS];
static int indent_top = 0;         // stack pointer
static bool at_line_start = true;  // flag indicating start of a new line
static int line_indent =0;
static bool saw_tab = false;
static bool counting_indent = true;

// frees the text stored in a token
void free_token(Token *token) {
    if (token && token->attribute) {
        free(token->attribute);
        token->attribute = NULL;
    }
}
static void reset_line_state(void) {
    at_line_start = true;
    line_indent = 0;
    saw_tab = false;
    counting_indent = true;
}
// initializes the scanner module with input stream reference
void scanner_init(FILE *source) {
    source_file = source;
    current_line = 1;
    paren_depth = 0;
    indent_top = 0;
    indent_stack[0] = 0; // base indent level 0
    reset_line_state();

}
/* ---------- Helpers ---------- */

// checks whether a word is a reserved keyword
// returns the keyword's token type, or T_ID
static const struct {
    const char *word;
    Token_type type;
} keywords[] = {
    {"def", T_DEF},       {"do", T_DO},         {"Double", T_DOUBLE}, 
    {"else", T_ELSE},     {"if", T_IF},         {"import", T_IMPORT}, 
    {"Int", T_INT},       {"return", T_RETURN}, {"String", T_STRING}, 
    {"then", T_THEN},     {"Unit", T_UNIT},     {"val", T_VAL}, 
    {"var", T_VAR},       {"while", T_WHILE},
};

static Token_type check_keyword(const char *str) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strcmp(str, keywords[i].word) == 0) {
            return keywords[i].type;
        }
    }
    return T_ID;
}

// reports a lexical error to stderr, cleans up and returns error code 1.
static int lex_error(DynStr *buf) {
    dynstr_free(buf);
    fprintf(stderr, "Lexical error at line %d\n", current_line);
    return 1;
}

// fills in the token fields and returns 0 (success)
static int make_token(Token *token, Token_type type, char *attr) {
    token->type = type;
    token->attribute = attr; // NULL when the token carries no text
    token->line = current_line;
    return 0; 
}
// frees the buffer and returns a token without text 
static int emit(Token *token, DynStr *buf, Token_type type) {
    dynstr_free(buf);
    return make_token(token, type, NULL);
}
//'=' '<' '>': with a following '=' it is a two-char token, otherwise a one-char token
static int emit_pair(Token *token, DynStr *buf, int c, Token_type with_eq, Token_type alone) {
    if (c != '=') {
        ungetc(c, source_file);
    }
    return emit(token, buf, c == '=' ? with_eq : alone);
}
// ends a token whose last read character c is not part of it (returns c to stream and takes buffer text)
static int emit_text(Token *token, DynStr *buf, Token_type type, int c) {
    ungetc(c, source_file);
    return make_token(token, type, dynstr_take(buf));
}
static int check_indent(Token *token, DynStr *buf, int c) {
    if (saw_tab) {
        return lex_error(buf);
    }
    int top = indent_stack[indent_top];
    if (line_indent == top) {
        at_line_start = false;
        counting_indent = false;
        return -1;
    }
    Token_type type;
    if (line_indent > top) {
        if (indent_top + 1 >= MAX_INDENT_LEVELS) {
            fatal_oom();
        }
        indent_stack[++indent_top] = line_indent;
        type = T_INDENT;
    } else {
        if (indent_stack[indent_top - 1] < line_indent) { // lands between two levels
            return lex_error(buf);
        }
        indent_top--;
        type = T_DEDENT;
    }
    ungetc(c, source_file);
    return emit(token, buf, type);
}
/* ---------- States of the automaton ---------- */

typedef enum {
    /* ---------- Initial & Identifier states ---------- */
    STATE_START,             // initial state / whitespace skipping
    STATE_ID,                // processing identifier or keyword
    STATE_UNDERSCORE,        // read a leading '_', needs one more character
    /* ---------- Integer states ---------- */
    STATE_ZERO,              // read a '0'
    STATE_INT,               // inside an integer that started with 1-9
    STATE_INT_UNDERSCORE,    // read '_' in an integer, a digit must follow
    /* ---------- Double states ---------- */
    STATE_DOT,               // read '.', at least one digit must follow
    STATE_DOUBLE,            // processing Double fractional digits
    STATE_DOUBLE_UNDERSCORE, // read '_' in Double fractional part
    STATE_EXP,               // read 'e' or 'E', expecting a sign or a digit
    STATE_EXP_SIGN,          // read the sign of the exponent, digit must follow
    STATE_EXP_DIGIT,         // processing exponent digits
    STATE_EXP_UNDERSCORE,     // read '_' in the exponent
    /* ---------- Comments & Division states ---------- */
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT,
    STATE_BLOCK_COMMENT_SLASH,
    STATE_BLOCK_COMMENT_END,
    /* ---------- Operators & Assignment states ---------- */
    STATE_ASSIGN,
    STATE_EXCLAM,
    STATE_LESS,
    STATE_GREATER,
    /* ---------- String states ---------- */
   
    STATE_STRING_START,      // Node 'f1' (read initial '"')
    STATE_STRING_QUOTE2,     // Node 'f2' (read '""')
    STATE_STRING,            // Node 'STR' (inside single-line string literal)
    STATE_STRING_ESC,        // Node 'f_esc' (read '\' inside single-line string)
    STATE_STRING_MULTI,      // Node 'f_m' (inside """ multi-line string)
    STATE_STRING_MULTI_Q1,   // Helper for closing multi-line string (read 1st closing '"')
    STATE_STRING_MULTI_Q2,    // Helper for closing multi-line string (read 2nd closing '"')

    STATE_AT
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

    DynStr buffer;
    dynstr_init(&buffer);
    
    // Process indentation at the beginning of a line
   
    State state = STATE_START;
    int c;
    int comment_depth = 0;

    while (1) {
        c = fgetc(source_file);

        switch (state) {
        case STATE_START:
            //  skip whitespaces between tokens
            if (c == ' '){
                if (at_line_start && counting_indent){
                    line_indent++;
                }
                break;
            }
            if (c == '\t'){
                if (at_line_start && counting_indent) {
                    saw_tab = true;
                }
                break;
            }
            if (c == '\r') {
                break;
            }

           // track newlines, set line start flag and return T_EOL token
            if (c == '\n') {
                if (paren_depth > 0){
                    current_line++;
                    break;
                }
                if (at_line_start){
                    current_line++;
                    reset_line_state();
                    break;
                }
                int result = make_token(token, T_EOL, NULL);
                current_line++;
                reset_line_state();
                dynstr_free(&buffer);
                return result;
            }

            //  end of file token
            if (c == EOF) {
                if (!at_line_start) { // last line had no '\n': close it with EOL
                    reset_line_state();
                    return emit(token, &buffer, T_EOL);
                }
                if (indent_top > 0) { // close open blocks, one DEDENT per call
                    indent_top--;
                    return emit(token, &buffer, T_DEDENT);
                }
                return emit(token, &buffer, T_EOF);
            }
            if (at_line_start && c != '/') {
                int res = check_indent(token, &buffer, c);
                if (res != -1) {
                    return res;
                }
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

            if (c == '/') {
                if (at_line_start) {
                    counting_indent = false; 
                }
                state = STATE_SLASH;
                break;
            }
            if (c == '=') {
                state = STATE_ASSIGN;
                break;
            }
            if (c == '!') {
                state = STATE_EXCLAM;
                break;
            }
            if (c == '<') {
                state = STATE_LESS;
                break;
            }
            if (c == '>') {
                state = STATE_GREATER;
                break;
            }
            if (c == '"') {
                state = STATE_STRING_START; 
                break;
            }
            if (c == '@') {
                state = STATE_AT;
                break;
            }
            

            //  single-character operators and delimiters
            switch (c) {
            case '+':
                return emit(token, &buffer, T_PLUS);
            case '-':
               return emit(token, &buffer, T_MINUS);
            case '*':
                return emit(token, &buffer, T_MUL);
            case '(':
                paren_depth++;
                return emit(token, &buffer, T_LPAR);
            case ')':
                if (paren_depth > 0) {
                    paren_depth--;
                }
                return emit(token, &buffer, T_RPAR);
            case ',':
                return emit(token, &buffer, T_COMMA);
            case ':':
                return emit(token, &buffer, T_COLON);
            case ';':
                return emit(token, &buffer, T_SEMICOLON);
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
                return emit_text(token, &buffer, T_LIT_INT, c);
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
                return emit_text(token, &buffer, T_LIT_INT, c);
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
                return emit_text(token, &buffer, T_LIT_DOUBLE, c);
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
                return emit_text(token, &buffer, T_LIT_DOUBLE, c);
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
        /* ---------- Comments & Division ---------- */
        case STATE_SLASH:
            if (c == '/') {
                state = STATE_LINE_COMMENT;
            } else if (c == '*') {
                comment_depth = 1;
                state = STATE_BLOCK_COMMENT;
            } else {
                // single '/' -> division operator
                ungetc(c, source_file);
                at_line_start = false;
                return emit(token, &buffer, T_DIV);
            }
            break;

        case STATE_LINE_COMMENT:
            if (c == '\n' || c == EOF) {
               ungetc(c, source_file);
                state = STATE_START;
            }
            break;

        case STATE_BLOCK_COMMENT:
            if (c == '*') {
                state = STATE_BLOCK_COMMENT_END;
            } else if (c == '/'){
                state = STATE_BLOCK_COMMENT_SLASH;
            } else if (c == '\n') {
                current_line++; // track line numbers inside block comment
            } else if (c == EOF) {
                // unterminated block comment -> lexical error
                return lex_error(&buffer);
            }
            break;

            case STATE_BLOCK_COMMENT_SLASH:
            if (c == '*') {
                comment_depth++; 
                state = STATE_BLOCK_COMMENT;
            } else {
                ungetc(c, source_file); 
                state = STATE_BLOCK_COMMENT;
            }
            break;

        case STATE_BLOCK_COMMENT_END:
            if (c == '/') {
                comment_depth--;
                if (comment_depth == 0){
                    state = STATE_START;
                }else{
                    state = STATE_BLOCK_COMMENT;
                }
                // closing block comment */ -> return to START
            } else if (c == '*') {
                // stay in END state to handle consecutive asterisks (e.g. '***')
                state = STATE_BLOCK_COMMENT_END;
            } else {
                if (c == '\n') {
                    current_line++;
                } else if (c == EOF) {
                    return lex_error(&buffer);
                }
                state = STATE_BLOCK_COMMENT;
            }
            break;

        /* ---------- Comparison & Assignment operators ---------- */
        case STATE_ASSIGN:
            return emit_pair(token, &buffer, c, T_EQ, T_ASSIGN);

        case STATE_EXCLAM:
            if (c == '=') {
                return emit(token, &buffer, T_NEQ); // '!='
            } else {
                // standalone '!' - lexical error
                return lex_error(&buffer);
            }
            break;

        case STATE_LESS:
            return emit_pair(token, &buffer, c, T_LESS_OR_EQ, T_LESS_T);

        case STATE_GREATER:
           return emit_pair(token, &buffer, c, T_GREATER_OR_EQ, T_GREATER_T);

            /* ---------- String states (f1, f2, STR, f_esc, f_m) ---------- */
        case STATE_STRING_START: // Node 'f1'
            if (c == '"') {
                state = STATE_STRING_QUOTE2; // Node 'f2' (read '""')
            } else if (c == '\\') {
                state = STATE_STRING_ESC; // Node 'f_esc'
            } else if (c == '\n' || c == EOF) {
                return lex_error(&buffer); // Unescaped LF is illegal in single-line string
            } else {
                dynstr_append(&buffer, (char)c);
                state = STATE_STRING; // Node 'STR'
            }
            break;

        case STATE_STRING_QUOTE2: // Node 'f2'
            if (c == '"') {
                state = STATE_STRING_MULTI; // Node 'f_m' (read '"""')
            } else {
                // Empty string "" followed by non-quote character
                return emit_text(token, &buffer, T_LIT_STRING, c);
            }
            break;

        case STATE_STRING: // Node 'STR'
            if (c == '"') {
                return make_token(token, T_LIT_STRING, dynstr_take(&buffer));
            } else if (c == '\\') {
                state = STATE_STRING_ESC; // Node 'f_esc'
            } else if (c == '\n' || c == EOF) {
                return lex_error(&buffer); // Unescaped LF or EOF -> lexical error 1
            } else {
                dynstr_append(&buffer, (char)c);
            }
            break;

        case STATE_STRING_ESC: // Node 'f_esc'
            if (c == '"') {
                dynstr_append(&buffer, '"');
            } else if (c == '\\') {
                dynstr_append(&buffer, '\\');
            } else if (c == 'n') {
                dynstr_append(&buffer, '\n');
            } else if (c == 'r') {
                dynstr_append(&buffer, '\r');
            } else if (c == 't') {
                dynstr_append(&buffer, '\t');
            } else {
                // Illegal escape sequence- lexical error 1
                return lex_error(&buffer);
            }
            state = STATE_STRING;
            break;

        case STATE_STRING_MULTI: // Node 'f_m'
            if (c == '"') {
                state = STATE_STRING_MULTI_Q1;
            } else if (c == EOF) {
                return lex_error(&buffer);
            } else {
                if (c == '\n') {
                    current_line++;
                }
                dynstr_append(&buffer, (char)c);
            }
            break;

        case STATE_STRING_MULTI_Q1:
            if (c == '"') {
                state = STATE_STRING_MULTI_Q2;
            } else {
                dynstr_append(&buffer, '"');
                ungetc(c, source_file);
                state = STATE_STRING_MULTI;
            }
            break;

        case STATE_STRING_MULTI_Q2:
            if (c == '"') {
                return make_token(token, T_LIT_STRING, dynstr_take(&buffer));
            } else {
                dynstr_append(&buffer, '"');
                dynstr_append(&buffer, '"');
                ungetc(c, source_file);
                state = STATE_STRING_MULTI;
            }
            break;
        case STATE_AT:
            if (c == 'm' && fgetc(source_file) == 'a' &&
                fgetc(source_file) == 'i' && fgetc(source_file) == 'n') {
                return emit(token, &buffer, T_AT_MAIN);
            }
            return lex_error(&buffer);
        }
    }
}
