#include "kumir.h"

const char* current_filename = "main"; // Инициализируется в main.c
static int current_line = 1;
static Token current_token;
static const char* src_ptr;

static void advance() { current_token = get_next_token(&src_ptr, &current_line); }

void parse_error(const char* file, int line, const char* msg, const char* detail) {
    // ЦВЕТНОЙ ВЫВОД ОШИБОК
    printf("\n\033[1;31m[ОШИБКА СИНТАКСИСА]\033[0m Файл: \033[1;33m%s\033[0m, Строка \033[1;36m%d\033[0m\n\033[1;37m%s\033[0m \033[1;35m%s\033[0m\n", file, line, msg, detail ? detail : ""); 
    exit(1);
}

static ASTNode* create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode)); 
    node->type = type; 
    node->line = current_token.line; 
    node->file = current_filename; // Привязка файла к узлу
    return node;
}
static void add_child(ASTNode* parent, ASTNode* child) {
    parent->children_count++;
    parent->children = realloc(parent->children, parent->children_count * sizeof(ASTNode*));
    parent->children[parent->children_count - 1] = child;
}

static ASTNode* parse_expr();
static ASTNode* parse_statement();

static ASTNode* parse_factor() {
    ASTNode* node = NULL;

    if (current_token.type == TOKEN_MINUS) {
        advance();
        ASTNode* inner = parse_factor();
        ASTNode* zero = create_node(AST_NUM);
        zero->int_value = 0;
        ASTNode* neg = create_node(AST_BINOP);
        neg->string_value = "-";
        neg->left = zero;
        neg->right = inner;
        return neg;
    }

    if (current_token.type == TOKEN_NOT) {
        advance();
        ASTNode* inner = parse_factor();
        ASTNode* not_node = create_node(AST_BINOP);
        not_node->string_value = "не";
        not_node->left = inner;
        not_node->right = NULL;
        return not_node;
    }

    if (current_token.type == TOKEN_NUMBER) {
        node = create_node(AST_NUM);
        node->int_value = atoll(current_token.value);
        advance();
    }
    else if (current_token.type == TOKEN_FLOAT_LIT) { node = create_node(AST_FLOAT); node->float_value = atof(current_token.value); advance(); }
    else if (current_token.type == TOKEN_DA || current_token.type == TOKEN_NET) { node = create_node(AST_NUM); node->int_value = (current_token.type == TOKEN_DA) ? 1 : 0; advance(); }
    else if (current_token.type == TOKEN_STRING) { node = create_node(AST_STR); node->string_value = current_token.value; advance(); }
    else if (current_token.type == TOKEN_LBRACKET) {
        advance(); node = create_node(AST_ARRAY_LIT);
        if (current_token.type != TOKEN_RBRACKET) {
            add_child(node, parse_expr());
            while (current_token.type == TOKEN_COMMA) { advance(); add_child(node, parse_expr()); }
        }
        if (current_token.type != TOKEN_RBRACKET) parse_error(current_filename, current_token.line, "Ожидалось ']'", "");
        advance();
    }
    else if (current_token.type == TOKEN_IDENTIFIER) {
        char* name = current_token.value; int line = current_token.line; advance();
        if (current_token.type == TOKEN_LPAREN) {
            node = create_node(AST_FUNC_CALL); node->string_value = name; node->line = line; advance();
            if (current_token.type != TOKEN_RPAREN) {
                add_child(node, parse_expr());
                while (current_token.type == TOKEN_COMMA) { advance(); add_child(node, parse_expr()); }
            }
            if (current_token.type != TOKEN_RPAREN) parse_error(current_filename, line, "Ожидалось ')'", name);
            advance();
        } else {
            node = create_node(AST_VAR); node->string_value = name; node->line = line;
        }
    }
    else if (current_token.type == TOKEN_LPAREN) {
        advance(); node = parse_expr();
        if (current_token.type != TOKEN_RPAREN) parse_error(current_filename, current_token.line, "Ожидалось ')'", "");
        advance();
    } else parse_error(current_filename, current_token.line, "Неизвестный символ", current_token.value);

    while (node && current_token.type == TOKEN_LBRACKET) {
        advance(); ASTNode* idx = create_node(AST_INDEX_ACCESS);
        idx->left = node; idx->right = parse_expr();
        if (current_token.type != TOKEN_RBRACKET) parse_error(current_filename, current_token.line, "Ожидалось ']'", "");
        advance(); node = idx;
    }
    return node;
}

