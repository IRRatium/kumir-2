#ifndef KUMIR_H
#define KUMIR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================
// ЛЕКСЕР: Типы токенов
// ========================
typedef enum {
    TOKEN_ALG,          // алг
    TOKEN_NACH,         // нач
    TOKEN_KON,          // кон
    TOKEN_VYVOD,        // вывод
    TOKEN_TYPE_CEL,     // цел
    TOKEN_ZNACH,        // знач (возврат значения из алгоритма)
    TOKEN_IDENTIFIER,   // имя переменной или алгоритма
    TOKEN_NUMBER,       // число (5, 100)
    TOKEN_STRING,       // "строка"
    TOKEN_ASSIGN,       // :=
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_MUL,          // *
    TOKEN_DIV,          // /
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_COMMA,        // ,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} KTokenType;

typedef struct {
    KTokenType type;
    char* value;
    int line;
} Token;

// ========================
// ПАРСЕР: Узлы AST
// ========================
typedef enum {
    AST_PROGRAM,    // Корень: список алгоритмов
    AST_FUNC_DEF,   // Определение алгоритма: алг [цел] Имя([параметры]) нач...кон
    AST_FUNC_CALL,  // Вызов алгоритма: Имя(арг1, арг2)
    AST_PARAM,      // Параметр алгоритма: цел a
    AST_BODY,       // Тело алгоритма (нач...кон)
    AST_PRINT,      // вывод
    AST_VAR_DECL,   // Объявление переменной: цел a
    AST_ASSIGN,     // Присваивание: a := выражение
    AST_ZNACH,      // Возврат значения: знач := выражение
    AST_BINOP,      // Бинарная операция: a + b
    AST_VAR,        // Переменная
    AST_NUM,        // Число
    AST_STR         // Строка
} ASTNodeType;

// Схема хранения данных в узлах:
//
//  AST_FUNC_DEF:  string_value=имя, int_value=1(цел)/0(void),
//                 children[]=AST_PARAM, left=AST_BODY
//  AST_FUNC_CALL: string_value=имя, children[]=аргументы
//  AST_PARAM:     string_value=имя, int_value=1(цел)
//  AST_BODY:      children[]=операторы
//  AST_ASSIGN:    string_value=имя_переменной, left=выражение
//  AST_ZNACH:     left=выражение
//  AST_BINOP:     string_value=оператор(+,-,*,/), left, right

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
// ПРОТОТИПЫ ФУНКЦИЙ
// ========================
Token get_next_token(const char** source, int* current_line);
ASTNode* parse(const char* source);
void execute(ASTNode* node);
void parse_error(int line, const char* msg, const char* detail);
void runtime_error(int line, const char* msg, const char* detail);

#endif
