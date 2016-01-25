/*****************************************************************************
 *
 *	 z80.c
 *	 Portable Z80 emulator V2.8
 *
 *	 Copyright (C) 1998,1999,2000 Juergen Buchmueller, all rights reserved.
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
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *	 Changes in 3.0
 *	 - 'finished' switch to dynamically overrideable cycle count tables
 *   Changes in 2.9:
 *	 - added methods to access and override the cycle count tables
 *	 - fixed handling and timing of multiple DD/FD prefixed opcodes
 *   Changes in 2.8:
 *	 - OUTI/OUTD/OTIR/OTDR also pre-decrement the B register now.
 *	   This was wrong because of a bug fix on the wrong side
 *	   (astrocade sound driver).
 *   Changes in 2.7:
 *	  - removed z80_vm specific code, it's not needed (and never was).
 *   Changes in 2.6:
 *	  - BUSY_LOOP_HACKS needed to call change_pc16() earlier, before
 *		checking the opcodes at the new address, because otherwise they
 *		might access the old (wrong or even NULL) banked memory region.
 *		Thanks to Sean Young for finding this nasty bug.
 *   Changes in 2.5:
 *	  - Burning cycles always adjusts the ICount by a multiple of 4.
 *	  - In REPEAT_AT_ONCE cases the R register wasn't incremented twice
 *		per repetition as it should have been. Those repeated opcodes
 *		could also underflow the ICount.
 *	  - Simplified TIME_LOOP_HACKS for BC and added two more for DE + HL
 *		timing loops. I think those hacks weren't endian safe before too.
 *   Changes in 2.4:
 *	  - z80_reset zaps the entire context, sets IX and IY to 0xffff(!) and
 *		sets the Z flag. With these changes the Tehkan World Cup driver
 *		_seems_ to work again.
 *   Changes in 2.3:
 *	  - External termination of the execution loop calls z80_burn() and
 *		z80_vm_burn() to burn an amount of cycles (R adjustment)
 *	  - Shortcuts which burn CPU cycles (BUSY_LOOP_HACKS and TIME_LOOP_HACKS)
 *		now also adjust the R register depending on the skipped opcodes.
 *   Changes in 2.2:
 *	  - Fixed bugs in CPL, SCF and CCF instructions flag handling.
 *	  - Changed variable EA and ARG16() function to UINT32; this
 *		produces slightly more efficient code.
 *	  - The DD/FD XY CB opcodes where XY is 40-7F and Y is not 6/E
 *		are changed to calls to the X6/XE opcodes to reduce object size.
 *		They're hardly ever used so this should not yield a speed penalty.
 *	 New in 2.0:
 *    - Optional more exact Z80 emulation (#define Z80_EXACT 1) according
 *		to a detailed description by Sean Young which can be found at:
 *		http://www.msxnet.org/tech/Z80/z80undoc.txt
 *****************************************************************************/

#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "z80.h"

/****************************************************************************/
/* The Z80 registers. HALT is set to 1 when the CPU is halted, the refresh  */
/* register is calculated as follows: refresh=(Regs.R&127)|(Regs.R2&128)    */
/****************************************************************************/
typedef struct {
/* 00 */    PAIR    PREPC,PC,SP,AF,BC,DE,HL,IX,IY;
/* 24 */    PAIR    AF2,BC2,DE2,HL2;
/* 34 */    UINT8   R,R2,IFF1,IFF2,HALT,IM,I;
/* 3B */    UINT8   irq_max;            /* number of daisy chain devices        */
/* 3C */	INT8	request_irq;		/* daisy chain next request device		*/
/* 3D */	INT8	service_irq;		/* daisy chain next reti handling device */
/* 3E */	UINT8	nmi_state;			/* nmi line state */
/* 3F */	UINT8	irq_state;			/* irq line state */
/* 40 */    UINT8   int_state[Z80_MAXDAISY];
/* 44 */    Z80_DaisyChain irq[Z80_MAXDAISY];
/* 84 */    int     (*irq_callback)(int irqline);
/* 88 */    int     extra_cycles;       /* extra cycles for interrupts */
}   Z80_Regs;

#define CF  0x01
#define NF	0x02
#define PF	0x04
#define VF	PF
#define XF	0x08
#define HF	0x10
#define YF	0x20
#define ZF	0x40
#define SF	0x80

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

#define	_PPC	Z80.PREPC.d		/* previous program counter */

#define _PCD	Z80.PC.d
#define _PC 	Z80.PC.w.l

#define _SPD	Z80.SP.d
#define _SP 	Z80.SP.w.l

#define _AFD	Z80.AF.d
#define _AF 	Z80.AF.w.l
#define _A		Z80.AF.b.h
#define _F		Z80.AF.b.l

#define _BCD	Z80.BC.d
#define _BC 	Z80.BC.w.l
#define _B		Z80.BC.b.h
#define _C		Z80.BC.b.l

#define _DED	Z80.DE.d
#define _DE 	Z80.DE.w.l
#define _D		Z80.DE.b.h
#define _E		Z80.DE.b.l

#define _HLD	Z80.HL.d
#define _HL 	Z80.HL.w.l
#define _H		Z80.HL.b.h
#define _L		Z80.HL.b.l

#define _IXD	Z80.IX.d
#define _IX 	Z80.IX.w.l
#define _HX 	Z80.IX.b.h
#define _LX 	Z80.IX.b.l

#define _IYD	Z80.IY.d
#define _IY 	Z80.IY.w.l
#define _HY 	Z80.IY.b.h
#define _LY 	Z80.IY.b.l

#define _I      Z80.I
#define _R      Z80.R
#define _R2     Z80.R2
#define _IM     Z80.IM
#define _IFF1	Z80.IFF1
#define _IFF2	Z80.IFF2
#define _HALT	Z80.HALT

int z80_ICount;
static Z80_Regs Z80;
static UINT32 EA;
static int after_EI = 0;

static UINT8 SZ[256];		/* zero and sign flags */
static UINT8 SZ_BIT[256];	/* zero, sign and parity/overflow (=zero) flags for BIT opcode */
static UINT8 SZP[256];		/* zero, sign and parity flags */
static UINT8 SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static UINT8 SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */

static UINT8 *SZHVC_add = 0;
static UINT8 *SZHVC_sub = 0;

/* tmp1 value for ini/inir/outi/otir for [C.1-0][io.1-0] */
static UINT8 irep_tmp1[4][4] = {
	{0,0,1,0},{0,1,0,1},{1,0,1,1},{0,1,1,0}
};

/* tmp1 value for ind/indr/outd/otdr for [C.1-0][io.1-0] */
static UINT8 drep_tmp1[4][4] = {
	{0,1,0,0},{1,0,0,1},{0,0,1,0},{0,1,0,1}
};

/* tmp2 value for all in/out repeated opcodes for B.7-0 */
static UINT8 breg_tmp2[256] = {
	0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,
	0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,
	1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1
};

static UINT8 cc_op[0x100] = {
 4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,
 8,10, 7, 6, 4, 4, 7, 4,12,11, 7, 6, 4, 4, 7, 4,
 7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4,
 7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 5,10,10,10,10,11, 7,11, 5,10,10, 0,10,17, 7,11,
 5,10,10,11,10,11, 7,11, 5, 4,10,11,10, 0, 7,11,
 5,10,10,19,10,11, 7,11, 5, 4,10, 4,10, 0, 7,11,
 5,10,10, 4,10,11, 7,11, 5, 6,10, 4,10, 0, 7,11};

static UINT8 cc_cb[0x100] = {
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8};

static UINT8 cc_ed[0x100] = {
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
12,12,15,20, 8, 8, 8, 9,12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8, 9,12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8,18,12,12,15,20, 8, 8, 8,18,
12,12,15,20, 8, 8, 8, 8,12,12,15,20, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

static UINT8 cc_xy[0x100] = {
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4,14,20,10, 9, 9, 9, 4, 4,15,20,10, 9, 9, 9, 4,
 4, 4, 4, 4,23,23,19, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 9, 9, 9, 9, 9, 9,19, 9, 9, 9, 9, 9, 9, 9,19, 9,
19,19,19,19,19,19, 4,19, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
 4,14, 4,23, 4,15, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,10, 4, 4, 4, 4, 4, 4};

static UINT8 cc_xycb[0x100] = {
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23};

/* extra cycles if jr/jp/call taken and 'interrupt latency' on rst 0-7 */
static UINT8 cc_ex[0x100] = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* DJNZ */
 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,	/* JR NZ/JR Z */
 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,	/* JR NC/JR C */
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 5, 5, 5, 5, 0, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0,	/* LDIR/CPIR/INIR/OTIR LDDR/CPDR/INDR/OTDR */
 6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
 6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
 6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2,
 6, 0, 0, 0, 7, 0, 0, 2, 6, 0, 0, 0, 7, 0, 0, 2};

static UINT8 *cc[6] = { cc_op, cc_cb, cc_ed, cc_xy, cc_xycb, cc_ex };
#define Z80_TABLE_dd	Z80_TABLE_xy
#define Z80_TABLE_fd	Z80_TABLE_xy

static void take_interrupt(void);