static ASTNode* parse_term() {
    ASTNode* node = parse_factor();
    while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV || current_token.type == TOKEN_MOD) {
        ASTNode* parent = create_node(AST_BINOP);
        parent->string_value = current_token.type == TOKEN_MUL ? "*" : (current_token.type == TOKEN_DIV ? "/" : "%");
        parent->left = node; advance(); parent->right = parse_factor(); node = parent;
    }
    return node;
}
static ASTNode* parse_arith() {
    ASTNode* node = parse_term();
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        ASTNode* parent = create_node(AST_BINOP); parent->string_value = current_token.type == TOKEN_PLUS ? "+" : "-";
        parent->left = node; advance(); parent->right = parse_term(); node = parent;
    }
    return node;
}
static ASTNode* parse_comp() {
    ASTNode* node = parse_arith();
    while (current_token.type >= TOKEN_EQ && current_token.type <= TOKEN_GE) {
        ASTNode* parent = create_node(AST_BINOP);
        if (current_token.type == TOKEN_EQ) parent->string_value = "="; else if (current_token.type == TOKEN_NEQ) parent->string_value = "<>";
        else if (current_token.type == TOKEN_LT) parent->string_value = "<"; else if (current_token.type == TOKEN_GT) parent->string_value = ">";
        else if (current_token.type == TOKEN_LE) parent->string_value = "<="; else if (current_token.type == TOKEN_GE) parent->string_value = ">=";
        parent->left = node; advance(); parent->right = parse_arith(); node = parent;
    }
    return node;
}
static ASTNode* parse_expr() {
    ASTNode* node = parse_comp();
    while (current_token.type == TOKEN_AND || current_token.type == TOKEN_OR) {
        ASTNode* parent = create_node(AST_BINOP); parent->string_value = current_token.type == TOKEN_AND ? "и" : "или";
        parent->left = node; advance(); parent->right = parse_comp(); node = parent;
    }
    return node;
}

static ASTNode* parse_statement() {
    // ИСПРАВЛЕНИЕ: ПОДДЕРЖКА "тип переменная := значение" В ОДНУ СТРОКУ
    if (current_token.type == TOKEN_TYPE_CEL || current_token.type == TOKEN_TYPE_LIT || current_token.type == TOKEN_TYPE_LOG || current_token.type == TOKEN_TYPE_VESH || current_token.type == TOKEN_TYPE_TAB) {
        advance(); 
        if (current_token.type != TOKEN_IDENTIFIER) parse_error(current_filename, current_token.line, "Ожидалось имя", "");
        
        char* var_name = current_token.value;
        int var_line = current_token.line;
        advance(); 
        
        if (current_token.type == TOKEN_ASSIGN) {
            advance();
            ASTNode* assign = create_node(AST_ASSIGN);
            ASTNode* var_target = create_node(AST_VAR);
            var_target->string_value = var_name;
            var_target->line = var_line;
            assign->left = var_target;
            assign->right = parse_expr();
            return assign; // Во время выполнения переменная будет создана при присваивании
        } else {
            ASTNode* decl = create_node(AST_VAR_DECL); 
            decl->string_value = var_name; 
            return decl;
        }
    }
    if (current_token.type == TOKEN_ESLI) {
        advance(); ASTNode* if_node = create_node(AST_IF); add_child(if_node, parse_expr());
        if (current_token.type != TOKEN_TO) parse_error(current_filename, current_token.line, "Ожидалось 'то'", "");
        advance(); ASTNode* then_body = create_node(AST_BODY);
        while (current_token.type != TOKEN_INACHE && current_token.type != TOKEN_VSE && current_token.type != TOKEN_EOF) add_child(then_body, parse_statement());
        add_child(if_node, then_body);
        if (current_token.type == TOKEN_INACHE) {
            advance(); ASTNode* else_body = create_node(AST_BODY);
            while (current_token.type != TOKEN_VSE && current_token.type != TOKEN_EOF) add_child(else_body, parse_statement());
            add_child(if_node, else_body);
        }
        if (current_token.type != TOKEN_VSE) parse_error(current_filename, current_token.line, "Ожидалось 'все'", "");
        advance(); return if_node;
    }
    if (current_token.type == TOKEN_NC) {
        advance();
        if (current_token.type == TOKEN_POKA) {
            advance(); ASTNode* w_node = create_node(AST_WHILE); add_child(w_node, parse_expr());
            ASTNode* body = create_node(AST_BODY);
            while (current_token.type != TOKEN_KC && current_token.type != TOKEN_EOF) add_child(body, parse_statement());
            advance(); add_child(w_node, body); return w_node;
        } else {
            ASTNode* rep_node = create_node(AST_REPEAT); add_child(rep_node, parse_expr());
            if (current_token.type != TOKEN_RAZ) parse_error(current_filename, current_token.line, "Ожидалось 'раз'", "");
            advance(); ASTNode* body = create_node(AST_BODY);
            while (current_token.type != TOKEN_KC && current_token.type != TOKEN_EOF) add_child(body, parse_statement());
            advance(); add_child(rep_node, body); return rep_node;
        }
    }
    if (current_token.type == TOKEN_VOZVRAT) {
        ASTNode* ret = create_node(AST_RETURN); advance();
        ret->left = parse_expr(); return ret;
    }
    if (current_token.type == TOKEN_VYVOD) {
        ASTNode* print_node = create_node(AST_PRINT); advance(); add_child(print_node, parse_expr());
        while (current_token.type == TOKEN_COMMA) { advance(); add_child(print_node, parse_expr()); }
        return print_node;
    }

    if (current_token.type == TOKEN_IDENTIFIER) {
        ASTNode* target = parse_factor();
        if (current_token.type == TOKEN_ASSIGN) {
            ASTNode* assign = create_node(AST_ASSIGN);
            assign->left = target; advance();
            assign->right = parse_expr(); return assign;
        }
        if (target->type == AST_FUNC_CALL) return target;
        parse_error(current_filename, target->line, "Ожидалось ':=' после переменной", "");
    }
    parse_error(current_filename, current_token.line, "Неизвестная команда", current_token.value); return NULL;
}

