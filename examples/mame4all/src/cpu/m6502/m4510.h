/*****************************************************************************
 *
 *	 m4510.c
 *	 Portable 4510 emulator V1.0beta
 *
 *	 Copyright (c) 2000 Peter Trauner, all rights reserved.
 *
 *	 - This source code is released as freeware for non-commercial purposes.
 *	 - You are free to use and redistribute this code in modified or
 *	   unmodified form, provided you list me in the credits.
 *	 - If you modify this source code, you must add a notice to each modified
 *	   source file that it has been changed.  If you're a nice person, you
 *	   will clearly mark each change too.  :)
 *	 - If you wish to use this for commercial purposes, please contact me at
 *	   pullmoll@t-online.de
 *	 - The author of this copywritten work reserves the right to change the
 *	   terms of its usage and license at any time, including retroactively
 *	 - This entire notice must remain in the source code.
 *
 *****************************************************************************/

#ifndef _M4510_H
#define _M4510_H

#include "cpuintrf.h"
#include "osd_cpu.h"
#include "m6502.h"

enum {
	M4510_PC=1, M4510_S, M4510_P, M4510_A, M4510_X, M4510_Y,
	M4510_Z, M4510_B, M4510_EA, M4510_ZP,
	M4510_NMI_STATE, M4510_IRQ_STATE, 
	M4510_MEM_LOW,M4510_MEM_HIGH,
	M4510_MEM0, M4510_MEM1, M4510_MEM2, M4510_MEM3,
	M4510_MEM4, M4510_MEM5, M4510_MEM6, M4510_MEM7
};

#define M4510_INT_NONE	M6502_INT_NONE
#define M4510_INT_IRQ 	M6502_INT_IRQ
#define M4510_INT_NMI 	M6502_INT_NMI

#define M4510_NMI_VEC 	M6502_NMI_VEC
#define M4510_RST_VEC 	M6502_RST_VEC
#define M4510_IRQ_VEC 	M6502_IRQ_VEC

extern int m4510_ICount;				/* cycle count */

extern void m4510_reset(void *param);
extern void m4510_exit(void);
extern int	m4510_execute(int cycles);
extern unsigned m4510_get_context (void *dst);
extern void m4510_set_context (void *src);
extern unsigned m4510_get_pc (void);
extern void m4510_set_pc (unsigned val);
extern unsigned m4510_get_sp (void);
extern void m4510_set_sp (unsigned val);
extern unsigned m4510_get_reg (int regnum);
extern void m4510_set_reg (int regnum, unsigned val);
extern void m4510_set_nmi_line(int state);
extern void m4510_set_irq_line(int irqline, int state);
extern void m4510_set_irq_callback(int (*callback)(int irqline));
extern void m4510_state_save(void *file);
extern void m4510_state_load(void *file);
extern const char *m4510_info(void *context, int regnum);
extern unsigned m4510_dasm(char *buffer, unsigned pc);

#endif


