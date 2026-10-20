#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct line_entry { size_t start; size_t length; };
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo) { (void)signo; timed_out = 1; }   // обработчик сигнала будильника

static int add_line(struct line_entry **lines, size_t *count, size_t *cap, size_t start, size_t length)
{
    if (*count == *cap) {                                         // если в таблице кончилось место
        size_t nc = (*cap == 0) ? 16 : *cap * 2;                  // вычисляем новый размер (удваиваем)
        struct line_entry *tmp = realloc(*lines, nc * sizeof **lines); // выделяем новую память
        if (tmp == NULL) return -1;                               // если память не дали — ошибка
        *lines = tmp;                                             // сохраняем новый указатель
        *cap = nc;                                                // обновляем вместимость
    }
    (*lines)[*count].start = start;                               // записываем отступ строки
    (*lines)[*count].length = length;                             // записываем её длину
    ++*count;                                                     // увеличиваем счётчик строк
    return 0;
}

static int read_line_with_timeout(char *buf, size_t cap)
{
    size_t used = 0;                                              // сколько символов уже прочитали
    while (used + 1 < cap) {                                      // пока есть место в буфере
        char c;                                                   // переменная под очередной символ
        ssize_t n = read(STDIN_FILENO, &c, 1);                    // читаем ровно один байт с клавиатуры
        if (n < 0) {                                              // если read вернул ошибку
            if (errno == EINTR && timed_out) return 0;            // если прервал будильник — выходим
            if (errno == EINTR) continue;                         // если другой сигнал — просто повторяем
            return -1;                                            // настоящая ошибка чтения
        }
        if (n == 0) return used ? (buf[used] = '\0', 1) : -1;     // если конец файла (Ctrl+D)
        if (c == '\n') { buf[used] = '\0'; return 1; }            // если нажали Enter — строка закончена

        unsigned char uc = (unsigned char)c;                      // приводим к беззнаковому для проверки
        if (uc < 0x20 && c != '\t') continue;                     // игнорируем управляющие символы (стрелки)
        buf[used++] = c;                                          // сохраняем обычный символ в буфер
    }
    buf[used] = '\0';                                             // завершаем строку нулём
    while (1) {                                                   // дочитываем остаток слишком длинной строки
        char c;                                                   // временная переменная
        ssize_t n = read(STDIN_FILENO, &c, 1);                    // читаем по байту
        if (n <= 0 || c == '\n') break;                           // пока не дойдём до Enter
    }
    return 1;                                                     // успешно прочитали
}

