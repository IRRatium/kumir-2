#include "kumir.h"

#ifdef _WIN32
#include <windows.h>
#endif

char* read_file_content(const char* filename) {
    FILE* file = fopen(filename, "rb"); if (!file) return NULL; 
    fseek(file, 0, SEEK_END); long length = ftell(file); fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1); fread(buffer, 1, length, file); buffer[length] = '\0';
    fclose(file); return buffer;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8); // Фикс для нормальной кодировки в консоли!
#endif

    if (argc < 2) { printf("Использование: kumir2 <файл.kum>\n"); return 1; }
    if (!strstr(argv[1], ".kum")) { printf("Ошибка: Файл должен иметь расширение .kum\n"); return 1; }

    char* source = read_file_content(argv[1]);
    if (!source) { printf("Ошибка: не удалось прочитать файл %s\n", argv[1]); return 1; }
    
    ASTNode* ast = parse(source);
    execute(ast);
    free(source); return 0;
}
