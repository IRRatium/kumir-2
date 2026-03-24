#ifndef KUMIR_H
#define KUMIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- ЛЕКСЕР (Токены) ---
// Переименовали TokenType в KTokenType, чтобы избежать конфликта с windows.h
typedef enum {
    TOKEN_ALG,      // алг
    TOKEN_NACH,     // нач
    TOKEN_KON,      // кон
    TOKEN_VYVOD,    // вывод
    TOKEN_STRING,   // "текст"
    TOKEN_EOF,      // Конец файла
    TOKEN_UNKNOWN
} KTokenType;

typedef struct {
    KTokenType type;
    char* value;
} Token;

// --- ПАРСЕР (Абстрактное синтаксическое дерево - AST) ---
typedef enum {
    AST_PROGRAM,
    AST_PRINT
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char* string_value;
    struct ASTNode** children;
    int children_count;
} ASTNode;

// Функции
Token get_next_token(const char** source);
ASTNode* parse(const char* source);
void execute(ASTNode* node);

#endif
