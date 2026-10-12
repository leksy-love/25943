#include <stdio.h>
#include <unistd.h>

int main(void)
{
    FILE *file;  // Указатель для работы с файлом

    // Вывод идентификаторов пользователя до изменения UID
    printf("Before setuid:\n");
    printf("Real UID: %d\n", getuid());       // Реальный UID пользователя
    printf("Effective UID: %d\n", geteuid()); // Эффективный UID (используется для проверки прав)

    // Попытка открыть файл data.txt только для чтения
    file = fopen("data.txt", "r");

    if (file == NULL)
        perror("fopen");          // Если открыть не удалось — выводим ошибку
    else {
        printf("File opened\n");  // Файл успешно открыт
        fclose(file);             // Закрываем файл
    }

    // Понижаем привилегии: эффективный UID становится равным реальному UID
    if (setuid(getuid()) == -1) {
        perror("setuid");         // Если setuid завершился ошибкой — выводим её
        return 1;
    }

    // Вывод идентификаторов пользователя после изменения UID
    printf("\nAfter setuid:\n");
    printf("Real UID: %d\n", getuid());
    printf("Effective UID: %d\n", geteuid());

    // Повторная попытка открыть файл data.txt уже с новыми привилегиями
    file = fopen("data.txt", "r");

    if (file == NULL)
        perror("fopen");          // Если открыть не удалось — выводим ошибку
    else {
        printf("File opened\n");  // Файл успешно открыт
        fclose(file);             // Закрываем файл
    }

    return 0;
}
