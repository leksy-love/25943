#define _POSIX_C_SOURCE 200809L

// Подключаем стандартные библиотеки
#include <stdio.h>                      // printf, fprintf, perror
#include <stdlib.h>                     // atol, realloc, free, setenv, exit
#include <unistd.h>                     // getopt, getuid, geteuid, getgid, getegid,
                                        // getpid, getppid, getpgrp, getcwd, setpgid
#include <sys/resource.h>               // getrlimit, setrlimit, RLIMIT_FSIZE, RLIMIT_CORE
#include <string.h>                     // strchr

extern char **environ;                  // глобальный массив строк "NAME=value"

// Опция -i: печатает реальные и эффективные UID и GID
static void show_ids(void)
{
    printf("Real UID = %d, Effective UID = %d\n", (int)getuid(), (int)geteuid());
    printf("Real GID = %d, Effective GID = %d\n", (int)getgid(), (int)getegid());
}

// Опция -s: сделать процесс лидером своей группы процессов
static void become_leader(void)
{
    if (setpgid(0, 0) < 0) {            // 0,0 значит создать новую группу и поместить меня в неё
        perror("setpgid");
    } else {
        printf("Process is now a group leader, PGID = %d\n", (int)getpgrp());
    }
}

// Опция -p: печатает PID, PPID, PGID
static void show_pids(void)
{
    printf("PID = %d, PPID = %d, PGID = %d\n",
           (int)getpid(), (int)getppid(), (int)getpgrp());
}

// Опция -u: показать лимит на максимальный размер файла
static void show_ulimit(void)
{
    struct rlimit rl;                   // rlim_cur это мягкий лимит, rlim_max это жёсткий
    if (getrlimit(RLIMIT_FSIZE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    printf("ulimit (max file size) = %llu\n", (unsigned long long)rl.rlim_cur);
}

// Опция -U: изменить лимит на размер файла
static void set_ulimit(const char *v)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_FSIZE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)atol(v);      // atol: строка в длинное целое, "0" даёт 0, "abc" тоже 0
    if (setrlimit(RLIMIT_FSIZE, &rl) < 0) {
        perror("setrlimit");            // обычный пользователь может только уменьшать мягкий лимит
    } else {
        printf("ulimit (max file size) = %s\n", v);
    }
}

// Опция -c: показать размер core-файла
static void show_core(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    printf("Core size = %llu\n", (unsigned long long)rl.rlim_cur);
}

// Опция -C: изменить размер core-файла
static void set_core(const char *v)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)atol(v);
    if (setrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("setrlimit");
    } else {
        printf("Core size = %s\n", v);
    }
}

// Опция -d: напечатать текущую рабочую директорию
static void show_cwd(void)
{
    char buf[4096];                     // буфер под путь к папке
    if (getcwd(buf, sizeof buf) == NULL) {
        perror("getcwd");
    } else {
        printf("CWD = %s\n", buf);
    }
}

// Опция -v: напечатать все переменные среды
static void show_env(void)
{
    for (char **e = environ; *e != NULL; ++e)   // идём по массиву пока не встретим NULL
        printf("%s\n", *e);
}

// Опция -V: добавить или изменить переменную среды вида "NAME=value"
static void set_env(const char *nv)
{
    const char *eq = strchr(nv, '=');           // ищем знак '='
    if (eq == NULL) {
        fprintf(stderr, "Need name=value format\n");
        return;
    }
    size_t len = (size_t)(eq - nv);             // длина имени до символа '='
    char name[256];
    if (len >= sizeof name) len = sizeof name - 1;
    for (size_t i = 0; i < len; ++i) name[i] = nv[i];
    name[len] = '\0';                           // завершаем строку имени нулевым символом

    if (setenv(name, eq + 1, 1) < 0) {          // eq+1 это значение, 1 значит перезаписать если есть
        perror("setenv");
    } else {
        printf("Environment: %s = %s\n", name, eq + 1);
    }
}

// Строка для getopt: простые опции i s p u c d v, опции с аргументом U: C: V:
static const char *OPTSTRING = "ispuU:cC:dvV:";

int main(int argc, char **argv)
{
    int opt;                            // очередная опция от getopt
    int *opts = NULL;                   // массив кодов опций
    char **args = NULL;                 // массив аргументов опций
    size_t count = 0, cap = 0;          // сколько уже есть и сколько места выделено

    opterr = 0;                         // отключаем автоматические сообщения getopt

    // Первая фаза: собрать все опции в массив в порядке слева направо
    while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {

        if (count == cap) {             // если массив заполнен, расширяем в два раза
            size_t nc = (cap == 0) ? 8 : cap * 2;
            int  *t1 = realloc(opts, nc * sizeof *opts);
            char **t2 = realloc(args, nc * sizeof *args);
            if (t1 == NULL || t2 == NULL) {
                perror("realloc");
                free(opts); free(args);
                return EXIT_FAILURE;
            }
            opts = t1; args = t2; cap = nc;
        }

        opts[count] = opt;                          // сохраняем код опции
        args[count] = (optarg != NULL) ? optarg : NULL;   // и её аргумент, если есть
        ++count;
    }

    // Вторая фаза: обработать собранные опции справа налево
    for (size_t i = count; i-- > 0; ) {             // i-- > 0 значит сначала проверить потом уменьшить
        switch (opts[i]) {
            case 'i': show_ids();        break;
            case 's': become_leader();   break;
            case 'p': show_pids();       break;
            case 'u': show_ulimit();     break;
            case 'U': set_ulimit(args[i]); break;
            case 'c': show_core();       break;
            case 'C': set_core(args[i]);  break;
            case 'd': show_cwd();        break;
            case 'v': show_env();        break;
            case 'V': set_env(args[i]);   break;

            case '?':                                       // неизвестная опция
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
