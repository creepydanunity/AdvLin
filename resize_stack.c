#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>

#define IOCTL_SET_STACK_SIZE _IOW('s', 1, int)

int main(int argc, char *argv[]) {
    int new_size;

    if (argc != 2) {
        printf("Usage: %s <new_stack_size>\n", argv[0]);
	return 1;
    }

    new_size = atoi(argv[1]);
    if (new_size <= 0) {
        printf("Error: Stack size must be > 0");
	return 1;
    }

    int fd = open("/dev/int_stack", O_RDWR);
    if (fd < 0) {
        perror("open");
	return 1;
    }

    if (ioctl(fd, IOCTL_SET_STACK_SIZE, &new_size) == -1) {
	perror("ioctl");
	return 1;
    } else {
	printf("Stack resized to %d\n", new_size);
    }

    return 0;
}
