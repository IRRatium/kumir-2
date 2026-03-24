#include "kumir.h"

// ========================
// ТАБЛИЦА АЛГОРИТМОВ
// ========================
#define MAX_FUNCTIONS 64

typedef struct {
    char    name[128];
    ASTNode* def; // Указатель на AST_FUNC_DEF
} KumirFunction;

static KumirFunction func_table[MAX_FUNCTIONS];
static int           func_count = 0;

// ========================
// СТЕК ВЫЗОВОВ
// ========================
// Каждый вызов алгоритма получает свой фрейм с локальными переменными.
// Переменные из другого фрейма недоступны — как в настоящем Кумире.

#define MAX_VARS   100
#define MAX_FRAMES  64

typedef struct {
    char names[MAX_VARS][128];
    int  values[MAX_VARS];
    int  var_count;
    int  return_value; // Значение для 'знач'
    int  has_returned; // Флаг: был ли оператор 'знач :='
} CallFrame;

static CallFrame frames[MAX_FRAMES];
static int       frame_depth = -1;

// ========================
// FORWARD DECLARATIONS
// ========================
static int eval(ASTNode* node);
void execute(ASTNode* node);

// ========================
// УПРАВЛЕНИЕ ОШИБКАМИ
// ========================
void runtime_error(int line, const char* msg, const char* detail) {
    printf("\n[КРИТИЧЕСКАЯ ОШИБКА] Строка %d: %s '%s'\n",
           line, msg, detail ? detail : "");
    printf("Выполнение программы остановлено.\n");
    exit(1);
}

// ========================
// УПРАВЛЕНИЕ ФРЕЙМАМИ
// ========================
static void push_frame() {
    if (++frame_depth >= MAX_FRAMES) {
        printf("\n[КРИТИЧЕСКАЯ ОШИБКА] Переполнение стека вызовов!\n");
        printf("Возможна бесконечная рекурсия.\n");
        exit(1);
    }
    frames[frame_depth].var_count    = 0;
    frames[frame_depth].return_value = 0;
    frames[frame_depth].has_returned = 0;
}

static void pop_frame() {
    frame_depth--;
}

// ========================
// РАБОТА С ПЕРЕМЕННЫМИ
// ========================
static void declare_var(const char* name, int initial_value, int line) {
    CallFrame* f = &frames[frame_depth];

    // Запрещаем повторное объявление в одном фрейме
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0)
            runtime_error(line, "Переменная уже объявлена в этом алгоритме:", name);
    }

    if (f->var_count >= MAX_VARS)
        runtime_error(line, "Слишком много переменных в алгоритме", "");

    strcpy(f->names[f->var_count], name);
    f->values[f->var_count] = initial_value;
    f->var_count++;
}

static int get_var(const char* name, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0) return f->values[i];
    }
    runtime_error(line, "Переменная не объявлена:", name);
    return 0;
}

static void set_var(const char* name, int value, int line) {
    CallFrame* f = &frames[frame_depth];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->names[i], name) == 0) {
            f->values[i] = value;
            return;
        }
    }
    runtime_error(line, "Переменная не объявлена (присваивание):", name);
}

// ========================
// ВЫЗОВ АЛГОРИТМА
// ========================
static int call_func(ASTNode* call_node) {
    const char* name = call_node->string_value;
    int         line = call_node->line;

    // Ищем алгоритм в таблице
    ASTNode* def = NULL;
    for (int i = 0; i < func_count; i++) {
        if (strcmp(func_table[i].name, name) == 0) {
            def = func_table[i].def;
            break;
        }
    }
    if (!def)
        runtime_error(line, "Алгоритм не найден:", name);

    // Проверяем количество аргументов
    if (call_node->children_count != def->children_count)
        runtime_error(line, "Неверное количество аргументов для алгоритма:", name);

    // Вычисляем аргументы ДО создания нового фрейма
    // (чтобы они вычислялись в контексте вызывающего алгоритма)
    int args[MAX_VARS];
    for (int i = 0; i < call_node->children_count; i++) {
        args[i] = eval(call_node->children[i]);
    }

    // Создаём новый фрейм для вызванного алгоритма
    push_frame();

    // Объявляем параметры и заполняем их значениями аргументов
    for (int i = 0; i < def->children_count; i++) {
        ASTNode* param = def->children[i];
        declare_var(param->string_value, args[i], line);
    }

    // Выполняем тело алгоритма (нач...кон)
    ASTNode* body = def->left; // AST_BODY
    for (int i = 0; i < body->children_count; i++) {
        execute(body->children[i]);
        if (frames[frame_depth].has_returned) break;
    }

    // Забираем возвращаемое значение и убираем фрейм
    int ret = frames[frame_depth].return_value;
    pop_frame();
    return ret;
}

