#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {                             // Если нет аргументов
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {            // Перебираем все переданные файлы
        struct stat st;
        if (lstat(argv[i], &st) < 0) {          // Получаем инфу (lstat не раскрывает симлинки)
            perror(argv[i]);
            continue;
        }

        char type = '?';                        // Определяем тип файла
        if (S_ISDIR(st.st_mode)) type = 'd';    // Директория
        else if (S_ISREG(st.st_mode)) type = '-'; // Обычный файл

        char perms[10];                         // Формируем строку прав (rwx)
        perms[0] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        perms[1] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        perms[2] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        perms[3] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        perms[4] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        perms[5] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        perms[6] = (st.st_mode & S_IROTH) ? 'r' : '-';
        perms[7] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        perms[8] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        perms[9] = '\0';

        struct passwd *pw = getpwuid(st.st_uid);      // Имя владельца
        struct group  *gr = getgrgid(st.st_gid);      // Имя группы
        char *owner = pw ? pw->pw_name : "unknown";   // Если не нашли - unknown
        char *group = gr ? gr->gr_name : "unknown";

        char timebuf[80];                       // Форматируем время модификации
        struct tm *tm = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

        char *basename = strrchr(argv[i], '/'); // Отрезаем путь, оставляем только имя
        if (basename) basename++;
        else basename = argv[i];

        long size = S_ISREG(st.st_mode) ? (long)st.st_size : 0; // Размер только для файлов

        printf("%c%s %3ld %-8s %-8s %8ld %s %s\n", // Печатаем строку таблицы
               type, perms, (long)st.st_nlink, owner, group, size, timebuf, basename);
    }
    return 0;
}
