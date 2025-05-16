#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <tinyara/gidori.h>
#include <sys/ioctl.h>
#include <tinyara/prodconfig.h>
#include <tinyara/fs/ioctl.h>
#include <tinyara/fs/fs_utils.h>



int utils_gidori(int argc, char **args)
{
    if ((argc != 2) || !strncmp(args[1], "--help", strlen("--help") + 1)) 
    {
        printf("\nUsage : gidori fs_type\n");
		printf("charactor device\t= 1\n");
        printf("block device\t\t= 2\n");
        printf("mnt block device\t= 3\n");
        return -1;
    }

    if (!strcmp(args[1], "1"))
    {
        printf("[TEST] charactor device\n");

        int fd;
        char buf[BUFFER_SIZE];

        fd = open("/dev/gidori", O_RDWR);
        
        ioctl(fd, GIDORI_WRITE, (unsigned long)"babo");
        ioctl(fd, GIDORI_READ, (unsigned long)buf);
        close(fd);

        printf("buf : %s \n", (char *)buf);
    }
    else if (!strcmp(args[1], "2"))
    {
        printf("[TEST] mtd block device\n");
        
        int fd;
        char buf[BUFFER_SIZE];

        fd = open("/dev/mtdblock9", O_RDWR);

    	lseek(fd, 0, SEEK_SET);
    	write(fd, "babo", 5);
    	lseek(fd, 0, SEEK_SET);
    	read(fd, buf, 5);
        close(fd);

        printf("buf : %s \n", buf);
    }
    else if (!strcmp(args[1], "3"))
    {
        printf("[TEST] mount point device\n");
		
        int fd;
        char buf[BUFFER_SIZE];

		fd = open("/mnt/smartfs/test.txt", O_WRONLY | O_CREAT, 0644);
        write(fd, "babo", 5);
        close(fd);
        
        fd = open("/mnt/smartfs/test.txt", O_RDONLY, 0644);
		read(fd, buf, 5);
        close(fd);
        		
		printf("buf : %s \n", buf);
    }
    else
    {
        printf("Invalid input");
        return 1;
    }


    return OK;
}