// ========================
// ВЫЧИСЛИТЕЛЬ ВЫРАЖЕНИЙ
// ========================
static int eval(ASTNode* node) {
    if (!node) return 0;

    switch (node->type) {
        case AST_NUM:
            return node->int_value;

        case AST_VAR:
            return get_var(node->string_value, node->line);

        case AST_FUNC_CALL:
            return call_func(node);

        case AST_BINOP: {
            int l = eval(node->left);
            int r = eval(node->right);
            if (strcmp(node->string_value, "+") == 0) return l + r;
            if (strcmp(node->string_value, "-") == 0) return l - r;
            if (strcmp(node->string_value, "*") == 0) return l * r;
            if (strcmp(node->string_value, "/") == 0) {
                if (r == 0)
                    runtime_error(node->line, "Деление на ноль!", "");
                return l / r;
            }
            break;
        }

        default:
            break;
    }

    runtime_error(node->line, "Неизвестный тип выражения", "");
    return 0;
}

// ========================
// ВЫПОЛНЕНИЕ ОПЕРАТОРОВ
// ========================
void execute(ASTNode* node) {
    if (!node) return;

    // Если в текущем фрейме уже был 'знач :=' — ничего не выполняем
    if (frame_depth >= 0 && frames[frame_depth].has_returned) return;

    switch (node->type) {

        // --------------------------------------------------
        // Корень программы: регистрируем алгоритмы, запускаем главный
        // --------------------------------------------------
        case AST_PROGRAM: {
            // Проход 1: регистрируем все алгоритмы в таблице
            for (int i = 0; i < node->children_count; i++) {
                ASTNode* def = node->children[i];
                if (def->type == AST_FUNC_DEF) {
                    if (func_count >= MAX_FUNCTIONS) {
                        printf("Ошибка: слишком много алгоритмов (максимум %d)\n",
                               MAX_FUNCTIONS);
                        exit(1);
                    }
                    strcpy(func_table[func_count].name, def->string_value);
                    func_table[func_count].def = def;
                    func_count++;
                }
            }

            // Проход 2: ищем главный алгоритм
            // Главный — последний алгоритм без параметров
            ASTNode* main_alg = NULL;
            for (int i = node->children_count - 1; i >= 0; i--) {
                ASTNode* def = node->children[i];
                if (def->type == AST_FUNC_DEF && def->children_count == 0) {
                    main_alg = def;
                    break;
                }
            }
            // Запасной вариант: просто последний алгоритм
            if (!main_alg && node->children_count > 0) {
                main_alg = node->children[node->children_count - 1];
            }
            if (!main_alg) {
                printf("Ошибка: не найден ни один алгоритм в файле\n");
                exit(1);
            }

            // Выполняем главный алгоритм
            push_frame();
            ASTNode* body = main_alg->left;
            for (int i = 0; i < body->children_count; i++) {
                execute(body->children[i]);
                if (frames[frame_depth].has_returned) break;
            }
            pop_frame();
            break;
        }

        // --------------------------------------------------
        // Объявление переменной: цел a
        // --------------------------------------------------
        case AST_VAR_DECL:
            declare_var(node->string_value, 0, node->line);
            break;

        // --------------------------------------------------
        // Присваивание: a := выражение
        // --------------------------------------------------
        case AST_ASSIGN:
            set_var(node->string_value, eval(node->left), node->line);
            break;

        // --------------------------------------------------
        // Возврат значения: знач := выражение
        // --------------------------------------------------
        case AST_ZNACH:
            frames[frame_depth].return_value = eval(node->left);
            frames[frame_depth].has_returned = 1;
            break;

        // --------------------------------------------------
        // Вызов алгоритма как процедуры (результат игнорируется)
        // --------------------------------------------------
        case AST_FUNC_CALL:
            call_func(node);
            break;

        // --------------------------------------------------
        // Вывод: вывод "текст", переменная, выражение, ...
        // --------------------------------------------------
        case AST_PRINT:
            for (int i = 0; i < node->children_count; i++) {
                ASTNode* arg = node->children[i];
                if (arg->type == AST_STR)
                    printf("%s", arg->string_value);
                else
                    printf("%d", eval(arg));
            }
            printf("\n");
            break;

        default:
            break;
    }
}
