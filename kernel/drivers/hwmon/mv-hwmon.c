/********************************************************************************
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED.  The GPL License provides additional details about this warranty
 * disclaimer.
 ********************************************************************************/

/*
 * mv-hwmon.c
 *
 * For now, only implemented a temperature interface
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include <mach/galois_generic.h>
#include <mach/galois_platform.h>

#define MV_HWMON_DRV_NAME		"mv88de3100-hwmon"

#define RA_SM_CTRL			0x0014
#if defined(CONFIG_BERLIN2CD)
#define RA_TSEN_ADC_CTRL		0x0074
#else /* !defined(CONFIG_BERLIN2CD) */
#define RA_SM_ADC_CTRL			0x0018
#endif /* defined(CONFIG_BERLIN2CD) */
#define RA_SM_ADC_STATUS		0x001C
#define RA_SM_ADC_DATA			0x0020
#define RA_TSEN_ADC_STATUS		0x0024
#define RA_TSEN_ADC_DATA		0x0028

#define SM_CTRL				(SM_SYS_CTRL_REG_BASE + RA_SM_CTRL)
#if defined(CONFIG_BERLIN2CD)
#define TSEN_ADC_CTRL			(SM_SYS_CTRL_REG_BASE + RA_TSEN_ADC_CTRL)
#else /* !defined(CONFIG_BERLIN2CD) */
#define SM_ADC_CTRL			(SM_SYS_CTRL_REG_BASE + RA_SM_ADC_CTRL)
#endif /* defined(CONFIG_BERLIN2CD) */
#define SM_ADC_STATUS			(SM_SYS_CTRL_REG_BASE + RA_SM_ADC_STATUS)
#define SM_ADC_DATA			(SM_SYS_CTRL_REG_BASE + RA_SM_ADC_DATA)
#define TSEN_ADC_STATUS			(SM_SYS_CTRL_REG_BASE + RA_TSEN_ADC_STATUS)
#define TSEN_ADC_DATA			(SM_SYS_CTRL_REG_BASE + RA_TSEN_ADC_DATA)

#define BIT_SM_CTRL_SOC2SM_SW_INTR	(1<<2) /* SoC to SM Software interrupt */
#define BIT_SM_CTRL_ADC_SEL0		(1<<5) /* adc input channel select */
#define BIT_SM_CTRL_ADC_SEL1		(1<<6)
#define BIT_SM_CTRL_ADC_SEL2		(1<<7)
#define BIT_SM_CTRL_ADC_SEL3		(1<<8)
#define BIT_SM_CTRL_ADC_PU		(1<<9)	/* power up */
#define BIT_SM_CTRL_ADC_CKSEL0		(1<<10) /* clock divider select */
#define BIT_SM_CTRL_ADC_CKSEL1		(1<<11)
#define BIT_SM_CTRL_ADC_START		(1<<12) /* start digitalization process */
#define BIT_SM_CTRL_ADC_RESET		(1<<13) /* reset afer power up */
#define BIT_SM_CTRL_ADC_BG_RDY		(1<<14)	/* bandgap reference block ready */
#define BIT_SM_CTRL_ADC_CONT		(1<<15) /* continuous v.s single shot */
#define BIT_SM_CTRL_ADC_BUF_EN		(1<<16) /* enable anolog input buffer */
#define BIT_SM_CTRL_ADC_VREF_SEL	(1<<17) /* ext. v.s. int. ref select */
#define BIT_SM_CTRL_TSEN_EN		(1<<20)
#define BIT_SM_CTRL_TSEN_CLK_SEL	(1<<21)
#define BIT_SM_CTRL_TSEN_MODE_SEL	(1<<22)

#if defined(CONFIG_BERLIN2CD)
#define TSEN_WAIT_MAX			50000
#define NR_ATTRS			3
#else /* !defined(CONFIG_BERLIN2CD) */
#define BITS_SM_CTRL_ADC_SEL		5
#define BITS_SM_CTRL_ADC_CKSEL		10

#define TSEN_WAIT_MAX			20
#define ADC_WAIT_MAX			20
#define ADC_SAMPLES			256
#define NR_ATTRS			8
#endif /* defined(CONFIG_BERLIN2CD) */

struct mv_hwmon {
	struct platform_device         *hwmon_pdev;
	struct device                  *hwmon_dev;
	struct sensor_device_attribute *attrs[NR_ATTRS];
	struct mutex                   lock;
#if !defined(CONFIG_BERLIN2CD)
	struct mutex                   lock_adc;

