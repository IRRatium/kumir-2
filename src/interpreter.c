#include "kumir.h"

// Таблица переменных (Память Кумира)
typedef struct {
    char name[50];
    int value;
    int is_declared;
} Variable;

Variable sym_table[100];
int var_count = 0;

void runtime_error(int line, const char* msg, const char* detail) {
    printf("\n[КРИТИЧЕСКАЯ ОШИБКА] Строка %d: %s '%s'\n", line, msg, detail ? detail : "");
    printf("Выполнение программы остановлено.\n");
    exit(1);
}

// Поиск переменной
int get_var(char* name, int line) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) return sym_table[i].value;
    }
    runtime_error(line, "Переменная не объявлена:", name);
    return 0;
}

// Запись в переменную
void set_var(char* name, int value, int line) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            sym_table[i].value = value;
            return;
        }
    }
    runtime_error(line, "Попытка записать в необъявленную переменную:", name);
}

// Вычислитель выражений
int eval(ASTNode* node) {
    if (node->type == AST_NUM) return node->int_value;
    if (node->type == AST_VAR) return get_var(node->string_value, node->line);
    if (node->type == AST_BINOP) {
        int left = eval(node->left);
        int right = eval(node->right);
        if (strcmp(node->string_value, "+") == 0) return left + right;
        if (strcmp(node->string_value, "-") == 0) return left - right;
        if (strcmp(node->string_value, "*") == 0) return left * right;
        if (strcmp(node->string_value, "/") == 0) {
            if (right == 0) runtime_error(node->line, "Деление на ноль!", "");
            return left / right;
        }
    }
    return 0;
}

void execute(ASTNode* node) {
    if (!node) return;

    if (node->type == AST_PROGRAM) {
        for (int i = 0; i < node->children_count; i++) execute(node->children[i]);
    } 
    else if (node->type == AST_VAR_DECL) {
        // Объявление: цел a
        strcpy(sym_table[var_count].name, node->string_value);
        sym_table[var_count].value = 0;
        sym_table[var_count].is_declared = 1;
        var_count++;
    }
    else if (node->type == AST_ASSIGN) {
        // Присваивание: a := 5 + 5
        int val = eval(node->left);
        set_var(node->string_value, val, node->line);
    }
    else if (node->type == AST_PRINT) {
        // Вывод: может быть несколько аргументов через запятую
        for (int i = 0; i < node->children_count; i++) {
            ASTNode* arg = node->children[i];
            if (arg->type == AST_STR) {
                printf("%s", arg->string_value);
            } else {
                printf("%d", eval(arg));
            }
        }
        printf("\n");
    }
}
