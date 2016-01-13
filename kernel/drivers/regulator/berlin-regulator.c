/*
 * berlin-regulator.c  --  Marvell berlin
 * based on tps65910-regulator.c
 *
 * Copyright 2013 Marvell Inc.
 *
 * Author: Fan Wang <evanwang@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>
#include <mach/gpio.h>

/* supported VMMC voltages in uV */
static const unsigned int VMMC_VSEL_table[] = {
	3300000,
};
/* supported VQMMC voltages in uV */
static const unsigned int VQMMC_VSEL_table[] = {
	1800000, 3300000,
};

struct berlin_info {
	const char *name;
	const char *vin_name;
	u8 n_voltages;
	const unsigned int *voltage_table;
	int enable_time_us;
};

struct berlin_regulator_init_data {
	int idx;
	struct regulator_init_data *init_data;
	struct device_node *node;
	int gpio_num;
	bool is_sm;
};

/* regulator index definitions */
#define BERLIN_REG_VMMC		0
#define BERLIN_REG_VQMMC	1
static struct berlin_info berlin_regs[] = {
	{
		.name = "vmmc_sd",
		.n_voltages = ARRAY_SIZE(VMMC_VSEL_table),
		.voltage_table = VMMC_VSEL_table,
		.enable_time_us = 100,
	},
	{
		.name = "vqmmc_sd",
		.n_voltages = ARRAY_SIZE(VQMMC_VSEL_table),
		.voltage_table = VQMMC_VSEL_table,
		.enable_time_us = 100,
	},
};

struct berlin_reg {
	struct regulator_desc *desc;
	struct regulator_dev **rdev;
	struct berlin_info **info;
	struct berlin_regulator_init_data **berlin_data;
	int num_regulators;
};


static int berlin_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	switch (mode) {
	case REGULATOR_MODE_NORMAL:
	case REGULATOR_MODE_IDLE:
	case REGULATOR_MODE_STANDBY:
		return 0;
	}

	return -EINVAL;
}

static unsigned int berlin_get_mode(struct regulator_dev *dev)
{
	return 0;
}

static int berlin_get_voltage(struct regulator_dev *dev)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int ret = 1, value, id = rdev_get_id(dev);
	int gpio_num = pmic->berlin_data[id]->gpio_num;
	bool is_sm = pmic->berlin_data[id]->is_sm;

	pr_debug("in %s, idx:%d\n", __func__, pmic->berlin_data[id]->idx);
	switch (pmic->berlin_data[id]->idx) {
	case BERLIN_REG_VMMC:
		value = pmic->info[id]->voltage_table[0];
		pr_debug("value:%d\n", value);
		break;
	case BERLIN_REG_VQMMC:
		pr_debug("%s:gpio_num:%d, is_sm:%d\n", __func__, gpio_num, is_sm);
		if (is_sm)
			SM_GPIO_PortRead(gpio_num, &ret);
		else
			GPIO_PortRead(gpio_num, &ret);
		value = pmic->info[id]->voltage_table[ret];
		break;
	default:
		value = pmic->info[id]->voltage_table[0];
	}
	return value;
}

static int berlin_set_voltage_gpio_sel(struct regulator_dev *dev,
					 unsigned selector)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev), ret = 0;
	int gpio_num = pmic->berlin_data[id]->gpio_num;
	bool is_sm = pmic->berlin_data[id]->is_sm;

	pr_debug("in %s, idx:%d,selector:%d\n", __func__,
			pmic->berlin_data[id]->idx, selector);
	pr_debug("%s:gpio_num:%d, is_sm:%d\n", __func__, gpio_num, is_sm);

	switch (pmic->berlin_data[id]->idx) {
	case BERLIN_REG_VMMC:
		break;
	case BERLIN_REG_VQMMC:
		if (gpio_num >= 0) {
			if (selector == 0) { /* 1v8 */
				if (is_sm)
					SM_GPIO_PortWrite(gpio_num, 0);
				else
					GPIO_PortWrite(gpio_num, 0);
			} else { /* 3v3 */
				if (is_sm)
					SM_GPIO_PortWrite(gpio_num, 1);
				else
					GPIO_PortWrite(gpio_num, 1);
			}
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}

	return ret;
}

static int berlin_list_voltage(struct regulator_dev *dev,
					unsigned selector)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);

	pr_debug("in %s, n_voltages:%d, selector:%d\n", __func__, pmic->desc[id].n_voltages,
			selector);
	if (selector >= pmic->desc[id].n_voltages)
		return -EINVAL;
	pr_debug("voltage selected:%d\n", pmic->info[id]->voltage_table[selector]);

	return pmic->info[id]->voltage_table[selector];
}

