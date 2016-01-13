#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <asm/uaccess.h>
#include <mach/gpio.h>

#include "kgpio.h"

#define GALOIS_GPIO_NAME	"gpio"

#undef ENABLE_GPIO_DEBUG
#ifdef ENABLE_GPIO_DEBUG
#define DEBUG_PRINT	printk
#else
#define DEBUG_PRINT(...)
#endif

struct gpio_priv {
	int apb_gpio0_irq;
	int apb_gpio1_irq;
	int apb_gpio2_irq;
	int apb_gpio3_irq;
	int sm_gpio0_irq;
	int sm_gpio1_irq;
};

struct gpio_priv priv;

static spinlock_t gpio_irq_lock;
static DEFINE_MUTEX(irq_init_mutex);
static int irq_init = 0;	/* ioctl IRQ_INIT count by multiple processes */

static unsigned int gpio_dev_id = 0x12345678;
static DECLARE_WAIT_QUEUE_HEAD(gpio_wait_queue);
static unsigned int gpio_irq_mode = 0;
static unsigned int gpio_irq_status = 0;
static int gpio_mode[GPIO_PORT_NUM];
static unsigned int gpio_mode_status = 0;
static unsigned int sm_gpio_irq_mode = 0;
static unsigned int sm_gpio_irq_status = 0;
static int sm_gpio_mode[SM_GPIO_PORT_NUM];
static unsigned int sm_gpio_mode_status = 0;

static irqreturn_t gpio_interrupt_handle(int irq, void *dev_id)
{
	int i;

	spin_lock(&gpio_irq_lock);
	for (i = 0; i < GPIO_PORT_NUM; i++) {
		if ((gpio_irq_mode & (1 << i)) && (GPIO_PortHasInterrupt(i) == 1)) {
			DEBUG_PRINT("[K]irq:%d, port:%d\n", irq, i);
			GPIO_PortClearInterrupt(i);
			DEBUG_PRINT("[port:%d, %d]\n", i, GPIO_PortHasInterrupt(i));
			gpio_irq_status |= (1 << i);
		}
	}

	for (i = 0; i < SM_GPIO_PORT_NUM; i++) {
		if ((sm_gpio_irq_mode & (1 << i)) && (SM_GPIO_PortHasInterrupt(i) == 1)) {
			DEBUG_PRINT("[K]irq:%d, smport:%d\n", irq, i);
			SM_GPIO_PortClearInterrupt(i);
			DEBUG_PRINT("[port:%d, %d]\n", i, SM_GPIO_PortHasInterrupt(i));
			sm_gpio_irq_status |= (1 << i);
		}
	}

	if (gpio_irq_status || sm_gpio_irq_status)
		wake_up_interruptible(&gpio_wait_queue);
	spin_unlock(&gpio_irq_lock);
	return IRQ_HANDLED;
}

static int galois_gpio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int galois_gpio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t galois_gpio_read(struct file *file, char __user * buf,
		size_t count, loff_t *ppos)
{
	printk("error: please use ioctl.\n");
	return -EFAULT;
}

static ssize_t galois_gpio_write(struct file *file, const char __user * buf,
		size_t count, loff_t *ppos)
{
	printk("error: please use ioctl.\n");
	return -EFAULT;
}

/*
 * gpio_set_mode: set SoC GPIO port to specific mode
 * @port: port number
 * @mode: mode
 */
static int gpio_set_mode(int port, int mode)
{

	switch (mode) {
		case GPIO_MODE_DATA_IN:
			GPIO_PortSetInOut(port, 1);
			break;
		case GPIO_MODE_DATA_OUT:
			GPIO_PortSetInOut(port, 0);
			break;
		case GPIO_MODE_IRQ_LOWLEVEL:
			GPIO_PortSetInOut(port, 1);
			GPIO_PortInitIRQ(port, 0, 0);
			break;
		case GPIO_MODE_IRQ_HIGHLEVEL:
			GPIO_PortSetInOut(port, 1);
			GPIO_PortInitIRQ(port, 0, 1);
			break;
		case GPIO_MODE_IRQ_RISEEDGE:
			GPIO_PortSetInOut(port, 1);
			GPIO_PortInitIRQ(port, 1, 1);
			break;
		case GPIO_MODE_IRQ_FALLEDGE:
			GPIO_PortSetInOut(port, 1);
			GPIO_PortInitIRQ(port, 1, 0);
			break;
		default:
			printk("Unknown GPIO mode.\n");
			return -1;
	}

	gpio_mode_status |= (1<<port);
	gpio_mode[port] = mode;
	return 0;
}

