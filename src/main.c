#include "kumir.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Универсальное чтение файлов, понимающее русские названия на Windows!
char* read_file_content(const char* filename) {
    FILE* file = NULL;
#ifdef _WIN32
    // Преобразуем UTF-8 имя файла в UTF-16 для Windows
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
#endif

    if (argc < 2) { printf("Использование: kumir2 <файл.kum>\n"); return 1; }
    if (!strstr(argv[1], ".kum")) { printf("Ошибка: Файл должен иметь расширение .kum\n"); return 1; }

    char* source = read_file_content(argv[1]);
    if (!source) { printf("Ошибка: не удалось прочитать главный файл %s\n", argv[1]); return 1; }
    
    ASTNode* ast = parse(source);
    execute(ast);
    free(source); return 0;
}
