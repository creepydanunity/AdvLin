#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "int_stack"
#define CLASS_NAME "int_stack_class"
#define IOCTL_SET_STACK_SIZE _IOW('s', 1, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d.vasilev@innopolis.university");
MODULE_DESCRIPTION("Chardev for a stack<int>");
MODULE_VERSION("1.0");

static int majorNumber;
static struct class* intstackClass = NULL;
static struct device* intstackDevice = NULL;
static struct cdev c_dev;

static int *stack = NULL;
static int max_size = 8;
static int top = -1;
static DEFINE_MUTEX(stack_mutex);


static int stack_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "int_stack: Device opened.\n");
    return 0;
}

static int stack_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "int_stack: Device closed.\n");
    return 0;
}

static ssize_t stack_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    int value;
    if (len != sizeof(int)) {
        printk(KERN_WARNING "int_stack: Invalid write size.\n");
        return -EINVAL;
    }
    if (copy_from_user(&value, buffer, sizeof(int))) {
        return -EFAULT;
    }

    mutex_lock(&stack_mutex);
    if (top >= max_size - 1) {
        mutex_unlock(&stack_mutex);
        printk(KERN_WARNING "int_stack: Stack full.\n");
        return -ERANGE;
    }

    stack[++top] = value;
    mutex_unlock(&stack_mutex);
    printk(KERN_INFO "int_stack: Pushed %d\n", value);
    return sizeof(int);
}

static ssize_t stack_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int value;
    if (len != sizeof(int)) {
        printk(KERN_WARNING "int_stack: Invalid read size.\n");
        return -EINVAL;
    }

    mutex_lock(&stack_mutex);
    if (top < 0) {
        mutex_unlock(&stack_mutex);
        printk(KERN_WARNING "int_stack: Stack empty.\n");
        return 0;
    }

    value = stack[top--];
    mutex_unlock(&stack_mutex);

    if (copy_to_user(buffer, &value, sizeof(int))) {
        return -EFAULT;
    }

    printk(KERN_INFO "int_stack: Popped %d\n", value);
    return sizeof(int);
}

static long stack_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    int new_size;
    int *new_stack;

    switch (cmd) {
    case IOCTL_SET_STACK_SIZE:
        if (copy_from_user(&new_size, (int __user *)arg, sizeof(int))) {
            return -EFAULT;
        }
        if (new_size <= 0) {
            printk(KERN_WARNING "int_stack: Invalid stack size.\n");
            return -EINVAL;
        }

        mutex_lock(&stack_mutex);
        new_stack = kmalloc(sizeof(int) * new_size, GFP_KERNEL);
        if (!new_stack) {
            mutex_unlock(&stack_mutex);
            return -ENOMEM;
        }

        int copy_count = (top + 1 < new_size) ? (top + 1) : new_size;
        for (int i = 0; i < copy_count; i++) {
            new_stack[i] = stack[i];
        }

        top = copy_count - 1;

        kfree(stack);
        stack = new_stack;
        max_size = new_size;
        mutex_unlock(&stack_mutex);

        printk(KERN_INFO "int_stack: Resized stack to %d (copied %d elements).\n", new_size, copy_count);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = stack_open,
    .release = stack_release,
    .read = stack_read,
    .write = stack_write,
    .unlocked_ioctl = stack_ioctl,
};

static int __init intstack_init(void) {
    dev_t dev;
    int ret;

    if ((ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) < 0) {
        return ret;
    }
    majorNumber = MAJOR(dev);

    cdev_init(&c_dev, &fops);
    if ((ret = cdev_add(&c_dev, dev, 1)) < 0) {
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    intstackClass = class_create(CLASS_NAME);
    if (IS_ERR(intstackClass)) {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(intstackClass);
    }

    intstackDevice = device_create(intstackClass, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(intstackDevice)) {
        class_destroy(intstackClass);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(intstackDevice);
    }

    mutex_init(&stack_mutex);

    stack = kmalloc(sizeof(int) * max_size, GFP_KERNEL);
    if (!stack) {
        device_destroy(intstackClass, dev);
        class_destroy(intstackClass);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, 1);
        return -ENOMEM;
    }
    top = -1;

    printk(KERN_INFO "int_stack: Module loaded.\n");
    return 0;
}

static void __exit intstack_exit(void) {
    kfree(stack);
    device_destroy(intstackClass, MKDEV(majorNumber, 0));
    class_unregister(intstackClass);
    class_destroy(intstackClass);
    cdev_del(&c_dev);
    unregister_chrdev_region(MKDEV(majorNumber, 0), 1);

    printk(KERN_INFO "int_stack: Module unloaded.\n");
}

module_init(intstack_init);
module_exit(intstack_exit);
