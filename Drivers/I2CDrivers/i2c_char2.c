#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_data/serial-omap.h>
#include "i2c_char.h"

#define FIRST_MINOR 0
#define MINOR_CNT 1

static int my_open(struct inode *i, struct file *f)
{
	struct omap_i2c_dev *dev = container_of(i->i_cdev, struct omap_i2c_dev, cdev);
	f->private_data = dev;
	return 0;
}

static int my_close(struct inode *i, struct file *f)
{
	return 0;
}

static ssize_t my_read(struct file *f, char __user *buf, size_t count, loff_t *off)
{
	struct i2c_msg msg;
	char *tmp;
	struct omap_i2c_dev *dev = (struct omap_i2c_dev *)(f->private_data);
	int ret;
	ENTER();
	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;
	
	ret = omap_i2c_wait_for_bb(dev);
	if (ret < 0)
	{
		printk("Error ret = %d\n", ret);
		goto out;
	}

	printk("##### Invoking i2c_read #####\n");
	msg.addr = 0x50; //Slave Address;
	msg.flags = 0;
	msg.len = 32; //Number of bytes to read
	msg.buf = tmp;
	/* This is to cross verify the contents written to eeprom. The low level
 * driver has the hard coding where it always reads from eeprom offset 0x0060 */
	ret = i2c_read(dev, &msg, 1);

	printk("ret = %d\n", ret);

	omap_i2c_wait_for_bb(dev);
	if (ret >= 0)
		ret = copy_to_user(buf, tmp, count) ? -EFAULT : count;
	kfree(tmp);
out:
	return ret;
}

static ssize_t my_write(struct file *f, const char __user *buf, size_t count, loff_t *off)
{
	struct omap_i2c_dev *dev = (struct omap_i2c_dev *)(f->private_data);
	int ret;

	printk("##### Invoking i2c_write #####\n");
	/* Ignoring user space data as the things are hard-coded in low level
 	 * driver */
	ret = i2c_write(dev, NULL, 1);
	return (ret == 0 ? count : ret);
}

static struct file_operations driver_fops =
{
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.read = my_read,
	.write = my_write
};

int fcd_init(struct omap_i2c_dev *i2c_dev)
{
	static int i2c_num = 0;
	int init_result = alloc_chrdev_region(&i2c_dev->devt, 0, 1, "i2c_drv");

	if (0 > init_result)
	{
		printk(KERN_ALERT "Device Registration failed\n");
		return -1;
	}
	printk("Major Nr: %d\n", MAJOR(i2c_dev->devt));

	if (device_create(i2c_dev->i2c_class, NULL, i2c_dev->devt, NULL, "i2c_drv%d", i2c_num++) == NULL)
	{
		printk( KERN_ALERT "Device creation failed\n" );
		unregister_chrdev_region(i2c_dev->devt, 1);
		return -1;
	}

	cdev_init(&i2c_dev->cdev, &driver_fops);

	if (cdev_add(&i2c_dev->cdev, i2c_dev->devt, 1) == -1)
	{
		printk( KERN_ALERT "Device addition failed\n" );
		device_destroy(i2c_dev->i2c_class, i2c_dev->devt);
		unregister_chrdev_region(i2c_dev->devt, 1 );
		return -1;
	}
	return 0;
}

void fcd_exit(struct omap_i2c_dev *i2c_dev)
{
	device_destroy(i2c_dev->i2c_class, i2c_dev->devt);
	cdev_del(&i2c_dev->cdev);
	unregister_chrdev_region(i2c_dev->devt, MINOR_CNT);
}

