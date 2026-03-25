#include "kumir.h"
#include <math.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <wininet.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <dlfcn.h>
#define closesocket close
#endif

// ========================
// ЗНАЧЕНИЯ
// ========================
KValue make_int(long long v) { KValue val; val.type = VAL_INT;   val.i = v;   val.f = 0.0; val.s = NULL; val.arr = NULL; return val; }
KValue make_float(double v){ KValue val; val.type = VAL_FLOAT; val.i = 0;   val.f = v;   val.s = NULL; val.arr = NULL; return val; }
KValue make_str(const char* v) { KValue val; val.type = VAL_STR; val.s = strdup(v ? v : ""); val.arr = NULL; return val; }
KValue make_array(int size) {
    KValue val; val.type = VAL_ARRAY; val.arr = malloc(sizeof(KArray));
    val.arr->ref_count = 1; val.arr->length = size; val.arr->items = malloc(size * sizeof(KValue));
    for (int i = 0; i < size; i++) val.arr->items[i] = make_int(0);
    return val;
}

// НОВОЕ: Встроенные "Словари" на базе массивов [key1, val1, key2, val2...]
void array_append(KArray* arr, KValue val) {
    arr->length++;
    arr->items = realloc(arr->items, arr->length * sizeof(KValue));
    arr->items[arr->length - 1] = val;
}
KValue native_dict_create(KValue* args, int count) { return make_array(0); }
KValue native_dict_write(KValue* args, int count) {
    if (count < 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_STR) return make_int(0);
    KArray* dict = args[0].arr;
    for (int i = 0; i < dict->length; i += 2) {
        if (dict->items[i].type == VAL_STR && strcmp(dict->items[i].s, args[1].s) == 0) {
            dict->items[i+1] = args[2]; return make_int(1); // Обновление
        }
    }
    array_append(dict, args[1]); array_append(dict, args[2]); return make_int(1);
}
KValue native_dict_read(KValue* args, int count) {
    if (count < 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STR) return make_str("");
    KArray* dict = args[0].arr;
    for (int i = 0; i < dict->length; i += 2) {
        if (dict->items[i].type == VAL_STR && strcmp(dict->items[i].s, args[1].s) == 0) {
            return dict->items[i+1];
        }
    }
    return make_str(""); 
}

// ========================
// ТАБЛИЦА ФУНКЦИЙ
// ========================
#define MAX_FUNCS 512

typedef KValue (*NativeFunc)(KValue* args, int count);
typedef struct { char name[256]; ASTNode* def; NativeFunc native_ptr; } KumirFunction;

static KumirFunction func_table[MAX_FUNCS];
static int func_count = 0;

// ========================
// ФРЕЙМЫ ВЫЗОВОВ
// ========================
typedef struct {
    char   names[100][256];
    KValue values[100];
    int    var_count;
    KValue return_value;
    int    has_returned;
} CallFrame;

static CallFrame frames[64];
static int frame_depth = -1;

void runtime_error(const char* file, int line, const char* msg, const char* detail) {
    printf("\n\033[1;31m[ОШИБКА ВЫПОЛНЕНИЯ]\033[0m Файл: \033[1;33m%s\033[0m, Строка \033[1;36m%d\033[0m\n\033[1;37m%s\033[0m \033[1;35m%s\033[0m\n", file ? file : "?", line, msg, detail ? detail : ""); 
    exit(1);
}

// ========================
// СЕТЬ И HTTP (НОВОЕ)
// ========================
static char GLOBAL_USER_AGENT[512] = "Kumir2/1.0";
static char GLOBAL_HEADERS[4096]   = "";
static char GLOBAL_COOKIES[4096]   = "";

KValue native_set_useragent(KValue* args, int count) { if (args[0].type == VAL_STR) strncpy(GLOBAL_USER_AGENT, args[0].s, sizeof(GLOBAL_USER_AGENT) - 1); return make_int(0); }
KValue native_add_header(KValue* args, int count) { if (args[0].type == VAL_STR) { size_t remaining = sizeof(GLOBAL_HEADERS) - strlen(GLOBAL_HEADERS) - 3; if (strlen(args[0].s) < remaining) { strcat(GLOBAL_HEADERS, args[0].s); strcat(GLOBAL_HEADERS, "\r\n"); } } return make_int(0); }
KValue native_clear_headers(KValue* args, int count) { GLOBAL_HEADERS[0] = '\0'; return make_int(0); }
KValue native_add_cookie(KValue* args, int count) { if (args[0].type == VAL_STR) { size_t cur = strlen(GLOBAL_COOKIES); size_t remaining = sizeof(GLOBAL_COOKIES) - cur - 3; if (strlen(args[0].s) < remaining) { if (cur > 0) strcat(GLOBAL_COOKIES, "; "); strcat(GLOBAL_COOKIES, args[0].s); } } return make_int(0); }
KValue native_clear_cookies(KValue* args, int count) { GLOBAL_COOKIES[0] = '\0'; return make_int(0); }