#define PROTOTYPES(tablename,prefix) \
	INLINE void prefix##_00(void); INLINE void prefix##_01(void); INLINE void prefix##_02(void); INLINE void prefix##_03(void); \
	INLINE void prefix##_04(void); INLINE void prefix##_05(void); INLINE void prefix##_06(void); INLINE void prefix##_07(void); \
	INLINE void prefix##_08(void); INLINE void prefix##_09(void); INLINE void prefix##_0a(void); INLINE void prefix##_0b(void); \
	INLINE void prefix##_0c(void); INLINE void prefix##_0d(void); INLINE void prefix##_0e(void); INLINE void prefix##_0f(void); \
	INLINE void prefix##_10(void); INLINE void prefix##_11(void); INLINE void prefix##_12(void); INLINE void prefix##_13(void); \
	INLINE void prefix##_14(void); INLINE void prefix##_15(void); INLINE void prefix##_16(void); INLINE void prefix##_17(void); \
	INLINE void prefix##_18(void); INLINE void prefix##_19(void); INLINE void prefix##_1a(void); INLINE void prefix##_1b(void); \
	INLINE void prefix##_1c(void); INLINE void prefix##_1d(void); INLINE void prefix##_1e(void); INLINE void prefix##_1f(void); \
	INLINE void prefix##_20(void); INLINE void prefix##_21(void); INLINE void prefix##_22(void); INLINE void prefix##_23(void); \
	INLINE void prefix##_24(void); INLINE void prefix##_25(void); INLINE void prefix##_26(void); INLINE void prefix##_27(void); \
	INLINE void prefix##_28(void); INLINE void prefix##_29(void); INLINE void prefix##_2a(void); INLINE void prefix##_2b(void); \
	INLINE void prefix##_2c(void); INLINE void prefix##_2d(void); INLINE void prefix##_2e(void); INLINE void prefix##_2f(void); \
	INLINE void prefix##_30(void); INLINE void prefix##_31(void); INLINE void prefix##_32(void); INLINE void prefix##_33(void); \
	INLINE void prefix##_34(void); INLINE void prefix##_35(void); INLINE void prefix##_36(void); INLINE void prefix##_37(void); \
	INLINE void prefix##_38(void); INLINE void prefix##_39(void); INLINE void prefix##_3a(void); INLINE void prefix##_3b(void); \
	INLINE void prefix##_3c(void); INLINE void prefix##_3d(void); INLINE void prefix##_3e(void); INLINE void prefix##_3f(void); \
	INLINE void prefix##_40(void); INLINE void prefix##_41(void); INLINE void prefix##_42(void); INLINE void prefix##_43(void); \
	INLINE void prefix##_44(void); INLINE void prefix##_45(void); INLINE void prefix##_46(void); INLINE void prefix##_47(void); \
	INLINE void prefix##_48(void); INLINE void prefix##_49(void); INLINE void prefix##_4a(void); INLINE void prefix##_4b(void); \
	INLINE void prefix##_4c(void); INLINE void prefix##_4d(void); INLINE void prefix##_4e(void); INLINE void prefix##_4f(void); \
	INLINE void prefix##_50(void); INLINE void prefix##_51(void); INLINE void prefix##_52(void); INLINE void prefix##_53(void); \
	INLINE void prefix##_54(void); INLINE void prefix##_55(void); INLINE void prefix##_56(void); INLINE void prefix##_57(void); \
	INLINE void prefix##_58(void); INLINE void prefix##_59(void); INLINE void prefix##_5a(void); INLINE void prefix##_5b(void); \
	INLINE void prefix##_5c(void); INLINE void prefix##_5d(void); INLINE void prefix##_5e(void); INLINE void prefix##_5f(void); \
	INLINE void prefix##_60(void); INLINE void prefix##_61(void); INLINE void prefix##_62(void); INLINE void prefix##_63(void); \
	INLINE void prefix##_64(void); INLINE void prefix##_65(void); INLINE void prefix##_66(void); INLINE void prefix##_67(void); \
	INLINE void prefix##_68(void); INLINE void prefix##_69(void); INLINE void prefix##_6a(void); INLINE void prefix##_6b(void); \
	INLINE void prefix##_6c(void); INLINE void prefix##_6d(void); INLINE void prefix##_6e(void); INLINE void prefix##_6f(void); \
	INLINE void prefix##_70(void); INLINE void prefix##_71(void); INLINE void prefix##_72(void); INLINE void prefix##_73(void); \
	INLINE void prefix##_74(void); INLINE void prefix##_75(void); INLINE void prefix##_76(void); INLINE void prefix##_77(void); \
	INLINE void prefix##_78(void); INLINE void prefix##_79(void); INLINE void prefix##_7a(void); INLINE void prefix##_7b(void); \
	INLINE void prefix##_7c(void); INLINE void prefix##_7d(void); INLINE void prefix##_7e(void); INLINE void prefix##_7f(void); \
	INLINE void prefix##_80(void); INLINE void prefix##_81(void); INLINE void prefix##_82(void); INLINE void prefix##_83(void); \
	INLINE void prefix##_84(void); INLINE void prefix##_85(void); INLINE void prefix##_86(void); INLINE void prefix##_87(void); \
	INLINE void prefix##_88(void); INLINE void prefix##_89(void); INLINE void prefix##_8a(void); INLINE void prefix##_8b(void); \
	INLINE void prefix##_8c(void); INLINE void prefix##_8d(void); INLINE void prefix##_8e(void); INLINE void prefix##_8f(void); \
	INLINE void prefix##_90(void); INLINE void prefix##_91(void); INLINE void prefix##_92(void); INLINE void prefix##_93(void); \
	INLINE void prefix##_94(void); INLINE void prefix##_95(void); INLINE void prefix##_96(void); INLINE void prefix##_97(void); \
	INLINE void prefix##_98(void); INLINE void prefix##_99(void); INLINE void prefix##_9a(void); INLINE void prefix##_9b(void); \
	INLINE void prefix##_9c(void); INLINE void prefix##_9d(void); INLINE void prefix##_9e(void); INLINE void prefix##_9f(void); \
	INLINE void prefix##_a0(void); INLINE void prefix##_a1(void); INLINE void prefix##_a2(void); INLINE void prefix##_a3(void); \
	INLINE void prefix##_a4(void); INLINE void prefix##_a5(void); INLINE void prefix##_a6(void); INLINE void prefix##_a7(void); \
	INLINE void prefix##_a8(void); INLINE void prefix##_a9(void); INLINE void prefix##_aa(void); INLINE void prefix##_ab(void); \
	INLINE void prefix##_ac(void); INLINE void prefix##_ad(void); INLINE void prefix##_ae(void); INLINE void prefix##_af(void); \
	INLINE void prefix##_b0(void); INLINE void prefix##_b1(void); INLINE void prefix##_b2(void); INLINE void prefix##_b3(void); \
	INLINE void prefix##_b4(void); INLINE void prefix##_b5(void); INLINE void prefix##_b6(void); INLINE void prefix##_b7(void); \
	INLINE void prefix##_b8(void); INLINE void prefix##_b9(void); INLINE void prefix##_ba(void); INLINE void prefix##_bb(void); \
	INLINE void prefix##_bc(void); INLINE void prefix##_bd(void); INLINE void prefix##_be(void); INLINE void prefix##_bf(void); \
	INLINE void prefix##_c0(void); INLINE void prefix##_c1(void); INLINE void prefix##_c2(void); INLINE void prefix##_c3(void); \
	INLINE void prefix##_c4(void); INLINE void prefix##_c5(void); INLINE void prefix##_c6(void); INLINE void prefix##_c7(void); \
	INLINE void prefix##_c8(void); INLINE void prefix##_c9(void); INLINE void prefix##_ca(void); INLINE void prefix##_cb(void); \
	INLINE void prefix##_cc(void); INLINE void prefix##_cd(void); INLINE void prefix##_ce(void); INLINE void prefix##_cf(void); \
	INLINE void prefix##_d0(void); INLINE void prefix##_d1(void); INLINE void prefix##_d2(void); INLINE void prefix##_d3(void); \
	INLINE void prefix##_d4(void); INLINE void prefix##_d5(void); INLINE void prefix##_d6(void); INLINE void prefix##_d7(void); \
	INLINE void prefix##_d8(void); INLINE void prefix##_d9(void); INLINE void prefix##_da(void); INLINE void prefix##_db(void); \
	INLINE void prefix##_dc(void); INLINE void prefix##_dd(void); INLINE void prefix##_de(void); INLINE void prefix##_df(void); \
	INLINE void prefix##_e0(void); INLINE void prefix##_e1(void); INLINE void prefix##_e2(void); INLINE void prefix##_e3(void); \
	INLINE void prefix##_e4(void); INLINE void prefix##_e5(void); INLINE void prefix##_e6(void); INLINE void prefix##_e7(void); \
	INLINE void prefix##_e8(void); INLINE void prefix##_e9(void); INLINE void prefix##_ea(void); INLINE void prefix##_eb(void); \
	INLINE void prefix##_ec(void); INLINE void prefix##_ed(void); INLINE void prefix##_ee(void); INLINE void prefix##_ef(void); \
	INLINE void prefix##_f0(void); INLINE void prefix##_f1(void); INLINE void prefix##_f2(void); INLINE void prefix##_f3(void); \
	INLINE void prefix##_f4(void); INLINE void prefix##_f5(void); INLINE void prefix##_f6(void); INLINE void prefix##_f7(void); \
	INLINE void prefix##_f8(void); INLINE void prefix##_f9(void); INLINE void prefix##_fa(void); INLINE void prefix##_fb(void); \
	INLINE void prefix##_fc(void); INLINE void prefix##_fd(void); INLINE void prefix##_fe(void); INLINE void prefix##_ff(void); \
static void (*tablename[0x100])(void) = {	\
    prefix##_00,prefix##_01,prefix##_02,prefix##_03,prefix##_04,prefix##_05,prefix##_06,prefix##_07, \
    prefix##_08,prefix##_09,prefix##_0a,prefix##_0b,prefix##_0c,prefix##_0d,prefix##_0e,prefix##_0f, \
    prefix##_10,prefix##_11,prefix##_12,prefix##_13,prefix##_14,prefix##_15,prefix##_16,prefix##_17, \
    prefix##_18,prefix##_19,prefix##_1a,prefix##_1b,prefix##_1c,prefix##_1d,prefix##_1e,prefix##_1f, \
    prefix##_20,prefix##_21,prefix##_22,prefix##_23,prefix##_24,prefix##_25,prefix##_26,prefix##_27, \
    prefix##_28,prefix##_29,prefix##_2a,prefix##_2b,prefix##_2c,prefix##_2d,prefix##_2e,prefix##_2f, \
    prefix##_30,prefix##_31,prefix##_32,prefix##_33,prefix##_34,prefix##_35,prefix##_36,prefix##_37, \
    prefix##_38,prefix##_39,prefix##_3a,prefix##_3b,prefix##_3c,prefix##_3d,prefix##_3e,prefix##_3f, \
    prefix##_40,prefix##_41,prefix##_42,prefix##_43,prefix##_44,prefix##_45,prefix##_46,prefix##_47, \
    prefix##_48,prefix##_49,prefix##_4a,prefix##_4b,prefix##_4c,prefix##_4d,prefix##_4e,prefix##_4f, \
    prefix##_50,prefix##_51,prefix##_52,prefix##_53,prefix##_54,prefix##_55,prefix##_56,prefix##_57, \
    prefix##_58,prefix##_59,prefix##_5a,prefix##_5b,prefix##_5c,prefix##_5d,prefix##_5e,prefix##_5f, \
    prefix##_60,prefix##_61,prefix##_62,prefix##_63,prefix##_64,prefix##_65,prefix##_66,prefix##_67, \
    prefix##_68,prefix##_69,prefix##_6a,prefix##_6b,prefix##_6c,prefix##_6d,prefix##_6e,prefix##_6f, \
    prefix##_70,prefix##_71,prefix##_72,prefix##_73,prefix##_74,prefix##_75,prefix##_76,prefix##_77, \
    prefix##_78,prefix##_79,prefix##_7a,prefix##_7b,prefix##_7c,prefix##_7d,prefix##_7e,prefix##_7f, \
    prefix##_80,prefix##_81,prefix##_82,prefix##_83,prefix##_84,prefix##_85,prefix##_86,prefix##_87, \
    prefix##_88,prefix##_89,prefix##_8a,prefix##_8b,prefix##_8c,prefix##_8d,prefix##_8e,prefix##_8f, \
    prefix##_90,prefix##_91,prefix##_92,prefix##_93,prefix##_94,prefix##_95,prefix##_96,prefix##_97, \
    prefix##_98,prefix##_99,prefix##_9a,prefix##_9b,prefix##_9c,prefix##_9d,prefix##_9e,prefix##_9f, \
    prefix##_a0,prefix##_a1,prefix##_a2,prefix##_a3,prefix##_a4,prefix##_a5,prefix##_a6,prefix##_a7, \
    prefix##_a8,prefix##_a9,prefix##_aa,prefix##_ab,prefix##_ac,prefix##_ad,prefix##_ae,prefix##_af, \
    prefix##_b0,prefix##_b1,prefix##_b2,prefix##_b3,prefix##_b4,prefix##_b5,prefix##_b6,prefix##_b7, \
    prefix##_b8,prefix##_b9,prefix##_ba,prefix##_bb,prefix##_bc,prefix##_bd,prefix##_be,prefix##_bf, \
    prefix##_c0,prefix##_c1,prefix##_c2,prefix##_c3,prefix##_c4,prefix##_c5,prefix##_c6,prefix##_c7, \
    prefix##_c8,prefix##_c9,prefix##_ca,prefix##_cb,prefix##_cc,prefix##_cd,prefix##_ce,prefix##_cf, \
    prefix##_d0,prefix##_d1,prefix##_d2,prefix##_d3,prefix##_d4,prefix##_d5,prefix##_d6,prefix##_d7, \
    prefix##_d8,prefix##_d9,prefix##_da,prefix##_db,prefix##_dc,prefix##_dd,prefix##_de,prefix##_df, \
    prefix##_e0,prefix##_e1,prefix##_e2,prefix##_e3,prefix##_e4,prefix##_e5,prefix##_e6,prefix##_e7, \
    prefix##_e8,prefix##_e9,prefix##_ea,prefix##_eb,prefix##_ec,prefix##_ed,prefix##_ee,prefix##_ef, \
    prefix##_f0,prefix##_f1,prefix##_f2,prefix##_f3,prefix##_f4,prefix##_f5,prefix##_f6,prefix##_f7, \
	prefix##_f8,prefix##_f9,prefix##_fa,prefix##_fb,prefix##_fc,prefix##_fd,prefix##_fe,prefix##_ff  \
}

