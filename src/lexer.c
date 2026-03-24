#include "kumir.h"
#include <ctype.h>

// Наша собственная функция копирования строки (замена системной strndup)
char* kumir_strndup(const char* s, size_t n) {
    char* p = malloc(n + 1);
    if (p) {
        strncpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

// Пропуск пробелов и переносов
void skip_whitespace(const char** source) {
    while (**source == ' ' || **source == '\t' || **source == '\n' || **source == '\r') {
        (*source)++;
    }
}

Token get_next_token(const char** source) {
    Token token = {TOKEN_UNKNOWN, NULL};
    skip_whitespace(source);

    if (**source == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }

    // Обработка строк "..."
    if (**source == '"') {
        (*source)++; // пропускаем первую кавычку
        const char* start = *source;
        while (**source != '"' && **source != '\0') (*source)++;
        
        int length = *source - start;
        token.type = TOKEN_STRING;
        token.value = kumir_strndup(start, length); // Используем нашу функцию
        
        if (**source == '"') (*source)++; // пропускаем вторую кавычку
        return token;
    }

    // Обработка ключевых слов (алг, нач, кон, вывод)
    const char* start = *source;
    // Пока идут русские буквы в UTF-8 (байты > 127) или английские
    while (isalnum((unsigned char)**source) || (unsigned char)**source > 127) {
        (*source)++;
    }

    int length = *source - start;
    if (length > 0) {
        char* word = kumir_strndup(start, length); // Используем нашу функцию
        if (strcmp(word, "алг") == 0) token.type = TOKEN_ALG;
        else if (strcmp(word, "нач") == 0) token.type = TOKEN_NACH;
        else if (strcmp(word, "кон") == 0) token.type = TOKEN_KON;
        else if (strcmp(word, "вывод") == 0) token.type = TOKEN_VYVOD;
        else token.type = TOKEN_UNKNOWN;
        
        token.value = word;
        return token;
    }

    (*source)++; // Если непонятный символ
    return token;
}
