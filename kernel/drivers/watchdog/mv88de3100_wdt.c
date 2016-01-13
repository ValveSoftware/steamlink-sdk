#include <linux/module.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <mach/galois_generic.h>


#define WDT_CR			0x00
#define WDT_TORR		0x04
#define WDT_CCVR		0x08
#define WDT_CRR			0x0C
#define WDT_STAT		0x10
#define WDT_EOI			0x14
#define WDT_COMP_PARAMS_5	0xE4
#define WDT_COMP_PARAMS_4	0xE8
#define WDT_COMP_PARAMS_3	0xEC
#define WDT_COMP_PARAMS_2	0xF0
#define WDT_COMP_PARAMS_1	0xF4
#define WDT_COMP_VERSION	0xF8
#define WDT_COMP_TYPE		0xFC

#define WDT_CR_BIT_RST_PULSE	2
#define WDT_CR_BIT_RESP_MODE	1
#define WDT_CR_BIT_EN		0

#define WDT_2_PCLK		0x0
#define WDT_4_PCLK		0x1
#define WDT_8_PCLK		0x2
#define WDT_16_PCLK		0x3
#define WDT_32_PCLK		0x4
#define WDT_64_PCLK		0x5
#define WDT_128_PCLK		0x6
#define WDT_256_PCLK		0x7

#define WDT_DEFAULT_TIMEOUT	(10)
#define WDT_TIMEOUT_MIN		(1)
#define WDT_TIMEOUT_MAX		(20)
#define WDT_ENABLE		(0x1)
#define WDT_DISABLE		(0xFFFFFFFE)

enum wdt_resp_mode {
	WDT_HW_RESET = 0x0,
	WDT_SW_IRQ   = 0x1<<WDT_CR_BIT_RESP_MODE,
};


struct mv88de3100_wdt {
	struct resource      *mem;
	void __iomem         *reg_base;
	struct device        *dev;
	struct miscdevice     miscdev;
	spinlock_t            lock;
	unsigned long         open_lock;
	int                   timeout;
	enum wdt_resp_mode    mode;
};

struct platform_device *pdev_mv88de3100_wdt;


static inline u32 rdlp(struct mv88de3100_wdt *wdt, int offset)
{
	return __raw_readl(wdt->reg_base + offset);
}

static inline void wrlp(struct mv88de3100_wdt *wdt, int offset, u32 data)
{
	__raw_writel(data, wdt->reg_base + offset);
}


static void mv88de3100_wdt_keepalive(struct mv88de3100_wdt *wdt)
{
	wrlp(wdt, WDT_CRR, 0x76);
}

/*
 * According to datasheet, once this bit has been enabled,
 * it can be cleared only by a system reset.
 */
static void __mv88de3100_wdt_stop(struct mv88de3100_wdt *wdt)
{
	unsigned int val = 0;
	val = rdlp(wdt, WDT_CR);
	wrlp(wdt, WDT_CR, WDT_DISABLE & val);
}

static void mv88de3100_wdt_stop(struct mv88de3100_wdt *wdt)
{
	spin_lock(&wdt->lock);
	__mv88de3100_wdt_stop(wdt);
	spin_unlock(&wdt->lock);
}

static void mv88de3100_wdt_start(struct mv88de3100_wdt *wdt)
{
	unsigned int val = 0;

	spin_lock(&wdt->lock);
	__mv88de3100_wdt_stop(wdt);

	wdt->mode = WDT_HW_RESET;
	wrlp(wdt, WDT_CR, WDT_8_PCLK<<WDT_CR_BIT_RST_PULSE | wdt->mode);

	/* avoid timeout occurs at init */
	mv88de3100_wdt_keepalive(wdt);

	/* enable watchdog */
	val = rdlp(wdt, WDT_CR);
	wrlp(wdt, WDT_CR, WDT_ENABLE<<WDT_CR_BIT_EN | val);
	spin_unlock(&wdt->lock);
}

