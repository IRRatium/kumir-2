#include "kumir.h"
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define closesocket close
#endif

KValue make_int(int v) { KValue val; val.type = VAL_INT; val.i = v; val.f = 0.0; val.s = NULL; val.arr = NULL; return val; }
KValue make_float(double v) { KValue val; val.type = VAL_FLOAT; val.i = 0; val.f = v; val.s = NULL; val.arr = NULL; return val; }
KValue make_str(const char* v) { KValue val; val.type = VAL_STR; val.s = strdup(v ? v : ""); val.arr = NULL; return val; }
KValue make_array(int size) {
    KValue val; val.type = VAL_ARRAY; val.arr = malloc(sizeof(KArray));
    val.arr->ref_count = 1; val.arr->length = size; val.arr->items = malloc(size * sizeof(KValue));
    for(int i=0; i<size; i++) val.arr->items[i] = make_int(0);
    return val;
}

typedef KValue (*NativeFunc)(KValue* args, int count);
typedef struct { char name[128]; ASTNode* def; NativeFunc native_ptr; } KumirFunction;

static KumirFunction func_table[256];
static int func_count = 0;

typedef struct { char names[100][128]; KValue values[100]; int var_count; KValue return_value; int has_returned; } CallFrame;
static CallFrame frames[64];
static int frame_depth = -1;

void runtime_error(int line, const char* msg, const char* detail) {
    printf("\n[ОШИБКА] Строка %d: %s '%s'\n", line, msg, detail ? detail : ""); exit(1);
}

// ==== НАСТОЯЩИЙ И БЕЗОПАСНЫЙ ВВОД ДЛЯ WINDOWS ====
KValue native_vvod(KValue* args, int count) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    wchar_t wbuf[1024]; DWORD read;
    ReadConsoleW(hConsole, wbuf, 1024, &read, NULL);
    if (read > 0 && wbuf[read-1] == L'\n') read--;
    if (read > 0 && wbuf[read-1] == L'\r') read--;
    wbuf[read] = L'\0';
    char buf[3072]; WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, 3072, NULL, NULL);
    return make_str(buf);
#else
    char buf[1024]; fgets(buf, sizeof(buf), stdin); buf[strcspn(buf, "\r\n")] = 0; return make_str(buf);
#endif
}

// ОС и Прочее
KValue native_len(KValue* args, int count) {
    if (args[0].type == VAL_STR) return make_int(strlen(args[0].s));
    if (args[0].type == VAL_ARRAY) return make_int(args[0].arr->length);
    return make_int(0);
}
KValue native_os_cmd(KValue* args, int count) { if (args[0].type == VAL_STR) system(args[0].s); return make_int(0); }
KValue native_os_read(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str(""); char* c = read_file_content(args[0].s);
    if (!c) return make_str(""); KValue v = make_str(c); free(c); return v;
}
KValue native_os_write(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(0);
    FILE* f = fopen(args[0].s, "w"); if(f){ fputs(args[1].s, f); fclose(f); return make_int(1);} return make_int(0);
}

// ==== НОВЫЕ ФУНКЦИИ ДЛЯ СТРОК (ДЛЯ ПАРСИНГА) ====
KValue native_str_find(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(-1);
    char* p = strstr(args[0].s, args[1].s);
    if (p) return make_int(p - args[0].s);
    return make_int(-1);
}
KValue native_str_sub(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_INT) return make_str("");
    int start = args[1].i; int end = args[2].i;
    int len = strlen(args[0].s);
    if (start < 0) start = 0; if (end > len) end = len;
    if (start > end || start >= len) return make_str("");
    char* buf = malloc(end - start + 1);
    strncpy(buf, args[0].s + start, end - start); buf[end - start] = '\0';
    KValue res = make_str(buf); free(buf); return res;
}
KValue native_str_to_int(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_int(0);
    return make_int(atoi(args[0].s));
}

// ==== СЫРЫЕ СОКЕТЫ ====
KValue native_sock_create(KValue* args, int count) {
    int s = socket(AF_INET, SOCK_STREAM, 0); return make_int(s);
}
KValue native_sock_connect(KValue* args, int count) {
    if (args[0].type != VAL_INT || args[1].type != VAL_STR || args[2].type != VAL_INT) return make_int(0);
    struct hostent* host = gethostbyname(args[1].s); if(!host) return make_int(0);
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(args[2].i);
    memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);
    if(connect(args[0].i, (struct sockaddr*)&addr, sizeof(addr)) < 0) return make_int(0);
    return make_int(1); // Успех
}
KValue native_sock_send(KValue* args, int count) {
    if (args[0].type == VAL_INT && args[1].type == VAL_STR) send(args[0].i, args[1].s, strlen(args[1].s), 0);
    return make_int(0);
}
KValue native_sock_recv(KValue* args, int count) {
    if (args[0].type != VAL_INT || args[1].type != VAL_INT) return make_str("");
    char* buf = calloc(1, args[1].i + 1);
    int r = recv(args[0].i, buf, args[1].i, 0);
    if (r <= 0) { free(buf); return make_str(""); }
    KValue res = make_str(buf); free(buf); return res;
}
KValue native_sock_close(KValue* args, int count) {
    if (args[0].type == VAL_INT) closesocket(args[0].i); return make_int(0);
}

