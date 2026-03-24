#include "kumir.h"

int current_line = 1;
Token current_token;
const char* src_ptr;

// Функция для получения следующего токена с учетом строки
void advance() {
    current_token = get_next_token(&src_ptr, &current_line);
}

// Вывод красивой ошибки синтаксиса
void parse_error(int line, const char* msg, const char* detail) {
    printf("\n[ОШИБКА СИНТАКСИСА] Строка %d: %s '%s'\n", line, msg, detail ? detail : "");
    exit(1);
}

ASTNode* create_node(ASTNodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type; node->string_value = NULL; node->int_value = 0;
    node->left = NULL; node->right = NULL; node->children = NULL; node->children_count = 0;
    node->line = current_token.line;
    return node;
}

void add_child(ASTNode* parent, ASTNode* child) {
    parent->children_count++;
    parent->children = realloc(parent->children, parent->children_count * sizeof(ASTNode*));
    parent->children[parent->children_count - 1] = child;
}

// Предопределяем функции для математики
ASTNode* parse_expr();

ASTNode* parse_factor() {
    ASTNode* node = NULL;
    if (current_token.type == TOKEN_NUMBER) {
        node = create_node(AST_NUM);
        node->int_value = atoi(current_token.value);
        advance();
    } else if (current_token.type == TOKEN_IDENTIFIER) {
        node = create_node(AST_VAR);
        node->string_value = current_token.value;
        advance();
    } else if (current_token.type == TOKEN_STRING) {
        node = create_node(AST_STR);
        node->string_value = current_token.value;
        advance();
    } else if (current_token.type == TOKEN_LPAREN) {
        advance();
        node = parse_expr();
        if (current_token.type != TOKEN_RPAREN) parse_error(current_token.line, "Ожидалась закрывающая скобка", ")");
        advance();
    } else {
        parse_error(current_token.line, "Неизвестное выражение или символ", current_token.value ? current_token.value : "Конец файла");
    }
    return node;
}

ASTNode* parse_term() {
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

ASTNode* parse_expr() {
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

ASTNode* parse(const char* source) {
    src_ptr = source;
    current_line = 1;
    advance();

    ASTNode* program = create_node(AST_PROGRAM);

    while (current_token.type != TOKEN_EOF) {
        // Объявление переменной (цел a)
        if (current_token.type == TOKEN_TYPE_CEL) {
            advance();
            if (current_token.type != TOKEN_IDENTIFIER) parse_error(current_token.line, "Ожидалось имя переменной после", "цел");
            ASTNode* decl = create_node(AST_VAR_DECL);
            decl->string_value = current_token.value;
            add_child(program, decl);
            advance();
        } 
        // Присваивание (a := 5)
        else if (current_token.type == TOKEN_IDENTIFIER) {
            ASTNode* assign = create_node(AST_ASSIGN);
            assign->string_value = current_token.value;
            advance();
            if (current_token.type != TOKEN_ASSIGN) parse_error(current_token.line, "Ожидалось присваивание", ":=");
            advance();
            assign->left = parse_expr();
            add_child(program, assign);
        }
        // Вывод (вывод "Текст", a)
        else if (current_token.type == TOKEN_VYVOD) {
            ASTNode* print_node = create_node(AST_PRINT);
            advance();
            add_child(print_node, parse_expr());
            while (current_token.type == TOKEN_COMMA) {
                advance();
                add_child(print_node, parse_expr());
            }
            add_child(program, print_node);
        }
        else { advance(); } // Пропускаем алг, нач, кон
    }
    return program;
}
