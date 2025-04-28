#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define IOCTL_SET_STACK_SIZE _IOW('s', 1, int)

#define DEVICE "/dev/int_stack"

void usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s set-size N\n", prog);
    printf("  %s push N\n", prog);
    printf("  %s pop\n", prog);
    printf("  %s unwind\n", prog);
}

int main(int argc, char *argv[]) {
    int fd, value, ret;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (strcmp(argv[1], "set-size") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }
        int size = atoi(argv[2]);
        if (size <= 0) {
            printf("ERROR: size should be > 0\n");
            close(fd);
            return 1;
        }
        if (ioctl(fd, IOCTL_SET_STACK_SIZE, &size) == -1) {
            perror("ioctl");
            close(fd);
            return 1;
        }
    } else if (strcmp(argv[1], "push") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return 1;
        }
        value = atoi(argv[2]);
        ret = write(fd, &value, sizeof(int));
        if (ret < 0) {
            if (errno == ERANGE) {
                printf("ERROR: stack is full\n");
                close(fd);
                exit(ERANGE);
            } else {
                perror("write");
                close(fd);
                return 1;
            }
        }
    } else if (strcmp(argv[1], "pop") == 0) {
        int res;
        ret = read(fd, &res, sizeof(int));
        if (ret == 0) {
            printf("NULL\n");
        } else if (ret < 0) {
            perror("read");
            close(fd);
            return 1;
        } else {
            printf("%d\n", res);
        }
    } else if (strcmp(argv[1], "unwind") == 0) {
        int res;
        while ((ret = read(fd, &res, sizeof(int))) > 0) {
            printf("%d\n", res);
        }
    } else {
        usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
