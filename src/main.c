#include "kumir.h"

#ifdef _WIN32
#include <windows.h>
#endif

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Ошибка: не удалось открыть файл %s\n", filename);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char** argv) {
    // Поддержка русского языка в консоли Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        printf("Использование: kumir2 <файл.kum>\n");
        return 1;
    }

    const char* filename = argv[1];
    
    // Проверка расширения .kum
    if (!strstr(filename, ".kum")) {
        printf("Ошибка: Файл должен иметь расширение .kum\n");
        return 1;
    }

    char* source = read_file(filename);
    
    // Запускаем интерпретатор
    ASTNode* ast = parse(source);
    execute(ast);

    free(source);
    return 0;
}
