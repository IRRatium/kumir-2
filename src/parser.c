#include "kumir.h"

static int       current_line = 1;
static Token     current_token;
static const char* src_ptr;

static void advance() {
    current_token = get_next_token(&src_ptr, &current_line);
}

void parse_error(int line, const char* msg, const char* detail) {
    printf("\n[ОШИБКА СИНТАКСИСА] Строка %d: %s '%s'\n", line, msg, detail ? detail : "");
    exit(1);
}

static ASTNode* create_node(ASTNodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type          = type;
    node->string_value  = NULL;
    node->int_value     = 0;
    node->left          = NULL;
    node->right         = NULL;
    node->children      = NULL;
    node->children_count = 0;
    node->line          = current_token.line;
    return node;
}

static void add_child(ASTNode* parent, ASTNode* child) {
    parent->children_count++;
    parent->children = realloc(parent->children,
                               parent->children_count * sizeof(ASTNode*));
    parent->children[parent->children_count - 1] = child;
}

// ========================
// Выражения
// ========================
static ASTNode* parse_expr();

static ASTNode* parse_factor() {
    // Число: 42
    if (current_token.type == TOKEN_NUMBER) {
        ASTNode* node = create_node(AST_NUM);
        node->int_value = atoi(current_token.value);
        advance();
        return node;
    }

    // Идентификатор: либо переменная, либо вызов алгоритма
    if (current_token.type == TOKEN_IDENTIFIER) {
        char* name = current_token.value;
        int   line = current_token.line;
        advance();

        if (current_token.type == TOKEN_LPAREN) {
            // Вызов алгоритма в выражении: Имя(арг1, арг2)
            ASTNode* call = create_node(AST_FUNC_CALL);
            call->string_value = name;
            call->line = line;
            advance(); // consume '('

            if (current_token.type != TOKEN_RPAREN) {
                add_child(call, parse_expr());
                while (current_token.type == TOKEN_COMMA) {
                    advance();
                    add_child(call, parse_expr());
                }
            }
            if (current_token.type != TOKEN_RPAREN)
                parse_error(current_token.line, "Ожидалась ')' в вызове алгоритма", name);
            advance(); // consume ')'
            return call;
        } else {
            // Просто переменная
            ASTNode* node = create_node(AST_VAR);
            node->string_value = name;
            node->line = line;
            return node;
        }
    }

    // Строка: "текст"
    if (current_token.type == TOKEN_STRING) {
        ASTNode* node = create_node(AST_STR);
        node->string_value = current_token.value;
        advance();
        return node;
    }

    // Скобочное выражение: (a + b)
    if (current_token.type == TOKEN_LPAREN) {
        advance();
        ASTNode* node = parse_expr();
        if (current_token.type != TOKEN_RPAREN)
            parse_error(current_token.line, "Ожидалась закрывающая скобка", ")");
        advance();
        return node;
    }

    parse_error(current_token.line, "Неожиданный символ",
                current_token.value ? current_token.value : "конец файла");
    return NULL; // не достигается
}

static ASTNode* parse_term() {
    ASTNode* node = parse_factor();
    while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV) {
        ASTNode* parent = create_node(AST_BINOP);
        parent->string_value = current_token.type == TOKEN_MUL ? "*" : "/";
        parent->left = node;
        advance();
        parent->right = parse_factor();
        node = parent;
    }
    return node;
}

static ASTNode* parse_expr() {
    ASTNode* node = parse_term();
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        ASTNode* parent = create_node(AST_BINOP);
        parent->string_value = current_token.type == TOKEN_PLUS ? "+" : "-";
        parent->left = node;
        advance();
        parent->right = parse_term();
        node = parent;
    }
    return node;
}

