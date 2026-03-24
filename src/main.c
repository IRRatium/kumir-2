#include "kumir.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Глобальная функция для библиотек
char* read_file_content(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        // Не пишем ошибку сразу (может файл искался по-другому)
        return NULL; 
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
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        printf("Использование: kumir2 <файл.kum>\n");
        return 1;
    }

    const char* filename = argv[1];
    
    if (!strstr(filename, ".kum")) {
        printf("Ошибка: Файл должен иметь расширение .kum\n");
        return 1;
    }

    char* source = read_file_content(filename);
    if (!source) {
        printf("Ошибка: не удалось прочитать файл %s\n", filename);
        return 1;
    }
    
    ASTNode* ast = parse(source);
    execute(ast);

    free(source);
    return 0;
}