static ASTNode* parse_func_def() {
    ASTNode* func = create_node(AST_FUNC_DEF); advance();
    if (current_token.type == TOKEN_TYPE_CEL || current_token.type == TOKEN_TYPE_VESH || current_token.type == TOKEN_TYPE_LIT || current_token.type == TOKEN_TYPE_LOG || current_token.type == TOKEN_TYPE_TAB) advance();
    func->string_value = current_token.value; advance();
    if (current_token.type == TOKEN_LPAREN) {
        advance();
        while (current_token.type != TOKEN_RPAREN && current_token.type != TOKEN_EOF) {
            advance();
            ASTNode* param = create_node(AST_PARAM); param->string_value = current_token.value;
            add_child(func, param); advance();
            if (current_token.type == TOKEN_COMMA) advance();
        }
        advance();
    }
    if (current_token.type != TOKEN_NACH) parse_error(current_filename, current_token.line, "Ожидалось 'нач'", "");
    advance(); ASTNode* body = create_node(AST_BODY);
    while (current_token.type != TOKEN_KON && current_token.type != TOKEN_EOF) add_child(body, parse_statement());
    advance(); func->left = body; return func;
}

ASTNode* parse(const char* source) {
    src_ptr = source; current_line = 1; advance();
    ASTNode* program = create_node(AST_PROGRAM);

    while (current_token.type != TOKEN_EOF) {
        if (current_token.type == TOKEN_ISPOLZOVAT) {
            int err_line = current_line;
            advance();
            char libname[256]; strcpy(libname, current_token.value);
            advance();
            if (current_token.type == TOKEN_UNKNOWN) {
                advance();
                if (current_token.type == TOKEN_IDENTIFIER) advance();
            }
            if (!strstr(libname, ".")) strcat(libname, ".kum");

            char* lib_source = read_file_content(libname);
            if (lib_source) {
                const char* old_src = src_ptr; int old_line = current_line; Token old_token = current_token;
                
                // СОХРАНЯЕМ ИМЯ ФАЙЛА, ЧТОБЫ УЗЛЫ БИБЛИОТЕКИ ЗНАЛИ СВОЙ ФАЙЛ
                const char* old_file = current_filename;
                char* lib_filename_ptr = strdup(libname);
                current_filename = lib_filename_ptr; 

                src_ptr = lib_source; current_line = 1; advance();
                while (current_token.type != TOKEN_EOF) {
                    if (current_token.type == TOKEN_ALG) add_child(program, parse_func_def()); else advance();
                }
                
                // ВОССТАНАВЛИВАЕМ
                current_filename = old_file;
                src_ptr = old_src; current_line = old_line; current_token = old_token; free(lib_source);
            } else {
                parse_error(current_filename, err_line, "КРИТИЧЕСКАЯ ОШИБКА: Не удалось найти библиотеку", libname);
            }
        } else if (current_token.type == TOKEN_ALG) {
            add_child(program, parse_func_def());
        } else {
            parse_error(current_filename, current_token.line, "Ожидалось 'алг' или 'использовать'", current_token.value);
        }
    }
    return program;
}