static int berlin_gpio_regulator_is_enabled(struct regulator_dev *dev)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev), ret = 0;
	int gpio_num = pmic->berlin_data[id]->gpio_num;
	bool is_sm = pmic->berlin_data[id]->is_sm;

	switch (pmic->berlin_data[id]->idx) {
	case BERLIN_REG_VMMC:
		pr_debug("%s:gpio_num:%d, is_sm:%d\n", __func__, gpio_num, is_sm);
		if (gpio_num >= 0) {
			if (is_sm)
				SM_GPIO_PortRead(gpio_num, &ret);
			else
				GPIO_PortRead(gpio_num, &ret);
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = 1;
		break;
	}
	pr_debug("in %s, is_enabled:%d\n", __func__, ret);
	return ret;
}

static int berlin_enable(struct regulator_dev *dev)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev), ret = 0;
	int gpio_num = pmic->berlin_data[id]->gpio_num;
	bool is_sm = pmic->berlin_data[id]->is_sm;

	switch (pmic->berlin_data[id]->idx) {
	case BERLIN_REG_VMMC:
		if (gpio_num >= 0) {
			if (is_sm)
				SM_GPIO_PortWrite(gpio_num, 1);
			else
				GPIO_PortWrite(gpio_num, 1);
		} else {
			ret = -EINVAL;
		}

		break;
	default:
		break;
	}
	return ret;
}

static int berlin_disable(struct regulator_dev *dev)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev), ret = 0;
	int gpio_num = pmic->berlin_data[id]->gpio_num;
	bool is_sm = pmic->berlin_data[id]->is_sm;

	switch (pmic->berlin_data[id]->idx) {
	case BERLIN_REG_VMMC:
		if (gpio_num >= 0) {
			if (is_sm)
				SM_GPIO_PortWrite(gpio_num, 0);
			else
				GPIO_PortWrite(gpio_num, 0);
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int berlin_enable_time(struct regulator_dev *dev)
{
	struct berlin_reg *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	return pmic->info[id]->enable_time_us;
}

/* Regulator ops */
static struct regulator_ops berlin_ops_gpio = {
	.is_enabled		= berlin_gpio_regulator_is_enabled,
	.enable			= berlin_enable,
	.disable		= berlin_disable,
	.enable_time		= berlin_enable_time,
	.set_mode		= berlin_set_mode,
	.get_mode		= berlin_get_mode,
	.get_voltage		= berlin_get_voltage,
	.set_voltage_sel	= berlin_set_voltage_gpio_sel,
	.list_voltage		= berlin_list_voltage,
};

static int berlin_get_num_regulators_dt(struct platform_device *pdev)
{
	struct device_node *np, *regulators;
	int num = 0;

	np = pdev->dev.of_node;
	regulators = of_find_node_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&pdev->dev, "regulator node not found\n");
		return 0;
	}
	for_each_child_of_node(regulators, np)
		num++;
	return num;
}

static int berlin_parse_dt_reg_data(
		struct platform_device *pdev, struct berlin_reg *pmic)
{
	struct device_node *np, *regulators;
	struct berlin_regulator_init_data *berlin_data, *p;
	int idx = 0;

	np = pdev->dev.of_node;
	regulators = of_find_node_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&pdev->dev, "regulator node not found\n");
		return -EINVAL;
	}

	berlin_data = devm_kzalloc(&pdev->dev, sizeof(*berlin_data)
			* pmic->num_regulators,	GFP_KERNEL);
	if (!berlin_data) {
		dev_err(&pdev->dev, "Memory allocation failed for init_data\n");
		return -ENOMEM;
	}

	p = berlin_data;
	for_each_child_of_node(regulators, np) {
		for (idx = 0; idx < ARRAY_SIZE(berlin_regs); idx++) {
			if (!of_node_cmp(np->name,
					berlin_regs[idx].name)) {
				p->idx = idx;
				p->init_data = of_get_regulator_init_data(
							&pdev->dev, np);
				if (of_property_read_u32(
						np,
						"pwr-gpio", &p->gpio_num))
					p->gpio_num = -1;

				p->is_sm = of_property_match_string(
					np,
					"gpio-type", "sm") >= 0 ? true : false;
				p->node = np;
				pmic->berlin_data[idx] = p;
				pr_debug("idx:%d,init_data:%p,node:%p,\
					gpio_num:%d, is_sm:%d\n",
					idx, p->init_data, np,
					p->gpio_num, p->is_sm);
				p++;
				break;
			}
		}
	}
	if (berlin_data == p)
		return -EINVAL;
	return 0;
}

