/** 
 * Project IFJ26 compiler
 * File: scanner.c
 * Description: Implementation of the lexical analyzer 
 * Author: <xkotliy00> Yaryna Kotliarenko
 */

#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Dynamic, expandable buffer for text of unknown length
typedef struct {
    char *data;      // pointer to the character array
    size_t length;   // number of characters currently stored
    size_t capacity; // total allocated capacity of the buffer
} DynStr;

// Initializes an empty string buffer
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

// Append a character to the string buffer, expanding if necessary
static bool dynstr_append(DynStr *s, char c) {
    if (s->length + 2 > s->capacity) {
        size_t new_cap = s->capacity * 2;
        char *new_data = realloc(s->data, new_cap);
        if (!new_data) return false;
        s->data = new_data;
        s->capacity = new_cap;
    }
    s->data[s->length] = c;
    s->length++;
    s->data[s->length] = '\0';
    return true;
}

// Transfer buffer ownership to token attribute and reset the buffer
static char *dynstr_take(DynStr *s) {
    char *result = s->data;
    s->data = NULL;
    s->length = 0;
    s->capacity = 0;
    return result;
}

// Free the memory allocated for the string buffer
static void dynstr_free(DynStr *s) {
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }
    s->length = 0;
    s->capacity = 0;
}

static FILE *source_file = NULL; 
static int current_line = 1;

void scanner_init(FILE *source) {
    source_file = source;
    current_line = 1;
}

void free_token(Token *token) {
    if (token && token->attribute) {
        free(token->attribute);
        token->attribute = NULL;
    }
}

// Сверка ключевых слов по спецификации IFJ26 (Раздел 1.1)
static Token_type check_keyword(const char *str) {
    if (strcmp(str, "def") == 0) return T_DEF;
    if (strcmp(str, "do") == 0) return T_DO;
    if (strcmp(str, "Double") == 0) return T_DOUBLE;
    if (strcmp(str, "else") == 0) return T_ELSE;
    if (strcmp(str, "if") == 0) return T_IF;
    if (strcmp(str, "import") == 0) return T_IMPORT;
    if (strcmp(str, "Int") == 0) return T_INT;
    if (strcmp(str, "return") == 0) return T_RETURN;
    if (strcmp(str, "String") == 0) return T_STRING;
    if (strcmp(str, "then") == 0) return T_THEN;
    if (strcmp(str, "Unit") == 0) return T_UNIT;
    if (strcmp(str, "val") == 0) return T_VAL;
    if (strcmp(str, "var") == 0) return T_VAR;
    if (strcmp(str, "while") == 0) return T_WHILE;

    return T_ID; // Идентификатор, если слова нет в списке ключевых
}

typedef enum {
    STATE_START,
    STATE_ID,
    STATE_UNDERSCORE
} State;

int get_token(Token *token) {
    if (!source_file || !token) {
        return 1;
    }

    State state = STATE_START;
    int c;

    DynStr buffer;
    if (!dynstr_init(&buffer)) {
        return 1; // Ошибка аллокации (внутренняя ошибка / лексический сбой)
    }

    while (1) {
        c = fgetc(source_file);

        switch (state) {
            case STATE_START:
                // 1. Пропуск пробелов и табуляций (S -> S)
                if (c == ' ' || c == '\t' || c == '\r') {
                    state = STATE_START;
                    break;
                }

                // 2. Токен EOL (перевод строки \n)
                if (c == '\n') {
                    dynstr_free(&buffer);
                    token->type = T_EOL;
                    token->attribute = NULL;
                    token->line = current_line;
                    current_line++;
                    return 0;
                }

                // 3. Токен EOF (конец файла)
                if (c == EOF) {
                    dynstr_free(&buffer);
                    token->type = T_EOF;
                    token->attribute = NULL;
                    token->line = current_line;
                    return 0;
                }

                // 4. Вход в идентификаторы по лекциям
                if (isalpha(c)) {
                    dynstr_append(&buffer, (char)c);
                    state = STATE_ID;
                    break;
                }

                if (c == '_') {
                    dynstr_append(&buffer, (char)c);
                    state = STATE_UNDERSCORE;
                    break;
                }

                // 5. Односимвольные операторы
                if (c == '+' || c == '-' || c == '*' || c == '(' || 
                    c == ')' || c == ',' || c == ':' || c == ';') {
                    dynstr_free(&buffer);
                    if (c == '+') token->type = T_PLUS;
                    else if (c == '-') token->type = T_MINUS;
                    else if (c == '*') token->type = T_MUL;
                    else if (c == '(') token->type = T_LPAR;
                    else if (c == ')') token->type = T_RPAR;
                    else if (c == ',') token->type = T_COMMA;
                    else if (c == ':') token->type = T_COLON;
                    else if (c == ';') token->type = T_SEMICOLON;

                    token->attribute = NULL;
                    token->line = current_line;
                    return 0;
                }

                // Неизвестный символ
                dynstr_free(&buffer);
                return 1;

            case STATE_UNDERSCORE:
                // Одиночное '_' запрещено спецификацией.
                // За '_' обязана идти буква, цифра или '_'
                if (isalnum(c) || c == '_') {
                    dynstr_append(&buffer, (char)c);
                    state = STATE_ID;
                } else {
                    dynstr_free(&buffer);
                    return 1; // Ошибка 1
                }
                break;

            case STATE_ID:
                if (isalnum(c) || c == '_') {
                    dynstr_append(&buffer, (char)c);
                } else {
                    // Возврат лишнего прочитанного символа обратно в поток по лекции!
                    ungetc(c, source_file);

                    char *attr_str = dynstr_take(&buffer);
                    Token_type kw_type = check_keyword(attr_str);

                    if (kw_type == T_ID) {
                        token->type = T_ID;
                        token->attribute = attr_str;
                    } else {
                        token->type = kw_type;
                        token->attribute = NULL;
                        free(attr_str);
                    }

                    token->line = current_line;
                    return 0;
                }
                break;
        }
    }
}