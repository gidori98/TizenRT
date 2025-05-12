#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <tinyara/gidori.h>
#include <sys/ioctl.h>
#include <tinyara/prodconfig.h>
#include <tinyara/fs/ioctl.h>

int utils_gidori(int argc, char **args)
{
    char buf[BUFFER_SIZE];

    int fd = open("/dev/gidori", O_RDWR);
    
    ioctl(fd, GIDORI_WRITE, (unsigned long)"babo");
    ioctl(fd, GIDORI_READ, (unsigned long)buf);

    printf("buf : %s \n", (char *)buf);

	close(fd);

    return OK;
}