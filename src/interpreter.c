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
#define closesocket close
#endif

// ========================
// ЗНАЧЕНИЯ
// ========================
KValue make_int(int v)    { KValue val; val.type = VAL_INT;   val.i = v;   val.f = 0.0; val.s = NULL; val.arr = NULL; return val; }
KValue make_float(double v){ KValue val; val.type = VAL_FLOAT; val.i = 0;   val.f = v;   val.s = NULL; val.arr = NULL; return val; }
KValue make_str(const char* v) { KValue val; val.type = VAL_STR; val.s = strdup(v ? v : ""); val.arr = NULL; return val; }
KValue make_array(int size) {
    KValue val; val.type = VAL_ARRAY; val.arr = malloc(sizeof(KArray));
    val.arr->ref_count = 1; val.arr->length = size; val.arr->items = malloc(size * sizeof(KValue));
    for (int i = 0; i < size; i++) val.arr->items[i] = make_int(0);
    return val;
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

void runtime_error(int line, const char* msg, const char* detail) {
    printf("\n[ОШИБКА] Строка %d: %s '%s'\n", line, msg, detail ? detail : ""); exit(1);
}

// ========================
// ГЛОБАЛЬНЫЕ НАСТРОЙКИ СЕТИ
// ========================
static char GLOBAL_USER_AGENT[512] = "Kumir2/1.0";
static char GLOBAL_HEADERS[4096]   = "";
static char GLOBAL_COOKIES[4096]   = "";

KValue native_set_useragent(KValue* args, int count) {
    if (args[0].type == VAL_STR)
        strncpy(GLOBAL_USER_AGENT, args[0].s, sizeof(GLOBAL_USER_AGENT) - 1);
    return make_int(0);
}
KValue native_add_header(KValue* args, int count) {
    if (args[0].type == VAL_STR) {
        size_t remaining = sizeof(GLOBAL_HEADERS) - strlen(GLOBAL_HEADERS) - 3;
        if (strlen(args[0].s) < remaining) {
            strcat(GLOBAL_HEADERS, args[0].s);
            strcat(GLOBAL_HEADERS, "\r\n");
        }
    }
    return make_int(0);
}
KValue native_clear_headers(KValue* args, int count) { GLOBAL_HEADERS[0] = '\0'; return make_int(0); }

KValue native_add_cookie(KValue* args, int count) {
    if (args[0].type == VAL_STR) {
        size_t cur = strlen(GLOBAL_COOKIES);
        size_t remaining = sizeof(GLOBAL_COOKIES) - cur - 3;
        if (strlen(args[0].s) < remaining) {
            if (cur > 0) strcat(GLOBAL_COOKIES, "; ");
            strcat(GLOBAL_COOKIES, args[0].s);
        }
    }
    return make_int(0);
}
KValue native_clear_cookies(KValue* args, int count) { GLOBAL_COOKIES[0] = '\0'; return make_int(0); }

// ========================
// ВВОД
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

// ========================
// ОС
// ========================
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
    return make_int(abs(args[0].i));
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
    else                           snprintf(buf, sizeof(buf), "%d", args[0].i);
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
    return make_int(atoi(args[0].s));
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
            else if (c2 == 0x81)                { out[j++] = 0xD1; out[j++] = 0x91; }
            else                                { out[j++] = c; out[j++] = c2; }
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
        pos++;
        char buf[65536]; int i = 0;
        while (*pos && *pos != '"') {
            if (*pos == '\\') {
                pos++;
                if (*pos == 'n')      buf[i++] = '\n';
                else if (*pos == 'r') buf[i++] = '\r';
                else if (*pos == 't') buf[i++] = '\t';
                else                  buf[i++] = *pos;
            } else buf[i++] = *pos;
            pos++;
            if (i >= 65534) break;
        }
        buf[i] = '\0'; return make_str(buf);
    }
    if (*pos == '-' || (*pos >= '0' && *pos <= '9')) return make_int(atoi(pos));
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
            pos++; char* start2 = pos;
            while (*pos && *pos != '"') pos++;
            if (cur == n) {
                int len = (int)(pos - start2);
                char* buf = malloc(len + 1); strncpy(buf, start2, len); buf[len] = '\0';
                KValue v = make_str(buf); free(buf); if (*pos) pos++; return v;
            }
            if (*pos) pos++;
        } else {
            char* start3 = pos;
            while (*pos && *pos != ',' && *pos != ']') pos++;
            if (cur == n) {
                int len = (int)(pos - start3);
                char* buf = malloc(len + 1); strncpy(buf, start3, len); buf[len] = '\0';
                KValue v = make_int(atoi(buf)); free(buf); return v;
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
        if (*pos == '"') {
            pos++;
            while (*pos && !(*pos == '"' && *(pos-1) != '\\')) pos++;
            if (*pos) pos++;
            continue;
        }
        if (*pos == '[' || *pos == '{') depth++;
        else if (*pos == ']' || *pos == '}') { depth--; if (depth == 0) break; }
        else if (*pos == ',' && depth == 1) n++;
        pos++;
    }
    return make_int(n + 1);
}

