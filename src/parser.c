#include "kumir.h"

ASTNode* create_node(ASTNodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->string_value = NULL;
    node->children = NULL;
    node->children_count = 0;
    return node;
}

void add_child(ASTNode* parent, ASTNode* child) {
    parent->children_count++;
    parent->children = realloc(parent->children, parent->children_count * sizeof(ASTNode*));
    parent->children[parent->children_count - 1] = child;
}

ASTNode* parse(const char* source) {
    ASTNode* program = create_node(AST_PROGRAM);
    Token token = get_next_token(&source);

    // Ожидаем структуру: алг -> нач -> [команды] -> кон
    while (token.type != TOKEN_EOF) {
        if (token.type == TOKEN_VYVOD) {
            Token str_token = get_next_token(&source);
            if (str_token.type == TOKEN_STRING) {
                ASTNode* print_node = create_node(AST_PRINT);
                print_node->string_value = str_token.value;
                add_child(program, print_node);
            }
        }
        token = get_next_token(&source);
    }
    return program;
}
