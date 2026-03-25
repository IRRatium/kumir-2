#ifndef KUMIR_H
#define KUMIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================
// ЛЕКСЕР
// ========================
typedef enum {
    TOKEN_ALG, TOKEN_NACH, TOKEN_KON, TOKEN_VYVOD, TOKEN_VOZVRAT, TOKEN_ISPOLZOVAT,
    TOKEN_TYPE_CEL, TOKEN_TYPE_VESH, TOKEN_TYPE_LIT, TOKEN_TYPE_LOG, TOKEN_TYPE_TAB,
    TOKEN_ESLI, TOKEN_TO, TOKEN_INACHE, TOKEN_VSE,
    TOKEN_NC, TOKEN_POKA, TOKEN_RAZ, TOKEN_KC,
    TOKEN_DA, TOKEN_NET, TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_FLOAT_LIT, TOKEN_STRING,
    TOKEN_ASSIGN, TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV, TOKEN_MOD,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_COMMA,
    TOKEN_EOF, TOKEN_UNKNOWN
} KTokenType;

typedef struct {
    KTokenType type; char* value; int line;
} Token;

// ========================
// ДАННЫЕ
// ========================
typedef enum { VAL_INT, VAL_FLOAT, VAL_STR, VAL_ARRAY } ValType;

struct KArray;

typedef struct {
    ValType type;
    long long i;   // ИСПРАВЛЕНО: long long вместо int — поддержка больших чисел
    double f;
    char* s;
    struct KArray* arr;
} KValue;

typedef struct KArray {
    int ref_count;
    int length;
    KValue* items;
} KArray;

// ========================
// AST УЗЛЫ
// ========================
typedef enum {
    AST_PROGRAM, AST_FUNC_DEF, AST_FUNC_CALL, AST_PARAM, AST_BODY,
    AST_PRINT, AST_VAR_DECL, AST_ASSIGN, AST_RETURN,
    AST_BINOP, AST_VAR, AST_NUM, AST_FLOAT, AST_STR,
    AST_IF, AST_WHILE, AST_REPEAT,
    AST_ARRAY_LIT, AST_INDEX_ACCESS
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char* string_value;
    long long int_value;   // ИСПРАВЛЕНО: long long
    double float_value;
    int line;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int children_count;
} ASTNode;

char* read_file_content(const char* filename);
Token get_next_token(const char** source, int* current_line);
ASTNode* parse(const char* source);
void execute(ASTNode* node);
void parse_error(int line, const char* msg, const char* detail);
void runtime_error(int line, const char* msg, const char* detail);

KValue make_int(long long v);   // ИСПРАВЛЕНО: long long
KValue make_float(double v);
KValue make_str(const char* v);
KValue make_array(int size);

#endif
