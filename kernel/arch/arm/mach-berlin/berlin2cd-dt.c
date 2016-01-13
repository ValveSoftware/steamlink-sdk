/*
 *  MARVELL BERLIN2CD Flattened Device Tree enabled machine
 *
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *		http://www.marvell.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/of_platform.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>

#include "common.h"

static void __init berlin2cd_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static char const *berlin2cd_dt_compat[] __initdata = {
	"marvell,berlin2cd",
	NULL
};

DT_MACHINE_START(BERLIN2CD_DT, "MV88DE3108")
	/* Maintainer: Jisheng Zhang<jszhang@marvell.com> */
	.map_io		= berlin_map_io,
        .init_early     = berlin2cd_reset,
	.init_irq	= berlin_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer		= &apb_timer,
	.init_machine	= berlin2cd_init,
	.dt_compat	= berlin2cd_dt_compat,
MACHINE_END