int main(int argc, char **argv)
{
    int fd;                                                       // файловый дескриптор
    struct stat st;                                               // структура с инфо о файле
    char *mapped = MAP_FAILED;                                    // указатель на файл в памяти
    size_t size, count = 0, cap = 0, start = 0, len = 0, i;       // переменные размера и таблицы
    struct line_entry *lines = NULL;                              // сама таблица строк
    struct sigaction sa;                                          // настройки сигнала
    int timer_disabled = 0;                                       // флаг: отключён ли таймер

    if (argc != 2) {                                              // если аргументов не 2 (имя программы + файл)
        fprintf(stderr, "Usage: %s filename\n", argv[0]);         // ругаемся и говорим, как использовать
        return EXIT_FAILURE;                                      // выходим с ошибкой
    }
    fd = open(argv[1], O_RDONLY);                                 // открываем файл только для чтения
    if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }         // если не открылся — ошибка
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return EXIT_FAILURE; } // узнаём размер файла
    if (st.st_size < 0) { fprintf(stderr, "Invalid size\n"); close(fd); return EXIT_FAILURE; } // проверка размера
    size = (size_t)st.st_size;                                    // сохраняем размер в size_t

    if (size > 0) {                                               // если файл не пустой
        mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); // отображаем файл в память
        if (mapped == MAP_FAILED) { perror("mmap"); close(fd); return EXIT_FAILURE; } // ошибка mmap
    }

    for (i = 0; i < size; ++i) {                                  // идём по всему файлу в памяти
        if (mapped[i] == '\n') {                                  // если нашли конец строки
            if (add_line(&lines, &count, &cap, start, len + 1) < 0) { // добавляем строку в таблицу (+1 за \n)
                perror("realloc"); free(lines);                   // если память не дали
                if (size) munmap(mapped, size);                   // отцепляем файл от памяти
                close(fd); return EXIT_FAILURE;                   // закрываем файл и выходим
            }
            start = i + 1; len = 0;                               // следующая строка начнётся сразу после \n
        } else ++len;                                             // если не \n, увеличиваем длину строки
    }
    if (len > 0 && add_line(&lines, &count, &cap, start, len) < 0) { // обрабатываем последнюю строку без \n
        perror("realloc"); free(lines);                           // если память не дали
        if (size) munmap(mapped, size);                           // отцепляем память
        close(fd); return EXIT_FAILURE;                           // выходим
    }

    memset(&sa, 0, sizeof sa);                                    // обнуляем структуру настроек сигнала
    sa.sa_handler = on_alarm;                                     // указываем функцию-обработчик
    sigemptyset(&sa.sa_mask);                                     // не блокируем другие сигналы
    sa.sa_flags = 0;                                              // ВАЖНО: без SA_RESTART, иначе read не прервётся
    if (sigaction(SIGALRM, &sa, NULL) < 0) {                      // регистрируем обработчик для будильника
        perror("sigaction"); free(lines);                         // если ошибка — чистим память
        if (size) munmap(mapped, size);                           // отцепляем память
        close(fd); return EXIT_FAILURE;                           // выходим
    }

    for (;;) {                                                    // бесконечный цикл запросов
        char input[64], *end;                                     // буфер под ввод и указатель для strtol
        long requested;                                           // сюда положим номер строки
        int status;                                               // результат чтения строки
        timed_out = 0;                                            // сбрасываем флаг будильника
        printf("Enter line number (0 to quit): ");                // печатаем приглашение
        fflush(stdout);                                           // выводим приглашение сразу на экран

        if (!timer_disabled) alarm(5);                            // включаем будильник (только если первый раз)

        status = read_line_with_timeout(input, sizeof input);     // читаем ввод пользователя

        alarm(0);                                                 // выключаем будильник (если успели ввести)

        if (timed_out || status == 0) {                           // если сработал таймер или нажали Ctrl+D
            printf("\nTime expired. File contents:\n");           // сообщаем, что время вышло
            if (size > 0 && fwrite(mapped, 1, size, stdout) != size) // печатаем весь файл прямо из памяти
                perror("fwrite");                                 // если не удалось напечатать — ошибка
            break;                                                // выходим из цикла
        }
        if (status < 0) break;                                    // если ошибка чтения — выходим

        timer_disabled = 1;                                       // как только ввели хоть что-то — отключаем таймер

        errno = 0;                                                // сбрасываем errno перед strtol
        requested = strtol(input, &end, 10);                      // преобразуем введённое в число

        while (*end == ' ' || *end == '\t' || *end == '\r') ++end; // пропускаем пробелы в конце

        if (end == input || *end != '\0' || errno == ERANGE || requested < 0) { // если ввели не число
            fprintf(stderr, "Please enter a non-negative integer.\n"); // ругаемся
            continue;                                             // просим ввести снова
        }

        if (requested == 0) break;                                // если ввели 0 — выходим

        if ((unsigned long)requested > count) {                   // если номер больше, чем строк в файле
            fprintf(stderr, "No such line. File has %zu lines.\n", count); // сообщаем об этом
            continue;                                             // продолжаем запрашивать
        }

        struct line_entry e = lines[(size_t)requested - 1];       // берём запись для нужной строки (индексы с 0)
        if (e.length > 0 && fwrite(mapped + e.start, 1, e.length, stdout) != e.length) // печатаем строку из памяти
            perror("fwrite");                                     // если ошибка — сообщаем

        if (e.length > 0 && mapped[e.start + e.length - 1] != '\n') // если строка не заканчивалась \n
            putchar('\n');                                        // добавляем перевод строки сами
    }

    alarm(0);                                                     // на всякий случай снимаем будильник
    free(lines);                                                  // освобождаем таблицу строк
    if (size > 0 && munmap(mapped, size) < 0) perror("munmap");   // "отцепляем" файл от памяти
    if (close(fd) < 0) { perror("close"); return EXIT_FAILURE; }  // закрываем файл
    return EXIT_SUCCESS;                                          // успешно завершаем работу
}
