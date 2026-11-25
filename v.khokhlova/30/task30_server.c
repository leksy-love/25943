#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/task30_vkhokhlova.sock"
#define BUFFER_SIZE 1024

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
    int client_fd;
    struct sockaddr_un address;
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    unlink(SOCKET_PATH);

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 1) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Server is working, waiting for a client...\n");
    fflush(stdout);

    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    while (1) {
        ssize_t count = read(client_fd, buffer, sizeof(buffer));

        if (count > 0) {
            for (ssize_t i = 0; i < count; i++) {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            if (write_all(STDOUT_FILENO, buffer, (size_t)count) == -1) {
                perror("write");
                break;
            }
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            perror("read");
            break;
        }
    }

    // Отключение клиента завершает работу сервера.
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("Server is closed.\n");
    return EXIT_SUCCESS;
}