static int mv88de3100_wdt_set_timeout(struct mv88de3100_wdt *wdt, int new)
{
	struct clk *cfg_clk;
	unsigned long tclk, tmp = 0;

	if (new < WDT_TIMEOUT_MIN) {
		dev_warn(wdt->dev,
				"new timeout less than minimum value (%d), "
				"force to be default (%d)\n",
				WDT_TIMEOUT_MIN, WDT_DEFAULT_TIMEOUT);
		wdt->timeout = WDT_DEFAULT_TIMEOUT;
	} else if (new > WDT_TIMEOUT_MAX) {
		dev_warn(wdt->dev,
				"new timeout bigger than maximum value (%d), "
				"force to be max\n", WDT_TIMEOUT_MAX);
		wdt->timeout = WDT_TIMEOUT_MAX;
	} else {
		dev_info(wdt->dev, "new timeout (%d, approximant)\n", new);
		wdt->timeout = new;
	}

	cfg_clk = clk_get(NULL, "cfg");
	BUG_ON(IS_ERR(cfg_clk));
	tclk = clk_get_rate(cfg_clk);
	printk(KERN_DEBUG "cfg: %lu\n", tclk);

	while (1<<(16+tmp) < tclk * wdt->timeout)
		tmp++;
	tmp = tmp + (tmp<<4);
	wrlp(wdt, WDT_TORR, tmp);

	return 0;
}

static int mv88de3100_wdt_open(struct inode *inode, struct file *file)
{
	struct mv88de3100_wdt *wdt = platform_get_drvdata(pdev_mv88de3100_wdt);

	file->private_data = (void *)wdt;
	if (test_and_set_bit(0, &wdt->open_lock)) {
		dev_warn(wdt->dev, "open again, ignored\n");
		return -EBUSY;
	}
	mv88de3100_wdt_start(wdt);

	return nonseekable_open(inode, file);
}

static int mv88de3100_wdt_release(struct inode *inode, struct file *file)
{
	struct mv88de3100_wdt *wdt;
	wdt = file->private_data;
	mv88de3100_wdt_stop(wdt);
	clear_bit(0, &wdt->open_lock);
	return 0;
}

static ssize_t mv88de3100_wdt_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	struct mv88de3100_wdt *wdt;

	wdt = file->private_data;
	if (len)
		mv88de3100_wdt_keepalive(wdt);

	return len;
}

static long mv88de3100_wdt_ioctl(struct file *file, unsigned int cmd,
		                                    unsigned long arg)
{
	struct mv88de3100_wdt *wdt;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_timeout;
	const struct watchdog_info ident = {
		.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
		.firmware_version = 0,
		.identity = "mv88de3100_wdt"
	};

	wdt = file->private_data;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident,
			sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);
	case WDIOC_KEEPALIVE:
		spin_lock(&wdt->lock);
		mv88de3100_wdt_keepalive(wdt);
		spin_unlock(&wdt->lock);
		return 0;
	case WDIOC_SETTIMEOUT:
		spin_lock(&wdt->lock);
		if (get_user(new_timeout, p))
			return -EFAULT;
		if (mv88de3100_wdt_set_timeout(wdt, new_timeout))
			return -EINVAL;
		mv88de3100_wdt_keepalive(wdt);
		spin_unlock(&wdt->lock);
		return put_user(wdt->timeout, p);
	case WDIOC_GETTIMEOUT:
		return put_user(wdt->timeout, p);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations mv88de3100_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= mv88de3100_wdt_write,
	.unlocked_ioctl	= mv88de3100_wdt_ioctl,
	.open		= mv88de3100_wdt_open,
	.release	= mv88de3100_wdt_release,
};

