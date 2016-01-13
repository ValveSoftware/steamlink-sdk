#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <asm/uaccess.h>

#include "i2c_master.h"

#define DBGPRINTK(...)
/*#define TWSI_DEBUG*/

#define GALOIS_TWSIBUS0		0
#define GALOIS_TWSIBUS1		1
#define GALOIS_TWSIBUS2		2
#define GALOIS_TWSIBUS3		3
#define GALOIS_TWSIBUS_NUM	4

/* TWSI device operation defines */
#define TWSI_SUB_LENGTH 4

static DEFINE_MUTEX(ll_twsi_mutex);

static galois_twsi_info_t *galois_twsi_info[GALOIS_TWSIBUS_NUM];
static int twsi_opentime[GALOIS_TWSIBUS_NUM];
static char twsi_name[10];	/* for other TWSI */
static char twsi1_name[10];	/* only for TWSI1 */

static int twsi_irqs[GALOIS_TWSIBUS_NUM];
static int twsi1_irqs[12];	/* for TWSI1 */

static char wr_kbuf[4096];
static char rd_kbuf[4096];

static int twsi_major;

static irqreturn_t galois_twsi_intr(int irq, void *dev_id)
{
	galois_twsi_info_t *twsi_info = (galois_twsi_info_t *)dev_id;

	if ((twsi_info->mst_id >= GALOIS_TWSIBUS_NUM)
			|| (!twsi_info->usable))
		return IRQ_NONE;

	DBGPRINTK("into irs.\n");
	i2c_master_irs(twsi_info->mst_id);
	DBGPRINTK("out of irs.\n");
	return IRQ_HANDLED;
}

static inline void galois_twsi_lock(int master_id)
{
	mutex_lock(&ll_twsi_mutex);
}

static inline void galois_twsi_unlock(int master_id)
{
	mutex_unlock(&ll_twsi_mutex);
}

static inline int galois_twsi_get(int master_id)
{
	return ++(twsi_opentime[master_id]);
}

static inline int galois_twsi_put(int master_id)
{
	return --(twsi_opentime[master_id]);
}

static inline bool galois_twsi_is_idle(int master_id)
{
	return (i2c_master_is_idle(master_id) == I2C_OK) ? true : false;
}

int galois_twsi_i2c_open(int master_id)
{
	int irq, ret = 0;

	if (master_id >= GALOIS_TWSIBUS_NUM)
		return -ENODEV;

	galois_twsi_lock(master_id);

	if (!galois_twsi_info[master_id]) {
		galois_twsi_info[master_id] =
			kzalloc(sizeof(galois_twsi_info_t), GFP_KERNEL);
		if (!galois_twsi_info[master_id]) {
			printk("twsi%d: couldn't allocate memory for data.\n",
				master_id);
			ret = -ENOMEM;
			goto out;
		}

		/* initialize twsi_info */
		irq = twsi_irqs[master_id];
		galois_twsi_info[master_id]->irq = irq;
		galois_twsi_info[master_id]->mst_id = master_id;
		/* usable=0: can't be used */
		galois_twsi_info[master_id]->usable = 1;
		/* setup default speed */
		galois_twsi_info[master_id]->speed_type = I2C_STANDARD_SPEED;
		galois_twsi_info[master_id]->speed = 400; /* high speed 400K */

		/* check if i2c master is busy or not */
		if (!galois_twsi_is_idle(master_id)) {
			ret = -EBUSY;
			goto out;
		}

		i2c_master_init(master_id);

		/* set default speed */
		ret = i2c_master_set_speed(master_id,
					galois_twsi_info[master_id]->speed_type,
					galois_twsi_info[master_id]->speed);
		if (ret != I2C_OK) {
			printk("twsi_master_set_speed(%d,%d,%d) failed.\n",
				master_id,
				galois_twsi_info[master_id]->speed_type,
				galois_twsi_info[master_id]->speed);
			ret = -EACCES;
			goto out;
		}

		if (master_id == GALOIS_TWSIBUS1) {
			int i;
			for (i = 0; i < 12; i++) {
				snprintf(twsi1_name, sizeof(twsi1_name),
						"twsi%d_%d", master_id, i);
				ret = request_irq(twsi1_irqs[i], galois_twsi_intr,
						  IRQF_SHARED, twsi1_name,
						  galois_twsi_info[master_id]);
				if (ret) {
					printk("twsi%d: failure requesting irq %i\n",
						master_id, i);
					goto out;
				}
			}
		} else {
			snprintf(twsi_name, sizeof(twsi_name),
					"twsi%d", master_id);
			ret = request_irq(irq, galois_twsi_intr,
					  IRQF_SHARED, twsi_name,
					  galois_twsi_info[master_id]);
			if (ret) {
				printk("twsi%d: failure requesting irq %i\n",
					master_id, irq);
				goto out;
			}
		}
	}

	galois_twsi_get(master_id);
out:
	if (ret) {
		if (galois_twsi_info[master_id] != NULL) {
			kfree(galois_twsi_info[master_id]);
			galois_twsi_info[master_id] = NULL;
		}
	}

	galois_twsi_unlock(master_id);

	return ret;
}
EXPORT_SYMBOL(galois_twsi_i2c_open);

