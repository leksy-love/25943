#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>                   // mmap, munmap, PROT_READ, MAP_PRIVATE, MAP_FAILED
#include <sys/stat.h>                   // fstat, struct stat
#include <sys/types.h>
#include <unistd.h>

struct line_entry { size_t start; size_t length; };
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo) { (void)signo; timed_out = 1; }

static int add_line(struct line_entry **lines, size_t *count, size_t *cap,
                    size_t start, size_t length)
{
    if (*count == *cap) {
        size_t nc = (*cap == 0) ? 16 : *cap * 2;
        struct line_entry *tmp = realloc(*lines, nc * sizeof **lines);
        if (tmp == NULL) return -1;
        *lines = tmp;
        *cap = nc;
    }
    (*lines)[*count].start = start;
    (*lines)[*count].length = length;
    ++*count;
    return 0;
}

// Читаем строку посимвольно, отбрасываем управляющие символы.
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
    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0 || c == '\n') break;
    }
    return 1;
}

int main(int argc, char **argv)
{
    int fd;
    struct stat st;
    char *mapped = MAP_FAILED;
    size_t size, count = 0, cap = 0, start = 0, len = 0, i;
    struct line_entry *lines = NULL;
    struct sigaction sa;
    int timer_disabled = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return EXIT_FAILURE; }
    if (st.st_size < 0) { fprintf(stderr, "Invalid size\n"); close(fd); return EXIT_FAILURE; }
    size = (size_t)st.st_size;
    if (size > 0) {
        mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) { perror("mmap"); close(fd); return EXIT_FAILURE; }
    }

    // Таблица строк — просто проход по массиву
    for (i = 0; i < size; ++i) {
        if (mapped[i] == '\n') {
            if (add_line(&lines, &count, &cap, start, len + 1) < 0) {
                perror("realloc"); free(lines);
                if (size) munmap(mapped, size);
                close(fd); return EXIT_FAILURE;
            }
            start = i + 1; len = 0;
        } else ++len;
    }
    if (len > 0 && add_line(&lines, &count, &cap, start, len) < 0) {
        perror("realloc"); free(lines);
        if (size) munmap(mapped, size);
        close(fd); return EXIT_FAILURE;
    }

    // Обработчик SIGALRM
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                    // без SA_RESTART
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction"); free(lines);
        if (size) munmap(mapped, size);
        close(fd); return EXIT_FAILURE;
    }

    for (;;) {
        char input[64], *end;
        long requested;
        int status;
        timed_out = 0;
        printf("Enter line number (0 to quit): ");
        fflush(stdout);

        if (!timer_disabled) alarm(5);  // будильник только один раз

        status = read_line_with_timeout(input, sizeof input);

        alarm(0);

        if (timed_out || status == 0) {
            printf("\nTime expired. File contents:\n");
            if (size > 0 && fwrite(mapped, 1, size, stdout) != size)
                perror("fwrite");
            break;
        }
        if (status < 0) break;

        timer_disabled = 1;             // после первого ввода таймер отключаем

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

        // Печатаем запрошенную строку прямо из памяти
        struct line_entry e = lines[(size_t)requested - 1];
        if (e.length > 0 && fwrite(mapped + e.start, 1, e.length, stdout) != e.length)
            perror("fwrite");
        if (e.length > 0 && mapped[e.start + e.length - 1] != '\n')
           