static int __devinit mv88de3100_wdt_probe(struct platform_device *pdev)
{
	struct mv88de3100_wdt *wdt;
	int ret = 0;
	int size = 0;
	struct device_node *np = pdev->dev.of_node;

	wdt = kzalloc(sizeof(struct mv88de3100_wdt), GFP_KERNEL);
	if (wdt == NULL) {
		dev_err(&pdev->dev, "no memory for wdt driver\n");
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "alloc: 0x%p\n", wdt);

	wdt->dev = &pdev->dev;
	wdt->timeout = WDT_DEFAULT_TIMEOUT;
	spin_lock_init(&wdt->lock);

	wdt->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (wdt->mem == NULL) {
		dev_err(wdt->dev, "no memory resource specified\n");
		return -ENOENT;
	}

	size = resource_size(wdt->mem);
	if (!request_mem_region(wdt->mem->start, size, pdev->name)) {
		dev_err(wdt->dev, "failed to get memory region\n");
		return -EBUSY;
	}

	wdt->reg_base = of_iomap(np, 0);
	if (wdt->reg_base == NULL) {
		dev_err(wdt->dev, "failed to ioremap region\n");
		ret = -EINVAL;
		goto err_req;
	}

	platform_set_drvdata(pdev, wdt);
	pdev_mv88de3100_wdt = pdev;

	wdt->miscdev.parent = &pdev->dev;
	wdt->miscdev.minor = WATCHDOG_MINOR;
	wdt->miscdev.name = "watchdog";
	wdt->miscdev.fops = &mv88de3100_wdt_fops;

	ret = misc_register(&wdt->miscdev);
	if (ret) {
		dev_err(wdt->dev, "cannot register miscdev on minor: %d (ret: %d)\n",
			WATCHDOG_MINOR, ret);
		goto err_map;
	}

	mv88de3100_wdt_stop(wdt);
	ret = mv88de3100_wdt_set_timeout(wdt, WDT_DEFAULT_TIMEOUT);
	if (ret != 0) {
		dev_warn(wdt->dev,
				"failed to set timeout (%d)\n"
				"use default (%d)\n",
				wdt->timeout, WDT_DEFAULT_TIMEOUT);
	}

	printk(KERN_DEBUG "WDT_CR: %08X\n"
			  "WDT_TORR: %08X\n"
			  "WDT_CCVR: %08X\n"
			  "WDT_CRR: %08X\n"
			  "WDT_STAT: %08X\n"
			  "WDT_EOI: %08X\n"
			  "WDT_COMP_PARAMS_1: %08X\n",
			  rdlp(wdt, WDT_CR),
			  rdlp(wdt, WDT_TORR),
			  rdlp(wdt, WDT_CCVR),
			  rdlp(wdt, WDT_CRR),
			  rdlp(wdt, WDT_STAT),
			  rdlp(wdt, WDT_EOI),
			  rdlp(wdt, WDT_COMP_PARAMS_1));

	return 0;

err_map:
	iounmap(wdt->reg_base);
err_req:
	release_mem_region(wdt->mem->start, size);
	wdt->mem = NULL;
	return ret;
}

static int __devexit mv88de3100_wdt_remove(struct platform_device *pdev)
{
	struct mv88de3100_wdt *wdt = platform_get_drvdata(pdev);

	misc_deregister(&wdt->miscdev);


	iounmap(wdt->reg_base);
	release_mem_region(wdt->mem->start, resource_size(wdt->mem));
	wdt->mem = NULL;

	kfree(wdt);
	pdev_mv88de3100_wdt = NULL;

	return 0;
}

static void mv88de3100_wdt_shutdown(struct platform_device *pdev)
{
	struct mv88de3100_wdt *wdt = platform_get_drvdata(pdev);
	mv88de3100_wdt_stop(wdt);
}

#ifdef CONFIG_PM
static int mv88de3100_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mv88de3100_wdt *wdt = platform_get_drvdata(pdev);
	mv88de3100_wdt_stop(wdt);
	return 0;
}

static int mv88de3100_wdt_resume(struct platform_device *pdev)
{
	struct mv88de3100_wdt *wdt = platform_get_drvdata(pdev);
	mv88de3100_wdt_start(wdt);
	return 0;
}
#else
#define mv88de3100_wdt_suspend NULL
#define mv88de3100_wdt_resume  NULL
#endif /* CONFIG_PM */

static const struct of_device_id berlin_wdt_of_match[] = {
	{.compatible = "mrvl,berlin-wdt",},
	{},
};

static struct platform_driver mv88de3100_wdt_driver = {
	.probe		= mv88de3100_wdt_probe,
	.remove		= __devexit_p(mv88de3100_wdt_remove),
	.shutdown	= mv88de3100_wdt_shutdown,
	.suspend	= mv88de3100_wdt_suspend,
	.resume		= mv88de3100_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "mv88de3100_wdt",
		.of_match_table = berlin_wdt_of_match,
	},
};


static char banner[] __initdata =
	KERN_DEBUG "+---------------------------------------------+\n"
	           "| MV88DE3100 Watchdog Timer, (c) 2012 Marvell |\n"
		   "+---------------------------------------------+\n";

static int __init watchdog_init(void)
{
	printk(banner);
	return platform_driver_register(&mv88de3100_wdt_driver);
}

static void __exit watchdog_exit(void)
{
	platform_driver_unregister(&mv88de3100_wdt_driver);
}

module_init(watchdog_init);
module_exit(watchdog_exit);

MODULE_AUTHOR("Yiran Liao <yrliao@marvell.com>");
MODULE_DESCRIPTION("MV88DE3100 Watchdog Device Driver");
MODULE_LICENSE("GPL");
