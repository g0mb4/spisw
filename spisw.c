/* spisw.c - SPI software driver (bitbang)
 *
 * usage:
 *		make
 *		sudo insmod spisw.ko 	// insert module
 *		lsmod				// list modules
 *		modinfo spisw.ko 		// info
 * 		sudo rmmod spisw.ko  	// unload module
 * 		dmesg				// view printk()
 *
 * 2015, gmb */

/* standard libs cannot be used */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/delay.h>

#include "spisw.h"

#define DEVICE_NAME		"spisw" 	/* the device will appear at /dev/spisw using this value */
#define CLASS_NAME		"spiswcd"

#define DEBUG	1

MODULE_LICENSE("GPL");					/* affects runtime behavior */
MODULE_AUTHOR("gmb");					/* visible when modinfo is used */
MODULE_DESCRIPTION("SPI software driver"); 		/* visible when modinfo is used */
MODULE_VERSION("1.0"); 					/* visible when modinfo is used */

static int 			major_number;
static struct class* 		hwcd_class = NULL;
static struct device* 		hwcd_device = NULL;
static unsigned int		gpio_clck;
static unsigned int		gpio_mosi;
static unsigned int		gpio_miso;

static DEFINE_MUTEX(hwcd_mutex);	/* declare new mutex with value 1 (unlocked) */

/* the prototype functions for the character driver */
static int 		dev_open(struct inode *, struct file *);
static int 		dev_release(struct inode *, struct file *);
static ssize_t		dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t 		dev_write(struct file *, const char *, size_t, loff_t *);
static long		dev_ioctl(struct file *, unsigned int, unsigned long);

/* devices are represented as file structure in the kernel */
static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.unlocked_ioctl = dev_ioctl,
	.release = dev_release,		/* close()*/
};

/* LKM (Linux Kernel Module) initialization */
static int __init
spisw_init(void){

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] init() started\n");
#endif

	/* try to dynamically allocate a major number for the device */
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0){
		printk(KERN_ALERT "[spisw] failed to register a major number\n");
		return major_number;
	}

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] registered correctly with major number %d\n", major_number);
#endif

	/* register the device class */
	hwcd_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(hwcd_class)){
		printk(KERN_ALERT "[spisw] failed to register device class\n");
		goto err_class;
	}

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] device class registered correctly\n");
#endif

	/* register the device driver */
	hwcd_device = device_create(hwcd_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(hwcd_device)){
		printk(KERN_ALERT "[spisw] failed to create the device\n");
		goto err_dev;
	}

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] device class created correctly\n");
#endif

	mutex_init(&hwcd_mutex);	/* initialize the mutex dynamically */

	return 0;

err_dev:
	class_destroy(hwcd_class);
err_class:
	unregister_chrdev(major_number, DEVICE_NAME);
	return PTR_ERR(hwcd_class);          	/* correct way to return an error on a pointer */
}

/* LKM cleanup */
static void __exit
spisw_exit(void){
	device_destroy(hwcd_class, MKDEV(major_number, 0)); 	/* remove the device */
	class_unregister(hwcd_class);                          	/* unregister the device class */
	class_destroy(hwcd_class);                             	/* remove the device class */
	unregister_chrdev(major_number, DEVICE_NAME);          	/* unregister the major number */
	mutex_destroy(&hwcd_mutex);				/* destroy the mutex */

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] exited");
#endif
}

/* this function is called when the device is opened */
static int
dev_open(struct inode *inodep, struct file *filep){
	/*  try to acquire the mutex */
	if(!mutex_trylock(&hwcd_mutex)){
		printk(KERN_ALERT "[spisw] device in use by another process");
		return -EBUSY;
	}

	printk(KERN_INFO "[spisw] device is opened\n");
	return 0;
}

/* this function is called when the device is closed/released */
static int
dev_release(struct inode *inodep, struct file *filep){

	gpio_set_value(gpio_clck, SPISW_LOW);
	gpio_set_value(gpio_mosi, SPISW_LOW);

	gpio_free(gpio_clck);
	gpio_free(gpio_mosi);
	gpio_free(gpio_miso);

	mutex_unlock(&hwcd_mutex); 	/* unlock the mutex */

   	printk(KERN_INFO "[spisw] device is closed\n");
   	return 0;
}

unsigned char
read(void){
	unsigned char r = 0x00, i;

	for(i = 0; i < 8; i++){
		gpio_set_value(gpio_clck, SPISW_LOW);
		gpio_set_value(gpio_clck, SPISW_HIGH);
		r <<= 1;

		if(gpio_get_value(gpio_miso) != 0)
			r |= 0x1;
	}

#ifdef	DEBUG
	printk(KERN_INFO "[spisw] read byte : %#02x\n", r);
#endif

	return r;
}

