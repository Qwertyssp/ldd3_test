/*-------------------------------------------------------------------------
    > File Name :	 main.c
    > Author :		shang
    > Mail :		shangshipei@gmail.com 
    > Description :	shang 
    > Created Time :	2017年05月18日 星期四 17时45分07秒
    > Rev :		0.1
 ------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define GET_MAGIC	'k'
#define SCULL_IOCGQUANTUM _IOR(GET_MAGIC, 5, int)


const char *dev = "/dev/scull1";

int main(void)
{
	int fd; 
	int ret ;
	int quantum = 0;

	fd = open(dev, O_RDWR);
	if (fd < 0) 
		printf("open error !\n");

	ret = ioctl(fd, SCULL_IOCGQUANTUM, &quantum);

	printf("ret is %d, %d\n", ret , quantum);

	close(fd);

	return 0;
}
