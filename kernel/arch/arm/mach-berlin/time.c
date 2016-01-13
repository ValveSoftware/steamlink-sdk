/*
 * SoC's APB timer handling. Based on arch/arm/mach-picoxcell/time.c
 *
 * Author:	Jisheng Zhang <jszhang@marvell.com>
 * Copyright:	Marvell International Ltd.
 * Copyright (c) 2011 Picochip Ltd., Jamie Iles
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/dw_apb_timer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <asm/mach/time.h>
#include <asm/sched_clock.h>
#include <asm/smp_twd.h>

static struct dw_apb_clocksource *cs;

static void timer_get_base_and_rate(struct device_node *np,
				    void __iomem **base, u32 *rate)
{
	*base = of_iomap(np, 0);

	if (!*base)
		panic("Unable to map regs for %s", np->name);

	if (of_property_read_u32(np, "clock-freq", rate))
		panic("No clock-freq property for %s", np->name);
}

static void apb_add_clockevent(struct device_node *event_timer)
{
	void __iomem *iobase;
	struct dw_apb_clock_event_device *ced;
	u32 irq, rate;

	irq = irq_of_parse_and_map(event_timer, 0);
	if (irq == NO_IRQ)
		panic("No IRQ for clock event timer");

	timer_get_base_and_rate(event_timer, &iobase, &rate);

	ced = dw_apb_clockevent_init(0, event_timer->name, 300, iobase, irq,
				     rate);
	if (!ced)
		panic("Unable to initialise clockevent device");

	dw_apb_clockevent_register(ced);
}

static u32 apb_read_sched_clock(void)
{
	return (u32)dw_apb_clocksource_read(cs);
}

static void apb_add_clocksource(struct device_node *source_timer)
{
	void __iomem *iobase;
	u32 rate;

	timer_get_base_and_rate(source_timer, &iobase, &rate);

	cs = dw_apb_clocksource_init(300, source_timer->name, iobase, rate);
	if (!cs)
		panic("Unable to initialise clocksource device");

	dw_apb_clocksource_start(cs);
	dw_apb_clocksource_register(cs);

	setup_sched_clock(apb_read_sched_clock, 32, rate);
}

static const struct of_device_id apb_timer_ids[] __initconst = {
	{ .compatible = "berlin,apb-timer" },
	{},
};

static void __init apb_init(void)
{
	struct device_node *event_timer, *source_timer;

	event_timer = of_find_matching_node(NULL, apb_timer_ids);
	if (!event_timer)
		panic("No timer for clockevent");
	apb_add_clockevent(event_timer);
	of_node_put(event_timer);

	source_timer = of_find_matching_node(event_timer, apb_timer_ids);
	if (!source_timer)
		panic("No timer for clocksource");
	apb_add_clocksource(source_timer);

	of_node_put(source_timer);

	twd_local_timer_of_register();
}

struct sys_timer apb_timer = {
	.init = apb_init,
};
