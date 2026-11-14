/**
 * Project IFJ26 compiler
 * File: scanner.c
 * Description: Implementation of the lexical analyzer (DFA)
 * Author: <xkotliy00> Yaryna Kotliarenko
 */

#include "scanner.h"
#include "dynstr.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Scanner state ---------- */
static FILE *source_file = NULL; // Input file
static int current_line = 1;     // Current line number
static int token_start_line = 1; // Line number where current token started
/* ---------- Parentheses nesting ---------- */
static int paren_depth = 0; // Nesting level of parentheses ()

/* ---------- Indentation handling (INDENT / DEDENT) ---------- */
static int *indent_stack = NULL;    // Indent levels, grows with realloc
static int indent_cap = 0;          // Allocated number of levels
static int indent_top = 0;          // Stack pointer
static bool at_line_start = true;   // Start of line flag
static int line_indent = 0;         // Spaces count
static bool saw_tab = false;        // Tab detected flag
static bool counting_indent = true; // Indent counting flag

// Frees the text stored in a token
void free_token(Token *token) {
  if (token && token->attribute) {
    free(token->attribute);
    token->attribute = NULL;
  }
}
// Resets line indentation state
static void reset_line_state(void) {
  at_line_start = true;
  line_indent = 0;
  saw_tab = false;
  counting_indent = true;
}
// Initializes the scanner module with input stream reference
void scanner_init(FILE *source) {
  source_file = source;
  current_line = 1;
  paren_depth = 0;

  free(indent_stack);
  indent_cap = 16;
  indent_stack = malloc(indent_cap * sizeof(int));
  if (!indent_stack) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(99);
  }
  indent_top = 0;
  indent_stack[0] = 0; // Base indent level 0
  reset_line_state();
}
// Frees memory owned by the scanner
void scanner_free(void) {
  free(indent_stack);
  indent_stack = NULL;
  indent_cap = 0;
  indent_top = 0;
}
/* ---------- Helpers ---------- */
// Checks if a word is a keyword
static const struct {
  const char *word;
  Token_type type;
} keywords[] = {
    {"def", T_DEF},
    {"do", T_DO},
    {"Double", T_DOUBLE},
    {"else", T_ELSE},
    {"if", T_IF},
    {"import", T_IMPORT},
    {"Int", T_INT},
    {"return", T_RETURN},
    {"String", T_STRING},
    {"then", T_THEN},
    {"Unit", T_UNIT},
    {"val", T_VAL},
    {"var", T_VAR},
    {"while", T_WHILE},
    // keywords of the language extensions
    {"Boolean", T_BOOLEAN},
    {"true", T_TRUE},
    {"false", T_FALSE},
    {"case", T_CASE},
    {"class", T_CLASS},
    {"extension", T_EXTENSION},
    {"for", T_FOR},
    {"List", T_LIST},
    {"match", T_MATCH},
    {"type", T_TYPE},
};
// Returns keyword token type or T_ID
static Token_type check_keyword(const char *str) {
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (strcmp(str, keywords[i].word) == 0) {
      return keywords[i].type;
    }
  }
  return T_ID;
}

// Reports a lexical error to stderr, cleans up and returns 1
static int lex_error(DynStr *buf) {
  dynstr_free(buf);
  fprintf(stderr, "Lexical error at line %d\n", current_line);
  return 1;
}

