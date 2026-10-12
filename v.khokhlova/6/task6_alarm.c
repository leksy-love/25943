#define _POSIX_C_SOURCE 200809L             // включаем POSIX-2008 (sigaction, alarm и т.п.)

#include <errno.h>                          // errno и EINTR
#include <fcntl.h>                          // open, O_RDONLY
#include <signal.h>                         // sigaction, SIGALRM, sig_atomic_t
#include <stdio.h>                          // printf, fprintf, perror
#include <stdlib.h>                         // malloc, free, realloc, strtol, exit
#include <string.h>                         // memset
#include <sys/types.h>                      // off_t, ssize_t
#include <unistd.h>                         // read, write, lseek, close, alarm

struct line_entry { off_t start; size_t length; };   // запись таблицы: отступ и длина строки
static volatile sig_atomic_t timed_out = 0;          // флаг «время вышло», ставится из обработчика

static void on_alarm(int signo)              // обработчик SIGALRM
{
    (void)signo;                             // параметр не нужен, глушим предупреждение
    timed_out = 1;                           // просто выставляем флаг, не делаем ничего тяжёлого
}

static int add_line(struct line_entry **lines, size_t *count, size_t *capacity,
                    off_t start, size_t length)
{
    if (*count == *capacity) {                          // места в таблице больше нет
        size_t nc = (*capacity == 0) ? 16 : *capacity * 2;
        struct line_entry *tmp = realloc(*lines, nc * sizeof **lines);   // расширяем в 2 раза
        if (tmp == NULL) return -1;                     // не хватило памяти — сообщим наверх
        *lines = tmp; *capacity = nc;
    }
    (*lines)[*count].start = start;                     // записываем отступ строки
    (*lines)[*count].length = length;                   // и её длину (включая '\n')
    ++*count;                                           // увеличиваем счётчик строк
    return 0;
}

static int write_all(int fd, const char *buf, size_t length)
{
    size_t done = 0;
    while (done < length) {                             // write может записать меньше, чем просили
        ssize_t n = write(fd, buf + done, length - done);
        if (n < 0) { if (errno == EINTR) continue; return -1; }   // прервали — повтор
        if (n == 0) return -1;                          // ноль байт — что-то не так
        done += (size_t)n;                              // сдвигаемся на записанное
    }
    return 0;
}

// Читает одну строку со stdin через read(), чтобы SIGALRM мог прервать ожидание.
static int read_line_with_timeout(char *buf, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {                            // пока есть место в буфере
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);          // читаем ровно 1 байт — так сигнал прервёт чтение
        if (n < 0) {
            if (errno == EINTR && timed_out) return 0;  // прервали сигналом будильника — выходим
            if (errno == EINTR) continue;               // прервали другим сигналом — просто повтор
            return -1;                                  // настоящая ошибка чтения
        }
        if (n == 0) return used ? (buf[used] = '\0', 1) : -1;   // EOF: строка есть — ок, иначе конец
        if (c == '\n') { buf[used] = '\0'; return 1; }  // конец строки — завершаем
        buf[used++] = c;                                // обычный символ — накапливаем
    }
    buf[used] = '\0';                                   // строка длиннее буфера — обрезаем
    // Дочитываем остаток слишком длинной строки.
    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0 || c == '\n') break;
    }
    return 1;
}

static int print_whole_file(int fd, off_t file_size)
{
    char buf[4096];
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return -1; // прыгаем в начало файла
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);          // читаем блоками по 4 КБ
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;                              // EOF
        if (write_all(STDOUT_FILENO, buf, (size_t)n) < 0) return -1;   // печатаем на stdout
    }
    (void)file_size;                                    // параметр не нужен
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    char ch;
    ssize_t n;
    off_t pos = 0, line_start = 0;                      // pos — текущая позиция в файле
    size_t line_len = 0, count = 0, capacity = 0;       // line_len — длина текущей строки
    struct line_entry *lines = NULL;                    // таблица «отступ + длина»
    struct sigaction sa;

    if (argc != 2) {                                    // ожидаем ровно один аргумент — имя файла
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);                       // открываем файл низкоуровнево
    if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }

    while ((n = read(fd, &ch, 1)) > 0) {                // читаем файл по 1 байту, строим таблицу
        ++pos;                                          // сдвинулись на 1 байт
        if (ch == '\n') {                               // конец строки
            if (add_line(&lines, &count, &capacity, line_start, line_len + 1) < 0) {
                perror("realloc"); free(lines); close(fd); return EXIT_FAILURE;
            }
            line_start = pos; line_len = 0;             // следующая строка начнётся тут
        } else ++line_len;                              // просто считаем символы
    }
    if (n < 0) { perror("read"); free(lines); close(fd); return EXIT_FAILURE; }
    if (line_len > 0 && add_line(&lines, &count, &capacity, line_start, line_len) < 0) {
        perror("realloc"); free(lines); close(fd); return EXIT_FAILURE;   // последняя строка без '\n'
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;                           // обработчик SIGALRM
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                                    // без SA_RESTART — read прервётся
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction"); free(lines); close(fd); return EXIT_FAILURE;
    }

    for (;;) {                                          // цикл запросов номера строки
        char input[64], *end;
        long requested;
        int status;
        timed_out = 0;                                  // сбрасываем флаг таймаута
        printf("Enter line number (0 to quit; 5 seconds): ");
        fflush(stdout);                                 // приглашение должно появиться сразу
        alarm(5);                                       // ставим будильник на 5 секунд
        status = read_line_with_timeout(input, sizeof input);
        alarm(0);                                       // ввод закончился — отменяем будильник
        if (timed_out || status == 0) {                 // время вышло — печатаем весь файл
            printf("\nTime expired. File contents:\n");
            if (print_whole_file(fd, pos) < 0) perror("print file");
            break;
        }
        if (status < 0) break;                          // EOF на stdin — выход
        errno = 0;
        requested = strtol(input, &end, 10);            // строка -> число
        while (*end == ' ' || *end == '\t' || *end == '\r') ++end;   // пропускаем хвостовые пробелы
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
            struct line_entry e = lines[(size_t)requested - 1];    // берём запись строки
            char *buffer = malloc(e.length ? e.length : 1);        // буфер под строку
            size_t got = 0;
            if (buffer == NULL) { perror("malloc"); break; }
            if (lseek(fd, e.start, SEEK_SET) == (off_t)-1) {       // прыгаем к началу строки
                perror("lseek"); free(buffer); break;
            }
            while (got < e.length) {                               // читаем ровно её длину
                n = read(fd, buffer + got, e.length - got);
                if (n < 0 && errno == EINTR) continue;
                if (n < 0) { perror("read"); break; }
                if (n == 0) break;
                got += (size_t)n;
            }
            if (n < 0) { free(buffer); break; }
            if (write_all(STDOUT_FILENO, buffer, got) < 0) perror("write");   // печатаем
            if (got > 0 && buffer[got - 1] != '\n') putchar('\n');            // если нет '\n'
            free(buffer);
        }
    }
    alarm(0);                                           // на всякий случай снимаем будильник
    free(lines);                                        // освобождаем таблицу
    if (close(fd) < 0) { perror("close"); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}
