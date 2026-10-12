#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {                  // узел списка
    char *line;                // указатель на строку в динамической памяти
    struct node *next;         // указатель на следующий узел
};

int main(void)
{
    struct node *head = NULL, *tail = NULL, *p;   // head/tail — края списка, p — рабочий указатель
    char buffer[4096];                            // буфер для чтения строки

    while (fgets(buffer, sizeof buffer, stdin) != NULL) {  // читаем строку (вместе с \n)
        size_t len = strlen(buffer);              // длина без завершающего '\0'
        if (len > 0 && buffer[0] == '.')          // точка в начале — конец ввода
            break;

        p = malloc(sizeof *p);                    // память под узел списка
        if (p == NULL) {
            perror("malloc");
            while (head != NULL) {                // если не вышло — освобождаем всё, что успели
                p = head;
                head = head->next;
                free(p->line);
                free(p);
            }
            return EXIT_FAILURE;
        }

        p->line = malloc(len + 1);                // +1 — под завершающий '\0'
        if (p->line == NULL) {
            perror("malloc");
            free(p);                              // узел уже выделен — освобождаем его
            while (head != NULL) {                // и весь список заодно
                p = head;
                head = head->next;
                free(p->line);
                free(p);
            }
            return EXIT_FAILURE;
        }
        memcpy(p->line, buffer, len + 1);         // копируем строку вместе с '\0'
        p->next = NULL;

        if (tail == NULL)                         // список пуст — новый узел становится head
            head = p;
        else                                      // иначе цепляем к хвосту
            tail->next = p;
        tail = p;                                 // новый узел — теперь хвост
    }

    for (p = head; p != NULL; p = p->next) {      // вывод списка в порядке добавления
        fputs(p->line, stdout);
        if (p->line[0] != '\0' && p->line[strlen(p->line) - 1] != '\n')
            putchar('\n');                        // если строку не завершал '\n' — добавим
    }

    while (head != NULL) {                        // освобождаем память: сначала строку, потом узел
        p = head;
        head = head->next;
        free(p->line);
        free(p);
    }
    return EXIT_SUCCESS;
}