	int adc_data;
	int adc_enable;
#endif /* !defined(CONFIG_BERLIN2CD) */

	int tsen_temp;
	int tsen_temp_raw;
	int tsen_enable;
};

static struct mv_hwmon *gmh;

/*
 * TSEN_EN = 1
 * TSEN_CLK_SEL = 0, 1.25Mhz
 * TSEN_MODE_SEL = 0, 0 ~ 125 degree Centigrade
 */
static void tsen_enable(struct mv_hwmon *mh)
{
#if !defined(CONFIG_BERLIN2CD)
	unsigned int val = 0;

#endif /* !defined(CONFIG_BERLIN2CD) */
	mutex_lock(&mh->lock);
#if !defined(CONFIG_BERLIN2CD)
	val = __raw_readl(SM_CTRL);
	val |= BIT_SM_CTRL_TSEN_EN;
	val &= ~BIT_SM_CTRL_TSEN_CLK_SEL;
	val &= ~BIT_SM_CTRL_TSEN_MODE_SEL;
	__raw_writel(val, SM_CTRL);

#endif /* !defined(CONFIG_BERLIN2CD) */
	mh->tsen_enable = 1;
	mutex_unlock(&mh->lock);
}

static void tsen_disable(struct mv_hwmon *mh)
{
#if !defined(CONFIG_BERLIN2CD)
	unsigned int val = 0;

#endif /* !defined(CONFIG_BERLIN2CD) */
	mutex_lock(&mh->lock);
#if !defined(CONFIG_BERLIN2CD)
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_TSEN_EN;
	__raw_writel(val, SM_CTRL);

#endif /* !defined(CONFIG_BERLIN2CD) */
	mh->tsen_enable = 0;
	mutex_unlock(&mh->lock);
}

static int tsen_raw_read(struct mv_hwmon *mh)
{
#if defined(CONFIG_BERLIN2CD)
	int data, wait = 0;
	unsigned int status, val;
#else /* !defined(CONFIG_BERLIN2CD) */
	int data = 0;
	unsigned int status = 0;
	int wait = 0;
#endif /* defined(CONFIG_BERLIN2CD) */

	if (mh->tsen_enable == 0) {
		printk(KERN_WARNING "%s, tsen is not enabled yet.\n", __func__);
		return 0;
	}

	mutex_lock(&mh->lock);
#if defined(CONFIG_BERLIN2CD)
	val = __raw_readl(SM_CTRL);
	val &= ~(1 << 29);
	__raw_writel(val, SM_CTRL);

	val = __raw_readl(TSEN_ADC_CTRL);
	val &= ~(0xf << 21);
	val |= (8 << 22);
	val |= (1 << 21);
	__raw_writel(val, TSEN_ADC_CTRL);

	val = __raw_readl(SM_CTRL);
	val |= (1 << 19);
	__raw_writel(val, SM_CTRL);

	val = __raw_readl(TSEN_ADC_CTRL);
	val |= (1 << 8);
	__raw_writel(val, TSEN_ADC_CTRL);
#endif /* defined(CONFIG_BERLIN2CD) */
	while (1) {
		status = __raw_readl(TSEN_ADC_STATUS);
		if (status & 0x1)
			break;
		if (++wait > TSEN_WAIT_MAX) {
			mutex_unlock(&mh->lock);
			printk(KERN_WARNING "%s timeout, status 0x%08x.\n",
					__func__, status);
			return -1;
		}
	}

	data = __raw_readl(TSEN_ADC_DATA);
#if !defined(CONFIG_BERLIN2CD)
	/* 10 bit resolution */
	data &= 0x2FF;
#endif /* !defined(CONFIG_BERLIN2CD) */
	status &= ~0x1;
	__raw_writel(status, TSEN_ADC_STATUS);

#if defined(CONFIG_BERLIN2CD)
	val = __raw_readl(TSEN_ADC_CTRL);
	val &= ~(1 << 8);
	__raw_writel(val, TSEN_ADC_CTRL);
#endif /* defined(CONFIG_BERLIN2CD) */
	mutex_unlock(&mh->lock);

	return data;
}