// ========================
// Операторы внутри нач...кон
// ========================
static ASTNode* parse_statement() {
    // Объявление переменной: цел имя
    if (current_token.type == TOKEN_TYPE_CEL) {
        advance();
        if (current_token.type != TOKEN_IDENTIFIER)
            parse_error(current_token.line, "Ожидалось имя переменной после 'цел'", "");
        ASTNode* decl = create_node(AST_VAR_DECL);
        decl->string_value = current_token.value;
        advance();
        return decl;
    }

    // Возврат значения: знач := выражение
    if (current_token.type == TOKEN_ZNACH) {
        ASTNode* ret = create_node(AST_ZNACH);
        advance(); // consume 'знач'
        if (current_token.type != TOKEN_ASSIGN)
            parse_error(current_token.line, "Ожидалось ':=' после 'знач'", "");
        advance(); // consume ':='
        ret->left = parse_expr();
        return ret;
    }

    // Вывод: вывод выр, выр, ...
    if (current_token.type == TOKEN_VYVOD) {
        ASTNode* print_node = create_node(AST_PRINT);
        advance();
        add_child(print_node, parse_expr());
        while (current_token.type == TOKEN_COMMA) {
            advance();
            add_child(print_node, parse_expr());
        }
        return print_node;
    }

    // Присваивание или вызов алгоритма-процедуры
    if (current_token.type == TOKEN_IDENTIFIER) {
        char* name = current_token.value;
        int   line = current_token.line;
        advance();

        // Присваивание: a := выражение
        if (current_token.type == TOKEN_ASSIGN) {
            ASTNode* assign = create_node(AST_ASSIGN);
            assign->string_value = name;
            assign->line = line;
            advance(); // consume ':='
            assign->left = parse_expr();
            return assign;
        }

        // Вызов процедуры: МойАлг(a, b)  — результат игнорируется
        if (current_token.type == TOKEN_LPAREN) {
            ASTNode* call = create_node(AST_FUNC_CALL);
            call->string_value = name;
            call->line = line;
            advance(); // consume '('
            if (current_token.type != TOKEN_RPAREN) {
                add_child(call, parse_expr());
                while (current_token.type == TOKEN_COMMA) {
                    advance();
                    add_child(call, parse_expr());
                }
            }
            if (current_token.type != TOKEN_RPAREN)
                parse_error(current_token.line, "Ожидалась ')' в вызове алгоритма", name);
            advance();
            return call;
        }

        parse_error(line, "Ожидалось ':=' или '(' после имени", name);
    }

    parse_error(current_token.line, "Неизвестная команда",
                current_token.value ? current_token.value : "конец файла");
    return NULL;
}

// ========================
// Определение алгоритма
// ========================
// Синтаксис:
//   алг [цел] ИмяАлгоритма([цел param1, цел param2, ...])
//   нач
//     ...операторы...
//   кон
static ASTNode* parse_func_def() {
    ASTNode* func = create_node(AST_FUNC_DEF);
    func->int_value = 0; // 0 = процедура (void), 1 = функция (цел)
    advance(); // consume 'алг'

    // Опциональный тип возвращаемого значения
    if (current_token.type == TOKEN_TYPE_CEL) {
        func->int_value = 1;
        advance();
    }

    // Имя алгоритма
    if (current_token.type != TOKEN_IDENTIFIER)
        parse_error(current_token.line, "Ожидалось имя алгоритма после 'алг'", "");
    func->string_value = current_token.value;
    advance();

    // Параметры в скобках (опционально)
    if (current_token.type == TOKEN_LPAREN) {
        advance(); // consume '('
        while (current_token.type != TOKEN_RPAREN && current_token.type != TOKEN_EOF) {
            if (current_token.type != TOKEN_TYPE_CEL)
                parse_error(current_token.line,
                            "Ожидался тип параметра (например, 'цел')", "");
            advance();
            if (current_token.type != TOKEN_IDENTIFIER)
                parse_error(current_token.line, "Ожидалось имя параметра", "");

            ASTNode* param = create_node(AST_PARAM);
            param->string_value = current_token.value;
            param->int_value = 1; // цел
            add_child(func, param);
            advance();

            if (current_token.type == TOKEN_COMMA) advance();
        }
        if (current_token.type != TOKEN_RPAREN)
            parse_error(current_token.line, "Ожидалась ')' после параметров", "");
        advance(); // consume ')'
    }

    // нач
    if (current_token.type != TOKEN_NACH)
        parse_error(current_token.line,
                    "Ожидалось 'нач' после заголовка алгоритма", func->string_value);
    advance();

    // Тело: список операторов до 'кон'
    ASTNode* body = create_node(AST_BODY);
    while (current_token.type != TOKEN_KON && current_token.type != TOKEN_EOF) {
        add_child(body, parse_statement());
    }
    if (current_token.type != TOKEN_KON)
        parse_error(current_token.line,
                    "Ожидалось 'кон' в конце алгоритма", func->string_value);
    advance(); // consume 'кон'

    func->left = body;
    return func;
}

// ========================
// Точка входа парсера
// ========================
ASTNode* parse(const char* source) {
    src_ptr      = source;
    current_line = 1;
    advance();

    ASTNode* program = create_node(AST_PROGRAM);

    // Файл — это список определений алгоритмов
    while (current_token.type != TOKEN_EOF) {
        if (current_token.type == TOKEN_ALG) {
            add_child(program, parse_func_def());
        } else {
            parse_error(current_token.line,
                        "Ожидалось объявление алгоритма ('алг')",
                        current_token.value ? current_token.value : "");
        }
    }

    return program;
}