static int berlin_pmic_probe(struct platform_device *pdev)
{
	struct berlin_info *info;
	struct regulator_init_data *reg_data;
	struct regulator_dev *rdev;
	struct berlin_reg *pmic;
	struct regulator_config config = { };
	int i, err;

	pr_debug("berlin pmic probe.\n");
	pmic = devm_kzalloc(&pdev->dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic) {
		dev_err(&pdev->dev, "Memory allocation failed for pmic\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, pmic);

	pmic->num_regulators = berlin_get_num_regulators_dt(pdev);
	pr_debug("num_regulators:%d\n", pmic->num_regulators);

	pmic->desc = devm_kzalloc(&pdev->dev, pmic->num_regulators *
			sizeof(struct regulator_desc), GFP_KERNEL);
	if (!pmic->desc) {
		dev_err(&pdev->dev, "Memory alloc fails for desc\n");
		return -ENOMEM;
	}

	pmic->info = devm_kzalloc(&pdev->dev, pmic->num_regulators *
			sizeof(struct berlin_info *), GFP_KERNEL);
	if (!pmic->info) {
		dev_err(&pdev->dev, "Memory alloc fails for info\n");
		return -ENOMEM;
	}

	pmic->berlin_data = devm_kzalloc(&pdev->dev, pmic->num_regulators *
			sizeof(struct berlin_regulator_init_data *), GFP_KERNEL);
	if (!pmic->berlin_data) {
		dev_err(&pdev->dev, "Memory alloc fails for data\n");
		return -ENOMEM;
	}

	pmic->rdev = devm_kzalloc(&pdev->dev, pmic->num_regulators *
			sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!pmic->rdev) {
		dev_err(&pdev->dev, "Memory alloc fails for rdev\n");
		return -ENOMEM;
	}

	if (berlin_parse_dt_reg_data(pdev, pmic) < 0)
		return -EINVAL;

	for (i = 0; i < pmic->num_regulators; i++) {

		reg_data = pmic->berlin_data[i]->init_data;

		/* Regulator API handles empty constraints but not NULL
		 * constraints */
		if (!reg_data)
			continue;

		/* Register the regulators */
		info = berlin_regs + pmic->berlin_data[i]->idx;
		pmic->info[i] = info;

		pmic->desc[i].name = info->name;
		pmic->desc[i].supply_name = info->vin_name;
		pmic->desc[i].id = i;
		pmic->desc[i].n_voltages = info->n_voltages;

		if (pmic->berlin_data[i]->idx == BERLIN_REG_VMMC ||
				pmic->berlin_data[i]->idx == BERLIN_REG_VQMMC) {
			pmic->desc[i].ops = &berlin_ops_gpio;
		} else {
			pmic->desc[i].ops = &berlin_ops_gpio;
		}

		pmic->desc[i].type = REGULATOR_VOLTAGE;
		pmic->desc[i].owner = THIS_MODULE;

		config.dev = &pdev->dev;
		config.init_data = reg_data;
		config.driver_data = pmic;
		config.of_node = pmic->berlin_data[i]->node;
		rdev = regulator_register(&pmic->desc[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				pdev->name);
			err = PTR_ERR(rdev);
			goto err_unregister_regulator;
		}

		/* Save regulator for cleanup */
		pmic->rdev[i] = rdev;
	}
	return 0;

err_unregister_regulator:
	while (--i >= 0)
		regulator_unregister(pmic->rdev[i]);
	return err;
}

static int berlin_pmic_remove(struct platform_device *pdev)
{
	struct berlin_reg *pmic = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pmic->num_regulators; i++)
		regulator_unregister(pmic->rdev[i]);

	return 0;
}

static void berlin_pmic_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id berlin_pmic_of_match[] = {
	{.compatible = "mrvl,berlin-pmic",},
	{},
};
MODULE_DEVICE_TABLE(of, berlin_pmic_of_match);
static struct platform_driver berlin_pmic_driver = {
	.driver = {
		.name = "berlin-pmic",
		.of_match_table = berlin_pmic_of_match,
		.owner = THIS_MODULE,
	},
	.probe = berlin_pmic_probe,
	.remove = berlin_pmic_remove,
	.shutdown = berlin_pmic_shutdown,
};

static int __init berlin_pmic_init(void)
{
	return platform_driver_register(&berlin_pmic_driver);
}
subsys_initcall(berlin_pmic_init);

static void __exit berlin_pmic_cleanup(void)
{
	platform_driver_unregister(&berlin_pmic_driver);
}
module_exit(berlin_pmic_cleanup);

MODULE_AUTHOR("Fan Wang <evanwang@marvell.com>");
MODULE_DESCRIPTION("Marvell Berlin BG2 voltage regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:berlin-pmic");