static int tsen_celcius_calc(struct mv_hwmon *mh)
{
	int data;

	data = tsen_raw_read(mh);
	if (data < 0)
		printk(KERN_ERR "%s, tsen_raw_read fail\n", __func__);
#if defined(CONFIG_BERLIN2CD)
	if (data > 2047)
		data = -(4096 - data);
	else
		data = data;
	data = (data * 100) / 264 - 270;
#else /* !defined(CONFIG_BERLIN2CD) */
	data -= 527;
	data = (data*100)/197;
#endif /* defined(CONFIG_BERLIN2CD) */
	mh->tsen_temp = data;

	return data;
}

static ssize_t tsen_temp_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	if (mh->tsen_enable == 0)
		return sprintf(buf, "%s\n", "invalid");
	tsen_celcius_calc(mh);
	return sprintf(buf, "%u\n", mh->tsen_temp);
}

static ssize_t tsen_temp_raw_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	if (mh->tsen_enable == 0)
		return sprintf(buf, "%s\n", "invalid");
	mh->tsen_temp_raw = tsen_raw_read(mh);
	return sprintf(buf, "%u\n", mh->tsen_temp_raw);
}

static ssize_t tsen_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", mh->tsen_enable);
}

static ssize_t tsen_enable_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	unsigned long en;

	if (strict_strtoul(buf, 10, &en))
		return -EINVAL;

	if (en)
		tsen_enable(mh);
	else
		tsen_disable(mh);

	return count;
}

#if !defined(CONFIG_BERLIN2CD)
#ifndef CONFIG_MV88DE3100_SM
/*
 * ADC_CKSEL = 0, clk/2
 */
static void adc_init(struct mv_hwmon *mh)
{
	unsigned int val = 0;

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_CKSEL0;
	val &= ~BIT_SM_CTRL_ADC_CKSEL1;
	val |= BIT_SM_CTRL_ADC_PU;
	val |= BIT_SM_CTRL_ADC_RESET;
	__raw_writel(val, SM_CTRL);
	mutex_unlock(&mh->lock);
	msleep(1);

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_RESET;
	__raw_writel(val, SM_CTRL);
	mutex_unlock(&mh->lock);
}
#endif
#endif /* !defined(CONFIG_BERLIN2CD) */

