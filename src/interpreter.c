#include "kumir.h"

// ========================
// УТИЛИТЫ ДЛЯ ПЕРЕМЕННЫХ
// ========================
KValue make_int(int v) { KValue val; val.type = VAL_INT; val.i = v; val.s = NULL; return val; }
KValue make_str(const char* v) { KValue val; val.type = VAL_STR; val.i = 0; val.s = strdup(v ? v : ""); return val; }

// ========================
// ТАБЛИЦА АЛГОРИТМОВ И NATIVE ФУНКЦИЙ
// ========================
typedef KValue (*NativeFunc)(KValue* args, int count);

typedef struct {
    char name[128];
    ASTNode* def;
    NativeFunc native_ptr;
    int params_count;
} KumirFunction;

static KumirFunction func_table[128];
static int func_count = 0;

// ========================
// СТЕК ВЫЗОВОВ
// ========================
#define MAX_VARS 100
#define MAX_FRAMES 64

typedef struct {
    char names[MAX_VARS][128];
    KValue values[MAX_VARS];
    int var_count;
    KValue return_value;
    int has_returned;
} CallFrame;

static CallFrame frames[MAX_FRAMES];
static int frame_depth = -1;

// ========================
// NATIVE ФУНКЦИИ (OS & INTERNET)
// ========================
KValue native_input(KValue* args, int count) {
    char buf[1024]; fgets(buf, sizeof(buf), stdin);
    buf[strcspn(buf, "\r\n")] = 0; return make_str(buf);
}
KValue native_len(KValue* args, int count) {
    if (args[0].type == VAL_STR) return make_int(strlen(args[0].s)); return make_int(0);
}
KValue native_os_cmd(KValue* args, int count) {
    if (args[0].type == VAL_STR) system(args[0].s); return make_int(0);
}
KValue native_http_get(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    char cmd[1024]; snprintf(cmd, sizeof(cmd), "curl -sL \"%s\"", args[0].s);
    FILE* fp = popen(cmd, "r"); if (!fp) return make_str("");
    char* result = malloc(1); result[0] = '\0'; int len = 0; char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        int blen = strlen(buf); result = realloc(result, len + blen + 1);
        strcpy(result + len, buf); len += blen;
    }
    pclose(fp); KValue v = make_str(result); free(result); return v;
}
KValue native_os_read(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    char* c = read_file_content(args[0].s);
    if (!c) return make_str("");
    KValue v = make_str(c); free(c); return v;
}
KValue native_os_write(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(0);
    FILE* f = fopen(args[0].s, "w"); if(f){ fputs(args[1].s, f); fclose(f); return make_int(1);}
    return make_int(0);
}

void register_native(const char* name, int params, NativeFunc ptr) {
    strcpy(func_table[func_count].name, name);
    func_table[func_count].def = NULL; func_table[func_count].native_ptr = ptr;
    func_table[func_count].params_count = params; func_count++;
}

void runtime_error(int line, const char* msg, const char* detail) {
    printf("\n[КРИТИЧЕСКАЯ ОШИБКА] Строка %d: %s '%s'\n", line, msg, detail ? detail : ""); exit(1);
}

static void push_frame() {
    if (++frame_depth >= MAX_FRAMES) runtime_error(0, "Переполнение стека!", "");
    frames[frame_depth].var_count = 0; frames[frame_depth].has_returned = 0;
    frames[frame_depth].return_value = make_int(0);
}
static void pop_frame() { frame_depth--; }

static void declare_var(const char* name, KValue initial_value, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0) runtime_error(line, "Переменная уже объявлена:", name);
    }
    strcpy(f->names[f->var_count], name); f->values[f->var_count] = initial_value; f->var_count++;
}

static KValue get_var(const char* name, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) if (strcmp(f->names[i], name) == 0) return f->values[i];
    runtime_error(line, "Переменная не объявлена:", name); return make_int(0);
}

static void set_var(const char* name, KValue value, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0) { f->values[i] = value; return; }
    }
    runtime_error(line, "Переменная не объявлена:", name);
}

static KValue eval(ASTNode* node);
void execute(ASTNode* node);

static KValue call_func(ASTNode* call_node) {
    const char* name = call_node->string_value; int line = call_node->line;
    int f_idx = -1;
    for (int i = 0; i < func_count; i++) if (strcmp(func_table[i].name, name) == 0) { f_idx = i; break; }
    if (f_idx == -1) runtime_error(line, "Алгоритм не найден:", name);

    KValue args[MAX_VARS];
    for (int i = 0; i < call_node->children_count; i++) args[i] = eval(call_node->children[i]);

    if (func_table[f_idx].native_ptr) {
        return func_table[f_idx].native_ptr(args, call_node->children_count);
    }

    ASTNode* def = func_table[f_idx].def;
    push_frame();
    for (int i = 0; i < def->children_count; i++) {
        declare_var(def->children[i]->string_value, args[i], line);
    }
    ASTNode* body = def->left;
    for (int i = 0; i < body->children_count; i++) {
        execute(body->children[i]);
        if (frames[frame_depth].has_returned) break;
    }
    KValue ret = frames[frame_depth].return_value; pop_frame(); return ret;
}

