#include <stdio.h>
#include "scanner.h"

const char *token_type_to_string(Token_type type) {
    switch (type) {
        case T_EOF: return "T_EOF";
        case T_EOL: return "T_EOL";
        case T_INDENT: return "T_INDENT";
        case T_DEDENT: return "T_DEDENT";
        case T_DEF: return "T_DEF";
        case T_DO: return "T_DO";
        case T_DOUBLE: return "T_DOUBLE";
        case T_ELSE: return "T_ELSE";
        case T_IF: return "T_IF";
        case T_IMPORT: return "T_IMPORT";
        case T_INT: return "T_INT";
        case T_RETURN: return "T_RETURN";
        case T_STRING: return "T_STRING";
        case T_THEN: return "T_THEN";
        case T_UNIT: return "T_UNIT";
        case T_VAL: return "T_VAL";
        case T_VAR: return "T_VAR";
        case T_WHILE: return "T_WHILE";
        case T_ID: return "T_ID";
        case T_LIT_INT: return "T_LIT_INT";
        case T_LIT_DOUBLE: return "T_LIT_DOUBLE";
        case T_LIT_STRING: return "T_LIT_STRING";
        case T_AT_MAIN: return "T_AT_MAIN";
        case T_PLUS: return "T_PLUS";
        case T_MINUS: return "T_MINUS";
        case T_MUL: return "T_MUL";
        case T_DIV: return "T_DIV";
        case T_ASSIGN: return "T_ASSIGN";
        case T_EQ: return "T_EQ";
        case T_NEQ: return "T_NEQ";
        case T_LESS_T: return "T_LESS_T";
        case T_LESS_OR_EQ: return "T_LESS_OR_EQ";
        case T_GREATER_T: return "T_GREATER_T";
        case T_GREATER_OR_EQ: return "T_GREATER_OR_EQ";
        case T_LPAR: return "T_LPAR";
        case T_RPAR: return "T_RPAR";
        case T_COLON: return "T_COLON";
        case T_SEMICOLON: return "T_SEMICOLON";
        case T_COMMA: return "T_COMMA";
        default: return "UNKNOWN_TOKEN";
    }
}

int main(void) {
    FILE *f = fopen("test.ifj", "r");
    if (!f) {
        perror("Failed to open test.ifj");
        return 99;
    }

    scanner_init(f);

    Token token;
    int result;

    printf("--- SCANNER TEST START ---\n");
    while ((result = get_token(&token)) == 0) {
        printf("Line %d | Token: %-15s | Attr: %s\n", 
               token.line, token_type_to_string(token.type), token.attribute ? token.attribute : "NULL");
        if (token.type == T_EOF) {
            free_token(&token);
            break;
        }
        free_token(&token);
    }

    if (result != 0) {
        printf("-- LEXICAL ERROR OCCURRED! Error code: %d\n", result);
    }
    printf("--- SCANNER TEST END ---\n");

    fclose(f);
    return 0;
}