PROTOTYPES(Z80op,op);
PROTOTYPES(Z80cb,cb);
PROTOTYPES(Z80dd,dd);
PROTOTYPES(Z80ed,ed);
PROTOTYPES(Z80fd,fd);
PROTOTYPES(Z80xycb,xycb);

/****************************************************************************/
/* Burn an odd amount of cycles, that is instructions taking something      */
/* different from 4 T-states per opcode (and R increment)                   */
/****************************************************************************/
INLINE void BURNODD(int cycles, int opcodes, int cyclesum)
{
    if( cycles > 0 )
    {
		_R += (cycles / cyclesum) * opcodes;
		z80_ICount -= (cycles / cyclesum) * cyclesum;
    }
}

/***************************************************************
 * define an opcode function
 ***************************************************************/
#define OP(prefix,opcode)  INLINE void prefix##_##opcode(void)

/***************************************************************
 * adjust cycle count by n T-states
 ***************************************************************/
#define CC(prefix,opcode) z80_ICount -= cc[Z80_TABLE_##prefix][opcode]

/***************************************************************
 * execute an opcode
 ***************************************************************/
#define EXEC(prefix,opcode) 									\
{																\
	unsigned op = opcode;										\
	CC(prefix,op);												\
	(*Z80##prefix[op])();										\
}

/***************************************************************
 * Enter HALT state; write 1 to fake port on first execution
 ***************************************************************/
#define ENTER_HALT {											\
    _PC--;                                                      \
    _HALT = 1;                                                  \
	if( !after_EI ) 											\
		z80_burn( z80_ICount ); 								\
}

/***************************************************************
 * Leave HALT state; write 0 to fake port
 ***************************************************************/
#define LEAVE_HALT {                                            \
	if( _HALT ) 												\
	{															\
		_HALT = 0;												\
		_PC++;													\
	}															\
}

/***************************************************************
 * Input a byte from given I/O port
 ***************************************************************/
#define IN(port)   ((UINT8)cpu_readport(port))

/***************************************************************
 * Output a byte to given I/O port
 ***************************************************************/
#define OUT(port,value) cpu_writeport(port,value)

/***************************************************************
 * Read a byte from given memory location
 ***************************************************************/
#define RM(addr) (UINT8)cpu_readmem16(addr)

/***************************************************************
 * Read a word from given memory location
 ***************************************************************/
INLINE void RM16( UINT32 addr, PAIR *r )
{
	r->b.l = RM(addr);
	r->b.h = RM((addr+1)&0xffff);
}

/***************************************************************
 * Write a byte to given memory location
 ***************************************************************/
#define WM(addr,value) cpu_writemem16(addr,value)

/***************************************************************
 * Write a word to given memory location
 ***************************************************************/
INLINE void WM16( UINT32 addr, PAIR *r )
{
	WM(addr,r->b.l);
	WM((addr+1)&0xffff,r->b.h);
}

/***************************************************************
 * ROP() is identical to RM() except it is used for
 * reading opcodes. In case of system with memory mapped I/O,
 * this function can be used to greatly speed up emulation
 ***************************************************************/
INLINE UINT8 ROP(void)
{
	unsigned pc = _PCD;
	_PC++;
	return cpu_readop(pc);
}

/****************************************************************
 * ARG() is identical to ROP() except it is used
 * for reading opcode arguments. This difference can be used to
 * support systems that use different encoding mechanisms for
 * opcodes and opcode arguments
 ***************************************************************/
INLINE UINT8 ARG(void)
{
	unsigned pc = _PCD;
    _PC++;
	return cpu_readop_arg(pc);
}

INLINE UINT32 ARG16(void)
{
	unsigned pc = _PCD;
    _PC += 2;
	return cpu_readop_arg(pc) | (cpu_readop_arg((pc+1)&0xffff) << 8);
}

/***************************************************************
 * Calculate the effective address EA of an opcode using
 * IX+offset resp. IY+offset addressing.
 ***************************************************************/
#define EAX EA = (UINT32)(UINT16)(_IX+(INT8)ARG())
#define EAY EA = (UINT32)(UINT16)(_IY+(INT8)ARG())

/***************************************************************
 * POP
 ***************************************************************/
#define POP(DR) { RM16( _SPD, &Z80.DR ); _SP += 2; }

/***************************************************************
 * PUSH
 ***************************************************************/
#define PUSH(SR) { _SP -= 2; WM16( _SPD, &Z80.SR ); }

/***************************************************************
 * JP
 ***************************************************************/
#define JP {													\
	unsigned oldpc = _PCD-1;									\
	_PCD = ARG16(); 											\
	change_pc16(_PCD);											\
    /* speed up busy loop */                                    \
	if( _PCD == oldpc ) 										\
	{															\
		if( !after_EI ) 										\
			BURNODD( z80_ICount, 1, cc[Z80_TABLE_op][0xc3] );	\
	}															\
	else														\
	{															\
		UINT8 op = cpu_readop(_PCD);							\
		if( _PCD == oldpc-1 )									\
		{														\
			/* NOP - JP $-1 or EI - JP $-1 */					\
			if ( op == 0x00 || op == 0xfb ) 					\
			{													\
				if( !after_EI ) 								\
					BURNODD( z80_ICount-cc[Z80_TABLE_op][0x00], \
						2, cc[Z80_TABLE_op][0x00]+cc[Z80_TABLE_op][0xc3]); \
			}													\
		}														\
		else													\
		/* LD SP,#xxxx - JP $-3 (Galaga) */ 					\
		if( _PCD == oldpc-3 && op == 0x31 ) 					\
		{														\
			if( !after_EI ) 									\
				BURNODD( z80_ICount-cc[Z80_TABLE_op][0x31], 	\
					2, cc[Z80_TABLE_op][0x31]+cc[Z80_TABLE_op][0xc3]); \
		}														\
	}															\
}

/***************************************************************
 * JP_COND
 ***************************************************************/

#define JP_COND(cond)											\
	if( cond )													\
	{															\
		_PCD = ARG16(); 										\
		change_pc16(_PCD);										\
	}															\
	else														\
	{															\
		_PC += 2;												\
    }

/***************************************************************
 * JR
 ***************************************************************/
#define JR()													\
{																\
	unsigned oldpc = _PCD-1;									\
	INT8 arg = (INT8)ARG(); /* ARG() also increments _PC */ 	\
	_PC += arg; 			/* so don't do _PC += ARG() */      \
	change_pc16(_PCD);											\
    /* speed up busy loop */                                    \
	if( _PCD == oldpc ) 										\
	{															\
		if( !after_EI ) 										\
			BURNODD( z80_ICount, 1, cc[Z80_TABLE_op][0x18] );	\
	}															\
	else														\
	{															\
		UINT8 op = cpu_readop(_PCD);							\
		if( _PCD == oldpc-1 )									\
		{														\
			/* NOP - JR $-1 or EI - JR $-1 */					\
			if ( op == 0x00 || op == 0xfb ) 					\
			{													\
				if( !after_EI ) 								\
				   BURNODD( z80_ICount-cc[Z80_TABLE_op][0x00],	\
					   2, cc[Z80_TABLE_op][0x00]+cc[Z80_TABLE_op][0x18]); \
			}													\
		}														\
		else													\
		/* LD SP,#xxxx - JR $-3 */								\
		if( _PCD == oldpc-3 && op == 0x31 ) 					\
		{														\
			if( !after_EI ) 									\
			   BURNODD( z80_ICount-cc[Z80_TABLE_op][0x31],		\
				   2, cc[Z80_TABLE_op][0x31]+cc[Z80_TABLE_op][0x18]); \
		}														\
    }                                                           \
}

/***************************************************************
 * JR_COND
 ***************************************************************/
#define JR_COND(cond,opcode)									\
	if( cond )													\
	{															\
		INT8 arg = (INT8)ARG(); /* ARG() also increments _PC */ \
		_PC += arg; 			/* so don't do _PC += ARG() */  \
		CC(ex,opcode);											\
		change_pc16(_PCD);										\
	}															\
	else _PC++; 												\

/***************************************************************
 * CALL
 ***************************************************************/
#define CALL()													\
	EA = ARG16();												\
	PUSH( PC ); 												\
	_PCD = EA;													\
	change_pc16(_PCD)

/***************************************************************
 * CALL_COND
 ***************************************************************/
#define CALL_COND(cond,opcode)									\
	if( cond )													\
	{															\
		EA = ARG16();											\
		PUSH( PC ); 											\
		_PCD = EA;												\
		CC(ex,opcode);											\
		change_pc16(_PCD);										\
	}															\
	else														\
	{															\
		_PC+=2; 												\
	}

/***************************************************************
 * RET_COND
 ***************************************************************/
#define RET_COND(cond,opcode)									\
	if( cond )													\
	{															\
		POP(PC);												\
		change_pc16(_PCD);										\
		CC(ex,opcode);											\
	}

/***************************************************************
 * RETN
 ***************************************************************/
#define RETN	{												\
	POP(PC);													\
	change_pc16(_PCD);											\
    if( _IFF1 == 0 && _IFF2 == 1 )                              \
	{															\
		_IFF1 = 1;												\
		if( Z80.irq_state != CLEAR_LINE ||						\
			Z80.request_irq >= 0 )								\
		{														\
			take_interrupt();									\
        }                                                       \
	}															\
	else _IFF1 = _IFF2; 										\
}

/***************************************************************
 * RETI
 ***************************************************************/
#define RETI	{												\
	int device = Z80.service_irq;								\
	POP(PC);													\
    change_pc16(_PCD);                                          \
/* according to http://www.msxnet.org/tech/Z80/z80undoc.txt */  \
/*	_IFF1 = _IFF2;	*/											\
	if( device >= 0 )											\
	{															\
		Z80.irq[device].interrupt_reti(Z80.irq[device].irq_param); \
	}															\
}

/***************************************************************
 * LD	R,A
 ***************************************************************/
#define LD_R_A {												\
	_R = _A;													\
	_R2 = _A & 0x80;				/* keep bit 7 of R */		\
}

/***************************************************************
 * LD	A,R
 ***************************************************************/
#define LD_A_R {												\
	_A = (_R & 0x7f) | _R2; 									\
	_F = (_F & CF) | SZ[_A] | ( _IFF2 << 2 );					\
}

/***************************************************************
 * LD	I,A
 ***************************************************************/
#define LD_I_A {												\
	_I = _A;													\
}

/***************************************************************
 * LD	A,I
 ***************************************************************/
#define LD_A_I {												\
	_A = _I;													\
	_F = (_F & CF) | SZ[_A] | ( _IFF2 << 2 );					\
}

/***************************************************************
 * RST
 ***************************************************************/
#define RST(addr)												\
	PUSH( PC ); 												\
	_PCD = addr;												\
	change_pc16(_PCD)

/***************************************************************
 * INC	r8
 ***************************************************************/
INLINE UINT8 INC(UINT8 value)
{
	UINT8 res = value + 1;
	_F = (_F & CF) | SZHV_inc[res];
	return (UINT8)res;
}

/***************************************************************
 * DEC	r8
 ***************************************************************/
INLINE UINT8 DEC(UINT8 value)
{
	UINT8 res = value - 1;
	_F = (_F & CF) | SZHV_dec[res];
    return res;
}

/***************************************************************
 * RLCA
 ***************************************************************/
#define RLCA													\
	_A = (_A << 1) | (_A >> 7); 								\
	_F = (_F & (SF | ZF | PF)) | (_A & (YF | XF | CF))

/***************************************************************
 * RRCA
 ***************************************************************/
