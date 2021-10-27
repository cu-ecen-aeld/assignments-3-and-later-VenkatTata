/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h> 
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Venkat Tata"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev* char_dev_ptr;
	PDEBUG("open");
	//Opening handle
	char_dev_ptr= container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data= char_dev_ptr;
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	//Since not a driver for real hardware, release need not be handled
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	//Reference: https://github.com/cu-ecen-aeld/ldd3/blob/master/scull/main.c (line: 298)
	struct aesd_dev *dev = filp->private_data; 
	size_t bytes_read=0;
	struct aesd_buffer_entry* cb_entry = NULL;
	size_t entry_offset_byte_rtn=0;			
	size_t total_bytes_available_read=0;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	
	//Interruptible mutex lock 
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
		
	cb_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset_byte_rtn);
	if(cb_entry == NULL)
	{
		//retval already set to 0
		goto finish;
	}
	//Check if a complete or a partial read 
	//Partial read when number of bytes requested to be read from the offset is less than the bytes available to be read from the offset to end of entry
	total_bytes_available_read = cb_entry->size - entry_offset_byte_rtn;
	if(	count< total_bytes_available_read)
	{
		bytes_read=count;
	}
	else
	{
		bytes_read=total_bytes_available_read;
	}
	
	//Copy to userspace
	if (copy_to_user(buf , (cb_entry->buffptr + entry_offset_byte_rtn), bytes_read))
	{
		retval = -EFAULT;
		goto finish;
	}
	
	//If copy to user successful, update the file pointer
	*f_pos += bytes_read;
	
	//Update return value with the partial read or count 
	retval = bytes_read;
	
	finish:
			mutex_unlock(&dev->lock);
			return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval ;
	size_t not_copied_bytes ;
	struct aesd_dev* my_dev = NULL;
	const char* discarded_entry = NULL;
	//char* entry_complete = NULL;
	
	
	my_dev = (filp->private_data);

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

	//Interruptible mutex lock 
	if (mutex_lock_interruptible(&my_dev->lock))
	{	
		retval= -ERESTARTSYS;
		goto done;
	}
	
	//Allocates count bytes of memory always, resizes accordingly as required
	my_dev->write_entry.buffptr = krealloc(my_dev->write_entry.buffptr, count,GFP_KERNEL);
	if (my_dev->write_entry.buffptr == NULL)
	{ 
		retval = -ENOMEM;
		goto cleanup;
	}

	//Access circular buffer from user space in the efficient way without creating entry points
	//Number of bytes could not be copied from userspace buffer to kernel space 
	not_copied_bytes = copy_from_user((void *)(&my_dev->write_entry.buffptr[my_dev->write_entry.size]), buf, count);
	
	//Returns the number of bytes successfully copied
	retval=count;
	
	//if partial copy, update the return value with number of bytes only copied
	if(not_copied_bytes)
	{
		retval -= not_copied_bytes;
	}
	my_dev->write_entry.size += (count - not_copied_bytes);
	
	
	//Check if the complete entry was read which is found by checking if it contained new line character
	//If found, add to circular buffer
	if (strchr((char *)(my_dev->write_entry.buffptr), '\n')) 
	{
		// Add entry to queue, free oldest entry if full
		discarded_entry = aesd_circular_buffer_add_entry(&my_dev->buffer, &my_dev->write_entry);
		//Legal to pass NULL to kfree
		//If no overwrite, NULL returned. If overwritten, discarded entry is freed
        kfree(discarded_entry);
        my_dev->write_entry.buffptr = 0;
		my_dev->write_entry.size = 0;
	}

  cleanup:
	mutex_unlock(&my_dev->lock);
  done:
	return retval;
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));
	mutex_init(&aesd_device.lock);
	/**
	 * TODO: initialize the AESD specific portion of the device
	 */

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	aesd_circular_buffer_deallocate(&aesd_device.buffer);

	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
