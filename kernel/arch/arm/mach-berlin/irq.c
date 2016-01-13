/*
 *  Generic IRQ handling, APB Peripheral IRQ demultiplexing, etc.
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/cpu_pm.h>

#include <asm/hardware/gic.h>
#include <asm/mach/irq.h>
#include <asm/hardware/cache-l2x0.h>

#define MAX_ICTL_NR		2

#define APB_ICTL_INTEN		0x00
#define APB_ICTL_INTMASK	0x08
#define APB_ICTL_FINALSTATUS	0x30

struct apb_ictl_data {
	int			nr_irqs;
	unsigned int		virq_base;
	unsigned int		cascade_irq;
	void __iomem 		*ictl_base;
	struct irq_domain	*domain;
#ifdef CONFIG_CPU_PM
	u32			ictl_intmask;
	u32			ictl_inten;
#endif
};

static struct apb_ictl_data apb_data[MAX_ICTL_NR];
static int max_ictl_nr;

static inline u32 rdl(struct apb_ictl_data *ictl, int offset)
{
	return readl_relaxed(ictl->ictl_base + offset);
}

static inline void wrl(struct apb_ictl_data *ictl, int offset, u32 value)
{
	writel_relaxed(value, ictl->ictl_base + offset);
}

static void apb_enable_irq(struct irq_data *d)
{
	u32 u;
	struct irq_domain *domain = d->domain;
	struct apb_ictl_data *data = (struct apb_ictl_data *)domain->host_data;
	int pin = d->irq - data->virq_base;

	u = rdl(data, APB_ICTL_INTMASK);
	u &= ~(1 << (pin & 31));
	wrl(data, APB_ICTL_INTMASK, u);

	u = rdl(data, APB_ICTL_INTEN);
	u |= (1 << (pin & 31));
	wrl(data, APB_ICTL_INTEN, u);
}

static void apb_disable_irq(struct irq_data *d)
{
	u32 u;
	struct irq_domain *domain = d->domain;
	struct apb_ictl_data *data = (struct apb_ictl_data *)domain->host_data;
	int pin = d->irq - data->virq_base;

	u = rdl(data, APB_ICTL_INTMASK);
	u |= (1 << (pin & 31));
	wrl(data, APB_ICTL_INTMASK, u);

	u = rdl(data, APB_ICTL_INTEN);
	u &= ~(1 << (pin & 31));
	wrl(data, APB_ICTL_INTEN, u);
}

static void apb_mask_irq(struct irq_data *d)
{
	u32 u;
	struct irq_domain *domain = d->domain;
	struct apb_ictl_data *data = (struct apb_ictl_data *)domain->host_data;
	int pin = d->irq - data->virq_base;

	u = rdl(data, APB_ICTL_INTMASK);
	u |= (1 << (pin & 31));
	wrl(data, APB_ICTL_INTMASK, u);
}

static void apb_unmask_irq(struct irq_data *d)
{
	u32 u;
	struct irq_domain *domain = d->domain;
	struct apb_ictl_data *data = (struct apb_ictl_data *)domain->host_data;
	int pin = d->irq - data->virq_base;

	u = rdl(data, APB_ICTL_INTMASK);
	u &= ~(1 << (pin & 31));
	wrl(data, APB_ICTL_INTMASK, u);
}

static struct irq_chip apb_irq_chip = {
	.name		= "apb_ictl",
	.irq_enable	= apb_enable_irq,
	.irq_disable	= apb_disable_irq,
	.irq_mask	= apb_mask_irq,
	.irq_unmask	= apb_unmask_irq,
};

static void apb_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	int i;
	struct irq_domain *domain;
	struct apb_ictl_data *data;
	unsigned long mask, status, n;
	struct irq_chip *chip = irq_desc_get_chip(desc);

	chained_irq_enter(chip, desc);
	for (i = 0; i < max_ictl_nr; i++) {
		if (irq == apb_data[i].cascade_irq) {
			domain = apb_data[i].domain;
			data = (struct apb_ictl_data *)domain->host_data;
			break;
		}
	}
	if (i >= max_ictl_nr) {
		chained_irq_exit(chip, desc);
		pr_err("Spurious irq %d in APB ICTL\n", irq);
		return;
	}

	mask = rdl(data, APB_ICTL_INTMASK);
	while (1) {
		status = rdl(data, APB_ICTL_FINALSTATUS);
		status &= ~mask;
		if (status == 0)
			break;
		for_each_set_bit(n, &status, BITS_PER_LONG) {
			generic_handle_irq(apb_data[i].virq_base + n);
		}
	}
	chained_irq_exit(chip, desc);
}

static int apb_irq_domain_map(struct irq_domain *d, unsigned int irq,
			      irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &apb_irq_chip, handle_level_irq);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}

static int apb_irq_domain_xlate(struct irq_domain *d, struct device_node *node,
				const u32 *intspec, unsigned int intsize,
				unsigned long *out_hwirq,
				unsigned int *out_type)
{
	*out_hwirq = intspec[0];
	return 0;
}

const struct irq_domain_ops apb_irq_domain_ops = {
	.map		= apb_irq_domain_map,
	.xlate		= apb_irq_domain_xlate,
};

#ifdef CONFIG_CPU_PM
static inline void apb_ictl_save(unsigned int ictl_nr)
{
	struct apb_ictl_data *ictl = &apb_data[ictl_nr];

	ictl->ictl_intmask = rdl(ictl, APB_ICTL_INTMASK);
	ictl->ictl_inten = rdl(ictl, APB_ICTL_INTEN);
}

static inline void apb_ictl_restore(unsigned int ictl_nr)
{
	struct apb_ictl_data *ictl = &apb_data[ictl_nr];

	wrl(ictl, APB_ICTL_INTMASK, ictl->ictl_intmask);
	wrl(ictl, APB_ICTL_INTEN, ictl->ictl_inten);
}

static int apb_ictl_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	int i;

	for (i = 0; i < max_ictl_nr; i++) {
		switch (cmd) {
		case CPU_CLUSTER_PM_ENTER:
			apb_ictl_save(i);
			break;
		case CPU_CLUSTER_PM_ENTER_FAILED:
		case CPU_CLUSTER_PM_EXIT:
			apb_ictl_restore(i);
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block apb_ictl_notifier_block = {
	.notifier_call = apb_ictl_notifier,
};

static void __init apb_ictl_pm_init(struct apb_ictl_data *ictl)
{
	if (ictl == &apb_data[0])
		cpu_pm_register_notifier(&apb_ictl_notifier_block);
}
#else
static void __init apb_ictl_pm_init(struct apb_ictl_data *ictl)
{
}
#endif
static int __init apb_ictl_init(struct device_node *node, struct device_node *parent)
{
	int i, irq_base, ret;
	struct resource res;
	u32 cnt;

	if ((max_ictl_nr + 1) > MAX_ICTL_NR) {
		pr_err("Spurious APB ICTL DT node\n");
		ret = -EINVAL;
		goto err;
	}

	i = max_ictl_nr;
	ret = of_property_read_u32(node, "ictl-nr-irqs",
				   &cnt);
	if (ret) {
		pr_err("Not found ictl-nr-irqs property\n");
		ret = -EINVAL;
		goto err;
	}

	apb_data[i].ictl_base = of_iomap(node, 0);
	ret = of_address_to_resource(node, 0, &res);
	if (!apb_data[i].ictl_base) {
		pr_err("Not found reg property\n");
		ret = -EINVAL;
		goto err;
	}

	apb_data[i].cascade_irq = irq_of_parse_and_map(node, 0);
	if (!apb_data[i].cascade_irq) {
		pr_err("Not found irq property\n");
		ret = -EINVAL;
		goto err;
	}

	irq_base = irq_alloc_descs(-1, 0, cnt, 0);
	if (irq_base < 0) {
		pr_err("Failed to allocate IRQ numbers for apb ictl\n");
		ret = irq_base;
		goto err;
	}
	irq_set_chained_handler(apb_data[i].cascade_irq,
				apb_irq_demux);
	apb_data[i].nr_irqs = cnt;
	apb_data[i].virq_base = irq_base;
	apb_data[i].domain = irq_domain_add_legacy(node, cnt,
						   irq_base, 0,
						   &apb_irq_domain_ops,
						   &apb_data[i]);
	++max_ictl_nr;
	apb_ictl_pm_init(&apb_data[i]);
	return 0;
err:
	of_node_put(node);
	return ret;

}

static const struct of_device_id berlin_dt_irq_match[] = {
	{ .compatible = "arm,cortex-a9-gic", .data = gic_of_init, },
	{ .compatible = "snps,dw-apb-ictl", .data = apb_ictl_init, },
	{},
};

void __init berlin_init_irq(void)
{
	l2x0_of_init(0x70c00000, 0xfeffffff);
	of_irq_init(berlin_dt_irq_match);
}