#define RRCA                                                    \
	_F = (_F & (SF | ZF | PF)) | (_A & (YF | XF | CF)); 		\
    _A = (_A >> 1) | (_A << 7)

/***************************************************************
 * RLA
 ***************************************************************/
#define RLA {													\
	UINT8 res = (_A << 1) | (_F & CF);							\
	UINT8 c = (_A & 0x80) ? CF : 0; 							\
	_F = (_F & (SF | ZF | PF)) | c | (res & (YF | XF)); 		\
	_A = res;													\
}

/***************************************************************
 * RRA
 ***************************************************************/
#define RRA {                                                   \
	UINT8 res = (_A >> 1) | (_F << 7);							\
	UINT8 c = (_A & 0x01) ? CF : 0; 							\
	_F = (_F & (SF | ZF | PF)) | c | (res & (YF | XF)); 		\
	_A = res;													\
}

/***************************************************************
 * RRD
 ***************************************************************/
#define RRD {													\
	UINT8 n = RM(_HL);											\
	WM( _HL, (n >> 4) | (_A << 4) );							\
	_A = (_A & 0xf0) | (n & 0x0f);								\
	_F = (_F & CF) | SZP[_A];									\
}

/***************************************************************
 * RLD
 ***************************************************************/
#define RLD {                                                   \
    UINT8 n = RM(_HL);                                          \
	WM( _HL, (n << 4) | (_A & 0x0f) );							\
    _A = (_A & 0xf0) | (n >> 4);                                \
	_F = (_F & CF) | SZP[_A];									\
}

/***************************************************************
 * ADD	A,n
 ***************************************************************/
#define ADD(value)												\
{																\
	UINT32 ah = _AFD & 0xff00;									\
	UINT32 res = (UINT8)((ah >> 8) + value);					\
	_F = SZHVC_add[ah | res];									\
    _A = res;                                                   \
}

/***************************************************************
 * ADC	A,n
 ***************************************************************/
#define ADC(value)												\
{																\
	UINT32 ah = _AFD & 0xff00, c = _AFD & 1;					\
	UINT32 res = (UINT8)((ah >> 8) + value + c);				\
	_F = SZHVC_add[(c << 16) | ah | res];						\
    _A = res;                                                   \
}

/***************************************************************
 * SUB	n
 ***************************************************************/
#define SUB(value)												\
{																\
	UINT32 ah = _AFD & 0xff00;									\
	UINT32 res = (UINT8)((ah >> 8) - value);					\
	_F = SZHVC_sub[ah | res];									\
    _A = res;                                                   \
}

/***************************************************************
 * SBC	A,n
 ***************************************************************/
#define SBC(value)												\
{																\
	UINT32 ah = _AFD & 0xff00, c = _AFD & 1;					\
	UINT32 res = (UINT8)((ah >> 8) - value - c);				\
	_F = SZHVC_sub[(c<<16) | ah | res]; 						\
    _A = res;                                                   \
}

/***************************************************************
 * NEG
 ***************************************************************/
#define NEG {                                                   \
	UINT8 value = _A;											\
	_A = 0; 													\
	SUB(value); 												\
}

/***************************************************************
 * DAA
 ***************************************************************/
/*
#define DAA {													\
	int idx = _A;												\
	if( _F & CF ) idx |= 0x100; 								\
	if( _F & HF ) idx |= 0x200; 								\
	if( _F & NF ) idx |= 0x400; 								\
	_AF = DAATable[idx];										\
}
*/
#define DAA {													\
	UINT8 cf, nf, hf, lo, hi, diff;								\
	cf = _F & CF;												\
	nf = _F & NF;												\
	hf = _F & HF;												\
	lo = _A & 15;												\
	hi = _A / 16;												\
																\
	if (cf)														\
	{															\
		diff = (lo <= 9 && !hf) ? 0x60 : 0x66;					\
	}															\
	else														\
	{															\
		if (lo >= 10)											\
		{														\
			diff = hi <= 8 ? 0x06 : 0x66;						\
		}														\
		else													\
		{														\
			if (hi >= 10)										\
			{													\
				diff = hf ? 0x66 : 0x60;						\
			}													\
			else												\
			{													\
				diff = hf ? 0x06 : 0x00;						\
			}													\
		}														\
	}															\
	if (nf) _A -= diff;											\
	else _A += diff;												\
																\
	_F = SZP[_A] | (_F & NF);										\
	if (cf || (lo <= 9 ? hi >= 10 : hi >= 9)) _F |= CF;			\
	if (nf ? hf && lo <= 5 : lo >= 10)	_F |= HF;				\
}

/***************************************************************
 * AND	n
 ***************************************************************/
#define AND(value)												\
	_A &= value;												\
	_F = SZP[_A] | HF

/***************************************************************
 * OR	n
 ***************************************************************/
#define OR(value)												\
	_A |= value;												\
	_F = SZP[_A]

/***************************************************************
 * XOR	n
 ***************************************************************/
#define XOR(value)												\
	_A ^= value;												\
	_F = SZP[_A]

/***************************************************************
 * CP	n
 ***************************************************************/
#define CP(value)												\
{																\
	UINT32 ah = _AFD & 0xff00;									\
	UINT32 res = (UINT8)((ah >> 8) - value);					\
	_F = SZHVC_sub[ah | res];									\
}

/***************************************************************
 * EX   AF,AF'
 ***************************************************************/
#define EX_AF {                                                 \
	PAIR tmp;													\
    tmp = Z80.AF; Z80.AF = Z80.AF2; Z80.AF2 = tmp;              \
}

/***************************************************************
 * EX   DE,HL
 ***************************************************************/
#define EX_DE_HL {                                              \
	PAIR tmp;													\
    tmp = Z80.DE; Z80.DE = Z80.HL; Z80.HL = tmp;                \
}

/***************************************************************
 * EXX
 ***************************************************************/
#define EXX {                                                   \
	PAIR tmp;													\
    tmp = Z80.BC; Z80.BC = Z80.BC2; Z80.BC2 = tmp;              \
    tmp = Z80.DE; Z80.DE = Z80.DE2; Z80.DE2 = tmp;              \
    tmp = Z80.HL; Z80.HL = Z80.HL2; Z80.HL2 = tmp;              \
}

/***************************************************************
 * EX   (SP),r16
 ***************************************************************/
#define EXSP(DR)												\
{																\
	PAIR tmp = { { 0, 0, 0, 0 } };								\
	RM16( _SPD, &tmp ); 										\
	WM16( _SPD, &Z80.DR );										\
	Z80.DR = tmp;												\
}


/***************************************************************
 * ADD16
 ***************************************************************/
#define ADD16(DR,SR)											\
{																\
	UINT32 res = Z80.DR.d + Z80.SR.d;							\
	_F = (_F & (SF | ZF | VF)) |								\
		(((Z80.DR.d ^ res ^ Z80.SR.d) >> 8) & HF) | 			\
		((res >> 16) & CF); 									\
	Z80.DR.w.l = (UINT16)res;									\
}

/***************************************************************
 * ADC	r16,r16
 ***************************************************************/
#define ADC16(Reg)												\
{																\
	UINT32 res = _HLD + Z80.Reg.d + (_F & CF);					\
	_F = (((_HLD ^ res ^ Z80.Reg.d) >> 8) & HF) |				\
		((res >> 16) & CF) |									\
		((res >> 8) & SF) | 									\
		((res & 0xffff) ? 0 : ZF) | 							\
		(((Z80.Reg.d ^ _HLD ^ 0x8000) & (Z80.Reg.d ^ res) & 0x8000) >> 13); \
	_HL = (UINT16)res;											\
}

/***************************************************************
 * SBC	r16,r16
 ***************************************************************/
#define SBC16(Reg)												\
{																\
	UINT32 res = _HLD - Z80.Reg.d - (_F & CF);					\
	_F = (((_HLD ^ res ^ Z80.Reg.d) >> 8) & HF) | NF |			\
		((res >> 16) & CF) |									\
		((res >> 8) & SF) | 									\
		((res & 0xffff) ? 0 : ZF) | 							\
		(((Z80.Reg.d ^ _HLD) & (_HLD ^ res) &0x8000) >> 13);	\
	_HL = (UINT16)res;											\
}

/***************************************************************
 * RLC	r8
 ***************************************************************/
INLINE UINT8 RLC(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | (res >> 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RRC	r8
 ***************************************************************/
INLINE UINT8 RRC(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (res << 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RL	r8
 ***************************************************************/
INLINE UINT8 RL(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | (_F & CF)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RR	r8
 ***************************************************************/
INLINE UINT8 RR(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (_F << 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SLA	r8
 ***************************************************************/
INLINE UINT8 SLA(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = (res << 1) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SRA	r8
 ***************************************************************/
INLINE UINT8 SRA(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (res & 0x80)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SLL	r8
 ***************************************************************/
INLINE UINT8 SLL(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | 0x01) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SRL	r8
 ***************************************************************/
INLINE UINT8 SRL(UINT8 value)
{
	unsigned res = value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = (res >> 1) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * BIT  bit,r8
 ***************************************************************/
#define BIT(bit,reg)                                            \
	_F = (_F & CF) | HF | SZ_BIT[reg & (1<<bit)]

/***************************************************************
 * BIT	bit,(IX/Y+o)
 ***************************************************************/
#define BIT_XY(bit,reg)                                         \
    _F = (_F & CF) | HF | (SZ_BIT[reg & (1<<bit)] & ~(YF|XF)) | ((EA>>8) & (YF|XF))

/***************************************************************
 * RES	bit,r8
 ***************************************************************/
INLINE UINT8 RES(UINT8 bit, UINT8 value)
{
	return value & ~(1<<bit);
}

/***************************************************************
 * SET  bit,r8
 ***************************************************************/
INLINE UINT8 SET(UINT8 bit, UINT8 value)
{
	return value | (1<<bit);
}

/***************************************************************
 * LDI
 ***************************************************************/
#define LDI {													\
	UINT8 io = RM(_HL); 										\
	WM( _DE, io );												\
	_F &= SF | ZF | CF; 										\
	if( (_A + io) & 0x02 ) _F |= YF; /* bit 1 -> flag 5 */		\
    if( (_A + io) & 0x08 ) _F |= XF; /* bit 3 -> flag 3 */      \
    _HL++; _DE++; _BC--;                                        \
	if( _BC ) _F |= VF; 										\
}

/***************************************************************
 * CPI
 ***************************************************************/
#define CPI {													\
	UINT8 val = RM(_HL);										\
	UINT8 res = _A - val;										\
	_HL++; _BC--;												\
	_F = (_F & CF) | (SZ[res] & ~(YF|XF)) | ((_A ^ val ^ res) & HF) | NF;  \
	if( _F & HF ) res -= 1; 									\
	if( res & 0x02 ) _F |= YF; /* bit 1 -> flag 5 */			\
	if( res & 0x08 ) _F |= XF; /* bit 3 -> flag 3 */			\
    if( _BC ) _F |= VF;                                         \
}

/***************************************************************
 * INI
 ***************************************************************/
#define INI {													\
	UINT8 io = IN(_BC); 										\
    _B--;                                                       \
	WM( _HL, io );												\
	_HL++;														\
	_F = SZ[_B];												\
	if( io & SF ) _F |= NF; 									\
	if( (_C + io + 1) & 0x100 ) _F |= HF | CF;					\
    if( (irep_tmp1[_C & 3][io & 3] ^                            \
		 breg_tmp2[_B] ^										\
		 (_C >> 2) ^											\
		 (io >> 2)) & 1 )										\
		_F |= PF;												\
}

/***************************************************************
 * OUTI
 ***************************************************************/
#define OUTI {													\
	UINT8 io = RM(_HL); 										\
    _B--;                                                       \
	OUT( _BC, io ); 											\
	_HL++;														\
	_F = SZ[_B];												\
	if( io & SF ) _F |= NF; 									\
	if( (_C + io + 1) & 0x100 ) _F |= HF | CF;					\
    if( (irep_tmp1[_C & 3][io & 3] ^                            \
		 breg_tmp2[_B] ^										\
		 (_C >> 2) ^											\
		 (io >> 2)) & 1 )										\
        _F |= PF;                                               \
}

/***************************************************************
 * LDD
 ***************************************************************/
#define LDD {													\
	UINT8 io = RM(_HL); 										\
	WM( _DE, io );												\
	_F &= SF | ZF | CF; 										\
	if( (_A + io) & 0x02 ) _F |= YF; /* bit 1 -> flag 5 */		\
	if( (_A + io) & 0x08 ) _F |= XF; /* bit 3 -> flag 3 */		\
	_HL--; _DE--; _BC--;										\
	if( _BC ) _F |= VF; 										\
}

/***************************************************************
 * CPD
 ***************************************************************/
#define CPD {													\
	UINT8 val = RM(_HL);										\
	UINT8 res = _A - val;										\
	_HL--; _BC--;												\
	_F = (_F & CF) | (SZ[res] & ~(YF|XF)) | ((_A ^ val ^ res) & HF) | NF;  \
	if( _F & HF ) res -= 1; 									\
	if( res & 0x02 ) _F |= YF; /* bit 1 -> flag 5 */			\
	if( res & 0x08 ) _F |= XF; /* bit 3 -> flag 3 */			\
    if( _BC ) _F |= VF;                                         \
}

/***************************************************************
 * IND
 ***************************************************************/
#define IND {													\
    UINT8 io = IN(_BC);                                         \
	_B--;														\
	WM( _HL, io );												\
	_HL--;														\
	_F = SZ[_B];												\
    if( io & SF ) _F |= NF;                                     \
	if( (_C + io - 1) & 0x100 ) _F |= HF | CF;					\
	if( (drep_tmp1[_C & 3][io & 3] ^							\
		 breg_tmp2[_B] ^										\
		 (_C >> 2) ^											\
		 (io >> 2)) & 1 )										\
        _F |= PF;                                               \
}

/***************************************************************
 * OUTD
 ***************************************************************/
#define OUTD {													\
	UINT8 io = RM(_HL); 										\
    _B--;                                                       \
	OUT( _BC, io ); 											\
	_HL--;														\
	_F = SZ[_B];												\
    if( io & SF ) _F |= NF;                                     \
	if( (_C + io - 1) & 0x100 ) _F |= HF | CF;					\
	if( (drep_tmp1[_C & 3][io & 3] ^							\
		 breg_tmp2[_B] ^										\
		 (_C >> 2) ^											\
		 (io >> 2)) & 1 )										\
        _F |= PF;                                               \
}

/***************************************************************
 * LDIR
 ***************************************************************/
#define LDIR {                                                  \
	CC(ex,0xb0);												\
	_PC -= 2;													\
	do															\
	{															\
		LDI;													\
		if( _BC )												\
		{														\
			if( z80_ICount > 0 )								\
			{													\
				_R += 2;  /* increment R twice */				\
				CC(op,0xb0);									\
				CC(ex,0xb0);									\
			}													\
			else break; 										\
		}														\
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb0];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * CPIR
 ***************************************************************/
#define CPIR {                                                  \
	CC(ex,0xb1);												\
	_PC -= 2;													\
    do                                                          \
	{															\
		CPI;													\
		if( _BC && !(_F & ZF) ) 								\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xb1);									\
				CC(ex,0xb1);									\
			}													\
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb1];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * INIR
 ***************************************************************/
