#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>

static volatile sig_atomic_t beep_count = 0;
static struct termios saved_tio;

// Восстановление терминала при выходе
static void restore_tty(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_tio);
}

// Функция писка
void do_beep() {
    write(STDOUT_FILENO, "\a", 1);           // CTRL-G (звук)
    write(STDOUT_FILENO, "[BEEP]\n", 7);     // Визуальная метка
    beep_count++;
}

int main() {
    struct termios tio;
    unsigned char c;

    if (tcgetattr(STDIN_FILENO, &saved_tio) == -1) {
        perror("tcgetattr");
        return 1;
    }
    atexit(restore_tty);

    tio = saved_tio;
    // ЖЕСТКО ОТКЛЮЧАЕМ: 
    // ICANON (ждет Enter), ECHO (печатает ^C), ISIG (посылает сигнал), IEXTEN
    tio.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    tio.c_iflag &= ~(IXON | IXOFF);
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    printf("Press Ctrl-C to beep, Ctrl-\\ to quit.\n");
    fflush(stdout);

    // Читаем по одному байту
    while (read(STDIN_FILENO, &c, 1) == 1) {
        // 0x03 — это ASCII-код Ctrl+C
        if (c == 0x03) { 
            do_beep();
        }
        // 0x1C — это ASCII-код Ctrl+\ (SIGQUIT)
        else if (c == 0x1C) {
            printf("\nSIGQUIT received. Total beeps: %d\n", beep_count);
            break; // Выходим из цикла (atexit вернет терминал)
        }
    }

    return 0;
}
