#ifndef KUMIR_H
#define KUMIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================
// ЛЕКСЕР: Типы токенов
// ========================
typedef enum {
    TOKEN_ALG, TOKEN_NACH, TOKEN_KON, TOKEN_VYVOD, TOKEN_ZNACH, TOKEN_ISPOLZOVAT,
    TOKEN_TYPE_CEL, TOKEN_TYPE_LIT, TOKEN_TYPE_LOG,
    TOKEN_ESLI, TOKEN_TO, TOKEN_INACHE, TOKEN_VSE,
    TOKEN_NC, TOKEN_POKA, TOKEN_RAZ, TOKEN_KC,
    TOKEN_DA, TOKEN_NET, TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    TOKEN_ASSIGN, // :=
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE, // = <> < > <= >=
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_COMMA, TOKEN_EOF, TOKEN_UNKNOWN
} KTokenType;

typedef struct {
    KTokenType type;
    char* value;
    int line;
} Token;

// ========================
// ЗНАЧЕНИЯ ПЕРЕМЕННЫХ
// ========================
typedef enum { VAL_INT, VAL_STR } ValType;

typedef struct {
    ValType type;
    int i;
    char* s;
} KValue;

// ========================
// ПАРСЕР: Узлы AST
// ========================
typedef enum {
    AST_PROGRAM, AST_FUNC_DEF, AST_FUNC_CALL, AST_PARAM, AST_BODY,
    AST_PRINT, AST_VAR_DECL, AST_ASSIGN, AST_ZNACH,
    AST_BINOP, AST_VAR, AST_NUM, AST_STR,
    AST_IF, AST_WHILE, AST_REPEAT
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char* string_value;
    int int_value;
    int line;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int children_count;
} ASTNode;

// ========================
// ПРОТОТИПЫ
// ========================
char* read_file_content(const char* filename); // теперь глобально доступен
Token get_next_token(const char** source, int* current_line);
ASTNode* parse(const char* source);
void execute(ASTNode* node);
void parse_error(int line, const char* msg, const char* detail);
void runtime_error(int line, const char* msg, const char* detail);

KValue make_int(int v);
KValue make_str(const char* v);

#endif