#define INIR {                                                  \
	CC(ex,0xb2);												\
	_PC -= 2;													\
    do                                                          \
	{															\
		INI;													\
		if( _B )												\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xb2);									\
				CC(ex,0xb2);									\
            }                                                   \
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb2];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * OTIR
 ***************************************************************/
#define OTIR {                                                  \
	CC(ex,0xb3);												\
    _PC -= 2;                                                   \
    do                                                          \
	{															\
		OUTI;													\
		if( _B	)												\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xb3);									\
				CC(ex,0xb3);									\
            }                                                   \
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb3];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * LDDR
 ***************************************************************/
#define LDDR {                                                  \
	CC(ex,0xb8);												\
	_PC -= 2;													\
    do                                                          \
	{															\
		LDD;													\
		if( _BC )												\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xb8);									\
				CC(ex,0xb8);									\
            }                                                   \
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb8];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * CPDR
 ***************************************************************/
#define CPDR {                                                  \
	CC(ex,0xb9);												\
	_PC -= 2;													\
    do                                                          \
	{															\
		CPD;													\
		if( _BC && !(_F & ZF) ) 								\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xb9);									\
				CC(ex,0xb9);									\
			}													\
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xb9];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * INDR
 ***************************************************************/
#define INDR {                                                  \
	CC(ex,0xba);												\
    _PC -= 2;                                                   \
    do                                                          \
	{															\
		IND;													\
		if( _B )												\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xba);									\
				CC(ex,0xba);									\
            }                                                   \
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xba];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * OTDR
 ***************************************************************/
#define OTDR {                                                  \
	CC(ex,0xbb);												\
    _PC -= 2;                                                   \
    do                                                          \
	{															\
		OUTD;													\
		if( _B )												\
		{														\
			if( z80_ICount > 0 )								\
            {                                                   \
				_R += 2;  /* increment R twice */				\
				CC(op,0xbb);									\
				CC(ex,0xbb);									\
            }                                                   \
            else break;                                         \
        }                                                       \
		else													\
		{														\
			_PC += 2;											\
			z80_ICount += cc[Z80_TABLE_ex][0xbb];				\
            break;                                              \
		}														\
	} while( z80_ICount > 0 );									\
}

/***************************************************************
 * EI
 ***************************************************************/
#define EI {													\
	/* If interrupts were disabled, execute one more			\
     * instruction and check the IRQ line.                      \
     * If not, simply set interrupt flip-flop 2                 \
     */                                                         \
	if( _IFF1 == 0 )											\
	{															\
        _IFF1 = _IFF2 = 1;                                      \
		_PPC = _PCD;											\
		_R++;													\
        while( cpu_readop(_PCD) == 0xfb ) /* more EIs? */       \
		{														\
			CC(op,0xfb);										\
            _PPC =_PCD;                                         \
            _PC++;                                              \
			_R++;												\
		}														\
		if( Z80.irq_state != CLEAR_LINE ||						\
			Z80.request_irq >= 0 )								\
		{														\
			after_EI = 1;	/* avoid cycle skip hacks */		\
			EXEC(op,ROP()); 									\
			after_EI = 0;										\
            take_interrupt();                                   \
		} else EXEC(op,ROP());									\
    } else _IFF2 = 1;                                           \
}

#include "z80ops.h"

static void take_interrupt(void)
{
    if( _IFF1 )
    {
        int irq_vector;

        /* there isn't a valid previous program counter */
        _PPC = -1;

        /* Check if processor was halted */
		LEAVE_HALT;

        if( Z80.irq_max )           /* daisy chain mode */
        {
            if( Z80.request_irq >= 0 )
            {
                /* Clear both interrupt flip flops */
                _IFF1 = _IFF2 = 0;
                irq_vector = Z80.irq[Z80.request_irq].interrupt_entry(Z80.irq[Z80.request_irq].irq_param);
                Z80.request_irq = -1;
            } else return;
        }
        else
        {
            /* Clear both interrupt flip flops */
            _IFF1 = _IFF2 = 0;
            /* call back the cpu interface to retrieve the vector */
            irq_vector = (*Z80.irq_callback)(0);
        }

        /* Interrupt mode 2. Call [Z80.I:databyte] */
        if( _IM == 2 )
        {
			irq_vector = (irq_vector & 0xff) | (_I << 8);
            PUSH( PC );
			RM16( irq_vector, &Z80.PC );
			/* CALL opcode timing */
            Z80.extra_cycles += cc[Z80_TABLE_op][0xcd];
        }
        else
        /* Interrupt mode 1. RST 38h */
        if( _IM == 1 )
        {
            PUSH( PC );
            _PCD = 0x0038;
			/* RST $38 + 'interrupt latency' cycles */
			Z80.extra_cycles += cc[Z80_TABLE_op][0xff] + cc[Z80_TABLE_ex][0xff];
        }
        else
        {
            /* Interrupt mode 0. We check for CALL and JP instructions, */
            /* if neither of these were found we assume a 1 byte opcode */
            /* was placed on the databus                                */
            switch (irq_vector & 0xff0000)
            {
                case 0xcd0000:  /* call */
                    PUSH( PC );
					_PCD = irq_vector & 0xffff;
					 /* CALL $xxxx + 'interrupt latency' cycles */
					Z80.extra_cycles += cc[Z80_TABLE_op][0xcd] + cc[Z80_TABLE_ex][0xff];
                    break;
                case 0xc30000:  /* jump */
                    _PCD = irq_vector & 0xffff;
					/* JP $xxxx + 2 cycles */
					Z80.extra_cycles += cc[Z80_TABLE_op][0xc3] + cc[Z80_TABLE_ex][0xff];
                    break;
				default:		/* rst (or other opcodes?) */
                    PUSH( PC );
                    _PCD = irq_vector & 0x0038;
					/* RST $xx + 2 cycles */
					Z80.extra_cycles += cc[Z80_TABLE_op][_PCD] + cc[Z80_TABLE_ex][_PCD];
                    break;
            }
        }
        change_pc(_PCD);
    }
}

/****************************************************************************
 * Reset registers to their initial values
 ****************************************************************************/
void z80_reset(void *param)
{
	Z80_DaisyChain *daisy_chain = (Z80_DaisyChain *)param;
	int i, p;

	if( !SZHVC_add || !SZHVC_sub )
    {
		int oldval, newval, val;
		UINT8 *padd, *padc, *psub, *psbc;
        /* allocate big flag arrays once */
		SZHVC_add = (UINT8 *)malloc(2*256*256);
		SZHVC_sub = (UINT8 *)malloc(2*256*256);
		padd = &SZHVC_add[	0*256];
		padc = &SZHVC_add[256*256];
		psub = &SZHVC_sub[	0*256];
		psbc = &SZHVC_sub[256*256];
		for (oldval = 0; oldval < 256; oldval++)
		{
			for (newval = 0; newval < 256; newval++)
			{
				/* add or adc w/o carry set */
				val = newval - oldval;
				*padd = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
				*padd |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
                if( (newval & 0x0f) < (oldval & 0x0f) ) *padd |= HF;
				if( newval < oldval ) *padd |= CF;
				if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padd |= VF;
				padd++;

				/* adc with carry set */
				val = newval - oldval - 1;
				*padc = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
				*padc |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
                if( (newval & 0x0f) <= (oldval & 0x0f) ) *padc |= HF;
				if( newval <= oldval ) *padc |= CF;
				if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padc |= VF;
				padc++;

				/* cp, sub or sbc w/o carry set */
				val = oldval - newval;
				*psub = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
				*psub |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
                if( (newval & 0x0f) > (oldval & 0x0f) ) *psub |= HF;
				if( newval > oldval ) *psub |= CF;
				if( (val^oldval) & (oldval^newval) & 0x80 ) *psub |= VF;
				psub++;

				/* sbc with carry set */
				val = oldval - newval - 1;
				*psbc = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
				*psbc |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
                if( (newval & 0x0f) >= (oldval & 0x0f) ) *psbc |= HF;
				if( newval >= oldval ) *psbc |= CF;
				if( (val^oldval) & (oldval^newval) & 0x80 ) *psbc |= VF;
				psbc++;
			}
        }
    }

	for (i = 0; i < 256; i++)
	{
		p = 0;
		if( i&0x01 ) ++p;
		if( i&0x02 ) ++p;
		if( i&0x04 ) ++p;
		if( i&0x08 ) ++p;
		if( i&0x10 ) ++p;
		if( i&0x20 ) ++p;
		if( i&0x40 ) ++p;
		if( i&0x80 ) ++p;
		SZ[i] = i ? i & SF : ZF;
		SZ[i] |= (i & (YF | XF));		/* undocumented flag bits 5+3 */
		SZ_BIT[i] = i ? i & SF : ZF | PF;
		SZ_BIT[i] |= (i & (YF | XF));	/* undocumented flag bits 5+3 */
        SZP[i] = SZ[i] | ((p & 1) ? 0 : PF);
		SZHV_inc[i] = SZ[i];
		if( i == 0x80 ) SZHV_inc[i] |= VF;
		if( (i & 0x0f) == 0x00 ) SZHV_inc[i] |= HF;
		SZHV_dec[i] = SZ[i] | NF;
		if( i == 0x7f ) SZHV_dec[i] |= VF;
		if( (i & 0x0f) == 0x0f ) SZHV_dec[i] |= HF;
	}

	memset(&Z80, 0, sizeof(Z80));
	_IX = _IY = 0xffff; /* IX and IY are FFFF after a reset! */
	_F = ZF;			/* Zero flag is set */
	Z80.request_irq = -1;
	Z80.service_irq = -1;
    Z80.nmi_state = CLEAR_LINE;
	Z80.irq_state = CLEAR_LINE;

    if( daisy_chain )
	{
		while( daisy_chain->irq_param != -1 && Z80.irq_max < Z80_MAXDAISY )
		{
            /* set callbackhandler after reti */
			Z80.irq[Z80.irq_max] = *daisy_chain;
            /* device reset */
			if( Z80.irq[Z80.irq_max].reset )
				Z80.irq[Z80.irq_max].reset(Z80.irq[Z80.irq_max].irq_param);
			Z80.irq_max++;
            daisy_chain++;
        }
    }

    change_pc(_PCD);
}

