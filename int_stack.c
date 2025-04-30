#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/usb.h>

#define DEVICE_NAME "int_stack"
#define CLASS_NAME "int_stack_class"
#define IOCTL_SET_STACK_SIZE _IOW('s', 1, int)

#define USB_VENDOR_ID  0x045e
#define USB_PRODUCT_ID 0x0779

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d.vasilev@innopolis.university");
MODULE_DESCRIPTION("Chardev for a stack<int> with USB key lock");
MODULE_VERSION("1.1");

static int majorNumber;
static struct class* intstackClass = NULL;
static struct device* intstackDevice = NULL;
static struct cdev c_dev;

static int *stack = NULL;
static int max_size = 8;
static int top = -1;
static DEFINE_MUTEX(stack_mutex);
static dev_t dev_num;

static struct notifier_block usb_notifier;

static int is_usb_key_connected = 0;

static void try_create_device(void) {
    if (!intstackDevice && is_usb_key_connected) {
        intstackDevice = device_create(intstackClass, NULL, dev_num, NULL, DEVICE_NAME);
        if (IS_ERR(intstackDevice)) {
            printk(KERN_ERR "int_stack: Failed to create device\n");
            intstackDevice = NULL;
        } else {
            printk(KERN_INFO "int_stack: Device node created\n");
        }
    }
}

static void try_remove_device(void) {
    if (intstackDevice && !is_usb_key_connected) {
        device_destroy(intstackClass, dev_num);
        intstackDevice = NULL;
        printk(KERN_INFO "int_stack: Device node removed\n");
    }
}

static int usb_event(struct notifier_block *self, unsigned long action, void *dev) {
    struct usb_device *usb_dev = dev;

    if (!usb_dev || usb_dev->descriptor.idVendor != USB_VENDOR_ID || usb_dev->descriptor.idProduct != USB_PRODUCT_ID)
        return NOTIFY_OK;

    switch (action) {
        case USB_DEVICE_ADD:
            is_usb_key_connected = 1;
            try_create_device();
            break;
        case USB_DEVICE_REMOVE:
            is_usb_key_connected = 0;
            try_remove_device();
            break;
    }
    return NOTIFY_OK;
}

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
    int ret;
    struct usb_device *usb_dev;

    if ((ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME)) < 0)
        return ret;
    majorNumber = MAJOR(dev_num);

    cdev_init(&c_dev, &fops);
    if ((ret = cdev_add(&c_dev, dev_num, 1)) < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    intstackClass = class_create(CLASS_NAME);
    if (IS_ERR(intstackClass)) {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(intstackClass);
    }

    mutex_init(&stack_mutex);
    stack = kmalloc(sizeof(int) * max_size, GFP_KERNEL);
    if (!stack) {
        class_destroy(intstackClass);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }

    usb_notifier.notifier_call = usb_event;
    usb_register_notify(&usb_notifier);
    printk(KERN_INFO "int_stack: Module loaded.\n");
    return 0;
}

static void __exit intstack_exit(void) {
    usb_unregister_notify(&usb_notifier);
    try_remove_device();

    kfree(stack);
    device_destroy(intstackClass, dev_num);
    class_unregister(intstackClass);
    class_destroy(intstackClass);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "int_stack: Module unloaded.\n");
}

module_init(intstack_init);
module_exit(intstack_exit);