// ========================
// ОС И ВВОД
// ========================
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

KValue native_len(KValue* args, int count) {
    if (args[0].type == VAL_STR)   return make_int((int)strlen(args[0].s));
    if (args[0].type == VAL_ARRAY) return make_int(args[0].arr->length);
    return make_int(0);
}
KValue native_os_cmd(KValue* args, int count)  { if (args[0].type == VAL_STR) system(args[0].s); return make_int(0); }
KValue native_os_read(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    char* c = read_file_content(args[0].s);
    if (!c) return make_str("");
    KValue v = make_str(c); free(c); return v;
}
KValue native_os_write(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(0);
    FILE* f = fopen(args[0].s, "w");
    if (f) { fputs(args[1].s, f); fclose(f); return make_int(1); }
    return make_int(0);
}

// ========================
// МАТЕМАТИКА
// ========================
KValue native_sqrt(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_float(sqrt(v));
}
KValue native_abs(KValue* args, int count) {
    if (args[0].type == VAL_FLOAT) return make_float(fabs(args[0].f));
    return make_int(llabs(args[0].i));
}
KValue native_floor_f(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_float(floor(v));
}
KValue native_ceil_f(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_float(ceil(v));
}
KValue native_round_f(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_int((int)round(v));
}
KValue native_sin_f(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_float(sin(v));
}
KValue native_cos_f(KValue* args, int count) {
    double v = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    return make_float(cos(v));
}
KValue native_pow_f(KValue* args, int count) {
    double base = (args[0].type == VAL_FLOAT) ? args[0].f : (double)args[0].i;
    double exp  = (args[1].type == VAL_FLOAT) ? args[1].f : (double)args[1].i;
    return make_float(pow(base, exp));
}
KValue native_num_to_str(KValue* args, int count) {
    char buf[64];
    if (args[0].type == VAL_FLOAT) snprintf(buf, sizeof(buf), "%g", args[0].f);
    else                           snprintf(buf, sizeof(buf), "%lld", args[0].i);
    return make_str(buf);
}

// ========================
// СТРОКИ
// ========================
KValue native_str_find(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(-1);
    char* p = strstr(args[0].s, args[1].s);
    return p ? make_int((int)(p - args[0].s)) : make_int(-1);
}
KValue native_str_sub(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_INT) return make_str("");
    int start = args[1].i, end = args[2].i, len = (int)strlen(args[0].s);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start > end || start >= len) return make_str("");
    char* buf = malloc(end - start + 1);
    strncpy(buf, args[0].s + start, end - start); buf[end - start] = '\0';
    KValue res = make_str(buf); free(buf); return res;
}
KValue native_str_to_int(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_int(0);
    return make_int(atoll(args[0].s));
}
KValue native_str_replace(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR || args[2].type != VAL_STR) return make_str("");
    char* src = args[0].s, *from = args[1].s, *to = args[2].s;
    int src_len = (int)strlen(src), from_len = (int)strlen(from), to_len = (int)strlen(to);
    if (from_len == 0) return make_str(src);
    int cnt = 0; char* p = src;
    while ((p = strstr(p, from))) { cnt++; p += from_len; }
    char* result = malloc(src_len + cnt * (to_len - from_len) + 1);
    char* dst = result; p = src;
    while (*p) {
        if (strncmp(p, from, from_len) == 0) { memcpy(dst, to, to_len); dst += to_len; p += from_len; }
        else *dst++ = *p++;
    }
    *dst = '\0';
    KValue v = make_str(result); free(result); return v;
}

