
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/task32_vkhokhlova.sock"
#define BUFFER_SIZE 1024

static struct termios old_terminal;
static int terminal_changed = 0;
static volatile sig_atomic_t interrupted = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static void restore_terminal(void)
{
    if (terminal_changed) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);
        terminal_changed = 0;
    }
}

static int set_terminal_mode(void)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        return 0;
    }

    if (tcgetattr(STDIN_FILENO, &old_terminal) == -1) {
        perror("tcgetattr");
        return -1;
    }

    raw = old_terminal;

    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | IXOFF | ICRNL | INLCR | IGNCR);

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
        perror("tcsetattr");
        return -1;
    }

    terminal_changed = 1;
    return 0;
}

static int send_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = write(fd, data + sent, length - sent);

        if (count > 0) {
            sent += (size_t)count;
        } else if (count == -1 && errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }

    return 0;
}

static void discard_escape_sequence(void)
{
    struct pollfd input;
    unsigned char byte;

    input.fd = STDIN_FILENO;
    input.events = POLLIN;
    input.revents = 0;

    if (poll(&input, 1, 30) <= 0) {
        return;
    }

    if (read(STDIN_FILENO, &byte, 1) != 1) {
        return;
    }

    if (byte != '[' && byte != 'O') {
        return;
    }

    while (poll(&input, 1, 30) > 0) {
        if (read(STDIN_FILENO, &byte, 1) != 1) {
            break;
        }

        if (byte >= 0x40 && byte <= 0x7E) {
            break;
        }
    }
}

int main(void)
{
    int socket_fd;
    struct sockaddr_un address;
    struct sigaction action;
    char line[BUFFER_SIZE];
    size_t line_length = 0;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    signal(SIGPIPE, SIG_IGN);

    atexit(restore_terminal);

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH,
            sizeof(address.sun_path) - 1);

    if (connect(socket_fd, (struct sockaddr *)&address,
                sizeof(address)) == -1) {
        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    if (set_terminal_mode() == -1) {
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Connected. Ctrl+D exits client.\n");
    fflush(stdout);

    while (!interrupted) {
        unsigned char byte;
        ssize_t count = read(STDIN_FILENO, &byte, 1);

        if (count == -1 && errno == EINTR) {
            continue;
        }

        if (count <= 0) {
            break;
        }

        if (byte == 0x04) {
            break;
        }

        if (byte == 0x03) {
            continue;
        }

        if (byte == 0x1B) {
            discard_escape_sequence();
            continue;
        }

        if (byte == 0x7F || byte == 0x08) {
            if (line_length > 0) {
                line_length--;
                write(STDOUT_FILENO, "\b \b", 3);
            }

            continue;
        }

        if (byte == '\r' || byte == '\n') {
            line[line_length++] = '\n';

            if (send_all(socket_fd, line, line_length) == -1) {
                perror("send");
                break;
            }

            write(STDOUT_FILENO, "\n", 1);
            line_length = 0;
            continue;
        }

        if (byte >= 0x20 && byte <= 0x7E &&
            line_length < BUFFER_SIZE - 1) {
            line[line_length++] = (char)byte;
            write(STDOUT_FILENO, &byte, 1);
        }
    }

    restore_terminal();
    shutdown(socket_fd, SHUT_WR);
    close(socket_fd);
    printf("Client leave.\n");
    return EXIT_SUCCESS;
}

