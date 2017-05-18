/*-------------------------------------------------------------------------
    > File Name :	 scull.c
    > Author :		shang
    > Mail :		shangshipei@gmail.com 
    > Description :	shang 
    > Created Time :	2017年04月07日 星期五 16时45分48秒
    > Rev :		0.1
 ------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;

int scull_quantum = SCULL_QUANTUM;
int scull_qset 	 = SCULL_QSET;

MODULE_AUTHOR("shang");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	struct scull_qset *qs = dev->data;

	if (! qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct scull_qset));
	}

	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}

int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	int qset = dev->qset;
	int i;
	for (dptr = dev->data; dptr; dptr = next) 
	{
		if (dptr->data) {
			for ( i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;
	return 0;
}

int scull_read_procmem(char *buf , char **start, off_t offset, 
		int count, int *eof, void *data)
{
	int i, j , len = 0;
	int limit = count - 80;
	for (i = 0; i < scull_nr_devs && len <= limit; i++) {
		struct scull_dev *d = &scull_devices[i];
		struct scull_qset *qs = d->data;
		if (down_interruptible(&d->sem))
			return -ERESTARTSYS;
		len += sprintf(buf + len, "\nDevice %i: qset %i, q %i, sz %li\n", 
				i, d->qset, d->quantum, d->size);
		for (; qs && len <= limit; qs = qs->next) {
			len += sprintf(buf + len, "item at %p, qset at %p\n",
					qs, qs->data);
			if (qs->data && !qs->next) 
				for (j = 0; j < d->qset; j++) {
					if (qs->data[j])
						len += sprintf(buf + len, "	%4i: %8p\n",
								j , qs->data[j]);
				}
		}
		up(&scull_devices[i].sem);
	}
	*eof = 1;
	return len;
}

loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
	struct scull_dev *dev = filp->private_data;
	loff_t newpos;

	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  case 2: /* SEEK_END */
		newpos = dev->size + off;
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	PDEBUG("%s enter\n",  __FUNCTION__);
	int err = 0, tmp;
	int retval = 0;

	PDEBUG("%s enter cmd %d, %d\n",  __FUNCTION__, cmd, SCULL_IOCGQUANTUM);
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) 
		return -ENOTTY;

	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch(cmd) {
	case SCULL_IOCRESET:
		PDEBUG("%s, SCULL_IOCRESET\n", __FUNCTION__);
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		break;

	case SCULL_IOCSQUANTUM:
		PDEBUG("%s, SCULL_IOCSQUANTUM\n", __FUNCTION__);
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM:
		PDEBUG("%s, SCULL_IOCTQUANTUM\n", __FUNCTION__);
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM:
		PDEBUG("%s, SCULL_IOCGQUANTUM\n", __FUNCTION__);
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM:
		PDEBUG("%s, SCULL_IOCQQUANTUM\n", __FUNCTION__);
		return scull_quantum;

	case SCULL_IOCXQUANTUM:
		PDEBUG("%s, SCULL_IOCXQUANTUM\n", __FUNCTION__);
		if (!capable (CAP_SYS_ADMIN))
			return -EPERM;

		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM:
		PDEBUG("%s, SCULL_IOCHQUANTUM\n", __FUNCTION__);
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		tmp = scull_quantum;
		scull_quantum  = arg;
		return tmp;

	default:
		PDEBUG("%s, default\n", __FUNCTION__);
		return -ENOTTY;
	}

	PDEBUG("%s, out\n", __FUNCTION__);
	return retval;
}


int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev ;

	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY ) {
		scull_trim(dev);
	}
	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	return 0;
}


ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item , s_pos, q_pos , rest;

	PDEBUG("%s f_pos %d\n",  __FUNCTION__, *f_pos);
	ssize_t retval = 0;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	PDEBUG("%s dev->size %d, count %d, f_pos %d\n",__FUNCTION__, dev->size, count , *f_pos) ;
	if (*f_pos >= dev->size)
		goto out;

	if (* f_pos + count > dev->size)
		count = dev->size - *f_pos;

	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;

	s_pos = rest / quantum;
	q_pos = rest % quantum;

	PDEBUG("%s item %d, rest %d, s_pos %d, q_pos %d\n",__FUNCTION__,  item, rest, s_pos, q_pos);

	dptr = scull_follow(dev, item);

	if (dptr == NULL || !dptr->data || !dptr->data[s_pos])
		goto out;

	if (count > quantum - q_pos)
		count = quantum - q_pos;

	if (copy_to_user(buf , dptr->data[s_pos] + q_pos , count)) {
		retval = -EFAULT;
		goto out;
	}

	PDEBUG("%s count %d\n", __FUNCTION__, count);
	*f_pos += count;
	retval = count;
out:
	up(&dev->sem);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;

	int itemsize = quantum * qset;
	int item , s_pos, q_pos, rest;
	PDEBUG("%s, f_pos %d\n", __FUNCTION__,  *f_pos);
	ssize_t retval = -ENOMEM;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;

	s_pos = rest / quantum;
	q_pos = rest % quantum;

	PDEBUG("%s item %d, rest %d, s_pos %d, q_pos %d\n", __FUNCTION__,  item, rest, s_pos, q_pos);

	dptr = scull_follow(dev, item);
	if (dptr == NULL)
		goto out;
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dptr->data)
			goto out;
		memset(dptr->data, 0 , qset * sizeof(char *));
	}

	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dptr->data[s_pos])
			goto out;
	}

	if (count > quantum - q_pos)
		count = quantum - q_pos;
	if (copy_to_user(buf, dptr->data[s_pos] + q_pos,  count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
	PDEBUG("%s count %d\n", __FUNCTION__, count);
	if (dev->size < *f_pos)
		dev->size = *f_pos;

out:
	up(&dev->sem);
	return retval;
}


struct file_operations scull_fops = {
	.owner = THIS_MODULE,
	.llseek = scull_llseek,
	.read = scull_read,
	.write = scull_write,
	.unlocked_ioctl = scull_ioctl,
	.open = scull_open,
	.release = scull_release,
};

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

void scull_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scull_major , scull_minor);
	
	remove_proc_entry("scullmem", NULL);

	if (scull_devices) {
		for (i =0 ; i < scull_nr_devs; i++) {
			scull_trim(scull_devices + i);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}
	unregister_chrdev_region(devno, scull_nr_devs);
}


int scull_init_module(void)
{
	int result, i ; 
	dev_t dev = 0;

	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
		scull_major = MAJOR(dev);
	}

	if (result < 0) {
		printk(KERN_WARNING "scull: can;t get major %d\n", scull_major);
		return result;
	}

	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

        /* Initialize each device. */
	for (i = 0; i < scull_nr_devs; i++) {
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		sema_init(&scull_devices[i].sem, 1);
		scull_setup_cdev(&scull_devices[i], i);
	}
	create_proc_read_entry("scullmem", 0, NULL, scull_read_procmem, NULL);

	return 0;
fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