int galois_twsi_i2c_close(int master_id)
{
	galois_twsi_info_t *twsi_info;
	int ret = 0;

	if (master_id >= GALOIS_TWSIBUS_NUM)
		return -ENODEV;

	galois_twsi_lock(master_id);

	if (galois_twsi_info[master_id] == NULL) {
		ret = -ENODEV;
		goto out;
	}

	if (galois_twsi_put(master_id) > 0)
		goto out;

	twsi_info = galois_twsi_info[master_id];
	galois_twsi_info[master_id] = NULL;

	if (twsi_info->mst_id == GALOIS_TWSIBUS1) {
		int i;
		for (i = 0; i < 12; i++)
			free_irq(twsi1_irqs[i], twsi_info);
	} else
		free_irq(twsi_info->irq, twsi_info);
	kfree(twsi_info);
out:
	galois_twsi_unlock(master_id);

	return ret;
}
EXPORT_SYMBOL(galois_twsi_i2c_close);

int galois_twsi_i2c_writeread(int master_id, int addr, int type,
				unsigned char *wbuf, int wlen,
				unsigned char *rbuf, int rlen)
{
	int ret = 0;

	if (master_id >= GALOIS_TWSIBUS_NUM)
		return -ENODEV;

	galois_twsi_lock(master_id);

	if (galois_twsi_info[master_id] == NULL) {
		ret = -ENODEV;
		goto out;
	}

	if (!galois_twsi_info[master_id]->usable) {
		ret = -EBUSY;
		goto out;
	}

	ret = i2c_master_writeread_bytes(master_id, addr, type,
				wbuf, wlen, rbuf, rlen, 0);
out:
	galois_twsi_unlock(master_id);

	return ret;
}
EXPORT_SYMBOL(galois_twsi_i2c_writeread);

static int galois_twsi_open(struct inode *inode, struct file *file)
{
	int twsi_master_id, ret;

	twsi_master_id = iminor(inode);
	ret = galois_twsi_i2c_open(twsi_master_id);
	if (!ret)
		file->private_data = galois_twsi_info[twsi_master_id];

	return ret;
}

static int galois_twsi_release(struct inode *inode, struct file *file)
{
	galois_twsi_info_t *twsi_info = (galois_twsi_info_t *)file->private_data;

	return galois_twsi_i2c_close(twsi_info->mst_id);
}

static ssize_t galois_twsi_read(struct file *file, char __user * buf,
		size_t count, loff_t *ppos)
{
	printk("error: please use ioctl.\n");
	return -EFAULT;
}

static ssize_t galois_twsi_write(struct file *file, const char __user * buf,
		size_t count, loff_t *ppos)
{
	printk("error: please use ioctl.\n");
	return -EFAULT;
}