// Fills in the token fields and returns 0
static int make_token(Token *token, Token_type type, char *attr) {
  token->type = type;
  token->attribute = attr; // NULL when the token carries no text
  token->line = current_line;
  return 0;
}
// Frees the buffer and returns a token without text
static int emit(Token *token, DynStr *buf, Token_type type) {
  dynstr_free(buf);
  return make_token(token, type, NULL);
}
// Checks two-char operators (==, !=, <=, >=)
static int emit_pair(Token *token, DynStr *buf, int c, Token_type with_eq,
                     Token_type alone) {
  if (c != '=') {
    ungetc(c, source_file);
  }
  return emit(token, buf, c == '=' ? with_eq : alone);
}
// Returns token with text attribute
static int emit_text(Token *token, DynStr *buf, Token_type type, int c) {
  ungetc(c, source_file);
  return make_token(token, type, dynstr_take(buf));
}
// Processes indentation at line start
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
    if (indent_top + 1 >= indent_cap) {
      // Stack is full, double its size
      int *bigger = realloc(indent_stack, 2 * indent_cap * sizeof(int));
      if (!bigger) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(99);
      }
      indent_stack = bigger;
      indent_cap *= 2;
    }
    indent_stack[++indent_top] = line_indent;
    type = T_INDENT;
  } else {
    if (indent_stack[indent_top - 1] <
        line_indent) { // lands between two levels
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
  STATE_START,      // Initial state / whitespace skipping
  STATE_ID,         // Processing identifier or keyword
  STATE_UNDERSCORE, // Read a leading '_', needs one more character
  /* ---------- Integer states ---------- */
  STATE_ZERO,           // Read a '0'
  STATE_INT,            // Inside an integer that started with 1-9
  STATE_INT_UNDERSCORE, // Read '_' in an integer, a digit must follow
  /* ---------- Double states ---------- */
  STATE_DOT,               // Read '.'
  STATE_INT_DOT,           // Integer followed by '.'
  STATE_DOUBLE,            // Processing Double
  STATE_DOUBLE_UNDERSCORE, // Read '_' in Double
  STATE_EXP,               // Read 'e' or 'E'
  STATE_EXP_SIGN,          // Read the sign of the exponent
  STATE_EXP_DIGIT,         // Processing exponent digits
  STATE_EXP_UNDERSCORE,    // Read '_' in the exponent
  /* ---------- Comments & Division states ---------- */
  STATE_SLASH,               // '/' read
  STATE_LINE_COMMENT,        // Line comment //
  STATE_BLOCK_COMMENT,       // Block comment /*
  STATE_BLOCK_COMMENT_SLASH, // '/' inside block comment
  STATE_COMMENT_NL_REST,     // after a block comment that contained LF
  STATE_COMMENT_NL_SLASH,    // '/' read in STATE_COMMENT_NL_REST
  STATE_BLOCK_COMMENT_END,   // '*' inside block comment
  /* ---------- Operators & Assignment states ---------- */
  STATE_ASSIGN,  // '=' read
  STATE_EXCLAM,  // '!' read
  STATE_AMP,     // '&' read
  STATE_PIPE,    // '|' read
  STATE_LESS,    // '<' read
  STATE_GREATER, // '>' read
  /* ---------- String states ---------- */

  STATE_STRING_START,    // Node 'f1' (read initial '"')
  STATE_STRING_QUOTE2,   // Node 'f2' (read '""')
  STATE_STRING,          // Node 'STR' (inside single-line string literal)
  STATE_STRING_ESC,      // Node 'f_esc' (read '\' inside single-line string)
  STATE_STRING_MULTI,    // Node 'f_m' (inside """ multi-line string)
  STATE_STRING_MULTI_Q1, // Helper for closing multi-line string (read 1st
                         // closing '"')
  STATE_STRING_MULTI_Q2, // Helper for closing multi-line string (read 2nd
                         // closing '"')
  STATE_STRING_MULTI_Q3, // Read 3 or more closing '"' in a row

  STATE_AT // '@' read
} State;

/**
 * @brief Reads the next token from the input stream.
 * @param token Pointer to the Token structure to be filled.
 * @return 0 on success, 1 on lexical error.
 * @note Dynamically allocated attribute memory must be freed using
 * free_token().
 */
int get_token(Token *token) {
  if (!source_file || !token) {
    return 1;
  }
  token->attribute = NULL;

  DynStr buffer;
  dynstr_init(&buffer);

  State state = STATE_START;
  int c;
  int comment_depth = 0;
  bool comment_had_nl = false;

  while (1) {
    c = fgetc(source_file);

    switch (state) {
    case STATE_START:
      // Skip spaces and track line indent
      if (c == ' ') {
        if (at_line_start && counting_indent) {
          line_indent++;
        }
        break;
      }
      if (c == '\t') {
        if (at_line_start && counting_indent) {
          saw_tab = true;
        }
        break;
      }
      if (c == '\r') {
        break;
      }
      // Track line numbers and return T_EOL
      if (c == '\n') {
        if (paren_depth > 0) {
          current_line++;
          break;
        }
        if (at_line_start) {
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

      // End of file token
      if (c == EOF) {
        if (!at_line_start) {
          reset_line_state();
          return emit(token, &buffer, T_EOL);
        }
        if (indent_top > 0) {
          indent_top--;
          return emit(token, &buffer, T_DEDENT);
        }
        return emit(token, &buffer, T_EOF);
      }
      // Check indentation level at line start
      if (at_line_start && c != '/') {
        int res = check_indent(token, &buffer, c);
        if (res != -1) {
          return res;
        }
      }
      // Identifiers or keywords
      if (isalpha(c)) {
        dynstr_append(&buffer, (char)c);
        state = STATE_ID;
        break;
      }

      // Identifier starting with '_'
      if (c == '_') {
        dynstr_append(&buffer, (char)c);
        state = STATE_UNDERSCORE;
        break;
      }

      // Zero literal or Double starting with '0'
      if (c == '0') {
        dynstr_append(&buffer, (char)c);
        state = STATE_ZERO;
        break;
      }

      // Non-zero integer literal ('1'-'9')
      if (isdigit(c)) {
        dynstr_append(&buffer, (char)c);
        state = STATE_INT;
        break;
      }

      //  Double may start with a dot, e.g. ".5"
      if (c == '.') {
        dynstr_append(&buffer, '.');
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
      if (c == '&') {
        state = STATE_AMP;
        break;
      }
      if (c == '|') {
        state = STATE_PIPE;
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
        token_start_line = current_line;
        state = STATE_STRING_START;
        break;
      }
      if (c == '@') {
        state = STATE_AT;
        break;
      }
      // Single-character operators and symbols
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
        // Invalid character error
        return lex_error(&buffer);
      }

    case STATE_UNDERSCORE:
      // Standalone '_' is forbidden. Must be followed by alnum or another '_'
      if (isalnum(c) || c == '_') {
        dynstr_append(&buffer, (char)c);
        state = STATE_ID;
      } else {
        return lex_error(&buffer);
      }
      break;

    case STATE_ID:
      // Accumulate characters for identifiers/keywords
      if (isalnum(c) || c == '_') {
        dynstr_append(&buffer, (char)c);
      } else {
        ungetc(c, source_file);

        char *attr_str = dynstr_take(&buffer);
        Token_type kw_type = check_keyword(attr_str);

        if (kw_type == T_ID) {
          return make_token(token, T_ID, attr_str);
        } else {
          free(attr_str);
          return make_token(token, kw_type, NULL);
        }
      }
      break;

    case STATE_ZERO:
      if (c == '.') {
        dynstr_append(&buffer, (char)c);
        state = STATE_INT_DOT;
      } else if (c == 'e' || c == 'E') {
        dynstr_append(&buffer, (char)c);
        state = STATE_EXP;
      } else if (isdigit(c) || c == '_') {
        return lex_error(&buffer); // Leading zeros forbidden (e.g. "05")
      } else {
        return emit_text(token, &buffer, T_LIT_INT,
                         c); // Return simple "0" integer token
      }
      break;

    case STATE_INT:
      if (isdigit(c)) {
        dynstr_append(&buffer, (char)c);
      } else if (c == '_') {
        state = STATE_INT_UNDERSCORE; // We don't store '_', only remember it
                                      // was read
      } else if (c == '.') {
        dynstr_append(&buffer, (char)c);
        state = STATE_INT_DOT;
      } else if (c == 'e' || c == 'E') {
        dynstr_append(&buffer, (char)c);
        state = STATE_EXP;
      } else {
        return emit_text(
            token, &buffer, T_LIT_INT,
            c); // End of the integer, give the extra character back
      }
      break;

    case STATE_INT_UNDERSCORE:
      if (isdigit(c)) { // '_' is only allowed between digits
        dynstr_append(&buffer, (char)c);
        state = STATE_INT;
      } else {
        return lex_error(&buffer);
      }
      break;

    case STATE_DOT:
      if (isdigit(
              c)) { // "1." and "." alone are invalid, digit required after dot
        dynstr_append(&buffer, (char)c);
        state = STATE_DOUBLE;
      } else {
        ungetc(c, source_file);
        return emit(token, &buffer, T_DOT);
      }
      break;

    case STATE_INT_DOT:
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
      } else if (c == 'd' || c == 'D') { // Optional suffix: belongs to number,
                                         // nothing to put back
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
      if (c == '+' || c == '-') { // 'e'/'E' without sign or digits is invalid
        dynstr_append(&buffer, (char)c);
        state = STATE_EXP_SIGN;
      } else if (isdigit(c)) {
        dynstr_append(&buffer, (char)c);
        state = STATE_EXP_DIGIT;
      } else {
        return lex_error(&buffer); // e.g. "1e"
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
      } else if (c == '_') { // Skip appending '_' inside exponent digits
        state = STATE_EXP_UNDERSCORE;
      } else if (c == 'd' ||
                 c == 'D') { // Optional suffix, same as in STATE_DOUBLE
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
        // Single '/' -> division operator
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
      } else if (c == '/') {
        state = STATE_BLOCK_COMMENT_SLASH;
      } else if (c == '\n') {
        current_line++; // Track line numbers inside block comment
        comment_had_nl = true;
      } else if (c == EOF) {
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
        if (comment_depth > 0) {
          state = STATE_BLOCK_COMMENT;
        } else {
          state = comment_had_nl ? STATE_COMMENT_NL_REST : STATE_START;
        }

        // Closing block comment */ -> return to START
      } else if (c == '*') {
        // Stay in END state to handle consecutive asterisks (e.g. '***')
        state = STATE_BLOCK_COMMENT_END;
      } else {
        if (c == '\n') {
          current_line++;
          comment_had_nl = true;
        } else if (c == EOF) {
          return lex_error(&buffer);
        }
        state = STATE_BLOCK_COMMENT;
      }
      break;
    case STATE_COMMENT_NL_REST:
      if (c == ' ' || c == '\t' || c == '\r') {
        break; // only whitespace may follow
      }
      if (c == '\n' || c == EOF) {
        ungetc(c, source_file); // let STATE_START handle EOL / EOF
        state = STATE_START;
        break;
      }
      if (c == '/') {
        state = STATE_COMMENT_NL_SLASH; // maybe another comment
        break;
      }
      return lex_error(&buffer); // a token after such a comment

    case STATE_COMMENT_NL_SLASH:
      if (c == '/') {
        state = STATE_LINE_COMMENT; // '//' up to the end of line
      } else if (c == '*') {
        // Another block comment; comment_had_nl stays true, so after it
        // we return to STATE_COMMENT_NL_REST again
        comment_depth = 1;
        state = STATE_BLOCK_COMMENT;
      } else {
        return lex_error(&buffer); // '/' as division is a token -> error
      }
      break;

    /* ---------- Comparison, assignment & logical operators ---------- */
    case STATE_ASSIGN:
      return emit_pair(token, &buffer, c, T_EQ, T_ASSIGN);

    case STATE_EXCLAM:
      return emit_pair(token, &buffer, c, T_NEQ, T_NOT); // '!=' or '!'

    case STATE_AMP:
      if (c == '&') {
        return emit(token, &buffer, T_AND);
      }
      return lex_error(&buffer); // a single '&' is not an operator

    case STATE_PIPE:
      if (c == '|') {
        return emit(token, &buffer, T_OR);
      }
      return lex_error(&buffer); // a single '|' is not an operator

    case STATE_LESS:
      if (c == '-') {
        return emit(token, &buffer, T_ARROW_LEFT); // '<-' in for loops
      }
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
        return lex_error(
            &buffer); // Unescaped LF is illegal in single-line string
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
        state = STATE_STRING_MULTI_Q3;
      } else {
        dynstr_append(&buffer, '"');
        dynstr_append(&buffer, '"');
        ungetc(c, source_file);
        state = STATE_STRING_MULTI;
      }
      break;

    case STATE_STRING_MULTI_Q3:
      // The last three quotes of a run close the string, any extra quotes
      // before them belong to the content: """a""""" is the string a""
      if (c == '"') {
        dynstr_append(&buffer, '"');
      } else {
        ungetc(c, source_file);
        int res = make_token(token, T_LIT_STRING, dynstr_take(&buffer));
        token->line = token_start_line;
        return res;
      }
      break;

    case STATE_AT:
      // Alphanumeric or underscore
      if (isalnum(c) || c == '_') {
        dynstr_append(&buffer, (char)c);
        break;
      }
      ungetc(c, source_file);                 // Return the extra character
      if (strcmp(buffer.data, "main") == 0) { // Check if the attribute is
                                              // "main"
        return emit(token, &buffer, T_AT_MAIN);
      }
      return lex_error(&buffer);
    }
  }
}