/*
 * sm_gpio_set_mode: set SM GPIO port to specific mode
 * @port: port number
 * @mode: mode
 */
static int sm_gpio_set_mode(int port, int mode)
{
	switch (mode) {
		case GPIO_MODE_DATA_IN:
			SM_GPIO_PortSetInOut(port, 1);
			break;
		case GPIO_MODE_DATA_OUT:
			SM_GPIO_PortSetInOut(port, 0);
			break;
		case GPIO_MODE_IRQ_LOWLEVEL:
			SM_GPIO_PortSetInOut(port, 1);
			SM_GPIO_PortInitIRQ(port, 0, 0);
			break;
		case GPIO_MODE_IRQ_HIGHLEVEL:
			SM_GPIO_PortSetInOut(port, 1);
			SM_GPIO_PortInitIRQ(port, 0, 1);
			break;
		case GPIO_MODE_IRQ_RISEEDGE:
			SM_GPIO_PortSetInOut(port, 1);
			SM_GPIO_PortInitIRQ(port, 1, 1);
			break;
		case GPIO_MODE_IRQ_FALLEDGE:
			SM_GPIO_PortSetInOut(port, 1);
			SM_GPIO_PortInitIRQ(port, 1, 0);
			break;
		default:
			printk("Unknown SM GPIO mode.\n");
			return -1;
	}

	sm_gpio_mode_status |= (1<<port);
	sm_gpio_mode[port] = mode;

	return 0;
}

static int gpio_get_info(galois_gpio_reg_t *gpio_reg)
{
	int i, inout, data;
	for (i = 0; i < GPIO_PORT_NUM; i++) {
		GPIO_PortGetInOut(i, &inout);
		gpio_reg->port_ddr |= (inout << i);
		GPIO_PortGetData(i, &data);
		gpio_reg->port_dr |= (data << i);
	}

	for (i = 0; i < GPIO_PORT_NUM; i++) {
		gpio_reg->gpio_mode[i] = gpio_mode[i];
	}

	return 0;
}

static int sm_gpio_get_info(galois_gpio_reg_t *gpio_reg)
{
	int i, inout, data;

	for (i = 0; i < SM_GPIO_PORT_NUM; i++) {
		SM_GPIO_PortGetInOut(i, &inout);
		gpio_reg->port_ddr |= (inout << i);
		SM_GPIO_PortGetData(i, &data);
		gpio_reg->port_dr |= (data << i);
	}

	for (i = 0; i < SM_GPIO_PORT_NUM; i++) {
		gpio_reg->gpio_mode[i] = sm_gpio_mode[i];
	}

	return 0;
}

