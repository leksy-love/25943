#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>

#define MAXCOL 40                       // Максимальная длина строки (по условию 40)
#define CTRL_D 0x04                     // Код Ctrl-D (EOF)
#define CTRL_W 0x17                     // Код Ctrl-W
#define BELL   0x07                     // Код звукового сигнала (Ctrl-G)

static struct termios saved_tio;        // Структура для сохранения старых настроек терминала
static int tio_saved = 0;               // Флаг: сохранены ли настройки (чтобы не сломать терминал)

static char line[MAXCOL + 1];           // Буфер для текущей строки
static int  len = 0;                    // Текущая длина строки

// Функция возврата терминала в исходное состояние
static void restore_tty(void)
{
    if (tio_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_tio); // Возвращаем настройки как было
}

// Обработчик аварийных сигналов (Ctrl-C и т.д.), чтобы терминал не сломался
static void on_signal(int sig)
{
    (void)sig;
    restore_tty();                      // Обязательно чиним терминал
    _exit(1);                           // Выходим
}

// Вспомогательная функция для вывода одного символа
static void put(char c)
{
    write(STDOUT_FILENO, &c, 1);
}

// Функция звукового сигнала
static void beep(void)
{
    put(BELL);                          // Отправляем звук CTRL-G
    write(STDOUT_FILENO, "<BELL>", 6);  // Визуальная метка (чтобы было видно, что программа среагировала)
}

// Стирание n символов с экрана
static void erase_screen(int n)
{
    int i;
    for (i = 0; i < n; i++)
        write(STDOUT_FILENO, "\b \b", 3); // Шаг назад, пробел (затирает символ), снова шаг назад
}

// Переход на новую строку
static void new_line(void)
{
    write(STDOUT_FILENO, "\r\n", 2);    // Возврат каретки и перевод строки
    len = 0;                            // Сбрасываем длину
}

// Чтение символа с таймаутом (используется для стрелок)
static int read_timeout(unsigned char *c, int ms)
{
    struct pollfd pfd;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, ms) <= 0)         // Ждем ms миллисекунд. Если ничего нет - выходим
        return 0;
    return read(STDIN_FILENO, c, 1) == 1; // Читаем ровно 1 байт
}

// Пропуск управляющих последовательностей (например, от стрелок)
static void skip_escape(void)
{
    unsigned char c;

    if (!read_timeout(&c, 50))          // Ждем следующий символ 50 мс
        return;
    if (c == '[') {                     // Если это начало последовательности стрелки
        while (read_timeout(&c, 50)) {  // Читаем до конца последовательности
            if (c >= 0x40 && c <= 0x7E)
                break;                  // Конец последовательности
        }
    } else if (c == 'O') {
        read_timeout(&c, 50);
    }
}

// Добавление символа в строку (с проверкой на 40 символов)
static void add_char(char c)
{
    if (len == MAXCOL) {                // Если строка уже 40 символов
        if (c == ' ') {
            new_line();                 // Если пробел - просто переносим
            return;
        }

        int start = len;
        while (start > 0 && line[start - 1] != ' ') // Ищем начало последнего слова
            start--;

        if (start == 0) {               // Если это одно длинное слово
            new_line();                 // Переносим всё на новую строку
        } else {                        // Если слово не первое
            char word[MAXCOL + 1];
            int wlen = len - start;

            memcpy(word, line + start, wlen); // Запоминаем последнее слово
            erase_screen(wlen);               // Стираем его с экрана
            new_line();                       // Переходим на новую строку
            memcpy(line, word, wlen);         // Записываем слово в начало новой строки
            len = wlen;
            write(STDOUT_FILENO, line, len);  // Печатаем слово на новой строке
        }
    }

    line[len++] = c;                    // Добавляем символ в буфер
    put(c);                             // И печатаем его на экран
}

int main(void)
{
    struct termios tio;
    unsigned char c;
    ssize_t n;
    unsigned char erase_ch, kill_ch;

    if (!isatty(STDIN_FILENO)) {        // Проверяем, что ввод идет с терминала, а не из файла
        fprintf(stderr, "stdin должен быть терминалом\n");
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &saved_tio) == -1) { // Сохраняем настройки терминала
        perror("tcgetattr");
        return 1;
    }
    tio_saved = 1;
    atexit(restore_tty);                // Регистрируем функцию восстановления при выходе
    signal(SIGINT,  on_signal);         // Ловим Ctrl-C, чтобы починить терминал
    signal(SIGQUIT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP,  on_signal);

    erase_ch = saved_tio.c_cc[VERASE];  // Узнаем, какая клавиша настроена на стирание (обычно Backspace)
    kill_ch  = saved_tio.c_cc[VKILL];   // Узнаем, какая клавиша настроена на удаление строки (обычно Ctrl-U)

    tio = saved_tio;
    tio.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // Отключаем эхо, ожидание Enter, Ctrl-C и спецсимволы
    tio.c_iflag &= ~(IXON | IXOFF);     // Отключаем управление потоком (чтобы Ctrl-S/Ctrl-Q не мешали)
    tio.c_cc[VMIN]  = 1;                // Ждать хотя бы 1 символ
    tio.c_cc[VTIME] = 0;                // Без таймаута
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio) == -1) { // Применяем новые настройки
        perror("tcsetattr");
        return 1;
    }

    while ((n = read(STDIN_FILENO, &c, 1)) == 1) { // Читаем по одному байту
        if (c == CTRL_D) {              // Если нажат Ctrl-D
            if (len == 0)               // И строка пуста
                break;                  // Завершаем программу
            beep();                     // Если не пуста - пищим (по условию)
        } else if (c == erase_ch) {     // Если нажат Backspace
            if (len > 0) {
                erase_screen(1);        // Стираем символ с экрана
                len--;                  // Уменьшаем длину
            }
        } else if (c == kill_ch) {      // Если нажат Ctrl-U (KILL)
            erase_screen(len);          // Стираем всю строку
            len = 0;
        } else if (c == CTRL_W) {       // Если нажат Ctrl-W
            int k = len;
            while (k > 0 && line[k - 1] == ' ') // Ищем пробелы
                k--;
            while (k > 0 && line[k - 1] != ' ') // Ищем начало слова
                k--;
            erase_screen(len - k);      // Стираем слово с экрана
            len = k;
        } else if (c == 0x1B) {         // Если нажат Escape (начало стрелок)
            beep();                     // Пищим (по условию)
            skip_escape();              // И пропускаем остаток кода стрелки
        } else if (c == '\n' || c == '\r') { // Если нажат Enter
            new_line();                 // Переходим на новую строку
        } else if (isprint(c)) {        // Если нажата обычная буква/цифра
            add_char((char)c);          // Добавляем её
        } else {                        // Если нажато что-то непонятное
            beep();                     // Пищим
        }
    }

    write(STDOUT_FILENO, "\r\n", 2);    // Перед выходом переводим строку
    return 0;
}
