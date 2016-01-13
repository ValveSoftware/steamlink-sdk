/*
 *  Marvell Berlin SoC clk driver
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/galois_platform.h>

#include "clock.h"

static inline u32 rdlc(int offset)
{
	return __raw_readl(IOMEM(MEMMAP_CHIP_CTRL_REG_BASE + offset));
}

static u32 vcodiv[] = {10, 15, 20, 25, 30, 40, 50, 60, 80};
static u32 clkdiv[] = {1, 2, 4, 6, 8, 12, 1, 1};

static inline u32 cpu0_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_CLKSWITCH);
	if ((val >> 8) & 0x1)
		return 3;
	if ((val >> 7) & 0x1) {
		u32 clksel = (rdlc(RA_GBL_CLKSELECT) >> 9) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}

static inline u32 cfg_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_CLKSWITCH);
	if ((val >> 17) & 0x1)
		return 3;
	if ((val >> 16) & 0x1) {
		u32 clksel = (rdlc(RA_GBL_CLKSELECT) >> 26) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}

static inline u32 perif_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_CLKSWITCH);
	if ((val >> 26) & 0x1)
		return 3;
	if ((val >> 25) & 0x1) {
		u32 clksel = (rdlc(RA_GBL_CLKSELECT1) >> 12) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}

static inline u32 sdioxin_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_SDIOXINCLKCTRL);
	if ((val >> 6) & 0x1)
		return 3;
	if ((val >> 5) & 0x1) {
		u32 clksel = (val >> 7) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}

static inline u32 sdio1xin_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_SDIO1XINCLKCTRL);
	if ((val >> 6) & 0x1)
		return 3;
	if ((val >> 5) & 0x1) {
		u32 clksel = (val >> 7) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}

static inline u32 nfcecc_get_divider(void)
{
	u32 val;

	val = rdlc(RA_GBL_CLKSWITCH1);
	if ((val >> 3) & 0x1)
		return 3;
	if ((val >> 2) & 0x1) {
		u32 clksel = rdlc(RA_GBL_CLKSELECT2) & 0x7;
		return clkdiv[clksel];
	} else
		return 1;
}


/* input clock is 25MHZ */
#define CLKIN 25
unsigned long get_pll(struct clk *clk)
{
	u32 val, fbdiv, rfdiv, vcodivsel;
	unsigned long pll;

	val = rdlc(clk->ctl);
	fbdiv = (val >> 6) & 0x1ff;
	rfdiv = (val >> 1) & 0x1f;
	if (rfdiv == 0)
		rfdiv = 1;
	pll = CLKIN * fbdiv / rfdiv;
	val = rdlc(clk->ctl1);
	vcodivsel = (val >> 7) & 0xf;
	pll = pll * 10 / vcodiv[vcodivsel];

	return pll;
}

static unsigned long twd_get_rate(struct clk *clk)
{
	unsigned long pll = get_pll(clk);
	u32 divider = cpu0_get_divider();
	return 1000000 * pll / divider / 3;
}

static struct clkops twd_clk_ops = {
	.getrate	= twd_get_rate,
};

static struct clk twd_clk = {
	.ctl	= RA_GBL_CPUPLLCTL,
	.ctl1	= RA_GBL_CPUPLLCTL1,
	.ops	= &twd_clk_ops,
};

CLK(cpu0, RA_GBL_CPUPLLCTL, RA_GBL_CPUPLLCTL1);
CLK(cfg, RA_GBL_SYSPLLCTL, RA_GBL_SYSPLLCTL1);
CLK(perif, RA_GBL_SYSPLLCTL, RA_GBL_SYSPLLCTL1);
CLK(sdioxin, RA_GBL_SYSPLLCTL, RA_GBL_SYSPLLCTL1);
CLK(sdio1xin, RA_GBL_SYSPLLCTL, RA_GBL_SYSPLLCTL1);
CLK(nfcecc, RA_GBL_SYSPLLCTL, RA_GBL_SYSPLLCTL1);

static struct clk_lookup clks[] = {
	CLK_LOOKUP(NULL, "cpu0", cpu0_clk),
	CLK_LOOKUP(NULL, "cfg", cfg_clk),
	CLK_LOOKUP(NULL, "perif", perif_clk),
	CLK_LOOKUP("smp_twd", NULL, twd_clk),
	CLK_LOOKUP("f7ab0000.sdhci", NULL, sdioxin_clk),
	CLK_LOOKUP("f7ab0800.sdhci", NULL, sdio1xin_clk),
	CLK_LOOKUP("f7ab1000.sdhci", NULL, nfcecc_clk),
	CLK_LOOKUP("f7e81400.i2c", NULL, cfg_clk),
	CLK_LOOKUP("f7e81800.i2c", NULL, cfg_clk),
	CLK_LOOKUP("f7fc7000.i2c", NULL, cfg_clk),
	CLK_LOOKUP("f7fc8000.i2c", NULL, cfg_clk),
};

void __init berlin_clk_init(void)
{
	clkdev_add_table(clks, ARRAY_SIZE(clks));
}

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long rate;

	if (clk->ops->getrate)
		rate = clk->ops->getrate(clk);
	else
		rate = 0;

	return rate;
}
EXPORT_SYMBOL(clk_get_rate);
