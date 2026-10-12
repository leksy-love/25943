#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

struct line_entry {                 // одна запись таблицы: где строка начинается и сколько байт занимает
    off_t start;                    // позиция начала строки в файле
    size_t length;                  // длина строки (включая '\n', если он есть)
};

static int add_line(struct line_entry **lines, size_t *count, size_t *capacity,
                    off_t start, size_t length)
{
    if (*count == *capacity) {                              // если места нет — расширяем массив
        size_t new_capacity = (*capacity == 0) ? 16 : *capacity * 2;
        struct line_entry *tmp = realloc(*lines, new_capacity * sizeof **lines);
        if (tmp == NULL)
            return -1;                                      // не хватило памяти — сообщим наверх
        *lines = tmp;
        *capacity = new_capacity;
    }
    (*lines)[*count].start = start;                         // записываем начало строки
    (*lines)[*count].length = length;                       // и её длину
    ++*count;
    return 0;
}

static int write_all(int fd, const char *buf, size_t length)
{
    size_t done = 0;
    while (done < length) {                                 // write может записать меньше, чем просили
        ssize_t n = write(fd, buf + done, length - done);
        if (n < 0) {
            if (errno == EINTR) continue;                   // прервали сигналом — повторим
            return -1;
        }
        if (n == 0) return -1;
        done += (size_t)n;                                  // сдвигаем указатель на записанное
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    char ch;
    ssize_t n;
    off_t pos = 0, line_start = 0;                          // pos — текущая позиция в файле
    size_t line_len = 0, count = 0, capacity = 0;           // line_len — длина текущей строки
    struct line_entry *lines = NULL;                        // таблица строк

    if (argc != 2) {                                        // нужен ровно один аргумент — имя файла
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);                           // низкоуровневое открытие файла
    if (fd < 0) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    while ((n = read(fd, &ch, 1)) > 0) {                    // читаем файл по одному байту
        ++pos;
        if (ch == '\n') {                                   // конец строки — фиксируем запись
            if (add_line(&lines, &count, &capacity, line_start, line_len + 1) < 0) {
                perror("realloc");
                free(lines); close(fd); return EXIT_FAILURE;
            }
            line_start = pos;                               // следующая строка начнётся здесь
            line_len = 0;
        } else {
            ++line_len;                                     // просто считаем символы
        }
    }
    if (n < 0) {                                            // ошибка чтения
        perror("read");
        free(lines); close(fd); return EXIT_FAILURE;
    }
    if (line_len > 0) {                                     // последняя строка без '\n' в конце файла
        if (add_line(&lines, &count, &capacity, line_start, line_len) < 0) {
            perror("realloc");
            free(lines); close(fd); return EXIT_FAILURE;
        }
    }

    for (;;) {                                              // цикл запросов номера строки
        char input[64], *end;
        long requested;
        printf("Enter line number (0 to quit): ");
        fflush(stdout);
        if (fgets(input, sizeof input, stdin) == NULL) break;   // Ctrl+D — выход
        errno = 0;
        requested = strtol(input, &end, 10);                // преобразуем ввод в число
        if (end == input || (errno == ERANGE)) {            // не число или переполнение
            fprintf(stderr, "Please enter a valid line number.\n");
            continue;
        }
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
        if (*end != '\0' || requested < 0) {                // мусор после числа или отрицательное
            fprintf(stderr, "Please enter a non-negative line number.\n");
            continue;
        }
        if (requested == 0) break;                          // 0 — выход
        if ((unsigned long)requested > count) {             // строки с таким номером нет
            fprintf(stderr, "No such line. File has %zu lines.\n", count);
            continue;
        }
        {
            struct line_entry entry = lines[(size_t)requested - 1];   // берём запись из таблицы
            char *buffer = malloc(entry.length == 0 ? 1 : entry.length);
            size_t got = 0;
            if (buffer == NULL) { perror("malloc"); break; }
            if (lseek(fd, entry.start, SEEK_SET) == (off_t)-1) {      // прыгаем на начало строки
                perror("lseek"); free(buffer); break;
            }
            while (got < entry.length) {                              // читаем ровно её длину
                n = read(fd, buffer + got, entry.length - got);
                if (n < 0 && errno == EINTR) continue;
                if (n < 0) { perror("read"); break; }
                if (n == 0) break;
                got += (size_t)n;
            }
            if (n < 0) { free(buffer); break; }
            if (write_all(STDOUT_FILENO, buffer, got) < 0) {          // печатаем строку
                perror("write"); free(buffer); break;
            }
            if (got > 0 && buffer[got - 1] != '\n') putchar('\n');    // если нет '\n' — добавим
            free(buffer);
        }
    }

    free(lines);                                            // освобождаем таблицу
    if (close(fd) < 0) { perror("close"); return EXIT_FAILURE; }  // и закрываем файл
    return EXIT_SUCCESS;
}