/* this function is called whenever the device is being read from user space */
static ssize_t
dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
	int error_count = 0;

	unsigned char val = read();

	error_count = copy_to_user(buffer, &val, 1);

	if(error_count != 0){
		printk(KERN_INFO "[spisw] failed to send data to the user\n");
		return -EFAULT;              	/* return a bad address message */
	}

	return 1;
}

void
write(unsigned char byte){
	unsigned char bit;

	for(bit = 0x80; bit; bit >>= 1) {
	    if(byte & bit) {
	    	gpio_set_value(gpio_mosi, SPISW_HIGH);
	    } else {
	    	gpio_set_value(gpio_mosi, SPISW_LOW);
	    }

	    gpio_set_value(gpio_clck, SPISW_HIGH);
	    gpio_set_value(gpio_clck, SPISW_LOW);
	  }

#ifdef	DEBUG
      //printk(KERN_INFO "[spisw] written byte : %#02x\n", byte);
#endif
}

/* this function is called whenever the device is being written to from user space */
static ssize_t
dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){

	write((unsigned char) len);

	return 1;
}

static long
dev_ioctl(struct file *file, unsigned int ioctl_cmd, unsigned long ioctl_param){

	switch (ioctl_cmd) {
	case SPISW_SET_CLCK:
		/* not implemented */
		break;

	case SPISW_SET_MOSI:
		/* not implemented */
		break;

	case SPISW_SET_MISO:
		/* not implemented */
		break;

	case SPISW_W_BYTE:
		write((unsigned char) ioctl_param);
		return 0;

	case SPISW_R_BYTE:
		return ((long) read());

	case SPISW_INIT:
		/* arduino board pin 13 */
		gpio_clck = 40;

		if(gpio_is_valid(gpio_clck) == 0){
			printk(KERN_ALERT "[spisw] pin (%d) is not valid\n", gpio_clck);
			return -1;
		}


		if(gpio_request(gpio_clck, "spisw_clck") != 0){
			printk(KERN_ALERT "[spisw] unable to request clck pin (%d)\n", gpio_clck);
			return -1;
		}

		if(gpio_direction_output(gpio_clck, SPISW_LOW) != 0){
			printk(KERN_ALERT "[spisw] unable to set the direction to output in the clck pin\n");
			return -1;
		}

#ifdef	DEBUG
		printk(KERN_INFO "[spisw] clck pin (%d) is working as an output\n", gpio_clck);
#endif

		/* arduino board pin 11 */
		gpio_mosi = 43;

		if(gpio_is_valid(gpio_mosi) == 0){
			printk(KERN_ALERT "[spisw] pin (%d) is not valid\n", gpio_mosi);
			return -1;
		}


		if(gpio_request(gpio_mosi, "spisw_mosi") != 0){
			printk(KERN_ALERT "[spisw] unable to request mosi pin (%d)\n", gpio_mosi);
			return -1;
		}

		if(gpio_direction_output(gpio_mosi, SPISW_LOW) != 0){
			printk(KERN_ALERT "[spisw] unable to set the direction to output in the mosi pin\n");
			return -1;
		}

#ifdef	DEBUG
		printk(KERN_INFO "[spisw] mosi pin (%d) is working as an output\n", gpio_mosi);
#endif

		/* arduino board pin 12 */
		gpio_miso = 42;

		if(gpio_is_valid(gpio_miso) == 0){
			printk(KERN_ALERT "[spisw] pin (%d) is not valid\n", gpio_miso);
			return -1;
		}

		if(gpio_request(gpio_miso, "spisw_miso") != 0){
			printk(KERN_ALERT "[spisw] unable to request miso pin (%d)\n", gpio_miso);
			return -1;
		}

		if(gpio_direction_input(gpio_miso) != 0){
			printk(KERN_ALERT "[spisw] unable to set the direction to input in the miso pin\n");
			return -1;
		}

#ifdef	DEBUG
		printk(KERN_INFO "[spisw] miso pin (%d) is working as an input\n", gpio_miso);
#endif

		gpio_set_value(gpio_clck, SPISW_LOW);
		gpio_set_value(gpio_mosi, SPISW_LOW);

		return 0;
		break;

	default:
		printk(KERN_ALERT "[spisw] unsupported ioctl() command : %d\n", (int) ioctl_param);
		return -1;
	}

	return 0;
}

/* registers the init and exit function for the LKM */
module_init(spisw_init);
module_exit(spisw_exit);