static long galois_twsi_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	galois_twsi_info_t *kern_twsi_info = (galois_twsi_info_t *)file->private_data;
	galois_twsi_speed_t twsi_speed;
	ssize_t ret = 0;

	switch(cmd) {
		case TWSI_IOCTL_READWRITE:
			{
			galois_twsi_rw_t user_twsi_rw;
			int res;

			if (copy_from_user(&user_twsi_rw, (void __user *)arg, sizeof(galois_twsi_rw_t)))
				return -EFAULT;

			/* judge if this access is valid */
			if (user_twsi_rw.mst_id != kern_twsi_info->mst_id)
				return -EACCES;

			/* check data sizes */
			if (user_twsi_rw.rd_cnt > sizeof(rd_kbuf))
				return -ENOMEM;

			if (user_twsi_rw.wr_cnt > sizeof(wr_kbuf))
				return -ENOMEM;

			/* issue rw */
			mutex_lock(&ll_twsi_mutex);
			if (user_twsi_rw.wr_cnt > 0) {
				if (copy_from_user(wr_kbuf, (void __user *)user_twsi_rw.wr_buf,
						user_twsi_rw.wr_cnt)) {
					ret = -EFAULT;
					printk("error: copy_from_user failed\n");
					goto read_write_out;
				}
			}
			res = i2c_master_writeread_bytes(
					user_twsi_rw.mst_id,			/* master_id */
					user_twsi_rw.slv_addr,			/* slave_addr */
					user_twsi_rw.addr_type,			/* addr_type */
					wr_kbuf, user_twsi_rw.wr_cnt,		/* pwbuf, wnum */
					rd_kbuf, user_twsi_rw.rd_cnt, 0);	/* prbuf, rnum, dum? */

			if (res != I2C_OK) {
				ret = -EACCES;
				printk("error [%d]: twsi_master_writeread_bytes failed.\n", res);
				goto read_write_out;
			}

			/* return to userspace */
			if (user_twsi_rw.rd_cnt > 0) {
				if (copy_to_user((void __user *)user_twsi_rw.rd_buf, rd_kbuf,
							user_twsi_rw.rd_cnt)) {
					ret = -EFAULT;
					printk("error: copy_to_user failed\n");
					goto read_write_out;
				}
			}
read_write_out:
			mutex_unlock(&ll_twsi_mutex);
			return ret;
			}
		case TWSI_IOCTL_SETSPEED:
			if (copy_from_user(&twsi_speed, (void __user *)arg, sizeof(galois_twsi_speed_t)))
				return -EFAULT;

			/* judge if this access is valid */
			if (twsi_speed.mst_id != kern_twsi_info->mst_id) {
				printk("error: TWSI_IOCTL_SETSPEED: invalid user master id %d.\n",
						twsi_speed.mst_id);
				return -EACCES;
			}

			/* if speed==0, 100kHz by-default, or set it as user requests */
			if (twsi_speed.speed) {
				mutex_lock(&ll_twsi_mutex);
				ret = i2c_master_set_speed(twsi_speed.mst_id,
						twsi_speed.speed_type, twsi_speed.speed);
				if (ret != I2C_OK) {
					printk("error: twsi_master_set_speed(%d,%d,%d) failed.\n",
							twsi_speed.mst_id, twsi_speed.speed_type, twsi_speed.speed);
					mutex_unlock(&ll_twsi_mutex);
					return -EACCES;
				}
				kern_twsi_info->speed_type = twsi_speed.speed_type;
				kern_twsi_info->speed = twsi_speed.speed;
				kern_twsi_info->usable = 1;
				mutex_unlock(&ll_twsi_mutex);
			}

			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static struct file_operations galois_twsi_fops = {
	.owner		= THIS_MODULE,
	.open		= galois_twsi_open,
	.release	= galois_twsi_release,
	.write		= galois_twsi_write,
	.read		= galois_twsi_read,
	.unlocked_ioctl		= galois_twsi_ioctl,
};

/*
 * TWSI controller list
 */
static struct {
	uint minor;
	char *name;
} twsidev_list[] = {
	{0, "twsi0"},
	{1, "twsi1"},
	{2, "twsi2"},
	{3, "twsi3"},
};

/* TWSI may be needed to bring up other drivers */
static struct class *twsi_class;
#ifdef CONFIG_PM
static int twsi_resume(struct device *dev)
{
	int i;
	for (i = 0; i < GALOIS_TWSIBUS_NUM; i++) {
		if (galois_twsi_info[i] == NULL)
			continue;
		{
		int usable = galois_twsi_info[i]->usable;

		mutex_lock(&ll_twsi_mutex);

		printk("Restore I2C settings for master [%d]\n", i);

		if (i2c_master_is_idle(i) != I2C_OK) {
			printk("twsi%d: busy.\n", i);
			mutex_unlock(&ll_twsi_mutex);
			return 0;
		}

		i2c_master_init(i);

		/* if the master is used, need to set its speed */
		if (usable) {
			uint mst_id = galois_twsi_info[i]->mst_id;
			uint speed_type = galois_twsi_info[i]->speed_type;
			uint speed = galois_twsi_info[i]->speed;
			i2c_master_set_speed(mst_id, speed_type, speed);
		}

		mutex_unlock(&ll_twsi_mutex);
		}
	}

	return 0;
}

static struct dev_pm_ops twsi_pm_ops = {
	.resume		= twsi_resume,
};
#endif

static struct platform_driver twsi_drv = {
	.driver = {
		.name	= "mv88de3100_twsi",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &twsi_pm_ops,
#endif
	},
};

static struct platform_device twsi_dev = {
	.name = "mv88de3100_twsi",
};

static int __init twsi_dummy_probe(struct platform_device *dev)
{
	if (dev == &twsi_dev)
		return 0;
	return -ENODEV;
}

static int __init galois_twsi_init_driver(void)
{
	int i;
	struct device_node *np;
	struct resource r;

	np = of_find_compatible_node(NULL, NULL, "berlin,apb-twsi");
	if (!np)
		return -ENODEV;
	for (i = 0; i < 12; i++) {
		of_irq_to_resource(np, i, &r);
		twsi1_irqs[i] = r.start;
	}
	of_irq_to_resource(np, i, &r);
	twsi_irqs[0] = r.start;
	twsi_irqs[1] = twsi1_irqs[0];
	of_node_put(np);

	np = of_find_compatible_node(NULL, NULL, "berlin,sm-twsi");
	if (!np)
		return -ENODEV;
	for (i = 0; i < 2; i++) {
		of_irq_to_resource(np, i, &r);
		twsi_irqs[i + 2] = r.start;
	}
	of_node_put(np);

	for (i = 0; i < GALOIS_TWSIBUS_NUM; i++) {
		galois_twsi_info[i] = NULL;
		twsi_opentime[i] = 0;
	}

	/* register char device */
	twsi_major = register_chrdev(0, "twsi", &galois_twsi_fops);
	if (twsi_major < 0)
		printk("failed to register chrdev for twsi.\n");

	twsi_class = class_create(THIS_MODULE, "twsi");
	for (i = 0; i < ARRAY_SIZE(twsidev_list); i++) {
		device_create(twsi_class, NULL,
				MKDEV(twsi_major, twsidev_list[i].minor),
				NULL, twsidev_list[i].name);
	}

	platform_device_register(&twsi_dev);
	platform_driver_probe(&twsi_drv, twsi_dummy_probe);

	return 0;
}

static void __exit galois_twsi_exit_driver(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(twsidev_list); i++) {
		printk("minor=%d\n", twsidev_list[i].minor);
		device_destroy(twsi_class, MKDEV(twsi_major, twsidev_list[i].minor));
	}
	class_destroy(twsi_class);
	unregister_chrdev(twsi_major, "twsi");
}

module_init(galois_twsi_init_driver);
module_exit(galois_twsi_exit_driver);

MODULE_AUTHOR("Marvell-Galois");
MODULE_DESCRIPTION("Galois TWSI Driver");
MODULE_LICENSE("GPL");
