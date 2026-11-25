#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/task32_vkhokhlova.sock"
#define PID_FILE ".task32_server.pid"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 64

static volatile sig_atomic_t io_event = 0;
static volatile sig_atomic_t stop_server = 0;

static void handle_io(int signal_number)
{
    (void)signal_number;
    io_event = 1;
}

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop_server = 1;
}

static int write_all(int fd, const char *data, size_t length)
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

static int make_async(int fd)
{
    int flags;

    if (fcntl(fd, F_SETOWN, getpid()) == -1) {
        perror("fcntl F_SETOWN");
        return -1;
    }

    flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | FNDELAY | FASYNC) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];
    struct sockaddr_un address;
    struct sigaction action;
    sigset_t io_set;
    sigset_t old_set;
    FILE *pid_file;
    char buffer[BUFFER_SIZE];
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1;
    }

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = handle_io;

    if (sigaction(SIGIO, &action, NULL) == -1) {
        perror("sigaction SIGIO");
        return EXIT_FAILURE;
    }

    action.sa_handler = handle_stop;

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    sigemptyset(&io_set);
    sigaddset(&io_set, SIGIO);

    if (sigprocmask(SIG_BLOCK, &io_set, &old_set) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    unlink(SOCKET_PATH);

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH,
            sizeof(address.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    if (make_async(server_fd) == -1) {
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    pid_file = fopen(PID_FILE, "w");

    if (pid_file == NULL) {
        perror("fopen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    fprintf(pid_file, "%ld\n", (long)getpid());
    fclose(pid_file);

    printf("Server is working, waiting for clients. Make stop to stop server\n");
    fflush(stdout);

    while (!stop_server) {
        while (!io_event && !stop_server) {
            sigsuspend(&old_set);
        }

        io_event = 0;

        if (stop_server) {
            break;
        }

        while (1) {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK ||
                    errno == EINTR) {
                    break;
                }

                perror("accept");
                break;
            }

            if (make_async(client_fd) == -1) {
                close(client_fd);
                continue;
            }

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == -1) {
                    clients[i] = client_fd;
                    break;
                }
            }

            if (i == MAX_CLIENTS) {
                close(client_fd);
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == -1) {
                continue;
            }

            while (1) {
                ssize_t count = read(clients[i], buffer,
                                     sizeof(buffer));

                if (count > 0) {
                    ssize_t j;

                    for (j = 0; j < count; j++) {
                        buffer[j] = (char)toupper(
                            (unsigned char)buffer[j]);
                    }

                    if (write_all(STDOUT_FILENO, buffer,
                                  (size_t)count) == -1) {
                        perror("write");
                    }
                } else if (count == 0) {
                    close(clients[i]);
                    clients[i] = -1;
                    break;
                } else if (errno == EAGAIN ||
                           errno == EWOULDBLOCK) {
                    break;
                } else if (errno == EINTR) {
                    if (stop_server) {
                        break;
                    }
                    continue;
                } else {
                    perror("read");
                    close(clients[i]);
                    clients[i] = -1;
                    break;
                }
            }
        }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            close(clients[i]);
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    unlink(PID_FILE);

    printf("Server closed.\n");
    fflush(stdout);

    return EXIT_SUCCESS;
}

