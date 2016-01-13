/*
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/
#define APB_UART_INST0_BASE		0xf7fc9000

	.macro	addruart, rp, rv, tmp
	ldr	\rp, =APB_UART_INST0_BASE
	ldr	\rv, =APB_UART_INST0_BASE	@ virtual = physical
	.endm

#define UART_SHIFT	2
#include <asm/hardware/debug-8250.S>
