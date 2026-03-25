#include "kumir.h"
#include <ctype.h>

char* kumir_strndup(const char* s, size_t n) {
    char* p = malloc(n + 1);
    if (p) { strncpy(p, s, n); p[n] = '\0'; }
    return p;
}

void skip_ws_comments(const char** source, int* line) {
    while (**source != '\0') {
        if (**source == ' ' || **source == '\t' || **source == '\r') (*source)++;
        else if (**source == '\n') { (*line)++; (*source)++; }
        else if (**source == '|') { while (**source != '\n' && **source != '\0') (*source)++; }
        else break;
    }
}

Token get_next_token(const char** source, int* line) {
    Token token = {TOKEN_UNKNOWN, NULL, *line};
    skip_ws_comments(source, line);
    token.line = *line;
    if (**source == '\0') { token.type = TOKEN_EOF; return token; }

    if (strncmp(*source, ":=", 2) == 0) { *source += 2; token.type = TOKEN_ASSIGN; token.value = ":="; return token; }
    if (strncmp(*source, "<>", 2) == 0) { *source += 2; token.type = TOKEN_NEQ; token.value = "<>"; return token; }
    if (strncmp(*source, "<=", 2) == 0) { *source += 2; token.type = TOKEN_LE; token.value = "<="; return token; }
    if (strncmp(*source, ">=", 2) == 0) { *source += 2; token.type = TOKEN_GE; token.value = ">="; return token; }

    if (**source == '=') { (*source)++; token.type = TOKEN_EQ; token.value = "="; return token; }
    if (**source == '<') { (*source)++; token.type = TOKEN_LT; token.value = "<"; return token; }
    if (**source == '>') { (*source)++; token.type = TOKEN_GT; token.value = ">"; return token; }
    if (**source == '+') { (*source)++; token.type = TOKEN_PLUS; token.value = "+"; return token; }
    if (**source == '-') { (*source)++; token.type = TOKEN_MINUS; token.value = "-"; return token; }
    if (**source == '*') { (*source)++; token.type = TOKEN_MUL; token.value = "*"; return token; }
    if (**source == '/') { (*source)++; token.type = TOKEN_DIV; token.value = "/"; return token; }
    if (**source == '%') { (*source)++; token.type = TOKEN_MOD; token.value = "%"; return token; }
    if (**source == '(') { (*source)++; token.type = TOKEN_LPAREN; token.value = "("; return token; }
    if (**source == ')') { (*source)++; token.type = TOKEN_RPAREN; token.value = ")"; return token; }
    if (**source == '[') { (*source)++; token.type = TOKEN_LBRACKET; token.value = "["; return token; }
    if (**source == ']') { (*source)++; token.type = TOKEN_RBRACKET; token.value = "]"; return token; }
    if (**source == ',') { (*source)++; token.type = TOKEN_COMMA; token.value = ","; return token; }

    if (isdigit((unsigned char)**source)) {
        const char* start = *source; int has_dot = 0;
        while (isdigit((unsigned char)**source) || **source == '.') {
            if (**source == '.') has_dot = 1;
            (*source)++;
        }
        token.type = has_dot ? TOKEN_FLOAT_LIT : TOKEN_NUMBER;
        token.value = kumir_strndup(start, *source - start); return token;
    }

    if (**source == '"') {
        (*source)++;
        int cap = 256; 
        char* buf = malloc(cap); 
        int i = 0;
        
        while (**source != '"' && **source != '\0') {
            if (i >= cap - 2) { cap *= 2; buf = realloc(buf, cap); }
            
            if (**source == '\\') {
                (*source)++;
                if (**source == '\0') break;
                if (**source == 'n') buf[i++] = '\n';
                else if (**source == 'r') buf[i++] = '\r';
                else if (**source == 't') buf[i++] = '\t';
                else if (**source == 'e') buf[i++] = '\033'; // ПОДДЕРЖКА ЦВЕТОВ (ANSI ESCAPE)
                else if (**source == '"') buf[i++] = '"';
                else if (**source == '\\') buf[i++] = '\\';
                else buf[i++] = **source;
                (*source)++;
            } else {
                if (**source == '\n') (*line)++;
                buf[i++] = **source;
                (*source)++;
            }
        }
        buf[i] = '\0';
        token.type = TOKEN_STRING; 
        token.value = buf;
        
        if (**source == '"') (*source)++;
        return token;
    }

    const char* start = *source;
    while (isalnum((unsigned char)**source) || (unsigned char)**source > 127 || **source == '_') (*source)++;
    
    int len = *source - start;
    if (len > 0) {
        char* word = kumir_strndup(start, len);
        if      (strcmp(word, "алг")   == 0) token.type = TOKEN_ALG;
        else if (strcmp(word, "нач")   == 0) token.type = TOKEN_NACH;
        else if (strcmp(word, "кон")   == 0) token.type = TOKEN_KON;
        else if (strcmp(word, "вывод") == 0) token.type = TOKEN_VYVOD;
        else if (strcmp(word, "возврат")== 0) token.type = TOKEN_VOZVRAT;
        else if (strcmp(word, "цел")   == 0) token.type = TOKEN_TYPE_CEL;
        else if (strcmp(word, "вещ")   == 0) token.type = TOKEN_TYPE_VESH;
        else if (strcmp(word, "лит")   == 0) token.type = TOKEN_TYPE_LIT;
        else if (strcmp(word, "лог")   == 0) token.type = TOKEN_TYPE_LOG;
        else if (strcmp(word, "таб")   == 0) token.type = TOKEN_TYPE_TAB;
        else if (strcmp(word, "если")  == 0) token.type = TOKEN_ESLI;
        else if (strcmp(word, "то")    == 0) token.type = TOKEN_TO;
        else if (strcmp(word, "иначе") == 0) token.type = TOKEN_INACHE;
        else if (strcmp(word, "все")   == 0) token.type = TOKEN_VSE;
        else if (strcmp(word, "нц")    == 0) token.type = TOKEN_NC;
        else if (strcmp(word, "пока")  == 0) token.type = TOKEN_POKA;
        else if (strcmp(word, "раз")   == 0) token.type = TOKEN_RAZ;
        else if (strcmp(word, "кц")    == 0) token.type = TOKEN_KC;
        else if (strcmp(word, "использовать") == 0) token.type = TOKEN_ISPOLZOVAT;
        else if (strcmp(word, "да")    == 0) token.type = TOKEN_DA;
        else if (strcmp(word, "нет")   == 0) token.type = TOKEN_NET;
        else if (strcmp(word, "и")     == 0) token.type = TOKEN_AND;
        else if (strcmp(word, "или")   == 0) token.type = TOKEN_OR;
        else if (strcmp(word, "не")    == 0) token.type = TOKEN_NOT;
        else                                 token.type = TOKEN_IDENTIFIER;
        token.value = word; return token;
    }
    (*source)++; return token;
}