// ========================
// СЫРЫЕ СОКЕТЫ
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
// HTTPS (Windows, через WinINet)
// ИСПРАВЛЕНО: диагностика ошибок + SSL флаги
// ========================
#ifdef _WIN32
static char* wininet_request(const char* url_utf8, const char* post_data_utf8) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url_utf8, -1, NULL, 0);
    wchar_t* wurl = malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, url_utf8, -1, wurl, wlen);

    HINTERNET hInternet = InternetOpenA(GLOBAL_USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        printf("[ОШИБКА] https: InternetOpen провалился. Код WinINet: %lu\n", GetLastError());
        free(wurl);
        return strdup("");
    }

    // Таймауты: 10 секунд на коннект, 15 на чтение
    DWORD timeout_connect = 10000;
    DWORD timeout_recv    = 15000;
    InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_connect, sizeof(timeout_connect));
    InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_recv,    sizeof(timeout_recv));

    // ИСПРАВЛЕНО: добавлены флаги для обхода SSL-проблем
    DWORD flags = INTERNET_FLAG_RELOAD
                | INTERNET_FLAG_NO_CACHE_WRITE
                | INTERNET_FLAG_NO_COOKIES
                | INTERNET_FLAG_IGNORE_CERT_ERRORS;   // <-- ключевое исправление
    if (strncmp(url_utf8, "https://", 8) == 0) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hUrl  = NULL;
    HINTERNET hConn = NULL;

    if (post_data_utf8 && strlen(post_data_utf8) > 0) {
        URL_COMPONENTSW uc; memset(&uc, 0, sizeof(uc));
        uc.dwStructSize = sizeof(uc);
        wchar_t whost[512]; wchar_t wpath[2048];
        uc.lpszHostName = whost; uc.dwHostNameLength = 512;
        uc.lpszUrlPath  = wpath; uc.dwUrlPathLength  = 2048;
        InternetCrackUrlW(wurl, 0, 0, &uc);

        hConn = InternetConnectW(hInternet, whost, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConn) {
            printf("[ОШИБКА] https POST: InternetConnect провалился. Код: %lu\n", GetLastError());
            InternetCloseHandle(hInternet);
            free(wurl);
            return strdup("");
        }

        DWORD req_flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_IGNORE_CERT_ERRORS;
        if (uc.nScheme == INTERNET_SCHEME_HTTPS) req_flags |= INTERNET_FLAG_SECURE;

        HINTERNET hReq = HttpOpenRequestW(hConn, L"POST", wpath, NULL, NULL, NULL, req_flags, 0);
        if (!hReq) {
            printf("[ОШИБКА] https POST: HttpOpenRequest провалился. Код: %lu\n", GetLastError());
            InternetCloseHandle(hConn);
            InternetCloseHandle(hInternet);
            free(wurl);
            return strdup("");
        }

        // Снимаем проверку SSL-сертификата на уровне запроса тоже
        DWORD sec_flags;
        DWORD sec_flags_size = sizeof(sec_flags);
        InternetQueryOption(hReq, INTERNET_OPTION_SECURITY_FLAGS, &sec_flags, &sec_flags_size);
        sec_flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA
                  |  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                  |  SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        InternetSetOption(hReq, INTERNET_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));

        char full_headers[8192];
        snprintf(full_headers, sizeof(full_headers),
            "Content-Type: application/x-www-form-urlencoded\r\n%s%s%s",
            GLOBAL_HEADERS,
            strlen(GLOBAL_COOKIES) ? "Cookie: " : "",
            strlen(GLOBAL_COOKIES) ? GLOBAL_COOKIES : "");

        BOOL ok = HttpSendRequestA(hReq, full_headers, (DWORD)strlen(full_headers),
            (void*)post_data_utf8, (DWORD)strlen(post_data_utf8));
        if (!ok) {
            printf("[ОШИБКА] https POST: HttpSendRequest провалился. Код: %lu\n", GetLastError());
            InternetCloseHandle(hReq);
            InternetCloseHandle(hConn);
            InternetCloseHandle(hInternet);
            free(wurl);
            return strdup("");
        }
        hUrl = hReq;
    } else {
        char headers[8192];
        snprintf(headers, sizeof(headers), "%s%s%s",
            GLOBAL_HEADERS,
            strlen(GLOBAL_COOKIES) ? "Cookie: " : "",
            strlen(GLOBAL_COOKIES) ? GLOBAL_COOKIES : "");

        int wlenH = MultiByteToWideChar(CP_UTF8, 0, headers, -1, NULL, 0);
        wchar_t* wh = malloc(wlenH * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, headers, -1, wh, wlenH);
        hUrl = InternetOpenUrlW(hInternet, wurl, wh, (DWORD)wcslen(wh), flags, 0);
        free(wh);

        if (!hUrl) {
            DWORD err = GetLastError();
            printf("[ОШИБКА] https GET: InternetOpenUrl провалился. Код WinINet: %lu\n", err);
            // Расшифровка частых кодов
            if (err == 12007) printf("         (12007 = имя хоста не резолвится — нет интернета или DNS)\n");
            if (err == 12029) printf("         (12029 = нет соединения с сервером)\n");
            if (err == 12175) printf("         (12175 = ошибка SSL — попробуйте обновить Windows)\n");
            if (err == 12057) printf("         (12057 = отозванный сертификат)\n");
            InternetCloseHandle(hInternet);
            free(wurl);
            return strdup("");
        }
    }
    free(wurl);

    char* result = malloc(1); result[0] = '\0'; int total = 0;
    char buf[8192]; DWORD read;
    while (InternetReadFile(hUrl, buf, sizeof(buf) - 1, &read) && read > 0) {
        result = realloc(result, total + read + 1);
        memcpy(result + total, buf, read);
        total += read; result[total] = '\0';
    }
    InternetCloseHandle(hUrl);
    if (hConn) InternetCloseHandle(hConn);
    InternetCloseHandle(hInternet);
    return result;
}

