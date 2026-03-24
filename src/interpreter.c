#include "kumir.h"

void execute(ASTNode* node) {
    if (!node) return;

    if (node->type == AST_PROGRAM) {
        for (int i = 0; i < node->children_count; i++) {
            execute(node->children[i]);
        }
    } 
    else if (node->type == AST_PRINT) {
        printf("%s\n", node->string_value);
    }
}
