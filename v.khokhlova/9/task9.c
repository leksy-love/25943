#include <stdio.h>      // printf
#include <stdlib.h>     // exit
#include <unistd.h>     // fork, execlp, sleep
#include <sys/wait.h>   // waitpid

int main(int argc, char **argv) {
    if (argc != 2) {    // проверяем, что передан ровно 1 аргумент (имя файла)
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    printf("Parent: before fork\n");   // родитель печатает до создания ребенка

    pid_t pid = fork();                // создаем дочерний процесс
    if (pid < 0) {                     // если fork вернул -1, произошла ошибка
        perror("fork");
        return 1;
    } 
    else if (pid == 0) {               // если pid == 0, мы в дочернем процессе
        execlp("cat", "cat", argv[1], NULL); // заменяем код ребенка на команду cat с нашим файлом
        perror("execlp");              // если cat не запустился, выводим ошибку
        exit(1);                       // завершаем ребенка с кодом ошибки
    } 
    else {                             // если pid > 0, мы в родительском процессе
        printf("Parent: child is running, waiting...\n"); // родитель печатает промежуточный текст
        int status;                    // переменная для хранения статуса завершения
        waitpid(pid, &status, 0);      // родитель ЖДЕТ, пока ребенок полностью не завершится
        printf("Parent: child finished. This is the last line\n"); // печатается ПОСЛЕ ребенка
    }

    return 0;
}
