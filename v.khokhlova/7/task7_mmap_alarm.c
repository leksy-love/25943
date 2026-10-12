#define _POSIX_C_SOURCE 200809L             // включаем POSIX-2008 (mmap, sigaction, alarm)

#include <errno.h>                          // errno и EINTR
#include <fcntl.h>                          // open, O_RDONLY
#include <signal.h>                         // sigaction, SIGALRM, sig_atomic_t
#include <stdio.h>                          // printf, fprintf, perror, fwrite
#include <stdlib.h>                         // malloc, free, realloc, strtol, exit
#include <string.h>                         // memset
#include <sys/mman.h>                       // mmap, munmap, PROT_READ, MAP_PRIVATE, MAP_FAILED
#include <sys/stat.h>                       // fstat, struct stat
#include <sys/types.h>                      // off_t, ssize_t
#include <unistd.h>                         // read, close, alarm

struct line_entry { size_t start; size_t length; };  // запись таблицы: отступ и длина строки
static volatile sig_atomic_t timed_out = 0;          // флаг «время вышло», ставится из обработчика

static void on_alarm(int signo) { (void)signo; timed_out = 1; }  // обработчик SIGALRM: просто флаг

static int add_line(struct line_entry **lines, size_t *count, size_t *capacity,
                    size_t start, size_t length)
{
    if (*count == *capacity) {                          // места в таблице нет — расширяем
        size_t nc = (*capacity == 0) ? 16 : *capacity * 2;
        struct line_entry *tmp = realloc(*lines, nc * sizeof **lines);
        if (tmp == NULL) return -1;                     // не хватило памяти
        *lines = tmp; *capacity = nc;
    }
    (*lines)[*count].start = start;                     // отступ строки
    (*lines)[*count].length = length;                   // её длина (включая '\n')
    ++*count;                                           // увеличиваем счётчик
    return 0;
}

// Читает строку со stdin через read(), чтобы SIGALRM мог прервать ожидание.
static int read_line_with_timeout(char *buf, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {                            // пока есть место в буфере
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);          // читаем 1 байт
        if (n < 0) {
            if (errno == EINTR && timed_out) return 0;  // прервал будильник — выходим
            if (errno == EINTR) continue;               // прервал другой сигнал — повтор
            return -1;                                  // настоящая ошибка
        }
        if (n == 0) return used ? (buf[used] = '\0', 1) : -1;  // EOF
        if (c == '\n') { buf[used] = '\0'; return 1; }  // конец строки
        buf[used++] = c;                                // обычный символ
    }
    buf[used] = '\0';                                   // строка длиннее буфера — обрезаем
    return 1;
}

int main(int argc, char **argv)
{
    int fd;
    struct stat st;
    char *mapped = MAP_FAILED;                          // указатель на отображённую память
    size_t size, count = 0, capacity = 0, start = 0, len = 0, i;
    struct line_entry *lines = NULL;
    struct sigaction sa;

    if (argc != 2) {                                    // нужен ровно один аргумент — имя файла
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);                       // открываем файл
    if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return EXIT_FAILURE; }  // узнаём размер
    if (st.st_size < 0) { fprintf(stderr, "Invalid file size.\n"); close(fd); return EXIT_FAILURE; }
    size = (size_t)st.st_size;
    if (size > 0) {                                     // отображаем файл в память
        mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) { perror("mmap"); close(fd); return EXIT_FAILURE; }
    }

    for (i = 0; i < size; ++i) {                        // проходим по отображению как по массиву
        if (mapped[i] == '\n') {                        // конец строки — записываем в таблицу
            if (add_line(&lines, &count, &capacity, start, len + 1) < 0) {
                perror("realloc"); free(lines);
                if (size) munmap(mapped, size);
                close(fd); return EXIT_FAILURE;
            }
            start = i + 1; len = 0;                     // следующая строка начнётся тут
        } else ++len;                                   // считаем длину
    }
    if (len > 0 && add_line(&lines, &count, &capacity, start, len) < 0) {  // хвост без '\n'
        perror("realloc"); free(lines);
        if (size) munmap(mapped, size);
        close(fd); return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;                           // обработчик SIGALRM
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                                    // без SA_RESTART — read прервётся
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction"); free(lines);
        if (size) munmap(mapped, size);
        close(fd); return EXIT_FAILURE;
    }

    for (;;) {                                          // цикл запросов номера строки
        char input[64], *end;
        long requested;
        int status;
        timed_out = 0;                                  // сбрасываем флаг таймаута
        printf("Enter line number (0 to quit; 5 seconds): ");
        fflush(stdout);
        alarm(5);                                       // ставим будильник на 5 секунд
        status = read_line_with_timeout(input, sizeof input);
        alarm(0);                                       // ввод закончился — отменяем будильник
        if (timed_out || status == 0) {                 // время вышло — печатаем весь файл
            printf("\nTime expired. File contents:\n");
            if (size > 0 && fwrite(mapped, 1, size, stdout) != size)
                perror("fwrite");
            break;
        }
        if (status < 0) break;                          // EOF на stdin
        errno = 0;
        requested = strtol(input, &end, 10);            // строка -> число
        while (*end == ' ' || *end == '\t' || *end == '\r') ++end;
        if (end == input || *end != '\0' || errno == ERANGE || requested < 0) {
            fprintf(stderr, "Please enter a non-negative integer.\n");
            continue;
        }
        if (requested == 0) break;                      // 0 — выход
        if ((unsigned long)requested > count) {
            fprintf(stderr, "No such line. File has %zu lines.\n", count);
            continue;
        }
        {
            struct line_entry e = lines[(size_t)requested - 1];   // берём запись строки
            if (e.length > 0 && fwrite(mapped + e.start, 1, e.length, stdout) != e.length) {
                perror("fwrite"); break;                          // печатаем прямо из памяти
            }
            if (e.length > 0 && mapped[e.start + e.length - 1] != '\n')
                putchar('\n');                                    // если нет '\n'
        }
    }

    alarm(0);                                           // снимаем будильник
    free(lines);                                        // освобождаем таблицу
    if (size > 0 && munmap(mapped, size) < 0) perror("munmap");   // снимаем отображение
    if (close(fd) < 0) { perror("close"); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}
