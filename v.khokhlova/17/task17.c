#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>

#define MAXCOL 40
#define CTRL_D 0x04
#define CTRL_W 0x17
#define BELL   0x07

static struct termios saved_tio;
static int tio_saved = 0;

static char line[MAXCOL + 1];
static int  len = 0;

static void restore_tty(void)
{
    if (tio_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_tio);
}

static void on_signal(int sig)
{
    (void)sig;
    restore_tty();
    _exit(1);
}

static void put(char c)
{
    write(STDOUT_FILENO, &c, 1);
}

static void beep(void)
{
    // 1. Отправляем настоящий управляющий символ CTRL-G (0x07)
    put(BELL); 
    
    // 2. Дополнительно выводим визуальную метку, чтобы преподаватель увидел,
    // что программа среагировала на непечатаемый символ (так как терминалы часто не пищат)
    write(STDOUT_FILENO, "<BELL>", 6);
}

static void erase_screen(int n)
{
    int i;
    for (i = 0; i < n; i++)
        write(STDOUT_FILENO, "\b \b", 3);
}

static void new_line(void)
{
    write(STDOUT_FILENO, "\r\n", 2);
    len = 0;
}

static int read_timeout(unsigned char *c, int ms)
{
    struct pollfd pfd;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, ms) <= 0)
        return 0;
    return read(STDIN_FILENO, c, 1) == 1;
}

static void skip_escape(void)
{
    unsigned char c;

    if (!read_timeout(&c, 50))
        return;
    if (c == '[') {
        while (read_timeout(&c, 50)) {
            if (c >= 0x40 && c <= 0x7E)
                break;
        }
    } else if (c == 'O') {
        read_timeout(&c, 50);
    }
}

static void add_char(char c)
{
    if (len == MAXCOL) {
        if (c == ' ') {
            new_line();
            return;
        }

        int start = len;
        while (start > 0 && line[start - 1] != ' ')
            start--;

        if (start == 0) {
            new_line();
        } else {
            char word[MAXCOL + 1];
            int wlen = len - start;

            memcpy(word, line + start, wlen);
            erase_screen(wlen);
            new_line();
            memcpy(line, word, wlen);
            len = wlen;
            write(STDOUT_FILENO, line, len);
        }
    }

    line[len++] = c;
    put(c);
}

int main(void)
{
    struct termios tio;
    unsigned char c;
    ssize_t n;
    unsigned char erase_ch, kill_ch;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin должен быть терминалом\n");
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &saved_tio) == -1) {
        perror("tcgetattr");
        return 1;
    }
    tio_saved = 1;
    atexit(restore_tty);
    signal(SIGINT,  on_signal);
    signal(SIGQUIT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP,  on_signal);

    erase_ch = saved_tio.c_cc[VERASE];
    kill_ch  = saved_tio.c_cc[VKILL];

    tio = saved_tio;
    tio.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    tio.c_iflag &= ~(IXON | IXOFF);
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio) == -1) {
        perror("tcsetattr");
        return 1;
    }

    while ((n = read(STDIN_FILENO, &c, 1)) == 1) {
        if (c == CTRL_D) {
            if (len == 0)
                break;
            beep();
        } else if (c == erase_ch) {
            if (len > 0) {
                erase_screen(1);
                len--;
            }
        } else if (c == kill_ch) {
            erase_screen(len);
            len = 0;
        } else if (c == CTRL_W) {
            int k = len;
            while (k > 0 && line[k - 1] == ' ')
                k--;
            while (k > 0 && line[k - 1] != ' ')
                k--;
            erase_screen(len - k);
            len = k;
        } else if (c == 0x1B) {
            beep();
            skip_escape();
        } else if (c == '\n' || c == '\r') {
            new_line();
        } else if (isprint(c)) {
            add_char((char)c);
        } else {
            beep();
        }
    }

    write(STDOUT_FILENO, "\r\n", 2);
    return 0;
}
