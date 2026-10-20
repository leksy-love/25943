#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>
#include <limits.h>

extern char **environ;

// Опция -i: UID и GID
static void show_ids(void)
{
    printf("real uid = %d, effective uid = %d\n", (int)getuid(), (int)geteuid());
    printf("real gid = %d, effective gid = %d\n", (int)getgid(), (int)getegid());
}

// Опция -s: лидер группы
static void become_leader(void)
{
    if (setpgid(0, 0) < 0) {
        perror("setpgid");
    } else {
        printf("Process has become group leader, pgid = %d\n", (int)getpgrp());
    }
}

// Опция -p: PID, PPID, PGID
static void show_pids(void)
{
    printf("pid = %d\n", (int)getpid());
    printf("ppid = %d\n", (int)getppid());
    printf("pgid = %d\n", (int)getpgrp());
}

// Опция -u: показать максимальное количество процессов (ulimit -u) через sysconf
static void show_ulimit(void)
{
    long max_proc = sysconf(_SC_CHILD_MAX);
    if (max_proc == -1) {
        perror("sysconf");
        return;
    }
    printf("max user processes (-u): %ld\n", max_proc);
}

// Опция -U: изменить ulimit (в Solaris меняем RLIMIT_FSIZE, так как NPROC нет)
static void set_ulimit(const char *arg)
{
    if (!arg) {
        fprintf(stderr, "-U requires argument\n");
        return;
    }
    char *end;
    long v = strtol(arg, &end, 10);
    if (*end != '\0' || v < 0) {
        fprintf(stderr, "Invalid ulimit value: %s\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_FSIZE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)v;
    if (setrlimit(RLIMIT_FSIZE, &rl) < 0) {
        perror("setrlimit");
        return;
    }
    printf("ulimit (file size) set to %ld\n", v);
}

// Опция -c: показать размер core-файла
static void show_core(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    printf("core file size limit: soft=");
    if (rl.rlim_cur == RLIM_INFINITY) printf("unlimited");
    else printf("%ld", (long)rl.rlim_cur);
    printf(", hard=");
    if (rl.rlim_max == RLIM_INFINITY) printf("unlimited");
    else printf("%ld", (long)rl.rlim_max);
    printf("\n");
}

// Опция -C: изменить размер core-файла
static void set_core(const char *arg)
{
    if (!arg) {
        fprintf(stderr, "-C requires argument\n");
        return;
    }
    char *end;
    long v = strtol(arg, &end, 10);
    if (*end != '\0' || v < 0) {
        fprintf(stderr, "invalid core size: %s\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)v;
    if (setrlimit(RLIMIT_CORE, &rl) < 0) {
        perror("setrlimit");
        return;
    }
    printf("core file size set to %ld\n", v);
}

// Опция -d: текущая директория
static void show_cwd(void)
{
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof buf) == NULL) {
        perror("getcwd");
    } else {
        printf("cwd = %s\n", buf);
    }
}

// Опция -v: переменные среды
static void show_env(void)
{
    for (char **e = environ; *e != NULL; ++e)
        printf("%s\n", *e);
}

// Опция -V: изменить/добавить переменную среды
static void set_env(const char *arg)
{
    if (!arg) {
        fprintf(stderr, "-V requires name=value\n");
        return;
    }
    const char *eq = strchr(arg, '=');
    if (eq == NULL) {
        fprintf(stderr, "Need name=value format\n");
        return;
    }
    size_t len = (size_t)(eq - arg);
    char name[256];
    if (len >= sizeof name) len = sizeof name - 1;
    for (size_t i = 0; i < len; ++i) name[i] = arg[i];
    name[len] = '\0';

    if (setenv(name, eq + 1, 1) < 0) {
        perror("setenv");
    } else {
        printf("environment updated: %s\n", arg);
    }
}

static const char *OPTSTRING = "ispuU:cC:dvV:";

int main(int argc, char **argv)
{
    int opt;
    int *opts = NULL;
    char **args = NULL;
    size_t count = 0, cap = 0;

    if (argc == 1) {
        printf("you need to write arguments\n");
        return EXIT_SUCCESS;
    }

    opterr = 0;

    while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {
        if (count == cap) {
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
        opts[count] = opt;
        args[count] = (optarg != NULL) ? optarg : NULL;
        ++count;
    }

    for (size_t i = count; i-- > 0; ) {
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

            case '?':
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
