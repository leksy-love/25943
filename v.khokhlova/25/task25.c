#include <stdio.h>      // printf, putchar, perror
#include <stdlib.h>     // exit
#include <unistd.h>     // pipe, fork, read, write, close
#include <string.h>     // strlen
#include <ctype.h>      // toupper

int main() {
    int fd[2];                          // fd[0] для чтения, fd[1] для записи
    if (pipe(fd) == -1) {               // Создаем канал (трубу)
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();                 // Создаем дочерний процесс
    if (pid < 0) {
        perror("fork");
        exit(1);
    } 
    else if (pid == 0) {                // ДОЧЕРНИЙ ПРОЦЕСС (ПИСАТЕЛЬ)
        close(fd[0]);                   // Закрываем конец для чтения (нам он не нужен)
        const char *msg = "Hello World! Mixed Case Text.\n";
        write(fd[1], msg, strlen(msg)); // Пишем строку в канал
        close(fd[1]);                   // Закрываем конец для записи (сигнал, что данные кончились)
        exit(0);                        // Завершаем ребенка
    } 
    else {                              // РОДИТЕЛЬСКИЙ ПРОЦЕСС (ЧИТАТЕЛЬ)
        close(fd[1]);                   // Закрываем конец для записи
        char ch;
        // Читаем из канала по одному символу, пока read не вернет 0 (конец данных)
        while (read(fd[0], &ch, 1) > 0) { 
            putchar(toupper((unsigned char)ch)); // Превращаем в заглавную и печатаем
        }
        close(fd[0]);                   // Закрываем конец для чтения
    }
    return 0;
}
