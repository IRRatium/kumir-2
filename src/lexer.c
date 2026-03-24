#include "kumir.h"
#include <ctype.h>

char* kumir_strndup(const char* s, size_t n) {
    char* p = malloc(n + 1);
    if (p) { strncpy(p, s, n); p[n] = '\0'; }
    return p;
}

// Пропускаем пробелы и комментарии (комментарий начинается с '|')
void skip_whitespace_and_comments(const char** source, int* line) {
    while (**source != '\0') {
        if (**source == ' ' || **source == '\t' || **source == '\r') {
            (*source)++;
        } else if (**source == '\n') {
            (*line)++;
            (*source)++;
        } else if (**source == '|') {
            while (**source != '\n' && **source != '\0') (*source)++;
        } else {
            break;
        }
    }
}

Token get_next_token(const char** source, int* line) {
    Token token = {TOKEN_UNKNOWN, NULL, *line};
    skip_whitespace_and_comments(source, line);
    token.line = *line;

    if (**source == '\0') { token.type = TOKEN_EOF; return token; }

    // Односимвольные токены
    if (**source == '+') { (*source)++; token.type = TOKEN_PLUS;   return token; }
    if (**source == '-') { (*source)++; token.type = TOKEN_MINUS;  return token; }
    if (**source == '*') { (*source)++; token.type = TOKEN_MUL;    return token; }
    if (**source == '/') { (*source)++; token.type = TOKEN_DIV;    return token; }
    if (**source == '(') { (*source)++; token.type = TOKEN_LPAREN; return token; }
    if (**source == ')') { (*source)++; token.type = TOKEN_RPAREN; return token; }
    if (**source == ',') { (*source)++; token.type = TOKEN_COMMA;  return token; }

    // Оператор присваивания :=
    if (**source == ':' && *(*source + 1) == '=') {
        *source += 2;
        token.type = TOKEN_ASSIGN;
        return token;
    }

    // Числа
    if (isdigit((unsigned char)**source)) {
        const char* start = *source;
        while (isdigit((unsigned char)**source)) (*source)++;
        token.type = TOKEN_NUMBER;
        token.value = kumir_strndup(start, *source - start);
        return token;
    }

    // Строки в кавычках
    if (**source == '"') {
        (*source)++;
        const char* start = *source;
        while (**source != '"' && **source != '\0') {
            if (**source == '\n') (*line)++;
            (*source)++;
        }
        token.type = TOKEN_STRING;
        token.value = kumir_strndup(start, *source - start);
        if (**source == '"') (*source)++;
        return token;
    }

    // Ключевые слова и идентификаторы (включая кириллицу)
    const char* start = *source;
    while (isalnum((unsigned char)**source) || (unsigned char)**source > 127 || **source == '_') {
        (*source)++;
    }

    int length = *source - start;
    if (length > 0) {
        char* word = kumir_strndup(start, length);

        if      (strcmp(word, "алг")   == 0) token.type = TOKEN_ALG;
        else if (strcmp(word, "нач")   == 0) token.type = TOKEN_NACH;
        else if (strcmp(word, "кон")   == 0) token.type = TOKEN_KON;
        else if (strcmp(word, "вывод") == 0) token.type = TOKEN_VYVOD;
        else if (strcmp(word, "цел")   == 0) token.type = TOKEN_TYPE_CEL;
        else if (strcmp(word, "знач")  == 0) token.type = TOKEN_ZNACH;
        else                                 token.type = TOKEN_IDENTIFIER;

        token.value = word;
        return token;
    }

    (*source)++;
    return token;
}