void register_native(const char* name, NativeFunc ptr) {
    strcpy(func_table[func_count].name, name); func_table[func_count].def = NULL; func_table[func_count].native_ptr = ptr; func_count++;
}

static void push_frame() {
    if (++frame_depth >= 64) runtime_error(0, "Переполнение стека", "");
    frames[frame_depth].var_count = 0; frames[frame_depth].has_returned = 0; frames[frame_depth].return_value = make_int(0);
}
static void pop_frame() { frame_depth--; }

static KValue get_var(const char* name, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) if (strcmp(f->names[i], name) == 0) return f->values[i];
    runtime_error(line, "Переменная не найдена", name); return make_int(0);
}
static void set_var(const char* name, KValue value, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0) { f->values[i] = value; return; }
    }
    strcpy(f->names[f->var_count], name); f->values[f->var_count] = value; f->var_count++;
}

static KValue eval(ASTNode* node);
void execute(ASTNode* node);

static KValue call_func(ASTNode* call_node) {
    const char* name = call_node->string_value; int f_idx = -1;
    for (int i = 0; i < func_count; i++) if (strcmp(func_table[i].name, name) == 0) { f_idx = i; break; }
    if (f_idx == -1) runtime_error(call_node->line, "Алгоритм не найден", name);
    KValue args[100]; for (int i = 0; i < call_node->children_count; i++) args[i] = eval(call_node->children[i]);
    if (func_table[f_idx].native_ptr) return func_table[f_idx].native_ptr(args, call_node->children_count);
    
    ASTNode* def = func_table[f_idx].def; push_frame();
    for (int i = 0; i < def->children_count; i++) set_var(def->children[i]->string_value, args[i], call_node->line);
    for (int i = 0; i < def->left->children_count; i++) {
        execute(def->left->children[i]); if (frames[frame_depth].has_returned) break;
    }
    KValue ret = frames[frame_depth].return_value; pop_frame(); return ret;
}

static KValue eval(ASTNode* node) {
    if (!node) return make_int(0);
    if (node->type == AST_NUM) return make_int(node->int_value);
    if (node->type == AST_FLOAT) return make_float(node->float_value);
    if (node->type == AST_STR) return make_str(node->string_value);
    if (node->type == AST_VAR) return get_var(node->string_value, node->line);
    if (node->type == AST_FUNC_CALL) return call_func(node);
    
    if (node->type == AST_ARRAY_LIT) {
        KValue arr = make_array(node->children_count);
        for(int i=0; i<node->children_count; i++) arr.arr->items[i] = eval(node->children[i]);
        return arr;
    }
    if (node->type == AST_INDEX_ACCESS) {
        KValue arr = eval(node->left); KValue idx = eval(node->right);
        if (arr.type != VAL_ARRAY || idx.type != VAL_INT) runtime_error(node->line, "Ошибка индексации", "");
        if (idx.i < 0 || idx.i >= arr.arr->length) runtime_error(node->line, "Индекс вне границ", "");
        return arr.arr->items[idx.i];
    }

    if (node->type == AST_BINOP) {
        KValue l = eval(node->left); KValue r = eval(node->right);
        const char* op = node->string_value;
        if (strcmp(op, "+") == 0) {
            if (l.type == VAL_STR || r.type == VAL_STR) {
                char ls[1024], rs[1024], buf[2048];
                if(l.type==VAL_STR) strcpy(ls, l.s); else if(l.type==VAL_FLOAT) sprintf(ls, "%g", l.f); else sprintf(ls, "%d", l.i);
                if(r.type==VAL_STR) strcpy(rs, r.s); else if(r.type==VAL_FLOAT) sprintf(rs, "%g", r.f); else sprintf(rs, "%d", r.i);
                snprintf(buf, sizeof(buf), "%s%s", ls, rs); return make_str(buf);
            }
            if (l.type == VAL_FLOAT || r.type == VAL_FLOAT) return make_float((l.type==VAL_FLOAT?l.f:l.i) + (r.type==VAL_FLOAT?r.f:r.i));
            return make_int(l.i + r.i);
        }
        if (strcmp(op, "-") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) - (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i - r.i);
        if (strcmp(op, "*") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) * (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i * r.i);
        if (strcmp(op, "/") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) / (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i / r.i);
        if (strcmp(op, "%") == 0) return make_int(l.i % (r.i == 0 ? 1 : r.i));

        double lv = (l.type==VAL_FLOAT)?l.f:l.i; double rv = (r.type==VAL_FLOAT)?r.f:r.i;
        if (strcmp(op, "=") == 0) return l.type==VAL_STR ? make_int(strcmp(l.s, r.s)==0) : make_int(lv == rv);
        if (strcmp(op, "<>")== 0) return l.type==VAL_STR ? make_int(strcmp(l.s, r.s)!=0) : make_int(lv != rv);
        if (strcmp(op, "<") == 0) return make_int(lv < rv);
        if (strcmp(op, ">") == 0) return make_int(lv > rv);
        if (strcmp(op, "<=")== 0) return make_int(lv <= rv);
        if (strcmp(op, ">=")== 0) return make_int(lv >= rv);
        if (strcmp(op, "и") == 0) return make_int(l.i && r.i);
        if (strcmp(op, "или") == 0) return make_int(l.i || r.i);
    }
    return make_int(0);
}