KValue native_https_get(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    char* r = wininet_request(args[0].s, NULL);
    KValue v = make_str(r); free(r); return v;
}
KValue native_https_post(KValue* args, int count) {
    if (args[0].type != VAL_STR) return make_str("");
    const char* body = (count > 1 && args[1].type == VAL_STR) ? args[1].s : "";
    char* r = wininet_request(args[0].s, body);
    KValue v = make_str(r); free(r); return v;
}
#else
KValue native_https_get(KValue* args, int count)  { printf("[ОШИБКА] https_гет пока поддерживается только на Windows.\n"); return make_str(""); }
KValue native_https_post(KValue* args, int count) { printf("[ОШИБКА] https_пост пока поддерживается только на Windows.\n"); return make_str(""); }
#endif

// ========================
// РЕГИСТРАЦИЯ ФУНКЦИЙ
// ========================
void register_native(const char* name, NativeFunc ptr) {
    if (func_count >= MAX_FUNCS) runtime_error(0, "Переполнение таблицы функций", name);
    strncpy(func_table[func_count].name, name, 255);
    func_table[func_count].name[255] = '\0';
    func_table[func_count].def        = NULL;
    func_table[func_count].native_ptr = ptr;
    func_count++;
}

// ========================
// УПРАВЛЕНИЕ ФРЕЙМАМИ
// ========================
static void push_frame() {
    if (++frame_depth >= 64) runtime_error(0, "Переполнение стека вызовов", "");
    frames[frame_depth].var_count    = 0;
    frames[frame_depth].has_returned = 0;
    frames[frame_depth].return_value = make_int(0);
}
static void pop_frame() { frame_depth--; }

static KValue get_var(const char* name, int line) {
    for (int fd = frame_depth; fd >= 0; fd--) {
        CallFrame* f = &frames[fd];
        for (int i = 0; i < f->var_count; i++)
            if (strcmp(f->names[i], name) == 0) return f->values[i];
    }
    runtime_error(line, "Переменная не найдена", name); return make_int(0);
}

