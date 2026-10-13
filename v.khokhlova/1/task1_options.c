#define _POSIX_C_SOURCE 200809L             // нужен для getopt, setrlimit, setpgid

#include <stdio.h>                          // printf, fprintf, perror
#include <stdlib.h>                         // atol, exit, getenv, setenv, environ
#include <unistd.h>                         // getopt, getuid, geteuid, getgid, getegid,
                                            // getpid, getppid, getpgrp, getcwd, setpgid
#include <sys/resource.h>                   // getrlimit, setrlimit, RLIMIT_CORE
#include <sys/types.h>                      // pid_t, uid_t, gid_t
#include <string.h>                         // strlen

extern char **environ;                      // массив переменных среды

// Показать реальные и эффективные UID/GID
static void show_ids(void)
{
    printf("Real UID = %d, Effective UID = %d\n", (int)getuid(), (int)geteuid());
    printf("Real GID = %d, Effective GID = %d\n", (int)getgid(), (int)getegid());
}

// Сделать процесс лидером своей группы
static void become_leader(void)
{
    if (setpgid(0, 0) < 0)                  // 0,0 — «сделать группу с моим PID»
        perror("setpgid");
    else
        printf("Process is now a group leader, PGID = %d\n", (int)getpgrp());
}

// Показать PID, PPID и PGID
static void show_pids(void)
{
    printf("PID = %d, PPID = %d, PGID = %d\n",
           (int)getpid(), (int)getppid(), (int)getpgrp());
}

// Показать текущий ulimit (мягкий лимит)
static void show_ulimit(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    printf("Current ulimit = %llu\n", (unsigned long long)rl.rlim_cur);
}

// Изменить ulimit на заданное значение
static void set_ulimit(const char *value)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) { perror("getrlimit"); return; }
    rl.rlim_cur = (rlim_t)atol(value);      // мягкий лимит — новое значение
    if (setrlimit(RLIMIT_CORE, &rl) < 0) perror("setrlimit");
    else printf("ulimit changed to %s\n", value);
}

// Показать размер core-файла
static void show_core(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) { perror("getrlimit"); return; }
    printf("Core file size = %llu bytes\n", (unsigned long long)rl.rlim_cur);
}

// Изменить размер core-файла
static void set_core(const char *value)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) { perror("getrlimit"); return; }
    rl.rlim_cur = (rlim_t)atol(value);
    if (setrlimit(RLIMIT_CORE, &rl) < 0) perror("setrlimit");
    else printf("Core file size changed to %s\n", value);
}

// Показать текущую рабочую директорию
static void show_cwd(void)
{
    char buf[4096];
    if (getcwd(buf, sizeof buf) == NULL) perror("getcwd");
    else printf("CWD = %s\n", buf);
}

// Показать все переменные среды
static void show_env(void)
{
    for (char **e = environ; *e != NULL; ++e)   // environ — массив строк "NAME=value"
        printf("%s\n", *e);
}

// Добавить/изменить переменную среды
static void set_env(const char *name_value)
{
    const char *eq = strchr(name_value, '=');   // ищем знак '='
    if (eq == NULL) {
        fprintf(stderr, "Invalid -V argument (expected name=value)\n");
        return;
    }
    size_t name_len = (size_t)(eq - name_value);   // длина имени до '='
    char name[256];
    if (name_len >= sizeof name) name_len = sizeof name - 1;
    for (size_t i = 0; i < name_len; ++i) name[i] = name_value[i];   // копируем имя
    name[name_len] = '\0';
    if (setenv(name, eq + 1, 1) < 0)            // setenv(имя, значение, перезапись=1)
        perror("setenv");
    else
        printf("Environment: %s = %s\n", name, eq + 1);
}

// Описание наших опций для getopt:
// двоеточие после буквы значит, что у опции есть аргумент
static const char *OPTSTRING = "ispuU:cC:dvV:";

int main(int argc, char **argv)
{
    // getopt обрабатывает опции слева направо, но задание требует справа налево.
    // Поэтому сначала найдём все опции и соберём их в массив, потом пройдём в обратном порядке.
    int opt;
    int *opts = NULL;                    // массив кодов опций
    char **args = NULL;                  // массив аргументов (для U, C, V)
    size_t count = 0, cap = 0;

    opterr = 0;                          // сами решаем, что делать с неверными опциями

    while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {   // проходим по всем опциям
        if (count == cap) {                          // расширяем массивы
            size_t nc = (cap == 0) ? 8 : cap * 2;
            int *tmp1 = realloc(opts, nc * sizeof *opts);
            char **tmp2 = realloc(args, nc * sizeof *args);
            if (tmp1 == NULL || tmp2 == NULL) { perror("realloc"); free(opts); free(args); return EXIT_FAILURE; }
            opts = tmp1; args = tmp2; cap = nc;
        }
        opts[count] = opt;                           // сохраняем код опции
        args[count] = (optarg != NULL) ? optarg : NULL;  // и её аргумент, если есть
        ++count;
    }

    // Обрабатываем собранные опции СПРАВА НАЛЕВО
    for (size_t i = count; i-- > 0; ) {              // i идёт от count-1 до 0
        switch (opts[i]) {
            case 'i': show_ids();     break;
            case 's': become_leader(); break;
            case 'p': show_pids();    break;
            case 'u': show_ulimit();  break;
            case 'U': set_ulimit(args[i]); break;
            case 'c': show_core();    break;
            case 'C': set_core(args[i]);  break;
            case 'd': show_cwd();     break;
            case 'v': show_env();     break;
            case 'V': set_env(args[i]); break;
            case '?':                                    // нераспознанная опция
                if (optopt == 0)
                    fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
                else
                    fprintf(stderr, "Unknown option: -%c\n", optopt);
                break;
            default:
                break;
        }
    }

    free(opts);
    free(args);
    return EXIT_SUCCESS;
}