KValue native_str_upper(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    unsigned char* s = (unsigned char*)args[0].s;
    int len = (int)strlen(args[0].s);
    char* out = malloc(len + 1);
    int i = 0, j = 0;
    while (i < len) {
        unsigned char c = s[i];
        if (c >= 'a' && c <= 'z') { out[j++] = c - 32; i++; }
        else if (c == 0xD0 && i + 1 < len) {
            unsigned char c2 = s[i+1];
            if (c2 >= 0xB0 && c2 <= 0xBF) { out[j++] = 0xD0; out[j++] = c2 - 0x20; }
            else if (c2 == 0xB5)           { out[j++] = 0xD0; out[j++] = 0x81; }
            else                           { out[j++] = c; out[j++] = c2; }
            i += 2;
        } else if (c == 0xD1 && i + 1 < len) {
            unsigned char c2 = s[i+1];
            if (c2 >= 0x80 && c2 <= 0x8F) { out[j++] = 0xD0; out[j++] = c2 + 0x20; }
            else if (c2 == 0x91)           { out[j++] = 0xD0; out[j++] = 0x81; }
            else                           { out[j++] = c; out[j++] = c2; }
            i += 2;
        } else { out[j++] = c; i++; }
    }
    out[j] = '\0';
    KValue v = make_str(out); free(out); return v;
}

KValue native_str_lower(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    unsigned char* s = (unsigned char*)args[0].s;
    int len = (int)strlen(args[0].s);
    char* out = malloc(len + 1);
    int i = 0, j = 0;
    while (i < len) {
        unsigned char c = s[i];
        if (c >= 'A' && c <= 'Z') { out[j++] = c + 32; i++; }
        else if (c == 0xD0 && i + 1 < len) {
            unsigned char c2 = s[i+1];
            if (c2 >= 0x90 && c2 <= 0x9F)      { out[j++] = 0xD0; out[j++] = c2 + 0x20; }
            else if (c2 >= 0xA0 && c2 <= 0xAF) { out[j++] = 0xD1; out[j++] = c2 - 0x20; }
            else if (c2 == 0x81)               { out[j++] = 0xD1; out[j++] = 0x91; }
            else                               { out[j++] = c; out[j++] = c2; }
            i += 2;
        } else { out[j++] = c; i++; }
    }
    out[j] = '\0';
    KValue v = make_str(out); free(out); return v;
}

KValue native_url_encode(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    unsigned char* s = (unsigned char*)args[0].s;
    char* out = malloc(strlen(args[0].s) * 3 + 1);
    int j = 0;
    for (int i = 0; s[i]; i++) {
        if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') ||
            (s[i] >= '0' && s[i] <= '9') || s[i] == '-' || s[i] == '_' || s[i] == '.' || s[i] == '~') {
            out[j++] = s[i];
        } else { sprintf(out + j, "%%%02X", s[i]); j += 3; }
    }
    out[j] = '\0';
    KValue v = make_str(out); free(out); return v;
}

// ========================
// UTF-8 хелпер для JSON
// ========================
static int unicode_to_utf8(unsigned int cp, char* out) {
    if (cp < 0x80)   { out[0] = (char)cp; return 1; }
    if (cp < 0x800)  { out[0] = (char)(0xC0|(cp>>6)); out[1] = (char)(0x80|(cp&0x3F)); return 2; }
    out[0] = (char)(0xE0|(cp>>12)); out[1] = (char)(0x80|((cp>>6)&0x3F)); out[2] = (char)(0x80|(cp&0x3F)); return 3;
}

static const char* json_parse_str(const char* pos, char* buf, int buf_size) {
    int i = 0;
    while (*pos && *pos != '"') {
        if (i >= buf_size - 4) { pos++; continue; }
        if (*pos == '\\') {
            pos++;
            if      (*pos == 'n')  { buf[i++] = '\n'; pos++; }
            else if (*pos == 'r')  { buf[i++] = '\r'; pos++; }
            else if (*pos == 't')  { buf[i++] = '\t'; pos++; }
            else if (*pos == '"')  { buf[i++] = '"';  pos++; }
            else if (*pos == '\\') { buf[i++] = '\\'; pos++; }
            else if (*pos == 'u')  {
                pos++;
                char hex[5] = {0};
                for (int k = 0; k < 4 && *pos; k++, pos++) hex[k] = *pos;
                unsigned int cp = (unsigned int)strtol(hex, NULL, 16);
                char utf8[4];
                int n = unicode_to_utf8(cp, utf8);
                for (int k = 0; k < n; k++) buf[i++] = utf8[k];
            } else { buf[i++] = *pos; pos++; }
        } else { buf[i++] = *pos++; }
    }
    buf[i] = '\0';
    return pos;
}