void z80_exit(void)
{
	if (SZHVC_add) free(SZHVC_add);
	SZHVC_add = NULL;
	if (SZHVC_sub) free(SZHVC_sub);
	SZHVC_sub = NULL;
}

/****************************************************************************
 * Execute 'cycles' T-states. Return number of T-states really executed
 ****************************************************************************/
int z80_execute(int cycles)
{
      __label__ 
    l_0x00, l_0x01, l_0x02, l_0x03, l_0x04, l_0x05, l_0x06, l_0x07, l_0x08, l_0x09,
    l_0x0a, l_0x0b, l_0x0c, l_0x0d, l_0x0e, l_0x0f, l_0x10, l_0x11, l_0x12, l_0x13,
    l_0x14, l_0x15, l_0x16, l_0x17, l_0x18, l_0x19, l_0x1a, l_0x1b, l_0x1c, l_0x1d,
    l_0x1e, l_0x1f, l_0x20, l_0x21, l_0x22, l_0x23, l_0x24, l_0x25, l_0x26, l_0x27,
    l_0x28, l_0x29, l_0x2a, l_0x2b, l_0x2c, l_0x2d, l_0x2e, l_0x2f, l_0x30, l_0x31,
    l_0x32, l_0x33, l_0x34, l_0x35, l_0x36, l_0x37, l_0x38, l_0x39, l_0x3a, l_0x3b,
    l_0x3c, l_0x3d, l_0x3e, l_0x3f, l_0x40, l_0x41, l_0x42, l_0x43, l_0x44, l_0x45,
    l_0x46, l_0x47, l_0x48, l_0x49, l_0x4a, l_0x4b, l_0x4c, l_0x4d, l_0x4e, l_0x4f,
    l_0x50, l_0x51, l_0x52, l_0x53, l_0x54, l_0x55, l_0x56, l_0x57, l_0x58, l_0x59,
    l_0x5a, l_0x5b, l_0x5c, l_0x5d, l_0x5e, l_0x5f, l_0x60, l_0x61, l_0x62, l_0x63,
    l_0x64, l_0x65, l_0x66, l_0x67, l_0x68, l_0x69, l_0x6a, l_0x6b, l_0x6c, l_0x6d,
    l_0x6e, l_0x6f, l_0x70, l_0x71, l_0x72, l_0x73, l_0x74, l_0x75, l_0x76, l_0x77,
    l_0x78, l_0x79, l_0x7a, l_0x7b, l_0x7c, l_0x7d, l_0x7e, l_0x7f, l_0x80, l_0x81,
    l_0x82, l_0x83, l_0x84, l_0x85, l_0x86, l_0x87, l_0x88, l_0x89, l_0x8a, l_0x8b,
    l_0x8c, l_0x8d, l_0x8e, l_0x8f, l_0x90, l_0x91, l_0x92, l_0x93, l_0x94, l_0x95,
    l_0x96, l_0x97, l_0x98, l_0x99, l_0x9a, l_0x9b, l_0x9c, l_0x9d, l_0x9e, l_0x9f,
    l_0xa0, l_0xa1, l_0xa2, l_0xa3, l_0xa4, l_0xa5, l_0xa6, l_0xa7, l_0xa8, l_0xa9,
    l_0xaa, l_0xab, l_0xac, l_0xad, l_0xae, l_0xaf, l_0xb0, l_0xb1, l_0xb2, l_0xb3,
    l_0xb4, l_0xb5, l_0xb6, l_0xb7, l_0xb8, l_0xb9, l_0xba, l_0xbb, l_0xbc, l_0xbd,
    l_0xbe, l_0xbf, l_0xc0, l_0xc1, l_0xc2, l_0xc3, l_0xc4, l_0xc5, l_0xc6, l_0xc7,
    l_0xc8, l_0xc9, l_0xca, l_0xcb, l_0xcc, l_0xcd, l_0xce, l_0xcf, l_0xd0, l_0xd1,
    l_0xd2, l_0xd3, l_0xd4, l_0xd5, l_0xd6, l_0xd7, l_0xd8, l_0xd9, l_0xda, l_0xdb,
    l_0xdc, l_0xdd, l_0xde, l_0xdf, l_0xe0, l_0xe1, l_0xe2, l_0xe3, l_0xe4, l_0xe5,
    l_0xe6, l_0xe7, l_0xe8, l_0xe9, l_0xea, l_0xeb, l_0xec, l_0xed, l_0xee, l_0xef,
    l_0xf0, l_0xf1, l_0xf2, l_0xf3, l_0xf4, l_0xf5, l_0xf6, l_0xf7, l_0xf8, l_0xf9,
    l_0xfa, l_0xfb, l_0xfc, l_0xfd, l_0xfe, l_0xff;

      static const void* const a_jump_table[256] = {
    &&l_0x00, &&l_0x01, &&l_0x02, &&l_0x03, &&l_0x04, &&l_0x05, &&l_0x06, &&l_0x07,
    &&l_0x08, &&l_0x09, &&l_0x0a, &&l_0x0b, &&l_0x0c, &&l_0x0d, &&l_0x0e, &&l_0x0f,
    &&l_0x10, &&l_0x11, &&l_0x12, &&l_0x13, &&l_0x14, &&l_0x15, &&l_0x16, &&l_0x17,
    &&l_0x18, &&l_0x19, &&l_0x1a, &&l_0x1b, &&l_0x1c, &&l_0x1d, &&l_0x1e, &&l_0x1f,
    &&l_0x20, &&l_0x21, &&l_0x22, &&l_0x23, &&l_0x24, &&l_0x25, &&l_0x26, &&l_0x27,
    &&l_0x28, &&l_0x29, &&l_0x2a, &&l_0x2b, &&l_0x2c, &&l_0x2d, &&l_0x2e, &&l_0x2f,
    &&l_0x30, &&l_0x31, &&l_0x32, &&l_0x33, &&l_0x34, &&l_0x35, &&l_0x36, &&l_0x37,
    &&l_0x38, &&l_0x39, &&l_0x3a, &&l_0x3b, &&l_0x3c, &&l_0x3d, &&l_0x3e, &&l_0x3f,
    &&l_0x40, &&l_0x41, &&l_0x42, &&l_0x43, &&l_0x44, &&l_0x45, &&l_0x46, &&l_0x47,
    &&l_0x48, &&l_0x49, &&l_0x4a, &&l_0x4b, &&l_0x4c, &&l_0x4d, &&l_0x4e, &&l_0x4f,
    &&l_0x50, &&l_0x51, &&l_0x52, &&l_0x53, &&l_0x54, &&l_0x55, &&l_0x56, &&l_0x57,
    &&l_0x58, &&l_0x59, &&l_0x5a, &&l_0x5b, &&l_0x5c, &&l_0x5d, &&l_0x5e, &&l_0x5f,
    &&l_0x60, &&l_0x61, &&l_0x62, &&l_0x63, &&l_0x64, &&l_0x65, &&l_0x66, &&l_0x67,
    &&l_0x68, &&l_0x69, &&l_0x6a, &&l_0x6b, &&l_0x6c, &&l_0x6d, &&l_0x6e, &&l_0x6f,
    &&l_0x70, &&l_0x71, &&l_0x72, &&l_0x73, &&l_0x74, &&l_0x75, &&l_0x76, &&l_0x77,
    &&l_0x78, &&l_0x79, &&l_0x7a, &&l_0x7b, &&l_0x7c, &&l_0x7d, &&l_0x7e, &&l_0x7f,
    &&l_0x80, &&l_0x81, &&l_0x82, &&l_0x83, &&l_0x84, &&l_0x85, &&l_0x86, &&l_0x87,
    &&l_0x88, &&l_0x89, &&l_0x8a, &&l_0x8b, &&l_0x8c, &&l_0x8d, &&l_0x8e, &&l_0x8f,
    &&l_0x90, &&l_0x91, &&l_0x92, &&l_0x93, &&l_0x94, &&l_0x95, &&l_0x96, &&l_0x97,
    &&l_0x98, &&l_0x99, &&l_0x9a, &&l_0x9b, &&l_0x9c, &&l_0x9d, &&l_0x9e, &&l_0x9f,
    &&l_0xa0, &&l_0xa1, &&l_0xa2, &&l_0xa3, &&l_0xa4, &&l_0xa5, &&l_0xa6, &&l_0xa7,
    &&l_0xa8, &&l_0xa9, &&l_0xaa, &&l_0xab, &&l_0xac, &&l_0xad, &&l_0xae, &&l_0xaf,
    &&l_0xb0, &&l_0xb1, &&l_0xb2, &&l_0xb3, &&l_0xb4, &&l_0xb5, &&l_0xb6, &&l_0xb7,
    &&l_0xb8, &&l_0xb9, &&l_0xba, &&l_0xbb, &&l_0xbc, &&l_0xbd, &&l_0xbe, &&l_0xbf,
    &&l_0xc0, &&l_0xc1, &&l_0xc2, &&l_0xc3, &&l_0xc4, &&l_0xc5, &&l_0xc6, &&l_0xc7,
    &&l_0xc8, &&l_0xc9, &&l_0xca, &&l_0xcb, &&l_0xcc, &&l_0xcd, &&l_0xce, &&l_0xcf,
    &&l_0xd0, &&l_0xd1, &&l_0xd2, &&l_0xd3, &&l_0xd4, &&l_0xd5, &&l_0xd6, &&l_0xd7,
    &&l_0xd8, &&l_0xd9, &&l_0xda, &&l_0xdb, &&l_0xdc, &&l_0xdd, &&l_0xde, &&l_0xdf,
    &&l_0xe0, &&l_0xe1, &&l_0xe2, &&l_0xe3, &&l_0xe4, &&l_0xe5, &&l_0xe6, &&l_0xe7,
    &&l_0xe8, &&l_0xe9, &&l_0xea, &&l_0xeb, &&l_0xec, &&l_0xed, &&l_0xee, &&l_0xef,
    &&l_0xf0, &&l_0xf1, &&l_0xf2, &&l_0xf3, &&l_0xf4, &&l_0xf5, &&l_0xf6, &&l_0xf7,
    &&l_0xf8, &&l_0xf9, &&l_0xfa, &&l_0xfb, &&l_0xfc, &&l_0xfd, &&l_0xfe, &&l_0xff
    };

    unsigned op;
    
	z80_ICount = cycles - Z80.extra_cycles;
	Z80.extra_cycles = 0;

    do
	{
        _PPC = _PCD;
		_R++;
		
	    op = ROP();

	    CC(op,op);

        goto *a_jump_table[op];

       	l_0x00:op_00();goto end;
       	l_0x01:op_01();goto end;
       	l_0x02:op_02();goto end;
       	l_0x03:op_03();goto end;
       	l_0x04:op_04();goto end;
       	l_0x05:op_05();goto end;
       	l_0x06:op_06();goto end;
       	l_0x07:op_07();goto end;
       	l_0x08:op_08();goto end;
       	l_0x09:op_09();goto end;
       	l_0x0a:op_0a();goto end;
       	l_0x0b:op_0b();goto end;
       	l_0x0c:op_0c();goto end;
       	l_0x0d:op_0d();goto end;
       	l_0x0e:op_0e();goto end;
       	l_0x0f:op_0f();goto end;
       	l_0x10:op_10();goto end;
       	l_0x11:op_11();goto end;
       	l_0x12:op_12();goto end;
       	l_0x13:op_13();goto end;
       	l_0x14:op_14();goto end;
       	l_0x15:op_15();goto end;
       	l_0x16:op_16();goto end;
       	l_0x17:op_17();goto end;
       	l_0x18:op_18();goto end;
       	l_0x19:op_19();goto end;
       	l_0x1a:op_1a();goto end;
       	l_0x1b:op_1b();goto end;
       	l_0x1c:op_1c();goto end;
       	l_0x1d:op_1d();goto end;
       	l_0x1e:op_1e();goto end;
       	l_0x1f:op_1f();goto end;
       	l_0x20:op_20();goto end;
       	l_0x21:op_21();goto end;
       	l_0x22:op_22();goto end;
       	l_0x23:op_23();goto end;
       	l_0x24:op_24();goto end;
       	l_0x25:op_25();goto end;
       	l_0x26:op_26();goto end;
       	l_0x27:op_27();goto end;
       	l_0x28:op_28();goto end;
       	l_0x29:op_29();goto end;
       	l_0x2a:op_2a();goto end;
       	l_0x2b:op_2b();goto end;
       	l_0x2c:op_2c();goto end;
       	l_0x2d:op_2d();goto end;
       	l_0x2e:op_2e();goto end;
       	l_0x2f:op_2f();goto end;
       	l_0x30:op_30();goto end;
       	l_0x31:op_31();goto end;
       	l_0x32:op_32();goto end;
       	l_0x33:op_33();goto end;
       	l_0x34:op_34();goto end;
       	l_0x35:op_35();goto end;
       	l_0x36:op_36();goto end;
       	l_0x37:op_37();goto end;
       	l_0x38:op_38();goto end;
       	l_0x39:op_39();goto end;
       	l_0x3a:op_3a();goto end;
       	l_0x3b:op_3b();goto end;
       	l_0x3c:op_3c();goto end;
       	l_0x3d:op_3d();goto end;
       	l_0x3e:op_3e();goto end;
       	l_0x3f:op_3f();goto end;
       	l_0x40:op_40();goto end;
       	l_0x41:op_41();goto end;
       	l_0x42:op_42();goto end;
       	l_0x43:op_43();goto end;
       	l_0x44:op_44();goto end;
       	l_0x45:op_45();goto end;
       	l_0x46:op_46();goto end;
       	l_0x47:op_47();goto end;
       	l_0x48:op_48();goto end;
       	l_0x49:op_49();goto end;
       	l_0x4a:op_4a();goto end;
       	l_0x4b:op_4b();goto end;
       	l_0x4c:op_4c();goto end;
       	l_0x4d:op_4d();goto end;
       	l_0x4e:op_4e();goto end;
       	l_0x4f:op_4f();goto end;
       	l_0x50:op_50();goto end;
       	l_0x51:op_51();goto end;
       	l_0x52:op_52();goto end;
       	l_0x53:op_53();goto end;
       	l_0x54:op_54();goto end;
       	l_0x55:op_55();goto end;
       	l_0x56:op_56();goto end;
       	l_0x57:op_57();goto end;
       	l_0x58:op_58();goto end;
       	l_0x59:op_59();goto end;
       	l_0x5a:op_5a();goto end;
       	l_0x5b:op_5b();goto end;
       	l_0x5c:op_5c();goto end;
       	l_0x5d:op_5d();goto end;
       	l_0x5e:op_5e();goto end;
       	l_0x5f:op_5f();goto end;
       	l_0x60:op_60();goto end;
       	l_0x61:op_61();goto end;
       	l_0x62:op_62();goto end;
       	l_0x63:op_63();goto end;
       	l_0x64:op_64();goto end;
       	l_0x65:op_65();goto end;
       	l_0x66:op_66();goto end;
       	l_0x67:op_67();goto end;
       	l_0x68:op_68();goto end;
       	l_0x69:op_69();goto end;
       	l_0x6a:op_6a();goto end;
       	l_0x6b:op_6b();goto end;
       	l_0x6c:op_6c();goto end;
       	l_0x6d:op_6d();goto end;
       	l_0x6e:op_6e();goto end;
       	l_0x6f:op_6f();goto end;
       	l_0x70:op_70();goto end;
       	l_0x71:op_71();goto end;
       	l_0x72:op_72();goto end;
       	l_0x73:op_73();goto end;
       	l_0x74:op_74();goto end;
       	l_0x75:op_75();goto end;
       	l_0x76:op_76();goto end;
       	l_0x77:op_77();goto end;
       	l_0x78:op_78();goto end;
       	l_0x79:op_79();goto end;
       	l_0x7a:op_7a();goto end;
       	l_0x7b:op_7b();goto end;
       	l_0x7c:op_7c();goto end;
       	l_0x7d:op_7d();goto end;
       	l_0x7e:op_7e();goto end;
       	l_0x7f:op_7f();goto end;
       	l_0x80:op_80();goto end;
       	l_0x81:op_81();goto end;
       	l_0x82:op_82();goto end;
       	l_0x83:op_83();goto end;
       	l_0x84:op_84();goto end;
       	l_0x85:op_85();goto end;
       	l_0x86:op_86();goto end;
       	l_0x87:op_87();goto end;
       	l_0x88:op_88();goto end;
       	l_0x89:op_89();goto end;
       	l_0x8a:op_8a();goto end;
       	l_0x8b:op_8b();goto end;
       	l_0x8c:op_8c();goto end;
       	l_0x8d:op_8d();goto end;
       	l_0x8e:op_8e();goto end;
       	l_0x8f:op_8f();goto end;
       	l_0x90:op_90();goto end;
       	l_0x91:op_91();goto end;
       	l_0x92:op_92();goto end;
       	l_0x93:op_93();goto end;
       	l_0x94:op_94();goto end;
       	l_0x95:op_95();goto end;
       	l_0x96:op_96();goto end;
       	l_0x97:op_97();goto end;
       	l_0x98:op_98();goto end;
       	l_0x99:op_99();goto end;
       	l_0x9a:op_9a();goto end;
       	l_0x9b:op_9b();goto end;
       	l_0x9c:op_9c();goto end;
       	l_0x9d:op_9d();goto end;
       	l_0x9e:op_9e();goto end;
       	l_0x9f:op_9f();goto end;
       	l_0xa0:op_a0();goto end;
       	l_0xa1:op_a1();goto end;
       	l_0xa2:op_a2();goto end;
       	l_0xa3:op_a3();goto end;
       	l_0xa4:op_a4();goto end;
       	l_0xa5:op_a5();goto end;
       	l_0xa6:op_a6();goto end;
       	l_0xa7:op_a7();goto end;
       	l_0xa8:op_a8();goto end;
       	l_0xa9:op_a9();goto end;
       	l_0xaa:op_aa();goto end;
       	l_0xab:op_ab();goto end;
       	l_0xac:op_ac();goto end;
       	l_0xad:op_ad();goto end;
       	l_0xae:op_ae();goto end;
       	l_0xaf:op_af();goto end;
       	l_0xb0:op_b0();goto end;
       	l_0xb1:op_b1();goto end;
       	l_0xb2:op_b2();goto end;
       	l_0xb3:op_b3();goto end;
       	l_0xb4:op_b4();goto end;
       	l_0xb5:op_b5();goto end;
       	l_0xb6:op_b6();goto end;
       	l_0xb7:op_b7();goto end;
       	l_0xb8:op_b8();goto end;
       	l_0xb9:op_b9();goto end;
       	l_0xba:op_ba();goto end;
       	l_0xbb:op_bb();goto end;
       	l_0xbc:op_bc();goto end;
       	l_0xbd:op_bd();goto end;
       	l_0xbe:op_be();goto end;
       	l_0xbf:op_bf();goto end;
       	l_0xc0:op_c0();goto end;
       	l_0xc1:op_c1();goto end;
       	l_0xc2:op_c2();goto end;
       	l_0xc3:op_c3();goto end;
       	l_0xc4:op_c4();goto end;
       	l_0xc5:op_c5();goto end;
       	l_0xc6:op_c6();goto end;
       	l_0xc7:op_c7();goto end;
       	l_0xc8:op_c8();goto end;
       	l_0xc9:op_c9();goto end;
       	l_0xca:op_ca();goto end;
       	l_0xcb:op_cb();goto end;
       	l_0xcc:op_cc();goto end;
       	l_0xcd:op_cd();goto end;
       	l_0xce:op_ce();goto end;
       	l_0xcf:op_cf();goto end;
       	l_0xd0:op_d0();goto end;
       	l_0xd1:op_d1();goto end;
       	l_0xd2:op_d2();goto end;
       	l_0xd3:op_d3();goto end;
       	l_0xd4:op_d4();goto end;
       	l_0xd5:op_d5();goto end;
       	l_0xd6:op_d6();goto end;
       	l_0xd7:op_d7();goto end;
       	l_0xd8:op_d8();goto end;
       	l_0xd9:op_d9();goto end;
       	l_0xda:op_da();goto end;
       	l_0xdb:op_db();goto end;
       	l_0xdc:op_dc();goto end;
       	l_0xdd:op_dd();goto end;
       	l_0xde:op_de();goto end;
       	l_0xdf:op_df();goto end;
       	l_0xe0:op_e0();goto end;
       	l_0xe1:op_e1();goto end;
       	l_0xe2:op_e2();goto end;
       	l_0xe3:op_e3();goto end;
       	l_0xe4:op_e4();goto end;
       	l_0xe5:op_e5();goto end;
       	l_0xe6:op_e6();goto end;
       	l_0xe7:op_e7();goto end;
       	l_0xe8:op_e8();goto end;
       	l_0xe9:op_e9();goto end;
       	l_0xea:op_ea();goto end;
       	l_0xeb:op_eb();goto end;
       	l_0xec:op_ec();goto end;
       	l_0xed:op_ed();goto end;
       	l_0xee:op_ee();goto end;
       	l_0xef:op_ef();goto end;
       	l_0xf0:op_f0();goto end;
       	l_0xf1:op_f1();goto end;
       	l_0xf2:op_f2();goto end;
       	l_0xf3:op_f3();goto end;
       	l_0xf4:op_f4();goto end;
       	l_0xf5:op_f5();goto end;
       	l_0xf6:op_f6();goto end;
       	l_0xf7:op_f7();goto end;
       	l_0xf8:op_f8();goto end;
       	l_0xf9:op_f9();goto end;
       	l_0xfa:op_fa();goto end;
       	l_0xfb:op_fb();goto end;
       	l_0xfc:op_fc();goto end;
       	l_0xfd:op_fd();goto end;
       	l_0xfe:op_fe();goto end;
       	l_0xff:op_ff();goto end;

        end: ;
        
	} while( z80_ICount > 0 );

	z80_ICount -= Z80.extra_cycles;
    Z80.extra_cycles = 0;

    return cycles - z80_ICount;
}

