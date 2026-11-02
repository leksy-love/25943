#include <stdio.h>      // printf, fprintf
#include <stdlib.h>     // exit
#include <unistd.h>     // fork, execvp
#include <sys/wait.h>   // waitpid, WIFEXITED, WEXITSTATUS

int main(int argc, char **argv) {
    if (argc < 2) {     // Если нет аргументов (даже имени команды)
        fprintf(stderr, "Usage: %s command [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();                // Создаем дочерний процесс
    if (pid < 0) {
        perror("fork");
        return 1;
    } 
    else if (pid == 0) {               // Дочерний процесс
        // execvp ищет команду в PATH. Передаем массив аргументов начиная с argv[1]
        execvp(argv[1], &argv[1]);     
        perror("execvp");              // Сюда попадаем только если команда не найдена
        exit(127);                     // 127 - стандартный код "команда не найдена"
    } 
    else {                             // Родительский процесс
        int status;
        waitpid(pid, &status, 0);      // Ждем завершения команды
        if (WIFEXITED(status)) {       // Если процесс завершился нормально (не убит сигналом)
            printf("Exit code: %d\n", WEXITSTATUS(status)); // Печатаем код выхода
        } else {
            printf("Process terminated abnormally\n");
        }
    }
    return 0;
}