// ========================
// JSON ПАРСЕР
// ========================
KValue native_json_get(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_str("");
    char* json = args[0].s, *key = args[1].s;
    char search[256]; snprintf(search, sizeof(search), "\"%s\":", key);
    char* pos = strstr(json, search);
    if (!pos) return make_str("");
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos == '"') {
        pos++; char buf[65536]; pos = json_parse_str(pos, buf, sizeof(buf)); return make_str(buf);
    }
    if (*pos == '-' || (*pos >= '0' && *pos <= '9')) return make_int(atoll(pos));
    if (strncmp(pos, "true",  4) == 0) return make_int(1);
    if (strncmp(pos, "false", 5) == 0) return make_int(0);
    if (strncmp(pos, "null",  4) == 0) return make_str("");
    return make_str("");
}

KValue native_json_get_obj(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_STR) return make_str("");
    char* json = args[0].s, *key = args[1].s;
    char search[256]; snprintf(search, sizeof(search), "\"%s\":", key);
    char* pos = strstr(json, search);
    if (!pos) return make_str("");
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    char open = *pos, close = (open == '{') ? '}' : ']';
    if (open != '{' && open != '[') return make_str("");
    int depth = 1; char* start = pos; pos++;
    while (*pos && depth > 0) {
        if (*pos == '"') { pos++; while (*pos && !(*pos == '"' && *(pos-1) != '\\')) pos++; }
        if (*pos == open) depth++; else if (*pos == close) depth--;
        pos++;
    }
    int len = (int)(pos - start);
    char* buf = malloc(len + 1); strncpy(buf, start, len); buf[len] = '\0';
    KValue v = make_str(buf); free(buf); return v;
}

KValue native_json_arr_get(KValue* args, int count) {
    if (args[0].type != VAL_STR || args[1].type != VAL_INT) return make_str("");
    char* json = args[0].s; int n = args[1].i;
    char* pos = json;
    while (*pos && *pos != '[') pos++;
    if (!*pos) return make_str("");
    pos++; int cur = 0;
    while (*pos && cur <= n) {
        while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
        char open = *pos; char close = (open == '{') ? '}' : (open == '[' ? ']' : 0);
        if (open == '{' || open == '[') {
            if (cur == n) {
                int depth = 1; char* start = pos; pos++;
                while (*pos && depth > 0) {
                    if (*pos == '"') { pos++; while (*pos && !(*pos == '"' && *(pos-1) != '\\')) pos++; }
                    if (*pos == open) depth++; else if (*pos == close) depth--;
                    pos++;
                }
                int len = (int)(pos - start);
                char* buf = malloc(len + 1); strncpy(buf, start, len); buf[len] = '\0';
                KValue v = make_str(buf); free(buf); return v;
            } else {
                int depth = 1; pos++;
                while (*pos && depth > 0) {
                    if (*pos == '"') { pos++; while (*pos && !(*pos == '"' && *(pos-1) != '\\')) pos++; }
                    if (*pos == open) depth++; else if (*pos == close) depth--;
                    pos++;
                }
            }
        } else if (*pos == '"') {
            pos++;
            if (cur == n) {
                char buf[65536]; pos = json_parse_str(pos, buf, sizeof(buf));
                if (*pos == '"') pos++;
                return make_str(buf);
            }
            while (*pos && *pos != '"') { if (*pos == '\\') { pos++; if (*pos) pos++; } else pos++; }
            if (*pos) pos++;
        } else {
            char* start3 = pos;
            while (*pos && *pos != ',' && *pos != ']') pos++;
            if (cur == n) {
                int len = (int)(pos - start3);
                char* buf = malloc(len + 1); strncpy(buf, start3, len); buf[len] = '\0';
                KValue v = make_int(atoll(buf)); free(buf); return v;
            }
        }
        cur++;
        if (*pos == ',') pos++;
    }
    return make_str("");
}

KValue native_json_arr_len(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_int(0);
    char* json = args[0].s, *pos = json;
    while (*pos && *pos != '[') pos++;
    if (!*pos) return make_int(0);
    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
    if (*pos == ']') return make_int(0);
    int n = 0, depth = 1;
    while (*pos && depth > 0) {
        if (*pos == '"') { pos++; while (*pos && !(*pos == '"' && *(pos-1) != '\\')) pos++; if (*pos) pos++; continue; }
        if (*pos == '[' || *pos == '{') depth++;
        else if (*pos == ']' || *pos == '}') { depth--; if (depth == 0) break; }
        else if (*pos == ',' && depth == 1) n++;
        pos++;
    }
    return make_int(n + 1);
}