static void adc_reset(struct mv_hwmon *mh)
{
#if defined(CONFIG_BERLIN2CD)
	unsigned int val;
#else /* !defined(CONFIG_BERLIN2CD) */
	unsigned int val = 0;
#endif /* defined(CONFIG_BERLIN2CD) */

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val |= BIT_SM_CTRL_SOC2SM_SW_INTR;
	val |= BIT_SM_CTRL_ADC_PU;
	val |= BIT_SM_CTRL_ADC_SEL0;
	val |= BIT_SM_CTRL_ADC_SEL1;
	val |= BIT_SM_CTRL_ADC_SEL2;
	val &= ~BIT_SM_CTRL_ADC_SEL3;
	val &= ~BIT_SM_CTRL_ADC_CKSEL0;
	val &= ~BIT_SM_CTRL_ADC_CKSEL1;
	val &= ~BIT_SM_CTRL_ADC_START;
	val &= ~BIT_SM_CTRL_ADC_RESET;
	__raw_writel(val, SM_CTRL);
	__raw_writel(0, SM_ADC_STATUS);
#if defined(CONFIG_BERLIN2CD)
	val |= BIT_SM_CTRL_ADC_RESET;
	__raw_writel(val, SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_RESET;
	__raw_writel(val, SM_CTRL);
#endif /* defined(CONFIG_BERLIN2CD) */
	mutex_unlock(&mh->lock);
}

#if !defined(CONFIG_BERLIN2CD)
/*
 * ADC_BUF_EN = 0, disable
 * ADC_CONT = 1, single shot mode
 */
static void adc_enable(struct mv_hwmon *mh)
{
	unsigned int val = 0;

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_RESET;
	val |= BIT_SM_CTRL_ADC_PU;
	val &= ~BIT_SM_CTRL_ADC_BUF_EN;
	val |= BIT_SM_CTRL_ADC_BG_RDY;
	val &= ~BIT_SM_CTRL_ADC_CONT;
	__raw_writel(val, SM_CTRL);

	mh->adc_enable = 1;
	mutex_unlock(&mh->lock);
}

static void adc_disable(struct mv_hwmon *mh)
{
	unsigned int val = 0;

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_PU;
	__raw_writel(val, SM_CTRL);

	mh->adc_enable = 0;
	mutex_unlock(&mh->lock);
}

/*
 * ch: 0 ~ 15
 * clk: 0(clk/2), 1(clk/3), 2(clk/4), 3(clk/8)
 */
static void adc_select(struct mv_hwmon *mh, int ch, int clk)
{
	unsigned int val = 0;

	if (ch < 0 || ch > 15 || clk < 0 || clk > 3) {
		printk(KERN_ERR "%s fail, ch: %d, clk %d\n", __func__, ch, clk);
		return;
	}
	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= (~(15<<BITS_SM_CTRL_ADC_SEL) | (ch<<BITS_SM_CTRL_ADC_SEL));
	val &= (~(3<<BITS_SM_CTRL_ADC_CKSEL) | (clk<<BITS_SM_CTRL_ADC_CKSEL));
	__raw_writel(val, SM_CTRL);
	mutex_unlock(&mh->lock);
}

static void adc_start(struct mv_hwmon *mh)
{
	unsigned int val = 0;

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val |= BIT_SM_CTRL_ADC_START;
	__raw_writel(val, SM_CTRL);
	mutex_unlock(&mh->lock);
}

static void adc_stop(struct mv_hwmon *mh)
{
	unsigned int val = 0;

	mutex_lock(&mh->lock);
	val = __raw_readl(SM_CTRL);
	val &= ~BIT_SM_CTRL_ADC_START;
	__raw_writel(val, SM_CTRL);
	mutex_unlock(&mh->lock);
}

static int adc_raw_read(struct mv_hwmon *mh, int ch)
{
	int data = 0;
	unsigned int status = 0;
	int wait = 0;

	if (mh->adc_enable == 0) {
		printk(KERN_WARNING "%s, adc is not enabled yet.\n", __func__);
		return 0;
	}

	mutex_lock(&mh->lock);
	while (1) {
		status = __raw_readl(SM_ADC_STATUS);
		if (status & (1 << ch))
			break;
		if (++wait > ADC_WAIT_MAX) {
			mutex_unlock(&mh->lock);
			printk(KERN_WARNING "%s timeout, status 0x%08x.\n",
					__func__, status);
			return -1;
		}
	}
	data = __raw_readl(SM_ADC_DATA);
	/* 10 bit resolution */
	data &= 0x2FF;
	status &= 0xFFFF0000;
	__raw_writel(status, SM_ADC_STATUS);
	mutex_unlock(&mh->lock);

	return data;
}

static ssize_t adc_data_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	struct sensor_device_attribute *sa = to_sensor_dev_attr(attr);
	int i, sum;

	if (mh->adc_enable == 0)
		return sprintf(buf, "%s\n", "invalid");
	mutex_lock(&mh->lock_adc);
	adc_select(mh, sa->index, 0);
	for (i = 0, sum = 0; i < ADC_SAMPLES; i++) {
		adc_start(mh);
		sum += adc_raw_read(mh, sa->index);
	}
	mh->adc_data = sum/ADC_SAMPLES;
	adc_stop(mh);
	adc_reset(mh);
	mutex_unlock(&mh->lock_adc);

	return sprintf(buf, "%d\n", mh->adc_data);
}

static ssize_t adc_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", mh->adc_enable);
}

static ssize_t adc_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct mv_hwmon *mh = dev_get_drvdata(dev);
	unsigned long en;

	if (strict_strtoul(buf, 10, &en))
		return -EINVAL;

	if (en)
		adc_enable(mh);
	else
		adc_disable(mh);

	return count;
}
#endif /* !defined(CONFIG_BERLIN2CD) */

static struct sensor_device_attribute mv_sensor_attrs[] = {
#if !defined(CONFIG_BERLIN2CD)
	SENSOR_ATTR(adc_ch0_data, S_IRUGO, adc_data_show, NULL, 0),
	SENSOR_ATTR(adc_ch1_data, S_IRUGO, adc_data_show, NULL, 1),
	SENSOR_ATTR(adc_ch2_data, S_IRUGO, adc_data_show, NULL, 2),
	SENSOR_ATTR(adc_ch3_data, S_IRUGO, adc_data_show, NULL, 3),
	SENSOR_ATTR(adc_enable, 644, adc_enable_show, adc_enable_store, 4),
#endif /* !defined(CONFIG_BERLIN2CD) */
	SENSOR_ATTR(tsen_temp, S_IRUGO, tsen_temp_show, NULL, 5),
	SENSOR_ATTR(tsen_temp_raw, S_IRUGO, tsen_temp_raw_show, NULL, 6),
	SENSOR_ATTR(tsen_enable, 644, tsen_enable_show, tsen_enable_store, 7),
};