/****************************************************************************
 * Burn 'cycles' T-states. Adjust R register for the lost time
 ****************************************************************************/
void z80_burn(int cycles)
{
	if( cycles > 0 )
	{
		/* NOP takes 4 cycles per instruction */
		int n = (cycles + 3) / 4;
		_R += n;
		z80_ICount -= 4 * n;
	}
}

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
unsigned z80_get_context (void *dst)
{
	if( dst )
	    *(Z80_Regs*)dst = Z80;
	return sizeof(Z80_Regs);
}

/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void z80_set_context (void *src)
{
	if( src )
		Z80 = *(Z80_Regs*)src;
    change_pc(_PCD);
}

/****************************************************************************
 * Get a pointer to a cycle count table
 ****************************************************************************/
void *z80_get_cycle_table (int which)
{
	if (which >= 0 && which <= Z80_TABLE_xycb)
		return cc[which];
	return NULL;
}

/****************************************************************************
 * Set a new cycle count table
 ****************************************************************************/
void z80_set_cycle_table (int which, void *new_table)
{
	if (which >= 0 && which <= Z80_TABLE_xycb)
		cc[which] = (UINT8*)new_table;
}

/****************************************************************************
 * Return program counter
 ****************************************************************************/
unsigned z80_get_pc (void)
{
    return _PCD;
}