static void set_var(const char* name, KValue value, int line) {
    for (int fd = frame_depth; fd >= 0; fd--) {
        CallFrame* f = &frames[fd];
        for (int i = 0; i < f->var_count; i++) {
            if (strcmp(f->names[i], name) == 0) { f->values[i] = value; return; }
        }
    }
    CallFrame* f = &frames[frame_depth];
    if (f->var_count >= 100) runtime_error(line, "Превышен лимит переменных в функции (100)", name);
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
    if (f_idx == -1) runtime_error(call_node->line, "Алгоритм не найден", name);
    KValue args[100];
    for (int i = 0; i < call_node->children_count; i++) args[i] = eval(call_node->children[i]);
    if (func_table[f_idx].native_ptr) return func_table[f_idx].native_ptr(args, call_node->children_count);

    ASTNode* def = func_table[f_idx].def; push_frame();
    for (int i = 0; i < def->children_count; i++)
        set_var(def->children[i]->string_value, args[i], call_node->line);
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
    if (node->type == AST_VAR)       return get_var(node->string_value, node->line);
    if (node->type == AST_FUNC_CALL) return call_func(node);

    if (node->type == AST_ARRAY_LIT) {
        KValue arr = make_array(node->children_count);
        for (int i = 0; i < node->children_count; i++) arr.arr->items[i] = eval(node->children[i]);
        return arr;
    }
    if (node->type == AST_INDEX_ACCESS) {
        KValue arr = eval(node->left); KValue idx = eval(node->right);
        if (arr.type != VAL_ARRAY || idx.type != VAL_INT) runtime_error(node->line, "Ошибка индексации", "");
        if (idx.i < 0 || idx.i >= arr.arr->length) runtime_error(node->line, "Индекс вне границ", "");
        return arr.arr->items[idx.i];
    }

    if (node->type == AST_BINOP) {
        const char* op = node->string_value;
        if (strcmp(op, "не") == 0) return make_int(!eval(node->left).i);

        KValue l = eval(node->left);
        KValue r = eval(node->right);

        if (strcmp(op, "+") == 0) {
            if (l.type == VAL_STR || r.type == VAL_STR) {
                char tmp_l[64], tmp_r[64];
                const char *ls, *rs;
                if (l.type == VAL_STR)        ls = l.s;
                else if (l.type == VAL_FLOAT) { snprintf(tmp_l, sizeof(tmp_l), "%g", l.f); ls = tmp_l; }
                else                          { snprintf(tmp_l, sizeof(tmp_l), "%d", l.i); ls = tmp_l; }
                if (r.type == VAL_STR)        rs = r.s;
                else if (r.type == VAL_FLOAT) { snprintf(tmp_r, sizeof(tmp_r), "%g", r.f); rs = tmp_r; }
                else                          { snprintf(tmp_r, sizeof(tmp_r), "%d", r.i); rs = tmp_r; }
                size_t total = strlen(ls) + strlen(rs) + 1;
                char* buf = malloc(total);
                snprintf(buf, total, "%s%s", ls, rs);
                KValue res = make_str(buf); free(buf); return res;
            }
            if (l.type == VAL_FLOAT || r.type == VAL_FLOAT)
                return make_float((l.type==VAL_FLOAT?l.f:l.i) + (r.type==VAL_FLOAT?r.f:r.i));
            return make_int(l.i + r.i);
        }
        if (strcmp(op, "-") == 0)
            return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ?
                make_float((l.type==VAL_FLOAT?l.f:l.i) - (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i - r.i);
        if (strcmp(op, "*") == 0)
            return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ?
                make_float((l.type==VAL_FLOAT?l.f:l.i) * (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i * r.i);
        if (strcmp(op, "/") == 0)
            return (l.type==VAL_FLOAT||r.type==VAL_FLOAT) ?
                make_float((l.type==VAL_FLOAT?l.f:l.i) / (r.type==VAL_FLOAT?r.f:r.i)) : make_int(l.i / r.i);
        if (strcmp(op, "%") == 0) return make_int(l.i % (r.i == 0 ? 1 : r.i));

        double lv = (l.type==VAL_FLOAT)?l.f:l.i;
        double rv = (r.type==VAL_FLOAT)?r.f:r.i;
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
    if      (v.type == VAL_INT)   printf("%d", v.i);
    else if (v.type == VAL_FLOAT) printf("%g", v.f);
    else if (v.type == VAL_STR)   printf("%s", v.s);
    else if (v.type == VAL_ARRAY) {
        printf("[");
        for (int i = 0; i < v.arr->length; i++) { print_val(v.arr->items[i]); if (i < v.arr->length-1) printf(", "); }
        printf("]");
    }
}

void execute(ASTNode* node) {
    if (!node || (frame_depth >= 0 && frames[frame_depth].has_returned)) return;
    switch (node->type) {
        case AST_PROGRAM: {
#ifdef _WIN32
            WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            register_native("ввод",                    native_vvod);
            register_native("длина",                   native_len);
            register_native("корень",                  native_sqrt);
            register_native("абс",                     native_abs);
            register_native("пол",                     native_floor_f);
            register_native("потолок",                 native_ceil_f);
            register_native("округл",                  native_round_f);
            register_native("синус",                   native_sin_f);
            register_native("косинус",                 native_cos_f);
            register_native("степень",                 native_pow_f);
            register_native("число_в_строку",          native_num_to_str);
            register_native("ос_команда",              native_os_cmd);
            register_native("ос_чтение",               native_os_read);
            register_native("ос_запись",               native_os_write);
            register_native("строка_найти",            native_str_find);
            register_native("строка_срез",             native_str_sub);
            register_native("строка_в_число",          native_str_to_int);
            register_native("строка_заменить",         native_str_replace);
            register_native("строка_верхний",          native_str_upper);
            register_native("строка_нижний",           native_str_lower);
            register_native("урл_кодировать",          native_url_encode);
            register_native("json_получить",           native_json_get);
            register_native("json_объект",             native_json_get_obj);
            register_native("json_элемент",            native_json_arr_get);
            register_native("json_длина",              native_json_arr_len);
            register_native("сокет_создать",           native_sock_create);
            register_native("сокет_подключить",        native_sock_connect);
            register_native("сокет_отправить",         native_sock_send);
            register_native("сокет_получить",          native_sock_recv);
            register_native("сокет_закрыть",           native_sock_close);
            register_native("https_гет",               native_https_get);
            register_native("https_пост",              native_https_post);
            register_native("инет_юзерагент",          native_set_useragent);
            register_native("инет_заголовок",          native_add_header);
            register_native("инет_очистить_заголовки", native_clear_headers);
            register_native("инет_куки",               native_add_cookie);
            register_native("инет_очистить_куки",      native_clear_cookies);

            for (int i = 0; i < node->children_count; i++) {
                if (node->children[i]->type == AST_FUNC_DEF) {
                    if (func_count >= MAX_FUNCS) runtime_error(0, "Слишком много алгоритмов", "");
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
        case AST_VAR_DECL: set_var(node->string_value, make_int(0), node->line); break;
        case AST_ASSIGN:
            if (node->left->type == AST_VAR)
                set_var(node->left->string_value, eval(node->right), node->line);
            else if (node->left->type == AST_INDEX_ACCESS) {
                KValue arr = eval(node->left->left);
                KValue idx = eval(node->left->right);
                KValue val = eval(node->right);
                if (arr.type == VAL_ARRAY && idx.type == VAL_INT && idx.i >= 0 && idx.i < arr.arr->length)
                    arr.arr->items[idx.i] = val;
            }
            break;
        case AST_RETURN:
            frames[frame_depth].return_value = eval(node->left);
            frames[frame_depth].has_returned = 1;
            break;
        case AST_FUNC_CALL: call_func(node); break;
        case AST_IF: {
            if (eval(node->children[0]).i) {
                ASTNode* body = node->children[1];
                for (int i = 0; i < body->children_count; i++) {
                    execute(body->children[i]);
                    if (frames[frame_depth].has_returned) break;
                }
            } else if (node->children_count > 2) {
                ASTNode* body = node->children[2];
                for (int i = 0; i < body->children_count; i++) {
                    execute(body->children[i]);
                    if (frames[frame_depth].has_returned) break;
                }
            }
            break;
        }
        case AST_WHILE: {
            ASTNode* body = node->children[1];
            while (!frames[frame_depth].has_returned && eval(node->children[0]).i) {
                for (int i = 0; i < body->children_count; i++) {
                    execute(body->children[i]);
                    if (frames[frame_depth].has_returned) break;
                }
            }
            break;
        }
        case AST_REPEAT: {
            int times = eval(node->children[0]).i;
            ASTNode* body = node->children[1];
            for (int t = 0; t < times && !frames[frame_depth].has_returned; t++) {
                for (int i = 0; i < body->children_count; i++) {
                    execute(body->children[i]);
                    if (frames[frame_depth].has_returned) break;
                }
            }
            break;
        }
        case AST_PRINT:
            for (int i = 0; i < node->children_count; i++) print_val(eval(node->children[i]));
            printf("\n"); break;
        default: break;
    }
}