static int mv_hwmon_create_attrs(struct device *dev,
				 struct sensor_device_attribute *attrs[])
{
	int i, ni;
	int ret;

	for (i = 0; i < NR_ATTRS; i++) {
		attrs[i] = &mv_sensor_attrs[i];
		sysfs_attr_init(&attrs[i]->dev_attr.attr);
		ret =  device_create_file(dev, &attrs[i]->dev_attr);
		if (ret < 0) {
			printk(KERN_ERR "%s, device create file fail (%s).\n",
					__func__, attrs[i]->dev_attr.attr.name);
			goto out_device_create_file;
		}
	}

	return 0;

out_device_create_file:
	ni = i;
	for (i = 0; i < ni; i++)
		device_remove_file(dev, &attrs[i]->dev_attr);
	return ret;
}

static void mv_hwmon_remove_attrs(struct device *dev,
				  struct sensor_device_attribute *attrs[])
{
	int i;
	for (i = 0; i < NR_ATTRS; i++) {
		if (attrs[i] == NULL)
			continue;
		device_remove_file(dev, &attrs[i]->dev_attr);
	}
}

static int __devinit mv_hwmon_probe(struct platform_device *pdev)
{
	int ret;
	struct mv_hwmon *mh = gmh;

	mh->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(mh->hwmon_dev)) {
		printk(KERN_ERR "%s, register hwmon device fail.\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	dev_set_drvdata(mh->hwmon_dev, mh);

	ret = mv_hwmon_create_attrs(mh->hwmon_dev, mh->attrs);
	if (ret) {
		printk(KERN_ERR "%s, create attrs fail.\n", __func__);
		ret = -EINVAL;
		goto out_create_attrs;
	}

	mutex_init(&mh->lock);
#if defined(CONFIG_BERLIN2CD)
	adc_reset(mh);
#else /* !defined(CONFIG_BERLIN2CD) */
	mutex_init(&mh->lock_adc);
#ifndef CONFIG_MV88DE3100_SM
	adc_init(mh);
#endif
#endif /* defined(CONFIG_BERLIN2CD) */
	printk(KERN_INFO "%s, done. (mh: 0x%p)\n", __func__, mh);
	return 0;

out_create_attrs:
	hwmon_device_unregister(mh->hwmon_dev);
out:
	return ret;
}

static int __devexit mv_hwmon_remove(struct platform_device *pdev)
{
	mv_hwmon_remove_attrs(gmh->hwmon_dev, gmh->attrs);
	hwmon_device_unregister(gmh->hwmon_dev);
	return 0;
}

static struct platform_driver mv_hwmon_driver = {
	.driver	= {
		.name  = MV_HWMON_DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe  = mv_hwmon_probe,
	.remove = __devexit_p(mv_hwmon_remove),
};

static int __init mv_hwmon_init(void)
{
	int ret;
	struct mv_hwmon *mh;
	struct platform_device *pdev;

	mh = kzalloc(sizeof(struct mv_hwmon), GFP_KERNEL);
	if (mh == NULL) {
		printk(KERN_ERR "%s, alloc memory fail.\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	gmh = mh;

	pdev = platform_device_alloc(MV_HWMON_DRV_NAME, 0);
	if (!pdev) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s, device alloc failed.\n", __func__);
		goto out_platform_device_alloc;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR "%s, device add failed (%d)\n", __func__, ret);
		goto out_platform_device_add;
	}
	mh->hwmon_pdev = pdev;

	ret = platform_driver_register(&mv_hwmon_driver);
	if (ret != 0) {
		printk(KERN_ERR "%s, device add failed (%d)\n", __func__, ret);
		goto out_platform_driver_register;
	}
	return 0;

out_platform_driver_register:
	platform_device_del(pdev);
out_platform_device_add:
	platform_device_put(pdev);
out_platform_device_alloc:
	kfree(mh);
out:
	return ret;
}

static void __exit mv_hwmon_exit(void)
{
	platform_device_unregister(gmh->hwmon_pdev);
	platform_driver_unregister(&mv_hwmon_driver);
	kfree(gmh);
}

module_init(mv_hwmon_init);
module_exit(mv_hwmon_exit);

MODULE_AUTHOR("Marvell-Galois");
MODULE_DESCRIPTION("Marvell Hwmon Driver");
MODULE_LICENSE("GPL");