static KValue eval(ASTNode* node) {
    if (!node) return make_int(0);
    if (node->type == AST_NUM) return make_int(node->int_value);
    if (node->type == AST_STR) return make_str(node->string_value);
    if (node->type == AST_VAR) return get_var(node->string_value, node->line);
    if (node->type == AST_FUNC_CALL) return call_func(node);

    if (node->type == AST_BINOP) {
        KValue l = eval(node->left); KValue r = eval(node->right);
        const char* op = node->string_value;
        if (strcmp(op, "+") == 0) {
            if (l.type == VAL_STR || r.type == VAL_STR) {
                char buf[2048]; char ls[1024], rs[1024];
                if (l.type == VAL_STR) strcpy(ls, l.s); else sprintf(ls, "%d", l.i);
                if (r.type == VAL_STR) strcpy(rs, r.s); else sprintf(rs, "%d", r.i);
                snprintf(buf, sizeof(buf), "%s%s", ls, rs); return make_str(buf);
            }
            return make_int(l.i + r.i);
        }
        if (strcmp(op, "-") == 0) return make_int(l.i - r.i);
        if (strcmp(op, "*") == 0) return make_int(l.i * r.i);
        if (strcmp(op, "/") == 0) return make_int(r.i == 0 ? 0 : l.i / r.i);
        
        // Сравнения (работают и со строками)
        if (l.type == VAL_STR && r.type == VAL_STR) {
            int cmp = strcmp(l.s, r.s);
            if (strcmp(op, "=") == 0) return make_int(cmp == 0);
            if (strcmp(op, "<>") == 0) return make_int(cmp != 0);
        } else {
            if (strcmp(op, "=") == 0) return make_int(l.i == r.i);
            if (strcmp(op, "<>") == 0) return make_int(l.i != r.i);
            if (strcmp(op, "<") == 0) return make_int(l.i < r.i);
            if (strcmp(op, ">") == 0) return make_int(l.i > r.i);
            if (strcmp(op, "<=") == 0) return make_int(l.i <= r.i);
            if (strcmp(op, ">=") == 0) return make_int(l.i >= r.i);
        }
        if (strcmp(op, "и") == 0) return make_int(l.i && r.i);
        if (strcmp(op, "или") == 0) return make_int(l.i || r.i);
    }
    return make_int(0);
}

void execute(ASTNode* node) {
    if (!node) return;
    if (frame_depth >= 0 && frames[frame_depth].has_returned) return;

    switch (node->type) {
        case AST_PROGRAM: {
            // Регистрация Native-библиотек (ОС и Интернет)
            register_native("Ввод", 0, native_input);
            register_native("Длина", 1, native_len);
            register_native("ОС_Команда", 1, native_os_cmd);
            register_native("ОС_Чтение", 1, native_os_read);
            register_native("ОС_Запись", 2, native_os_write);
            register_native("Интернет_Запрос", 1, native_http_get);

            for (int i = 0; i < node->children_count; i++) {
                if (node->children[i]->type == AST_FUNC_DEF) {
                    strcpy(func_table[func_count].name, node->children[i]->string_value);
                    func_table[func_count].def = node->children[i]; func_count++;
                }
            }
            ASTNode* main_alg = NULL;
            for (int i = node->children_count - 1; i >= 0; i--) {
                if (node->children[i]->type == AST_FUNC_DEF && node->children[i]->children_count == 0) { main_alg = node->children[i]; break; }
            }
            if (!main_alg && node->children_count > 0) main_alg = node->children[node->children_count - 1];
            if (main_alg) { push_frame(); for (int i=0; i<main_alg->left->children_count; i++) execute(main_alg->left->children[i]); pop_frame(); }
            break;
        }
        case AST_VAR_DECL: declare_var(node->string_value, make_int(0), node->line); break;
        case AST_ASSIGN: set_var(node->string_value, eval(node->left), node->line); break;
        case AST_ZNACH: frames[frame_depth].return_value = eval(node->left); frames[frame_depth].has_returned = 1; break;
        case AST_FUNC_CALL: call_func(node); break;
        
        case AST_IF: {
            KValue cond = eval(node->children[0]);
            if (cond.i) { for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]); }
            else if (node->children_count > 2) { for (int i=0; i < node->children[2]->children_count; i++) execute(node->children[2]->children[i]); }
            break;
        }
        case AST_WHILE: {
            while (eval(node->children[0]).i && !frames[frame_depth].has_returned) {
                for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]);
            }
            break;
        }
        case AST_REPEAT: {
            int times = eval(node->children[0]).i;
            for (int t=0; t<times; t++) {
                if (frames[frame_depth].has_returned) break;
                for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]);
            }
            break;
        }
        case AST_PRINT:
            for (int i = 0; i < node->children_count; i++) {
                KValue v = eval(node->children[i]);
                if (v.type == VAL_STR) printf("%s", v.s); else printf("%d", v.i);
            }
            printf("\n");
            break;
        default: break;
    }
}
