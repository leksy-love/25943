#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>                     // sigaction, SIGALRM, sig_atomic_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

struct line_entry { off_t start; size_t length; };
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo)
{
    (void)signo;
    timed_out = 1;
}

static int add_line(struct line_entry **lines, size_t *count, size_t *capacity,
                    off_t start, size_t length)
{
    if (*count == *capacity) {
        size_t nc = (*capacity == 0) ? 16 : *capacity * 2;
        struct line_entry *tmp = realloc(*lines, nc * sizeof **lines);
        if (tmp == NULL) return -1;
        *lines = tmp;
        *capacity = nc;
    }
    (*lines)[*count].start = start;
    (*lines)[*count].length = length;
    ++*count;
    return 0;
}

static int write_all(int fd, const char *buf, size_t length)
{
    size_t done = 0;
    while (done < length) {
        ssize_t n = write(fd, buf + done, length - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

// Читаем строку посимвольно, чтобы SIGALRM мог прервать ожидание.
// Управляющие символы (стрелки и т.п.) отбрасываем.
static int read_line_with_timeout(char *buf, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0) {
            if (errno == EINTR && timed_out) return 0;
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return used ? (buf[used] = '\0', 1) : -1;
        if (c == '\n') { buf[used] = '\0'; return 1; }

        unsigned char uc = (unsigned char)c;
        if (uc < 0x20 && c != '\t') continue;
        buf[used++] = c;
    }
    buf[used] = '\0';
    while (1) {                         // съесть остаток длинной строки
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0 || c == '\n') break;
    }
    return 1;
}

static int print_whole_file(int fd, off_t size)
{
    char buf[4096];
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return -1;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        if (write_all(STDOUT_FILENO, buf, (size_t)n) < 0) return -1;
    }
    (void)size;
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    char ch;
    ssize_t n;
    off_t pos = 0, line_start = 0;
    size_t line_len = 0, count = 0, capacity = 0;
    struct line_entry *lines = NULL;
    struct sigaction sa;
    int timer_disabled = 0;             // таймер отключается после первого ввода

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }

    // Строим таблицу строк
    while ((n = read(fd, &ch, 1)) > 0) {
        ++pos;
        if (ch == '\n') {
            if (add_line(&lines, &count, &capacity, line_start, line_len + 1) < 0) {
                perror("realloc"); free(lines); close(fd); return EXIT_FAILURE;
            }
            line_start = pos;
            line_len = 0;
        } else {
            ++line_len;
        }
    }
    if (n < 0) { perror("read"); free(lines); close(fd); return EXIT_FAILURE; }
    if (line_len > 0) {
        if (add_line(&lines, &count, &capacity, line_start, line_len) < 0) {
            perror("realloc"); free(lines); close(fd); return EXIT_FAILURE;
        }
    }

    // Установка обработчика SIGALRM — БЕЗ SA_RESTART, чтобы read прерывался
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction"); free(lines); close(fd); return EXIT_FAILURE;
    }

    for (;;) {
        char input[64], *end;
        long requested;
        int status;
        timed_out = 0;
        printf("Enter line number (0 to quit): ");
        fflush(stdout);

        // Будильник только если пользователь ещё ни разу ничего не вводил
        if (!timer_disabled) alarm(5);

        status = read_line_with_timeout(input, sizeof input);

        alarm(0);

        if (timed_out || status == 0) {
            printf("\nTime expired. File contents:\n");
            if (print_whole_file(fd, pos) < 0) perror("print file");
            break;
        }
        if (status < 0) break;

        // Как только пользователь что-то ввёл — отключаем таймер навсегда
        timer_disabled = 1;

        errno = 0;
        requested = strtol(input, &end, 10);
        while (*end == ' ' || *end == '\t' || *end == '\r') ++end;
        if (end == input || *end != '\0' || errno == ERANGE || requested < 0) {
            fprintf(stderr, "Please enter a non-negative integer.\n");
            continue;
        }
        if (requested == 0) break;
        if ((unsigned long)requested > count) {
            fprintf(stderr, "No such line. File has %zu lines.\n", count);
            continue;
        }

        // Печатаем запрошенную строку
        struct line_entry e = lines[(size_t)requested - 1];
        char *buffer = malloc(e.length ? e.length : 1);
        size_t got = 0;
        if (buffer == NULL) { perror("malloc"); break; }
        if (lseek(fd, e.start, SEEK_SET) == (off_t)-1) {
            perror("lseek"); free(buffer); break;
        }
        while (got < e.length) {
            n = read(fd, buffer + got, e.length - got);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) { perror("read"); break; }
            if (n == 0) break;
            got += (size_t)n;
        }
        if (n < 0) { free(buffer); break; }
        if (write_all(STDOUT_FILENO, buffer, got) < 0) perror("write");
        if (got > 0 && buffer[got - 1] != '\n') putchar('\n');
        free(buffer);
    }

    alarm(0);
    free(lines);
    if (close(fd) < 0) { perror("close"); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}
