/*****************************************************************************
 *
 *	 sc61860.h
 *	 portable sharp 61860 emulator interface
 *   (sharp pocket computers)
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
 *	   peter.trauner@jk.uni-linz.ac.at
 *	 - The author of this copywritten work reserves the right to change the
 *	   terms of its usage and license at any time, including retroactively
 *	 - This entire notice must remain in the source code.
 *
 *****************************************************************************/
#ifndef _SC61860_H
#define _SC61860_H

/* unsolved problems
   the processor has 8 kbyte internal rom
   only readable with special instructions and program execution
   96 byte internal ram (used by most operations)
   64 kb external ram (first 8kbyte not seen for program execution?) */

#include "cpuintrf.h"
#include "osd_cpu.h"

typedef struct {
	int (*reset)(void);
	int (*brk)(void);
	int (*ina)(void);
	void (*outa)(int);
	int (*inb)(void);
	void (*outb)(int);
	void (*outc)(int);
} SC61860_CONFIG;

extern int sc61860_icount;				/* cycle count */

extern void sc61860_reset(void *param);
extern void sc61860_exit(void);
extern int sc61860_execute(int cycles);
extern unsigned sc61860_get_context(void *dst);
extern void sc61860_set_context(void *src);
extern unsigned sc61860_get_pc(void);
extern void sc61860_set_pc(unsigned val);
extern unsigned sc61860_get_sp(void);
extern void sc61860_set_sp(unsigned val);
extern unsigned sc61860_get_reg(int regnum);
extern void sc61860_set_reg(int regnum, unsigned val);
extern void sc61860_set_nmi_line(int state);
extern void sc61860_set_irq_line(int irqline, int state);
extern void sc61860_set_irq_callback(int (*callback)(int irqline));
extern void sc61860_state_save(void *file);
extern void sc61860_state_load(void *file);
extern const char *sc61860_info(void *context, int regnum);
extern unsigned sc61860_dasm(char *buffer, unsigned pc);

/* add these in the memory region for better usage of mame debugger */
READ_HANDLER(sc61860_read_internal);
WRITE_HANDLER(sc61860_write_internal);

// timer_pulse(1/500.0, 0,sc61860_2ms_tick)
void sc61860_2ms_tick(int param);
// this is though for power on/off of the sharps
UINT8 *sc61860_internal_ram(void);

#endif