void print_val(KValue v) {
    if (v.type == VAL_INT) printf("%d", v.i);
    else if (v.type == VAL_FLOAT) printf("%g", v.f);
    else if (v.type == VAL_STR) printf("%s", v.s);
    else if (v.type == VAL_ARRAY) {
        printf("[");
        for (int i=0; i<v.arr->length; i++) { print_val(v.arr->items[i]); if (i < v.arr->length - 1) printf(", "); }
        printf("]");
    }
}

void execute(ASTNode* node) {
    if (!node || (frame_depth >= 0 && frames[frame_depth].has_returned)) return;
    switch (node->type) {
        case AST_PROGRAM: {
#ifdef _WIN32
            WSADATA wsaData; WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
            register_native("ввод", native_vvod);
            register_native("длина", native_len);
            register_native("ос_команда", native_os_cmd);
            register_native("ос_чтение", native_os_read);
            register_native("ос_запись", native_os_write);
            register_native("строка_найти", native_str_find);
            register_native("строка_срез", native_str_sub);
            register_native("строка_в_число", native_str_to_int);
            register_native("сокет_создать", native_sock_create);
            register_native("сокет_подключить", native_sock_connect);
            register_native("сокет_отправить", native_sock_send);
            register_native("сокет_получить", native_sock_recv);
            register_native("сокет_закрыть", native_sock_close);

            for (int i=0; i<node->children_count; i++) {
                if (node->children[i]->type == AST_FUNC_DEF) {
                    strcpy(func_table[func_count].name, node->children[i]->string_value);
                    func_table[func_count].def = node->children[i]; func_count++;
                }
            }
            ASTNode* main_alg = NULL;
            for (int i=node->children_count-1; i>=0; i--) if (node->children[i]->type == AST_FUNC_DEF && node->children[i]->children_count == 0) { main_alg = node->children[i]; break; }
            if (!main_alg && node->children_count > 0) main_alg = node->children[node->children_count - 1];
            if (main_alg) { push_frame(); for (int i=0; i<main_alg->left->children_count; i++) execute(main_alg->left->children[i]); pop_frame(); }
            break;
        }
        case AST_VAR_DECL: set_var(node->string_value, make_int(0), node->line); break;
        case AST_ASSIGN:
            if (node->left->type == AST_VAR) set_var(node->left->string_value, eval(node->right), node->line);
            else if (node->left->type == AST_INDEX_ACCESS) {
                KValue arr = eval(node->left->left); KValue idx = eval(node->left->right); KValue val = eval(node->right);
                if (arr.type == VAL_ARRAY && idx.type == VAL_INT && idx.i >= 0 && idx.i < arr.arr->length) arr.arr->items[idx.i] = val;
            }
            break;
        case AST_RETURN: frames[frame_depth].return_value = eval(node->left); frames[frame_depth].has_returned = 1; break;
        case AST_FUNC_CALL: call_func(node); break;
        case AST_IF: {
            if (eval(node->children[0]).i) { for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]); }
            else if (node->children_count > 2) { for (int i=0; i < node->children[2]->children_count; i++) execute(node->children[2]->children[i]); }
            break;
        }
        case AST_WHILE: while (eval(node->children[0]).i && !frames[frame_depth].has_returned) for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]); break;
        case AST_REPEAT: {
            int times = eval(node->children[0]).i;
            for (int t=0; t<times; t++) { if (frames[frame_depth].has_returned) break; for (int i=0; i < node->children[1]->children_count; i++) execute(node->children[1]->children[i]); }
            break;
        }
        case AST_PRINT:
            for (int i = 0; i < node->children_count; i++) print_val(eval(node->children[i]));
            printf("\n"); break;
        default: break;
    }
}