// ========================
// СЫРЫЕ СОКЕТЫ (ЯДРО ИНТЕРНЕТА)
// ========================
KValue native_sock_create(KValue* args, int count) {
    int s = socket(AF_INET, SOCK_STREAM, 0); return make_int(s);
}
KValue native_sock_connect(KValue* args, int count) {
    if (args[0].type != VAL_INT || args[1].type != VAL_STR || args[2].type != VAL_INT) return make_int(0);
    struct hostent* host = gethostbyname(args[1].s); if (!host) return make_int(0);
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(args[2].i);
    memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);
    return make_int(connect(args[0].i, (struct sockaddr*)&addr, sizeof(addr)) < 0 ? 0 : 1);
}
KValue native_sock_bind(KValue* args, int count) {
    if (args[0].type != VAL_INT || args[1].type != VAL_STR || args[2].type != VAL_INT) return make_int(0);
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(args[1].s); 
    addr.sin_port = htons(args[2].i);
    return make_int(bind(args[0].i, (struct sockaddr*)&addr, sizeof(addr)) == 0 ? 1 : 0);
}
KValue native_sock_listen(KValue* args, int count) {
    if (args[0].type != VAL_INT || args[1].type != VAL_INT) return make_int(0);
    return make_int(listen(args[0].i, args[1].i) == 0 ? 1 : 0);
}
KValue native_sock_accept(KValue* args, int count) {
    if (args[0].type != VAL_INT) return make_int(0);
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int client_sock = accept(args[0].i, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
    return make_int(client_sock);
}
KValue native_sock_send(KValue* args, int count) {
    if (args[0].type == VAL_INT && args[1].type == VAL_STR)
        send(args[0].i, args[1].s, strlen(args[1].s), 0);
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
    if (args[0].type == VAL_INT) closesocket(args[0].i);
    return make_int(0);
}

// ========================
// FFI (ПРЯМОЙ ДОСТУП К СИСТЕМЕ)
// ========================
typedef long long (*FFIFunc0)();
typedef long long (*FFIFunc1)(long long);
typedef long long (*FFIFunc2)(long long, long long);
typedef long long (*FFIFunc3)(long long, long long, long long);
typedef long long (*FFIFunc4)(long long, long long, long long, long long);
typedef long long (*FFIFunc5)(long long, long long, long long, long long, long long);
typedef long long (*FFIFunc6)(long long, long long, long long, long long, long long, long long);

KValue native_api_call(KValue* args, int count) {
    if (count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) return make_int(0);
    
    void* lib = NULL;
    void* func = NULL;
#ifdef _WIN32
    lib = LoadLibraryA(args[0].s);
    if (lib) func = GetProcAddress((HMODULE)lib, args[1].s);
#else
    lib = dlopen(args[0].s, RTLD_LAZY);
    if (lib) func = dlsym(lib, args[1].s);
#endif

    if (!func) return make_int(0); 

    long long a[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 6 && (i + 2) < count; i++) {
        if (args[i+2].type == VAL_INT) a[i] = args[i+2].i;
        else if (args[i+2].type == VAL_STR) a[i] = (long long)(intptr_t)args[i+2].s;
    }

    long long res = 0;
    int arg_cnt = count - 2;
    if (arg_cnt == 0) res = ((FFIFunc0)func)();
    else if (arg_cnt == 1) res = ((FFIFunc1)func)(a[0]);
    else if (arg_cnt == 2) res = ((FFIFunc2)func)(a[0], a[1]);
    else if (arg_cnt == 3) res = ((FFIFunc3)func)(a[0], a[1], a[2]);
    else if (arg_cnt == 4) res = ((FFIFunc4)func)(a[0], a[1], a[2], a[3]);
    else if (arg_cnt == 5) res = ((FFIFunc5)func)(a[0], a[1], a[2], a[3], a[4]);
    else res = ((FFIFunc6)func)(a[0], a[1], a[2], a[3], a[4], a[5]);

    return make_int(res);
}

KValue native_mem_alloc(KValue* args, int count) {
    return make_int((long long)(intptr_t)calloc(1, args[0].i));
}
KValue native_mem_free(KValue* args, int count) {
    free((void*)(intptr_t)args[0].i); return make_int(0);
}
KValue native_mem_read_str(KValue* args, int count) {
    char* ptr = (char*)(intptr_t)args[0].i;
    return ptr ? make_str(ptr) : make_str("");
}
KValue native_mem_write_str(KValue* args, int count) {
    if(args[0].type == VAL_INT && args[1].type == VAL_STR) {
        strcpy((char*)(intptr_t)args[0].i, args[1].s);
    }
    return make_int(0);
}

// ========================
// УПРАВЛЕНИЕ СТЕКОМ И ВЫЗОВАМИ
// ========================
static void push_frame() {
    if (++frame_depth >= 64) runtime_error("Внутренний сбой", 0, "Переполнение стека вызовов", "");
    frames[frame_depth].var_count    = 0;
    frames[frame_depth].has_returned = 0;
    frames[frame_depth].return_value = make_int(0);
}
static void pop_frame() { frame_depth--; }

static KValue get_var(const char* name, ASTNode* node) {
    for (int fd = frame_depth; fd >= 0; fd--) {
        CallFrame* f = &frames[fd];
        for (int i = 0; i < f->var_count; i++)
            if (strcmp(f->names[i], name) == 0) return f->values[i];
    }
    runtime_error(node->file, node->line, "Переменная не найдена", name); return make_int(0);
}

static void set_var(const char* name, KValue value, ASTNode* node) {
    for (int fd = frame_depth; fd >= 0; fd--) {
        CallFrame* f = &frames[fd];
        for (int i = 0; i < f->var_count; i++) {
            if (strcmp(f->names[i], name) == 0) { f->values[i] = value; return; }
        }
    }
    CallFrame* f = &frames[frame_depth];
    if (f->var_count >= 100) runtime_error(node->file, node->line, "Превышен лимит переменных", name);
    strncpy(f->names[f->var_count], name, 255);
    f->names[f->var_count][255] = '\0';
    f->values[f->var_count] = value;
    f->var_count++;
}

static KValue eval(ASTNode* node);
void execute(ASTNode* node);

static KValue call_func(ASTNode* call_node) {
    const char* name = call_node->string_value; int f_idx = -1;
    for (int i = 0; i < func_count; i++)
        if (strcmp(func_table[i].name, name) == 0) { f_idx = i; break; }
    if (f_idx == -1) runtime_error(call_node->file, call_node->line, "Алгоритм не найден", name);
    KValue args[100];
    for (int i = 0; i < call_node->children_count; i++) args[i] = eval(call_node->children[i]);
    if (func_table[f_idx].native_ptr) return func_table[f_idx].native_ptr(args, call_node->children_count);

    ASTNode* def = func_table[f_idx].def; push_frame();
    for (int i = 0; i < def->children_count; i++)
        set_var(def->children[i]->string_value, args[i], call_node);
    for (int i = 0; i < def->left->children_count; i++) {
        execute(def->left->children[i]);
        if (frames[frame_depth].has_returned) break;
    }
    KValue ret = frames[frame_depth].return_value; pop_frame(); return ret;
}

static KValue eval(ASTNode* node) {
    if (!node) return make_int(0);
    if (node->type == AST_NUM)       return make_int(node->int_value);
    if (node->type == AST_FLOAT)     return make_float(node->float_value);
    if (node->type == AST_STR)       return make_str(node->string_value);
    if (node->type == AST_VAR)       return get_var(node->string_value, node);
    if (node->type == AST_FUNC_CALL) return call_func(node);

    if (node->type == AST_ARRAY_LIT) {
        KValue arr = make_array(node->children_count);
        for (int i = 0; i < node->children_count; i++) arr.arr->items[i] = eval(node->children[i]);
        return arr;
    }
    if (node->type == AST_INDEX_ACCESS) {
        KValue arr = eval(node->left); KValue idx = eval(node->right);
        if (arr.type != VAL_ARRAY || idx.type != VAL_INT) runtime_error(node->file, node->line, "Ошибка индексации", "");
        if (idx.i < 0 || idx.i >= arr.arr->length) runtime_error(node->file, node->line, "Индекс вне границ", "");
        return arr.arr->items[idx.i];
    }

    if (node->type == AST_BINOP) {
        const char* op = node->string_value;
        if (strcmp(op, "не") == 0) return make_int(!eval(node->left).i);
        KValue l = eval(node->left); KValue r = eval(node->right);
        
        if (strcmp(op, "+") == 0) {
            if (l.type == VAL_STR || r.type == VAL_STR) {
                char tmp_l[64], tmp_r[64]; const char *ls, *rs;
                if (l.type == VAL_STR)        ls = l.s;
                else if (l.type == VAL_FLOAT) { snprintf(tmp_l, sizeof(tmp_l), "%g", l.f); ls = tmp_l; }
                else                          { snprintf(tmp_l, sizeof(tmp_l), "%lld", l.i); ls = tmp_l; }
                if (r.type == VAL_STR)        rs = r.s;
                else if (r.type == VAL_FLOAT) { snprintf(tmp_r, sizeof(tmp_r), "%g", r.f); rs = tmp_r; }
                else                          { snprintf(tmp_r, sizeof(tmp_r), "%lld", r.i); rs = tmp_r; }
                size_t total = strlen(ls) + strlen(rs) + 1;
                char* buf = malloc(total); snprintf(buf, total, "%s%s", ls, rs);
                KValue res = make_str(buf); free(buf); return res;
            }
            if (l.type == VAL_FLOAT || r.type == VAL_FLOAT) return make_float((l.type==VAL_FLOAT?l.f:l.i) + (r.type==VAL_FLOAT?r.f:r.i));
            return make_int(l.i + r.i);
        }
        if (strcmp(op, "-") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) - (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i - r.i);
        if (strcmp(op, "*") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) * (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i * r.i);
        if (strcmp(op, "/") == 0) return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ? make_float((l.type==VAL_FLOAT?l.f:l.i) / (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i / r.i);
        if (strcmp(op, "%") == 0) return make_int(l.i % (r.i == 0 ? 1 : r.i));

        double lv = (l.type==VAL_FLOAT)?l.f:l.i; double rv = (r.type==VAL_FLOAT)?r.f:r.i;
        if (strcmp(op, "=")  == 0) return l.type==VAL_STR ? make_int(strcmp(l.s,r.s)==0) : make_int(lv==rv);
        if (strcmp(op, "<>") == 0) return l.type==VAL_STR ? make_int(strcmp(l.s,r.s)!=0) : make_int(lv!=rv);
        if (strcmp(op, "<")  == 0) return make_int(lv < rv);
        if (strcmp(op, ">")  == 0) return make_int(lv > rv);
        if (strcmp(op, "<=") == 0) return make_int(lv <= rv);
        if (strcmp(op, ">=") == 0) return make_int(lv >= rv);
        if (strcmp(op, "и")  == 0) return make_int(l.i && r.i);
        if (strcmp(op, "или")== 0) return make_int(l.i || r.i);
    }
    return make_int(0);
}

void print_val(KValue v) {
    if      (v.type == VAL_INT)   printf("%lld", v.i);
    else if (v.type == VAL_FLOAT) printf("%g", v.f);
    else if (v.type == VAL_STR)   printf("%s", v.s);
    else if (v.type == VAL_ARRAY) {
        printf("[");
        for (int i = 0; i < v.arr->length; i++) { print_val(v.arr->items[i]); if (i < v.arr->length-1) printf(", "); }
        printf("]");
    }
}

// ========================
// РЕГИСТРАЦИЯ ФУНКЦИЙ
// ========================
void register_native(const char* name, NativeFunc ptr) {
    if (func_count >= MAX_FUNCS) runtime_error("Внутренний сбой", 0, "Переполнение таблицы функций", name);
    strncpy(func_table[func_count].name, name, 255);
    func_table[func_count].name[255] = '\0';
    func_table[func_count].def        = NULL;
    func_table[func_count].native_ptr = ptr;
    func_count++;
}

void execute(ASTNode* node) {
    if (!node || (frame_depth >= 0 && frames[frame_depth].has_returned)) return;
    switch (node->type) {
        case AST_PROGRAM: {
#ifdef _WIN32
            WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            // ОСНОВНЫЕ
            register_native("ввод",                    native_vvod);
            register_native("длина",                   native_len);
            register_native("ос_команда",              native_os_cmd);
            register_native("ос_чтение",               native_os_read);
            register_native("ос_запись",               native_os_write);
            
            // МАТЕМАТИКА
            register_native("корень",                  native_sqrt);
            register_native("абс",                     native_abs);
            register_native("пол",                     native_floor_f);
            register_native("потолок",                 native_ceil_f);
            register_native("округл",                  native_round_f);
            register_native("синус",                   native_sin_f);
            register_native("косинус",                 native_cos_f);
            register_native("степень",                 native_pow_f);
            register_native("число_в_строку",          native_num_to_str);
            
            // СТРОКИ
            register_native("строка_найти",            native_str_find);
            register_native("строка_срез",             native_str_sub);
            register_native("строка_в_число",          native_str_to_int);
            register_native("строка_заменить",         native_str_replace);
            register_native("строка_верхний",          native_str_upper);
            register_native("строка_нижний",           native_str_lower);
            register_native("урл_кодировать",          native_url_encode);
            
            // JSON
            register_native("json_получить",           native_json_get);
            register_native("json_объект",             native_json_get_obj);
            register_native("json_элемент",            native_json_arr_get);
            register_native("json_длина",              native_json_arr_len);
            
            // СЛОВАРИ (НОВОЕ: НА БАЗЕ МАССИВОВ)
            register_native("словарь",                 native_dict_create);
            register_native("сл_записать",             native_dict_write);
            register_native("сл_читать",               native_dict_read);

            // СЕТЕВЫЕ ЗАГОЛОВКИ (НОВОЕ)
            register_native("сеть_юзерагент",          native_set_useragent);
            register_native("сеть_заголовок",          native_add_header);
            register_native("сеть_очистить_заголовки", native_clear_headers);
            register_native("сеть_куки",               native_add_cookie);
            register_native("сеть_очистить_куки",      native_clear_cookies);

            // СЫРЫЕ СОКЕТЫ
            register_native("сокет_создать",           native_sock_create);
            register_native("сокет_подключить",        native_sock_connect);
            register_native("сокет_бинд",              native_sock_bind);
            register_native("сокет_слушать",           native_sock_listen);
            register_native("сокет_принять",           native_sock_accept);
            register_native("сокет_отправить",         native_sock_send);
            register_native("сокет_получить",          native_sock_recv);
            register_native("сокет_закрыть",           native_sock_close);

            // FFI
            register_native("апи_вызов",               native_api_call);
            register_native("память_выделить",         native_mem_alloc);
            register_native("память_освободить",       native_mem_free);
            register_native("память_в_строку",         native_mem_read_str);
            register_native("строку_в_память",         native_mem_write_str);

            for (int i = 0; i < node->children_count; i++) {
                if (node->children[i]->type == AST_FUNC_DEF) {
                    if (func_count >= MAX_FUNCS) runtime_error("Внутренний сбой", 0, "Слишком много алгоритмов", "");
                    strncpy(func_table[func_count].name, node->children[i]->string_value, 255);
                    func_table[func_count].name[255] = '\0';
                    func_table[func_count].def = node->children[i];
                    func_table[func_count].native_ptr = NULL;
                    func_count++;
                }
            }
            ASTNode* main_alg = NULL;
            for (int i = node->children_count - 1; i >= 0; i--)
                if (node->children[i]->type == AST_FUNC_DEF && node->children[i]->children_count == 0)
                    { main_alg = node->children[i]; break; }
            if (!main_alg && node->children_count > 0) main_alg = node->children[node->children_count - 1];
            if (main_alg) {
                push_frame();
                for (int i = 0; i < main_alg->left->children_count; i++) execute(main_alg->left->children[i]);
                pop_frame();
            }
            break;
        }
        case AST_VAR_DECL: set_var(node->string_value, make_int(0), node); break;
        case AST_ASSIGN:
            if (node->left->type == AST_VAR)
                set_var(node->left->string_value, eval(node->right), node);
            else if (node->left->type == AST_INDEX_ACCESS) {
                KValue arr = eval(node->left->left);
                KValue idx = eval(node->left->right);
                KValue val = eval(node->right);
                if (arr.type == VAL_ARRAY && idx.type == VAL_INT && idx.i >= 0 && idx.i < arr.arr->length)
                    arr.arr->items[idx.i] = val;
            }
            break;
        case AST_RETURN: frames[frame_depth].return_value = eval(node->left); frames[frame_depth].has_returned = 1; break;
        case AST_FUNC_CALL: call_func(node); break;
        case AST_IF: {
            if (eval(node->children[0]).i) {
                for (int i = 0; i < node->children[1]->children_count; i++) { execute(node->children[1]->children[i]); if (frames[frame_depth].has_returned) break; }
            } else if (node->children_count > 2) {
                for (int i = 0; i < node->children[2]->children_count; i++) { execute(node->children[2]->children[i]); if (frames[frame_depth].has_returned) break; }
            }
            break;
        }
        case AST_WHILE: {
            while (!frames[frame_depth].has_returned && eval(node->children[0]).i) {
                for (int i = 0; i < node->children[1]->children_count; i++) { execute(node->children[1]->children[i]); if (frames[frame_depth].has_returned) break; }
            }
            break;
        }
        case AST_REPEAT: {
            int times = eval(node->children[0]).i;
            for (int t = 0; t < times && !frames[frame_depth].has_returned; t++) {
                for (int i = 0; i < node->children[1]->children_count; i++) { execute(node->children[1]->children[i]); if (frames[frame_depth].has_returned) break; }
            }
            break;
        }
        case AST_PRINT:
            for (int i = 0; i < node->children_count; i++) print_val(eval(node->children[i]));
            printf("\n"); break;
        default: break;
    }
}
