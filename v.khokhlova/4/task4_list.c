#include <stdio.h>                      // printf, fprintf, fputs, perror, putchar
#include <stdlib.h>                     // malloc, free, exit, EXIT_SUCCESS, EXIT_FAILURE
#include <string.h>                     // strlen, memcpy
#include <unistd.h>                     // read

// Узел связного списка: указатель на строку и указатель на следующий узел
struct node {
    char *line;
    struct node *next;
};

// Читаем строку посимвольно. Управляющие символы (стрелки) отбрасываем.
// Возвращаем длину строки без завершающего нуля, -1 при EOF, 0 при пустой строке.
static long read_line_filtered(char *buf, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0) return -1;
        if (n == 0) {
            if (used == 0) return -1;   // совсем ничего и EOF
            break;
        }
        if (c == '\n') break;           // конец строки

        unsigned char uc = (unsigned char)c;
        if (uc < 0x20 && c != '\t') {
            continue;                   // это управляющий символ: стрелки, Ctrl и т.п.
        }
        buf[used++] = c;
    }
    buf[used] = '\0';
    return (long)used;
}

int main(void)
{
    struct node *head = NULL, *tail = NULL, *p;   // head и tail это края списка
    char buffer[4096];                            // буфер для чтения строки

    for (;;) {
        long len = read_line_filtered(buffer, sizeof buffer);
        if (len < 0) break;                       // EOF
        if (len == 0) continue;                   // пустая строка — пропускаем
        if (buffer[0] == '.') break;              // точка в начале — конец ввода

        p = malloc(sizeof *p);                    // память под узел
        if (p == NULL) { perror("malloc"); return EXIT_FAILURE; }

        p->line = malloc((size_t)len + 1);        // +1 под завершающий \0
        if (p->line == NULL) { perror("malloc"); free(p); return EXIT_FAILURE; }
        memcpy(p->line, buffer, (size_t)len + 1);
        p->next = NULL;

        if (tail == NULL) head = p;               // список пуст — голова это новый узел
        else tail->next = p;                      // иначе цепляем за хвостом
        tail = p;                                 // новый узел становится хвостом
    }

    // Добавим перевод строки для нашего вывода, чтобы всё было на отдельных строках
    putchar('\n');

    for (p = head; p != NULL; p = p->next) {      // проходим по списку
        fputs(p->line, stdout);
        putchar('\n');
    }

    while (head != NULL) {                        // освобождаем память
        p = head;
        head = head->next;
        free(p->line);
        free(p);
    }
    return EXIT_SUCCESS;
}