static long galois_gpio_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	galois_gpio_data_t *arg_data = (galois_gpio_data_t *)arg;
	galois_gpio_reg_t *kreg;
	galois_gpio_data_t gpio_data;
	unsigned int __gpio_irq_status, __sm_gpio_irq_status;
	unsigned long flag;
	int ret = 0;

	switch(cmd) {
	case GPIO_IOCTL_LOCK:
		GPIO_PortLock(arg);
		break;
	case GPIO_IOCTL_UNLOCK:
		GPIO_PortUnlock(arg);
		break;
	case SM_GPIO_IOCTL_LOCK:
		SM_GPIO_PortLock(arg);
		break;
	case SM_GPIO_IOCTL_UNLOCK:
		SM_GPIO_PortUnlock(arg);
		break;
	case GPIO_IOCTL_SET:
	case SM_GPIO_IOCTL_SET:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_SET)
			gpio_set_mode(gpio_data.port, gpio_data.mode);
		else
			sm_gpio_set_mode(gpio_data.port, gpio_data.mode);
		break;
	case GPIO_IOCTL_GET:
	case SM_GPIO_IOCTL_GET:
		kreg = kzalloc(sizeof(galois_gpio_reg_t), GFP_KERNEL);
		if (kreg == NULL) {
			printk("error: failed to allocate memory.\n");
			return -ENOMEM;
		}
		if (cmd == GPIO_IOCTL_GET)
			gpio_get_info(kreg);
		else
			sm_gpio_get_info(kreg);
		if (copy_to_user((void __user *)arg, kreg, sizeof(galois_gpio_reg_t))) {
			kfree(kreg);
			return -EFAULT;
		}
		kfree(kreg);
		break;
	case GPIO_IOCTL_READ:
	case SM_GPIO_IOCTL_READ:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_READ) {
			if (gpio_mode[gpio_data.port] != GPIO_MODE_DATA_IN) {
				printk("GPIO port %d isn't set to data in.\n", gpio_data.port);
				return -ENODEV;
			}
			if (GPIO_PortRead(gpio_data.port, &gpio_data.data)) {
				printk("GPIO_PortRead error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
		} else {
			if (sm_gpio_mode[gpio_data.port] != GPIO_MODE_DATA_IN) {
				printk("SM GPIO port %d isn't set to data in.\n", gpio_data.port);
				return -ENODEV;
			}
			if (SM_GPIO_PortRead(gpio_data.port, &gpio_data.data)) {
				printk("SM_GPIO_PortRead error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
		}
		if (put_user(gpio_data.data, &arg_data->data))
			return -EFAULT;
		break;
	case GPIO_IOCTL_WRITE:
	case SM_GPIO_IOCTL_WRITE:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_WRITE) {
			if (gpio_mode[gpio_data.port] != GPIO_MODE_DATA_OUT) {
				printk("GPIO port %d isn't set to data out.\n", gpio_data.port);
				return -ENODEV;
			}
			if (GPIO_PortWrite(gpio_data.port, gpio_data.data)) {
				printk("GPIO_PortRead error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
		} else {
			if (sm_gpio_mode[gpio_data.port] != GPIO_MODE_DATA_OUT) {
				printk("SM GPIO port %d isn't set to data out.\n", gpio_data.port);
				return -ENODEV;
			}
			if (SM_GPIO_PortWrite(gpio_data.port, gpio_data.data)) {
				printk("SM_GPIO_PortRead error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
		}
		break;
	case GPIO_IOCTL_ENABLE_IRQ:
	case SM_GPIO_IOCTL_ENABLE_IRQ:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_ENABLE_IRQ) {
			if (GPIO_PortEnableIRQ(gpio_data.port)) {
				printk("GPIO_PortEnableIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			gpio_irq_mode |= (1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		} else {
			if (SM_GPIO_PortEnableIRQ(gpio_data.port)) {
				printk("SM_GPIO_PortEnableIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			sm_gpio_irq_mode |= (1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		}
		break;
	case GPIO_IOCTL_DISABLE_IRQ:
	case SM_GPIO_IOCTL_DISABLE_IRQ:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_DISABLE_IRQ) {
			if (GPIO_PortDisableIRQ(gpio_data.port)) {
				printk("GPIO_PortDisableIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			gpio_irq_mode &= ~(1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		} else {
			if (SM_GPIO_PortDisableIRQ(gpio_data.port)) {
				printk("SM_GPIO_PortDisableIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			sm_gpio_irq_mode &= ~(1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		}
		break;
	case GPIO_IOCTL_CLEAR_IRQ:
	case SM_GPIO_IOCTL_CLEAR_IRQ:
		if (copy_from_user(&gpio_data, (void __user *)arg, sizeof(galois_gpio_data_t)))
			return -EFAULT;
		if (cmd == GPIO_IOCTL_CLEAR_IRQ) {
			/* GPIO_PortClearInterrupt isn't necessary */
			if (GPIO_PortClearInterrupt(gpio_data.port)) {
				printk("GPIO_PortClearIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			gpio_irq_status &= ~(1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		} else {
			/* GPIO_PortClearInterrupt isn't necessary */
			if (SM_GPIO_PortClearInterrupt(gpio_data.port)) {
				printk("SM_GPIO_PortClearIRQ error (port=%d)\n", gpio_data.port);
				return -ENODEV;
			}
			spin_lock_irqsave(&gpio_irq_lock, flag);
			sm_gpio_irq_status &= ~(1 << gpio_data.port);
			spin_unlock_irqrestore(&gpio_irq_lock, flag);
		}
		break;
	case GPIO_IOCTL_READ_IRQ:
	case SM_GPIO_IOCTL_READ_IRQ:
		spin_lock_irqsave(&gpio_irq_lock, flag);
		__gpio_irq_status = gpio_irq_status;
		__sm_gpio_irq_status = sm_gpio_irq_status;
		spin_unlock_irqrestore(&gpio_irq_lock, flag);
		if (cmd == GPIO_IOCTL_READ_IRQ) {
			if (put_user(__gpio_irq_status, (unsigned int *)arg))
				return -EFAULT;
		} else {
			if (put_user(__sm_gpio_irq_status, (unsigned int *)arg))
				return -EFAULT;
		}
		break;
	case GPIO_IOCTL_INIT_IRQ:
		mutex_lock(&irq_init_mutex);
		if (irq_init > 0) {
			irq_init++;
			printk("GPIO irq init++ %d\n", irq_init);
			mutex_unlock(&irq_init_mutex);
			return 0;
		}
		/* reigster GPIO inst0 interrupt */
		ret = request_irq(priv.apb_gpio0_irq, gpio_interrupt_handle,
				IRQF_SHARED, GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			printk("failed to request_irq %d.\n", priv.apb_gpio0_irq);
			goto init_irq_exit;
		}
		/* reigster GPIO inst1 interrupt */
		ret = request_irq(priv.apb_gpio1_irq, gpio_interrupt_handle,
				IRQF_SHARED, GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
			printk("failed to request_irq %d.\n", priv.apb_gpio1_irq);
			goto init_irq_exit;
		}
		/* reigster GPIO inst2 interrupt */
		ret = request_irq(priv.apb_gpio2_irq, gpio_interrupt_handle,
				IRQF_SHARED, GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
			printk("failed to request_irq %d.\n", priv.apb_gpio2_irq);
			goto init_irq_exit;
		}
		/* reigster GPIO inst3 interrupt */
		ret = request_irq(priv.apb_gpio3_irq, gpio_interrupt_handle,
				IRQF_SHARED, GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio2_irq, &gpio_dev_id);
			printk("failed to request_irq %d.\n", priv.apb_gpio3_irq);
			goto init_irq_exit;
		}

		/* reigster SM GPIO inst1 interrupt */
		ret = request_irq(priv.sm_gpio1_irq, gpio_interrupt_handle,
				IRQF_SHARED, GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio2_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio3_irq, &gpio_dev_id);
			printk("failed to request_irq %d.\n", priv.sm_gpio1_irq);
			goto init_irq_exit;
		}
		/* reigster SM GPIO inst0 interrupt */
		ret = request_irq(priv.sm_gpio0_irq, gpio_interrupt_handle, IRQF_SHARED,
				GALOIS_GPIO_NAME, &gpio_dev_id);
		if (ret) {
			free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio2_irq, &gpio_dev_id);
			free_irq(priv.apb_gpio3_irq, &gpio_dev_id);
			free_irq(priv.sm_gpio1_irq, &gpio_dev_id);
			printk("error: failed to request_irq %d.\n", priv.sm_gpio0_irq);
			goto init_irq_exit;
		}
		irq_init++;
		printk("GPIO irq init++ %d\n", irq_init);
init_irq_exit:
		mutex_unlock(&irq_init_mutex);
		return ret;
		//break;
	case GPIO_IOCTL_EXIT_IRQ:
		mutex_lock(&irq_init_mutex);
		if (irq_init > 0) {
			irq_init--;
			printk("GPIO irq init-- %d", irq_init);
		}
		if (irq_init > 0) {
			mutex_unlock(&irq_init_mutex);
			return 0;
		}
		/* can it disable the irq when last handle is freed? */
		free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
		free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
		free_irq(priv.apb_gpio2_irq, &gpio_dev_id);
		free_irq(priv.apb_gpio3_irq, &gpio_dev_id);
		free_irq(priv.sm_gpio0_irq, &gpio_dev_id);
		free_irq(priv.sm_gpio1_irq, &gpio_dev_id);
		mutex_unlock(&irq_init_mutex);
		break;
	default:
		return -EPERM;
	}
	return 0;
}

static unsigned int galois_gpio_poll(struct file *file, poll_table *wait)
{
	unsigned int __gpio_irq_status, __sm_gpio_irq_status;
	unsigned long flag;

	poll_wait(file, &gpio_wait_queue, wait);

	spin_lock_irqsave(&gpio_irq_lock, flag);
	__gpio_irq_status = gpio_irq_status;
	__sm_gpio_irq_status = sm_gpio_irq_status;
	spin_unlock_irqrestore(&gpio_irq_lock, flag);

	if (__gpio_irq_status || __sm_gpio_irq_status)
		return (POLLIN | POLLRDNORM);
	return 0;
}

static struct file_operations galois_gpio_fops = {
	.owner		= THIS_MODULE,
	.open		= galois_gpio_open,
	.release	= galois_gpio_release,
	.write		= galois_gpio_write,
	.read		= galois_gpio_read,
	.unlocked_ioctl	= galois_gpio_ioctl,
	.poll		= galois_gpio_poll,
};

static struct miscdevice gpio_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= GALOIS_GPIO_NAME,
	.fops	= &galois_gpio_fops,
};

static int __init galois_gpio_init(void)
{
	int i;
	struct device_node *np;
	struct resource r;

	np = of_find_compatible_node(NULL, NULL, "berlin,apb-gpio");
	if (!np)
		return -ENODEV;
	of_irq_to_resource(np, 0, &r);
	priv.apb_gpio0_irq = r.start;
	of_irq_to_resource(np, 1, &r);
	priv.apb_gpio1_irq = r.start;
	of_irq_to_resource(np, 2, &r);
	priv.apb_gpio2_irq = r.start;
	of_irq_to_resource(np, 3, &r);
	priv.apb_gpio3_irq = r.start;
	of_node_put(np);

	np = of_find_compatible_node(NULL, NULL, "berlin,sm-gpio");
	if (!np)
		return -ENODEV;
	of_irq_to_resource(np, 0, &r);
	priv.sm_gpio0_irq = r.start;
	of_irq_to_resource(np, 1, &r);
	priv.sm_gpio1_irq = r.start;
	of_node_put(np);

	spin_lock_init(&gpio_irq_lock);

	for (i = 0; i < GPIO_PORT_NUM; i++)
		gpio_mode[i] = 0;
	for (i = 0; i < SM_GPIO_PORT_NUM; i++)
		sm_gpio_mode[i] = 0;

	return misc_register(&gpio_dev);
}

static void __exit galois_gpio_exit(void)
{
	int i;

	/* disable all GPIO interrupts */
	for (i = 0; i < GPIO_PORT_NUM; i++) {
		if (gpio_irq_mode & (1 << i))
			GPIO_PortDisableIRQ(i);
	}
	for (i = 0; i < SM_GPIO_PORT_NUM; i++) {
		if (sm_gpio_irq_mode & (1 << i))
			SM_GPIO_PortDisableIRQ(i);
	}

	/* un-registeer handler registered by GPIO */
	free_irq(priv.apb_gpio0_irq, &gpio_dev_id);
	free_irq(priv.apb_gpio1_irq, &gpio_dev_id);
	free_irq(priv.apb_gpio2_irq, &gpio_dev_id);
	free_irq(priv.apb_gpio3_irq, &gpio_dev_id);
	free_irq(priv.sm_gpio0_irq, &gpio_dev_id);
	free_irq(priv.sm_gpio1_irq, &gpio_dev_id);

	misc_deregister(&gpio_dev);
}

module_init(galois_gpio_init);
module_exit(galois_gpio_exit);

MODULE_AUTHOR("Marvell-Galois");
MODULE_DESCRIPTION("Galois GPIO Driver");
MODULE_LICENSE("GPL");
