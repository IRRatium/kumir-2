#ifndef KUMIR_H
#define KUMIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- ЛЕКСЕР (Токены) ---
typedef enum {
    TOKEN_ALG, TOKEN_NACH, TOKEN_KON, TOKEN_VYVOD,
    TOKEN_TYPE_CEL,         // цел
    TOKEN_IDENTIFIER,       // Имя переменной (a, b, сумма)
    TOKEN_NUMBER,           // Число (5, 100)
    TOKEN_STRING,           // "текст"
    TOKEN_ASSIGN,           // :=
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV, // + - * /
    TOKEN_LPAREN, TOKEN_RPAREN, // ( )
    TOKEN_COMMA,            // ,
    TOKEN_EOF, TOKEN_UNKNOWN
} KTokenType;

typedef struct {
    KTokenType type;
    char* value;
    int line; // Храним номер строки для ошибок!
} Token;

// --- ПАРСЕР (Узлы AST) ---
typedef enum {
    AST_PROGRAM, AST_PRINT, AST_VAR_DECL, AST_ASSIGN,
    AST_BINOP, AST_VAR, AST_NUM, AST_STR
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char* string_value;
    int int_value;
    int line; // Строка для ошибок выполнения
    struct ASTNode* left;   // Для математики (левая часть)
    struct ASTNode* right;  // Для математики (правая часть)
    struct ASTNode** children;
    int children_count;
} ASTNode;

// Функции
Token get_next_token(const char** source, int* current_line);
ASTNode* parse(const char* source);
void execute(ASTNode* node);

// Функции ошибок
void parse_error(int line, const char* msg, const char* detail);
void runtime_error(int line, const char* msg, const char* detail);

#endif
