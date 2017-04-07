/*-------------------------------------------------------------------------
    > File Name :	 hello.c
    > Author :		shang
    > Mail :		shangshipei@gmail.com 
    > Description :	shang 
    > Created Time :	2017年04月07日 星期五 10时28分51秒
    > Rev :		0.1
 ------------------------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");

static int hello_init(void)
{
	printk(KERN_ALERT "Hello. world\n");
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(hello_init);
module_exit(hello_exit);
