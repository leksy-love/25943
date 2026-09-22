#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    FILE *f;

    if (argc != 2) {
        fprintf(stderr, "Using: %s file \n", argv[0]);
        exit(1);
    }

    printf("Real UID = %d, Efficent UID = %d\n",
           getuid(), geteuid());

    f = fopen(argv[1], "r+");
    if (f == NULL)
        perror("fopen");
    else {
        printf("File open succsesfuly\n");
        fclose(f);
    }

    if (setuid(geteuid()) == -1)
        perror("setuid");

    printf("Real UID = %d, Efficent UID = %d\n",
           getuid(), geteuid());

    f = fopen(argv[1], "r+");
    if (f == NULL)
        perror("fopen");
    else {
        printf("File open succsesfuly\n");
        fclose(f);
    }

    return 0;
}
