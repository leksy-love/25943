#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/task31_vkhokhlova.sock"
#define PID_FILE ".task31_server.pid"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 64

static volatile sig_atomic_t stop_server = 0;

static void handle_signal(int signal_number)
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

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];
    struct sockaddr_un address;
    struct sigaction action;
    struct pollfd fds[MAX_CLIENTS + 1];
    char buffer[BUFFER_SIZE];
    FILE *pid_file;
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

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
        int result;

        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        for (i = 0; i < MAX_CLIENTS; i++) {
            fds[i + 1].fd = clients[i];
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }

        result = poll(fds, MAX_CLIENTS + 1, -1);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            break;
        }

        if (stop_server) {
            break;
        }

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd != -1) {
                int added = 0;

                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] == -1) {
                        clients[i] = client_fd;
                        added = 1;
                        break;
                    }
                }

                if (!added) {
                    close(client_fd);
                }
            } else if (errno != EINTR) {
                perror("accept");
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == -1) {
                continue;
            }

            if (fds[i + 1].revents &
                (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
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
                } else if (errno != EINTR) {
                    perror("read");
                    close(clients[i]);
                    clients[i] = -1;
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