/****************************************************************************
 * Set program counter
 ****************************************************************************/
void z80_set_pc (unsigned val)
{
	_PC = val;
	change_pc(_PCD);
}

/****************************************************************************
 * Return stack pointer
 ****************************************************************************/
unsigned z80_get_sp (void)
{
	return _SPD;
}

/****************************************************************************
 * Set stack pointer
 ****************************************************************************/
void z80_set_sp (unsigned val)
{
	_SP = val;
}

/****************************************************************************
 * Return a specific register
 ****************************************************************************/
unsigned z80_get_reg (int regnum)
{
	switch( regnum )
	{
		case Z80_PC: return Z80.PC.w.l;
		case Z80_SP: return Z80.SP.w.l;
		case Z80_AF: return Z80.AF.w.l;
		case Z80_BC: return Z80.BC.w.l;
		case Z80_DE: return Z80.DE.w.l;
		case Z80_HL: return Z80.HL.w.l;
		case Z80_IX: return Z80.IX.w.l;
		case Z80_IY: return Z80.IY.w.l;
        case Z80_R: return (Z80.R & 0x7f) | (Z80.R2 & 0x80);
		case Z80_I: return Z80.I;
		case Z80_AF2: return Z80.AF2.w.l;
		case Z80_BC2: return Z80.BC2.w.l;
		case Z80_DE2: return Z80.DE2.w.l;
		case Z80_HL2: return Z80.HL2.w.l;
		case Z80_IM: return Z80.IM;
		case Z80_IFF1: return Z80.IFF1;
		case Z80_IFF2: return Z80.IFF2;
		case Z80_HALT: return Z80.HALT;
		case Z80_NMI_STATE: return Z80.nmi_state;
		case Z80_IRQ_STATE: return Z80.irq_state;
		case Z80_DC0: return Z80.int_state[0];
		case Z80_DC1: return Z80.int_state[1];
		case Z80_DC2: return Z80.int_state[2];
		case Z80_DC3: return Z80.int_state[3];
        case REG_PREVIOUSPC: return Z80.PREPC.w.l;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = _SPD + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					return RM( offset ) | ( RM( offset + 1) << 8 );
			}
	}
    return 0;
}

/****************************************************************************
 * Set a specific register
 ****************************************************************************/
void z80_set_reg (int regnum, unsigned val)
{
	switch( regnum )
	{
		case Z80_PC: Z80.PC.w.l = val; break;
		case Z80_SP: Z80.SP.w.l = val; break;
		case Z80_AF: Z80.AF.w.l = val; break;
		case Z80_BC: Z80.BC.w.l = val; break;
		case Z80_DE: Z80.DE.w.l = val; break;
		case Z80_HL: Z80.HL.w.l = val; break;
		case Z80_IX: Z80.IX.w.l = val; break;
		case Z80_IY: Z80.IY.w.l = val; break;
        case Z80_R: Z80.R = val; Z80.R2 = val & 0x80; break;
		case Z80_I: Z80.I = val; break;
		case Z80_AF2: Z80.AF2.w.l = val; break;
		case Z80_BC2: Z80.BC2.w.l = val; break;
		case Z80_DE2: Z80.DE2.w.l = val; break;
		case Z80_HL2: Z80.HL2.w.l = val; break;
		case Z80_IM: Z80.IM = val; break;
		case Z80_IFF1: Z80.IFF1 = val; break;
		case Z80_IFF2: Z80.IFF2 = val; break;
		case Z80_HALT: Z80.HALT = val; break;
		case Z80_NMI_STATE: z80_set_nmi_line(val); break;
		case Z80_IRQ_STATE: z80_set_irq_line(0,val); break;
		case Z80_DC0: Z80.int_state[0] = val; break;
		case Z80_DC1: Z80.int_state[1] = val; break;
		case Z80_DC2: Z80.int_state[2] = val; break;
		case Z80_DC3: Z80.int_state[3] = val; break;
        default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = _SPD + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
				{
					WM( offset, val & 0xff );
					WM( offset+1, (val >> 8) & 0xff );
				}
			}
    }
}

/****************************************************************************
 * Set NMI line state
 ****************************************************************************/
void z80_set_nmi_line(int state)
{
	if( Z80.nmi_state == state ) return;

    Z80.nmi_state = state;
	if( state == CLEAR_LINE ) return;

	_PPC = -1;			/* there isn't a valid previous program counter */
	LEAVE_HALT; 		/* Check if processor was halted */

	_IFF1 = 0;
    PUSH( PC );
	_PCD = 0x0066;
	Z80.extra_cycles += 11;
}

/****************************************************************************
 * Set IRQ line state
 ****************************************************************************/
void z80_set_irq_line(int irqline, int state)
{
    Z80.irq_state = state;
	if( state == CLEAR_LINE ) return;

	if( Z80.irq_max )
	{
		int daisychain, device, int_state;
		daisychain = (*Z80.irq_callback)(irqline);
		device = daisychain >> 8;
		int_state = daisychain & 0xff;

		if( Z80.int_state[device] != int_state )
		{
			/* set new interrupt status */
            Z80.int_state[device] = int_state;
			/* check interrupt status */
			Z80.request_irq = Z80.service_irq = -1;

            /* search higher IRQ or IEO */
			for( device = 0 ; device < Z80.irq_max ; device ++ )
			{
				/* IEO = disable ? */
				if( Z80.int_state[device] & Z80_INT_IEO )
				{
					Z80.request_irq = -1;		/* if IEO is disable , masking lower IRQ */
					Z80.service_irq = device;	/* set highest interrupt service device */
				}
				/* IRQ = request ? */
				if( Z80.int_state[device] & Z80_INT_REQ )
					Z80.request_irq = device;
			}
			if( Z80.request_irq < 0 ) return;
		}
		else
		{
			return;
		}
	}
	take_interrupt();
}

/****************************************************************************
 * Set IRQ vector callback
 ****************************************************************************/
void z80_set_irq_callback(int (*callback)(int))
{
    Z80.irq_callback = callback;
}

/****************************************************************************
 * Save CPU state
 ****************************************************************************/
void z80_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT16(file, "z80", cpu, "AF", &Z80.AF.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "BC", &Z80.BC.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "DE", &Z80.DE.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "HL", &Z80.HL.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "IX", &Z80.IX.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "IY", &Z80.IY.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "PC", &Z80.PC.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "SP", &Z80.SP.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "AF2", &Z80.AF2.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "BC2", &Z80.BC2.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "DE2", &Z80.DE2.w.l, 1);
	state_save_UINT16(file, "z80", cpu, "HL2", &Z80.HL2.w.l, 1);
	state_save_UINT8(file, "z80", cpu, "R", &Z80.R, 1);
	state_save_UINT8(file, "z80", cpu, "R2", &Z80.R2, 1);
	state_save_UINT8(file, "z80", cpu, "IFF1", &Z80.IFF1, 1);
	state_save_UINT8(file, "z80", cpu, "IFF2", &Z80.IFF2, 1);
	state_save_UINT8(file, "z80", cpu, "HALT", &Z80.HALT, 1);
	state_save_UINT8(file, "z80", cpu, "IM", &Z80.IM, 1);
	state_save_UINT8(file, "z80", cpu, "I", &Z80.I, 1);
	state_save_UINT8(file, "z80", cpu, "irq_max", &Z80.irq_max, 1);
	state_save_INT8(file, "z80", cpu, "request_irq", &Z80.request_irq, 1);
	state_save_INT8(file, "z80", cpu, "service_irq", &Z80.service_irq, 1);
	state_save_UINT8(file, "z80", cpu, "int_state", Z80.int_state, 4);
	state_save_UINT8(file, "z80", cpu, "nmi_state", &Z80.nmi_state, 1);
	state_save_UINT8(file, "z80", cpu, "irq_state", &Z80.irq_state, 1);
	/* daisy chain needs to be saved by z80ctc.c somehow */
}

/****************************************************************************
 * Load CPU state
 ****************************************************************************/
void z80_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT16(file, "z80", cpu, "AF", &Z80.AF.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "BC", &Z80.BC.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "DE", &Z80.DE.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "HL", &Z80.HL.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "IX", &Z80.IX.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "IY", &Z80.IY.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "PC", &Z80.PC.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "SP", &Z80.SP.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "AF2", &Z80.AF2.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "BC2", &Z80.BC2.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "DE2", &Z80.DE2.w.l, 1);
	state_load_UINT16(file, "z80", cpu, "HL2", &Z80.HL2.w.l, 1);
	state_load_UINT8(file, "z80", cpu, "R", &Z80.R, 1);
	state_load_UINT8(file, "z80", cpu, "R2", &Z80.R2, 1);
	state_load_UINT8(file, "z80", cpu, "IFF1", &Z80.IFF1, 1);
	state_load_UINT8(file, "z80", cpu, "IFF2", &Z80.IFF2, 1);
	state_load_UINT8(file, "z80", cpu, "HALT", &Z80.HALT, 1);
	state_load_UINT8(file, "z80", cpu, "IM", &Z80.IM, 1);
	state_load_UINT8(file, "z80", cpu, "I", &Z80.I, 1);
	state_load_UINT8(file, "z80", cpu, "irq_max", &Z80.irq_max, 1);
	state_load_INT8(file, "z80", cpu, "request_irq", &Z80.request_irq, 1);
	state_load_INT8(file, "z80", cpu, "service_irq", &Z80.service_irq, 1);
	state_load_UINT8(file, "z80", cpu, "int_state", Z80.int_state, 4);
	state_load_UINT8(file, "z80", cpu, "nmi_state", &Z80.nmi_state, 1);
	state_load_UINT8(file, "z80", cpu, "irq_state", &Z80.irq_state, 1);
    /* daisy chain needs to be restored by z80ctc.c somehow */
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *z80_info(void *context, int regnum)
{
    switch( regnum )
	{
		case CPU_INFO_NAME: return "Z80";
        case CPU_INFO_FAMILY: return "Zilog Z80";
		case CPU_INFO_VERSION: return "3.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) 1998,1999 Juergen Buchmueller, all rights reserved.";
	}
	return "";
}

unsigned z80_dasm( char *buffer, unsigned pc )
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

