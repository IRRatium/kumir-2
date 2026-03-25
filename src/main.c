#include "kumir.h"

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

// Глобальная переменная для файлов
const char* current_filename = "main";

char* read_file_content(const char* filename) {
    FILE* file = NULL;
#ifdef _WIN32
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
    wchar_t* wstr = malloc(wchars_num * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, filename, -1, wstr, wchars_num);
    file = _wfopen(wstr, L"rb");
    free(wstr);
#else
    file = fopen(filename, "rb");
#endif

    if (!file) return NULL; 
    fseek(file, 0, SEEK_END); long length = ftell(file); fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1); fread(buffer, 1, length, file); buffer[length] = '\0';
    fclose(file); return buffer;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // ВКЛЮЧАЕМ ПОДДЕРЖКУ ANSI ЦВЕТОВ В КОНСОЛИ WINDOWS
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif

    if (argc < 2) { printf("\033[1;36mИспользование: kumir2 <файл.kum>\033[0m\n"); return 1; }
    if (!strstr(argv[1], ".kum")) { printf("\033[1;31mОшибка: Файл должен иметь расширение .kum\033[0m\n"); return 1; }

    current_filename = argv[1]; // СОХРАНЯЕМ ИМЯ ЗАПУСКАЕМОГО ФАЙЛА

    char* source = read_file_content(argv[1]);
    if (!source) { printf("\033[1;31mОшибка: не удалось прочитать главный файл %s\033[0m\n", argv[1]); return 1; }
    
    ASTNode* ast = parse(source);
    execute(ast);
    free(source); return 0;
}
