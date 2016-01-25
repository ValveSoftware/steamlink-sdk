/*****************************************************************************
 *
 *	 i8085.c
 *	 Portable I8085A emulator V1.2
 *
 *	 Copyright (c) 1999 Juergen Buchmueller, all rights reserved.
 *	 Partially based on information out of Z80Em by Marcel De Kogel
 *
 * changes in V1.2
 *	 - corrected cycle counts for these classes of opcodes
 *	   Thanks go to Jim Battle <frustum@pacbell.bet>
 *
 *					808x	 Z80
 *	   DEC A		   5	   4	\
 *	   INC A		   5	   4	 \
 *	   LD A,B		   5	   4	  >-- Z80 is faster
 *	   JP (HL)		   5	   4	 /
 *	   CALL cc,nnnn: 11/17	 10/17	/
 *
 *	   INC HL		   5	   6	\
 *	   DEC HL		   5	   6	 \
 *	   LD SP,HL 	   5	   6	  \
 *	   ADD HL,BC	  10	  11	   \
 *	   INC (HL) 	  10	  11		>-- 8080 is faster
 *	   DEC (HL) 	  10	  11	   /
 *	   IN A,(#) 	  10	  11	  /
 *	   OUT (#),A	  10	  11	 /
 *	   EX (SP),HL	  18	  19	/
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

#define VERBOSE 0

#include "driver.h"
#include "state.h"
#include "osd_cpu.h"
#include "i8085.h"
#include "i8085cpu.h"
#include "i8085daa.h"

#if VERBOSE
#include <stdio.h>
#include "driver.h"
#define LOG(x) logerror x
#else
#define LOG(x)
#endif

typedef struct {
	int 	cputype;	/* 0 8080, 1 8085A */
	PAIR	PC,SP,AF,BC,DE,HL,XX;
	UINT8	HALT;
	UINT8	IM; 		/* interrupt mask */
	UINT8	IREQ;		/* requested interrupts */
	UINT8	ISRV;		/* serviced interrupt */
	UINT32	INTR;		/* vector for INTR */
	UINT32	IRQ2;		/* scheduled interrupt address */
	UINT32	IRQ1;		/* executed interrupt address */
	INT8	nmi_state;
	INT8	irq_state[4];
	INT8	filler; /* align on dword boundary */
	int 	(*irq_callback)(int);
	void	(*sod_callback)(int state);
}	i8085_Regs;

int i8085_ICount = 0;

static i8085_Regs I;
static UINT8 ZS[256];
static UINT8 ZSP[256];

static UINT8 ROP(void)
{
	return cpu_readop(I.PC.w.l++);
}

static UINT8 ARG(void)
{
	return cpu_readop_arg(I.PC.w.l++);
}

static UINT16 ARG16(void)
{
	UINT16 w;
	w  = cpu_readop_arg(I.PC.d);
	I.PC.w.l++;
	w += cpu_readop_arg(I.PC.d) << 8;
	I.PC.w.l++;
	return w;
}

static UINT8 RM(UINT32 a)
{
	return cpu_readmem16(a);
}

static void WM(UINT32 a, UINT8 v)
{
	cpu_writemem16(a, v);
}

static	void illegal(void)
{
#if VERBOSE
	UINT16 pc = I.PC.w.l - 1;
	LOG(("i8085 illegal instruction %04X $%02X\n", pc, cpu_readop(pc)));
#endif
}

INLINE void execute_one(int opcode)
{
	switch (opcode)
	{
		case 0x00: i8085_ICount -= 4;	/* NOP	*/
			/* no op */
			break;
		case 0x01: i8085_ICount -= 10;	/* LXI	B,nnnn */
			I.BC.w.l = ARG16();
			break;
		case 0x02: i8085_ICount -= 7;	/* STAX B */
			WM(I.BC.d, I.AF.b.h);
			break;
		case 0x03: i8085_ICount -= 5;	/* INX	B */
			I.BC.w.l++;
			break;
		case 0x04: i8085_ICount -= 5;	/* INR	B */
			M_INR(I.BC.b.h);
			break;
		case 0x05: i8085_ICount -= 5;	/* DCR	B */
			M_DCR(I.BC.b.h);
			break;
		case 0x06: i8085_ICount -= 7;	/* MVI	B,nn */
			M_MVI(I.BC.b.h);
			break;
		case 0x07: i8085_ICount -= 4;	/* RLC	*/
			M_RLC;
			break;

		case 0x08: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0x09: i8085_ICount -= 10;	/* DAD	B */
			M_DAD(BC);
			break;
		case 0x0a: i8085_ICount -= 7;	/* LDAX B */
			I.AF.b.h = RM(I.BC.d);
			break;
		case 0x0b: i8085_ICount -= 5;	/* DCX	B */
			I.BC.w.l--;
			break;
		case 0x0c: i8085_ICount -= 5;	/* INR	C */
			M_INR(I.BC.b.l);
			break;
		case 0x0d: i8085_ICount -= 5;	/* DCR	C */
			M_DCR(I.BC.b.l);
			break;
		case 0x0e: i8085_ICount -= 7;	/* MVI	C,nn */
			M_MVI(I.BC.b.l);
			break;
		case 0x0f: i8085_ICount -= 4;	/* RRC	*/
			M_RRC;
			break;

		case 0x10: i8085_ICount -= 8;	/* ????  */
			illegal();
			break;
		case 0x11: i8085_ICount -= 10;	/* LXI	D,nnnn */
			I.DE.w.l = ARG16();
			break;
		case 0x12: i8085_ICount -= 7;	/* STAX D */
			WM(I.DE.d, I.AF.b.h);
			break;
		case 0x13: i8085_ICount -= 5;	/* INX	D */
			I.DE.w.l++;
			break;
		case 0x14: i8085_ICount -= 5;	/* INR	D */
			M_INR(I.DE.b.h);
			break;
		case 0x15: i8085_ICount -= 5;	/* DCR	D */
			M_DCR(I.DE.b.h);
			break;
		case 0x16: i8085_ICount -= 7;	/* MVI	D,nn */
			M_MVI(I.DE.b.h);
			break;
		case 0x17: i8085_ICount -= 4;	/* RAL	*/
			M_RAL;
			break;

		case 0x18: i8085_ICount -= 7;	/* ????? */
			illegal();
			break;
		case 0x19: i8085_ICount -= 10;	/* DAD	D */
			M_DAD(DE);
			break;
		case 0x1a: i8085_ICount -= 7;	/* LDAX D */
			I.AF.b.h = RM(I.DE.d);
			break;
		case 0x1b: i8085_ICount -= 5;	/* DCX	D */
			I.DE.w.l--;
			break;
		case 0x1c: i8085_ICount -= 5;	/* INR	E */
			M_INR(I.DE.b.l);
			break;
		case 0x1d: i8085_ICount -= 5;	/* DCR	E */
			M_DCR(I.DE.b.l);
			break;
		case 0x1e: i8085_ICount -= 7;	/* MVI	E,nn */
			M_MVI(I.DE.b.l);
			break;
		case 0x1f: i8085_ICount -= 4;	/* RAR	*/
			M_RAR;
			break;

		case 0x20:
			if( I.cputype )
			{
				i8085_ICount -= 7;		/* RIM	*/
				I.AF.b.h = I.IM;
			}
			else
			{
				i8085_ICount -= 7;		/* ???	*/
			}
			break;
		case 0x21: i8085_ICount -= 10;	/* LXI	H,nnnn */
			I.HL.w.l = ARG16();
			break;
		case 0x22: i8085_ICount -= 16;	/* SHLD nnnn */
			I.XX.w.l = ARG16();
			WM(I.XX.d, I.HL.b.l);
			I.XX.w.l++;
			WM(I.XX.d, I.HL.b.h);
			break;
		case 0x23: i8085_ICount -= 5;	/* INX	H */
			I.HL.w.l++;
			break;
		case 0x24: i8085_ICount -= 5;	/* INR	H */
			M_INR(I.HL.b.h);
			break;
		case 0x25: i8085_ICount -= 5;	/* DCR	H */
			M_DCR(I.HL.b.h);
			break;
		case 0x26: i8085_ICount -= 7;	/* MVI	H,nn */
			M_MVI(I.HL.b.h);
			break;
		case 0x27: i8085_ICount -= 4;	/* DAA	*/
			I.XX.d = I.AF.b.h;
			if (I.AF.b.l & CF) I.XX.d |= 0x100;
			if (I.AF.b.l & HF) I.XX.d |= 0x200;
			if (I.AF.b.l & NF) I.XX.d |= 0x400;
			I.AF.w.l = DAA[I.XX.d];
			break;

		case 0x28: i8085_ICount -= 7;	/* ???? */
			illegal();
			break;
		case 0x29: i8085_ICount -= 10;	/* DAD	H */
			M_DAD(HL);
			break;
		case 0x2a: i8085_ICount -= 16;	/* LHLD nnnn */
			I.XX.d = ARG16();
			I.HL.b.l = RM(I.XX.d);
			I.XX.w.l++;
			I.HL.b.h = RM(I.XX.d);
			break;
		case 0x2b: i8085_ICount -= 5;	/* DCX	H */
			I.HL.w.l--;
			break;
		case 0x2c: i8085_ICount -= 5;	/* INR	L */
			M_INR(I.HL.b.l);
			break;
		case 0x2d: i8085_ICount -= 5;	/* DCR	L */
			M_DCR(I.HL.b.l);
			break;
		case 0x2e: i8085_ICount -= 7;	/* MVI	L,nn */
			M_MVI(I.HL.b.l);
			break;
		case 0x2f: i8085_ICount -= 4;	/* CMA	*/
			I.AF.b.h ^= 0xff;
			I.AF.b.l |= HF + NF;
			break;

		case 0x30:
			if( I.cputype )
			{
				i8085_ICount -= 7;		/* SIM	*/
				if ((I.IM ^ I.AF.b.h) & 0x80)
					if (I.sod_callback) (*I.sod_callback)(I.AF.b.h >> 7);
				I.IM &= (IM_SID + IM_IEN + IM_TRAP);
				I.IM |= (I.AF.b.h & ~(IM_SID + IM_SOD + IM_IEN + IM_TRAP));
				if (I.AF.b.h & 0x80) I.IM |= IM_SOD;
			}
			else
			{
				i8085_ICount -= 4;		/* ???	*/
			}
			break;
		case 0x31: i8085_ICount -= 10;	/* LXI SP,nnnn */
			I.SP.w.l = ARG16();
			break;
		case 0x32: i8085_ICount -= 13;	/* STAX nnnn */
			I.XX.d = ARG16();
			WM(I.XX.d, I.AF.b.h);
			break;
		case 0x33: i8085_ICount -= 5;	/* INX	SP */
			I.SP.w.l++;
			break;
		case 0x34: i8085_ICount -= 10;	/* INR	M */
			I.XX.b.l = RM(I.HL.d);
			M_INR(I.XX.b.l);
			WM(I.HL.d, I.XX.b.l);
			break;
		case 0x35: i8085_ICount -= 10;	/* DCR	M */
			I.XX.b.l = RM(I.HL.d);
			M_DCR(I.XX.b.l);
			WM(I.HL.d, I.XX.b.l);
			break;
		case 0x36: i8085_ICount -= 10;	/* MVI	M,nn */
			I.XX.b.l = ARG();
			WM(I.HL.d, I.XX.b.l);
			break;
		case 0x37: i8085_ICount -= 4;	/* STC	*/
			I.AF.b.l = (I.AF.b.l & ~(HF + NF)) | CF;
			break;

		case 0x38: i8085_ICount -= 7;	/* ???? */
			illegal();
			break;
		case 0x39: i8085_ICount -= 10;	/* DAD SP */
			M_DAD(SP);
			break;
		case 0x3a: i8085_ICount -= 13;	/* LDAX nnnn */
			I.XX.d = ARG16();
			I.AF.b.h = RM(I.XX.d);
			break;
		case 0x3b: i8085_ICount -= 5;	/* DCX	SP */
			I.SP.w.l--;
			break;
		case 0x3c: i8085_ICount -= 5;	/* INR	A */
			M_INR(I.AF.b.h);
			break;
		case 0x3d: i8085_ICount -= 5;	/* DCR	A */
			M_DCR(I.AF.b.h);
			break;
		case 0x3e: i8085_ICount -= 7;	/* MVI	A,nn */
			M_MVI(I.AF.b.h);
			break;
		case 0x3f: i8085_ICount -= 4;	/* CMF	*/
			I.AF.b.l = ((I.AF.b.l & ~(HF + NF)) |
					   ((I.AF.b.l & CF) << 4)) ^ CF;
			break;

		case 0x40: i8085_ICount -= 5;	/* MOV	B,B */
			/* no op */
			break;
		case 0x41: i8085_ICount -= 5;	/* MOV	B,C */
			I.BC.b.h = I.BC.b.l;
			break;
		case 0x42: i8085_ICount -= 5;	/* MOV	B,D */
			I.BC.b.h = I.DE.b.h;
			break;
		case 0x43: i8085_ICount -= 5;	/* MOV	B,E */
			I.BC.b.h = I.DE.b.l;
			break;
		case 0x44: i8085_ICount -= 5;	/* MOV	B,H */
			I.BC.b.h = I.HL.b.h;
			break;
		case 0x45: i8085_ICount -= 5;	/* MOV	B,L */
			I.BC.b.h = I.HL.b.l;
			break;
		case 0x46: i8085_ICount -= 7;	/* MOV	B,M */
			I.BC.b.h = RM(I.HL.d);
			break;
		case 0x47: i8085_ICount -= 5;	/* MOV	B,A */
			I.BC.b.h = I.AF.b.h;
			break;

		case 0x48: i8085_ICount -= 5;	/* MOV	C,B */
			I.BC.b.l = I.BC.b.h;
			break;
		case 0x49: i8085_ICount -= 5;	/* MOV	C,C */
			/* no op */
			break;
		case 0x4a: i8085_ICount -= 5;	/* MOV	C,D */
			I.BC.b.l = I.DE.b.h;
			break;
		case 0x4b: i8085_ICount -= 5;	/* MOV	C,E */
			I.BC.b.l = I.DE.b.l;
			break;
		case 0x4c: i8085_ICount -= 5;	/* MOV	C,H */
			I.BC.b.l = I.HL.b.h;
			break;
		case 0x4d: i8085_ICount -= 5;	/* MOV	C,L */
			I.BC.b.l = I.HL.b.l;
			break;
		case 0x4e: i8085_ICount -= 7;	/* MOV	C,M */
			I.BC.b.l = RM(I.HL.d);
			break;
		case 0x4f: i8085_ICount -= 5;	/* MOV	C,A */
			I.BC.b.l = I.AF.b.h;
			break;

		case 0x50: i8085_ICount -= 5;	/* MOV	D,B */
			I.DE.b.h = I.BC.b.h;
			break;
		case 0x51: i8085_ICount -= 5;	/* MOV	D,C */
			I.DE.b.h = I.BC.b.l;
			break;
		case 0x52: i8085_ICount -= 5;	/* MOV	D,D */
			/* no op */
			break;
		case 0x53: i8085_ICount -= 5;	/* MOV	D,E */
			I.DE.b.h = I.DE.b.l;
			break;
		case 0x54: i8085_ICount -= 5;	/* MOV	D,H */
			I.DE.b.h = I.HL.b.h;
			break;
		case 0x55: i8085_ICount -= 5;	/* MOV	D,L */
			I.DE.b.h = I.HL.b.l;
			break;
		case 0x56: i8085_ICount -= 7;	/* MOV	D,M */
			I.DE.b.h = RM(I.HL.d);
			break;
		case 0x57: i8085_ICount -= 5;	/* MOV	D,A */
			I.DE.b.h = I.AF.b.h;
			break;

		case 0x58: i8085_ICount -= 5;	/* MOV	E,B */
			I.DE.b.l = I.BC.b.h;
			break;
		case 0x59: i8085_ICount -= 5;	/* MOV	E,C */
			I.DE.b.l = I.BC.b.l;
			break;
		case 0x5a: i8085_ICount -= 5;	/* MOV	E,D */
			I.DE.b.l = I.DE.b.h;
			break;
		case 0x5b: i8085_ICount -= 5;	/* MOV	E,E */
			/* no op */
			break;
		case 0x5c: i8085_ICount -= 5;	/* MOV	E,H */
			I.DE.b.l = I.HL.b.h;
			break;
		case 0x5d: i8085_ICount -= 5;	/* MOV	E,L */
			I.DE.b.l = I.HL.b.l;
			break;
		case 0x5e: i8085_ICount -= 7;	/* MOV	E,M */
			I.DE.b.l = RM(I.HL.d);
			break;
		case 0x5f: i8085_ICount -= 5;	/* MOV	E,A */
			I.DE.b.l = I.AF.b.h;
			break;

		case 0x60: i8085_ICount -= 5;	/* MOV	H,B */
			I.HL.b.h = I.BC.b.h;
			break;
		case 0x61: i8085_ICount -= 5;	/* MOV	H,C */
			I.HL.b.h = I.BC.b.l;
			break;
		case 0x62: i8085_ICount -= 5;	/* MOV	H,D */
			I.HL.b.h = I.DE.b.h;
			break;
		case 0x63: i8085_ICount -= 5;	/* MOV	H,E */
			I.HL.b.h = I.DE.b.l;
			break;
		case 0x64: i8085_ICount -= 5;	/* MOV	H,H */
			/* no op */
			break;
		case 0x65: i8085_ICount -= 5;	/* MOV	H,L */
			I.HL.b.h = I.HL.b.l;
			break;
		case 0x66: i8085_ICount -= 7;	/* MOV	H,M */
			I.HL.b.h = RM(I.HL.d);
			break;
		case 0x67: i8085_ICount -= 5;	/* MOV	H,A */
			I.HL.b.h = I.AF.b.h;
			break;

		case 0x68: i8085_ICount -= 5;	/* MOV	L,B */
			I.HL.b.l = I.BC.b.h;
			break;
		case 0x69: i8085_ICount -= 5;	/* MOV	L,C */
			I.HL.b.l = I.BC.b.l;
			break;
		case 0x6a: i8085_ICount -= 5;	/* MOV	L,D */
			I.HL.b.l = I.DE.b.h;
			break;
		case 0x6b: i8085_ICount -= 5;	/* MOV	L,E */
			I.HL.b.l = I.DE.b.l;
			break;
		case 0x6c: i8085_ICount -= 5;	/* MOV	L,H */
			I.HL.b.l = I.HL.b.h;
			break;
		case 0x6d: i8085_ICount -= 5;	/* MOV	L,L */
			/* no op */
			break;
		case 0x6e: i8085_ICount -= 7;	/* MOV	L,M */
			I.HL.b.l = RM(I.HL.d);
			break;
		case 0x6f: i8085_ICount -= 5;	/* MOV	L,A */
			I.HL.b.l = I.AF.b.h;
			break;

		case 0x70: i8085_ICount -= 7;	/* MOV	M,B */
			WM(I.HL.d, I.BC.b.h);
			break;
		case 0x71: i8085_ICount -= 7;	/* MOV	M,C */
			WM(I.HL.d, I.BC.b.l);
			break;
		case 0x72: i8085_ICount -= 7;	/* MOV	M,D */
			WM(I.HL.d, I.DE.b.h);
			break;
		case 0x73: i8085_ICount -= 7;	/* MOV	M,E */
			WM(I.HL.d, I.DE.b.l);
			break;
		case 0x74: i8085_ICount -= 7;	/* MOV	M,H */
			WM(I.HL.d, I.HL.b.h);
			break;
		case 0x75: i8085_ICount -= 7;	/* MOV	M,L */
			WM(I.HL.d, I.HL.b.l);
			break;
		case 0x76: i8085_ICount -= 4;	/* HALT */
			I.PC.w.l--;
			I.HALT = 1;
			if (i8085_ICount > 0) i8085_ICount = 0;
			break;
		case 0x77: i8085_ICount -= 7;	/* MOV	M,A */
			WM(I.HL.d, I.AF.b.h);
			break;

		case 0x78: i8085_ICount -= 5;	/* MOV	A,B */
			I.AF.b.h = I.BC.b.h;
			break;
		case 0x79: i8085_ICount -= 5;	/* MOV	A,C */
			I.AF.b.h = I.BC.b.l;
			break;
		case 0x7a: i8085_ICount -= 5;	/* MOV	A,D */
			I.AF.b.h = I.DE.b.h;
			break;
		case 0x7b: i8085_ICount -= 5;	/* MOV	A,E */
			I.AF.b.h = I.DE.b.l;
			break;
		case 0x7c: i8085_ICount -= 5;	/* MOV	A,H */
			I.AF.b.h = I.HL.b.h;
			break;
		case 0x7d: i8085_ICount -= 5;	/* MOV	A,L */
			I.AF.b.h = I.HL.b.l;
			break;
		case 0x7e: i8085_ICount -= 7;	/* MOV	A,M */
			I.AF.b.h = RM(I.HL.d);
			break;
		case 0x7f: i8085_ICount -= 5;	/* MOV	A,A */
			/* no op */
			break;

		case 0x80: i8085_ICount -= 4;	/* ADD	B */
			M_ADD(I.BC.b.h);
			break;
		case 0x81: i8085_ICount -= 4;	/* ADD	C */
			M_ADD(I.BC.b.l);
			break;
		case 0x82: i8085_ICount -= 4;	/* ADD	D */
			M_ADD(I.DE.b.h);
			break;
		case 0x83: i8085_ICount -= 4;	/* ADD	E */
			M_ADD(I.DE.b.l);
			break;
		case 0x84: i8085_ICount -= 4;	/* ADD	H */
			M_ADD(I.HL.b.h);
			break;
		case 0x85: i8085_ICount -= 4;	/* ADD	L */
			M_ADD(I.HL.b.l);
			break;
		case 0x86: i8085_ICount -= 7;	/* ADD	M */
			M_ADD(RM(I.HL.d));
			break;
		case 0x87: i8085_ICount -= 4;	/* ADD	A */
			M_ADD(I.AF.b.h);
			break;

		case 0x88: i8085_ICount -= 4;	/* ADC	B */
			M_ADC(I.BC.b.h);
			break;
		case 0x89: i8085_ICount -= 4;	/* ADC	C */
			M_ADC(I.BC.b.l);
			break;
		case 0x8a: i8085_ICount -= 4;	/* ADC	D */
			M_ADC(I.DE.b.h);
			break;
		case 0x8b: i8085_ICount -= 4;	/* ADC	E */
			M_ADC(I.DE.b.l);
			break;
		case 0x8c: i8085_ICount -= 4;	/* ADC	H */
			M_ADC(I.HL.b.h);
			break;
		case 0x8d: i8085_ICount -= 4;	/* ADC	L */
			M_ADC(I.HL.b.l);
			break;
		case 0x8e: i8085_ICount -= 7;	/* ADC	M */
			M_ADC(RM(I.HL.d));
			break;
		case 0x8f: i8085_ICount -= 4;	/* ADC	A */
			M_ADC(I.AF.b.h);
			break;

		case 0x90: i8085_ICount -= 4;	/* SUB	B */
			M_SUB(I.BC.b.h);
			break;
		case 0x91: i8085_ICount -= 4;	/* SUB	C */
			M_SUB(I.BC.b.l);
			break;
		case 0x92: i8085_ICount -= 4;	/* SUB	D */
			M_SUB(I.DE.b.h);
			break;
		case 0x93: i8085_ICount -= 4;	/* SUB	E */
			M_SUB(I.DE.b.l);
			break;
		case 0x94: i8085_ICount -= 4;	/* SUB	H */
			M_SUB(I.HL.b.h);
			break;
		case 0x95: i8085_ICount -= 4;	/* SUB	L */
			M_SUB(I.HL.b.l);
			break;
		case 0x96: i8085_ICount -= 7;	/* SUB	M */
			M_SUB(RM(I.HL.d));
			break;
		case 0x97: i8085_ICount -= 4;	/* SUB	A */
			M_SUB(I.AF.b.h);
			break;

		case 0x98: i8085_ICount -= 4;	/* SBB	B */
			M_SBB(I.BC.b.h);
			break;
		case 0x99: i8085_ICount -= 4;	/* SBB	C */
			M_SBB(I.BC.b.l);
			break;
		case 0x9a: i8085_ICount -= 4;	/* SBB	D */
			M_SBB(I.DE.b.h);
			break;
		case 0x9b: i8085_ICount -= 4;	/* SBB	E */
			M_SBB(I.DE.b.l);
			break;
		case 0x9c: i8085_ICount -= 4;	/* SBB	H */
			M_SBB(I.HL.b.h);
			break;
		case 0x9d: i8085_ICount -= 4;	/* SBB	L */
			M_SBB(I.HL.b.l);
			break;
		case 0x9e: i8085_ICount -= 7;	/* SBB	M */
			M_SBB(RM(I.HL.d));
			break;
		case 0x9f: i8085_ICount -= 4;	/* SBB	A */
			M_SBB(I.AF.b.h);
			break;

		case 0xa0: i8085_ICount -= 4;	/* ANA	B */
			M_ANA(I.BC.b.h);
			break;
		case 0xa1: i8085_ICount -= 4;	/* ANA	C */
			M_ANA(I.BC.b.l);
			break;
		case 0xa2: i8085_ICount -= 4;	/* ANA	D */
			M_ANA(I.DE.b.h);
			break;
		case 0xa3: i8085_ICount -= 4;	/* ANA	E */
			M_ANA(I.DE.b.l);
			break;
		case 0xa4: i8085_ICount -= 4;	/* ANA	H */
			M_ANA(I.HL.b.h);
			break;
		case 0xa5: i8085_ICount -= 4;	/* ANA	L */
			M_ANA(I.HL.b.l);
			break;
		case 0xa6: i8085_ICount -= 7;	/* ANA	M */
			M_ANA(RM(I.HL.d));
			break;
		case 0xa7: i8085_ICount -= 4;	/* ANA	A */
			M_ANA(I.AF.b.h);
			break;

		case 0xa8: i8085_ICount -= 4;	/* XRA	B */
			M_XRA(I.BC.b.h);
			break;
		case 0xa9: i8085_ICount -= 4;	/* XRA	C */
			M_XRA(I.BC.b.l);
			break;
		case 0xaa: i8085_ICount -= 4;	/* XRA	D */
			M_XRA(I.DE.b.h);
			break;
		case 0xab: i8085_ICount -= 4;	/* XRA	E */
			M_XRA(I.DE.b.l);
			break;
		case 0xac: i8085_ICount -= 4;	/* XRA	H */
			M_XRA(I.HL.b.h);
			break;
		case 0xad: i8085_ICount -= 4;	/* XRA	L */
			M_XRA(I.HL.b.l);
			break;
		case 0xae: i8085_ICount -= 7;	/* XRA	M */
			M_XRA(RM(I.HL.d));
			break;
		case 0xaf: i8085_ICount -= 4;	/* XRA	A */
			M_XRA(I.AF.b.h);
			break;

		case 0xb0: i8085_ICount -= 4;	/* ORA	B */
			M_ORA(I.BC.b.h);
			break;
		case 0xb1: i8085_ICount -= 4;	/* ORA	C */
			M_ORA(I.BC.b.l);
			break;
		case 0xb2: i8085_ICount -= 4;	/* ORA	D */
			M_ORA(I.DE.b.h);
			break;
		case 0xb3: i8085_ICount -= 4;	/* ORA	E */
			M_ORA(I.DE.b.l);
			break;
		case 0xb4: i8085_ICount -= 4;	/* ORA	H */
			M_ORA(I.HL.b.h);
			break;
		case 0xb5: i8085_ICount -= 4;	/* ORA	L */
			M_ORA(I.HL.b.l);
			break;
		case 0xb6: i8085_ICount -= 7;	/* ORA	M */
			M_ORA(RM(I.HL.d));
			break;
		case 0xb7: i8085_ICount -= 4;	/* ORA	A */
			M_ORA(I.AF.b.h);
			break;

		case 0xb8: i8085_ICount -= 4;	/* CMP	B */
			M_CMP(I.BC.b.h);
			break;
		case 0xb9: i8085_ICount -= 4;	/* CMP	C */
			M_CMP(I.BC.b.l);
			break;
		case 0xba: i8085_ICount -= 4;	/* CMP	D */
			M_CMP(I.DE.b.h);
			break;
		case 0xbb: i8085_ICount -= 4;	/* CMP	E */
			M_CMP(I.DE.b.l);
			break;
		case 0xbc: i8085_ICount -= 4;	/* CMP	H */
			M_CMP(I.HL.b.h);
			break;
		case 0xbd: i8085_ICount -= 4;	/* CMP	L */
			M_CMP(I.HL.b.l);
			break;
		case 0xbe: i8085_ICount -= 7;	/* CMP	M */
			M_CMP(RM(I.HL.d));
			break;
		case 0xbf: i8085_ICount -= 4;	/* CMP	A */
			M_CMP(I.AF.b.h);
			break;

		case 0xc0: i8085_ICount -= 5;	/* RNZ	*/
			M_RET( !(I.AF.b.l & ZF) );
			break;
		case 0xc1: i8085_ICount -= 10;	/* POP	B */
			M_POP(BC);
			break;
		case 0xc2: i8085_ICount -= 10;	/* JNZ	nnnn */
			M_JMP( !(I.AF.b.l & ZF) );
			break;
		case 0xc3: i8085_ICount -= 10;	/* JMP	nnnn */
			M_JMP(1);
			break;
		case 0xc4: i8085_ICount -= 11;	/* CNZ	nnnn */
			M_CALL( !(I.AF.b.l & ZF) );
			break;
		case 0xc5: i8085_ICount -= 11;	/* PUSH B */
			M_PUSH(BC);
			break;
		case 0xc6: i8085_ICount -= 7;	/* ADI	nn */
			I.XX.b.l = ARG();
			M_ADD(I.XX.b.l);
				break;
		case 0xc7: i8085_ICount -= 11;	/* RST	0 */
			M_RST(0);
			break;

		case 0xc8: i8085_ICount -= 5;	/* RZ	*/
			M_RET( I.AF.b.l & ZF );
			break;
		case 0xc9: i8085_ICount -= 4;	/* RET	*/
			M_RET(1);
			break;
		case 0xca: i8085_ICount -= 10;	/* JZ	nnnn */
			M_JMP( I.AF.b.l & ZF );
			break;
		case 0xcb: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0xcc: i8085_ICount -= 11;	/* CZ	nnnn */
			M_CALL( I.AF.b.l & ZF );
			break;
		case 0xcd: i8085_ICount -= 11;	/* CALL nnnn */
			M_CALL(1);
			break;
		case 0xce: i8085_ICount -= 7;	/* ACI	nn */
			I.XX.b.l = ARG();
			M_ADC(I.XX.b.l);
			break;
		case 0xcf: i8085_ICount -= 11;	/* RST	1 */
			M_RST(1);
			break;

		case 0xd0: i8085_ICount -= 5;	/* RNC	*/
			M_RET( !(I.AF.b.l & CF) );
			break;
		case 0xd1: i8085_ICount -= 10;	/* POP	D */
			M_POP(DE);
			break;
		case 0xd2: i8085_ICount -= 10;	/* JNC	nnnn */
			M_JMP( !(I.AF.b.l & CF) );
			break;
		case 0xd3: i8085_ICount -= 10;	/* OUT	nn */
			M_OUT;
			break;
		case 0xd4: i8085_ICount -= 11;	/* CNC	nnnn */
			M_CALL( !(I.AF.b.l & CF) );
			break;
		case 0xd5: i8085_ICount -= 11;	/* PUSH D */
			M_PUSH(DE);
			break;
		case 0xd6: i8085_ICount -= 7;	/* SUI	nn */
			I.XX.b.l = ARG();
			M_SUB(I.XX.b.l);
			break;
		case 0xd7: i8085_ICount -= 11;	/* RST	2 */
			M_RST(2);
			break;

		case 0xd8: i8085_ICount -= 5;	/* RC	*/
			M_RET( I.AF.b.l & CF );
			break;
		case 0xd9: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0xda: i8085_ICount -= 10;	/* JC	nnnn */
			M_JMP( I.AF.b.l & CF );
			break;
		case 0xdb: i8085_ICount -= 10;	/* IN	nn */
			M_IN;
			break;
		case 0xdc: i8085_ICount -= 11;	/* CC	nnnn */
			M_CALL( I.AF.b.l & CF );
			break;
		case 0xdd: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0xde: i8085_ICount -= 7;	/* SBI	nn */
			I.XX.b.l = ARG();
			M_SBB(I.XX.b.l);
			break;
		case 0xdf: i8085_ICount -= 11;	/* RST	3 */
			M_RST(3);
			break;

		case 0xe0: i8085_ICount -= 5;	/* RPE	  */
			M_RET( !(I.AF.b.l & VF) );
			break;
		case 0xe1: i8085_ICount -= 10;	/* POP	H */
			M_POP(HL);
			break;
		case 0xe2: i8085_ICount -= 10;	/* JPE	nnnn */
			M_JMP( !(I.AF.b.l & VF) );
			break;
		case 0xe3: i8085_ICount -= 18;	/* XTHL */
			M_POP(XX);
			M_PUSH(HL);
			I.HL.d = I.XX.d;
			break;
		case 0xe4: i8085_ICount -= 11;	/* CPE	nnnn */
			M_CALL( !(I.AF.b.l & VF) );
			break;
		case 0xe5: i8085_ICount -= 11;	/* PUSH H */
			M_PUSH(HL);
			break;
		case 0xe6: i8085_ICount -= 7;	/* ANI	nn */
			I.XX.b.l = ARG();
			M_ANA(I.XX.b.l);
			break;
		case 0xe7: i8085_ICount -= 11;	/* RST	4 */
			M_RST(4);
			break;

		case 0xe8: i8085_ICount -= 5;	/* RPO	*/
			M_RET( I.AF.b.l & VF );
			break;
		case 0xe9: i8085_ICount -= 5;	/* PCHL */
			I.PC.d = I.HL.w.l;
			change_pc16(I.PC.d);
			break;
		case 0xea: i8085_ICount -= 10;	/* JPO	nnnn */
			M_JMP( I.AF.b.l & VF );
			break;
		case 0xeb: i8085_ICount -= 4;	/* XCHG */
			I.XX.d = I.DE.d;
			I.DE.d = I.HL.d;
			I.HL.d = I.XX.d;
			break;
		case 0xec: i8085_ICount -= 11;	/* CPO	nnnn */
			M_CALL( I.AF.b.l & VF );
			break;
		case 0xed: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0xee: i8085_ICount -= 7;	/* XRI	nn */
			I.XX.b.l = ARG();
			M_XRA(I.XX.b.l);
			break;
		case 0xef: i8085_ICount -= 11;	/* RST	5 */
			M_RST(5);
			break;

		case 0xf0: i8085_ICount -= 5;	/* RP	*/
			M_RET( !(I.AF.b.l&SF) );
			break;
		case 0xf1: i8085_ICount -= 10;	/* POP	A */
			M_POP(AF);
			break;
		case 0xf2: i8085_ICount -= 10;	/* JP	nnnn */
			M_JMP( !(I.AF.b.l & SF) );
			break;
		case 0xf3: i8085_ICount -= 4;	/* DI	*/
			/* remove interrupt enable */
			I.IM &= ~IM_IEN;
			break;
		case 0xf4: i8085_ICount -= 11;	/* CP	nnnn */
			M_CALL( !(I.AF.b.l & SF) );
			break;
		case 0xf5: i8085_ICount -= 11;	/* PUSH A */
			M_PUSH(AF);
			break;
		case 0xf6: i8085_ICount -= 7;	/* ORI	nn */
			I.XX.b.l = ARG();
			M_ORA(I.XX.b.l);
			break;
		case 0xf7: i8085_ICount -= 11;	/* RST	6 */
			M_RST(6);
			break;

		case 0xf8: i8085_ICount -= 5;	/* RM	*/
			M_RET( I.AF.b.l & SF );
			break;
		case 0xf9: i8085_ICount -= 5;	/* SPHL */
			I.SP.d = I.HL.d;
			break;
		case 0xfa: i8085_ICount -= 10;	/* JM	nnnn */
			M_JMP( I.AF.b.l & SF );
			break;
		case 0xfb: i8085_ICount -= 4;	/* EI */
			/* set interrupt enable */
			I.IM |= IM_IEN;
			/* remove serviced IRQ flag */
			I.IREQ &= ~I.ISRV;
			/* reset serviced IRQ */
			I.ISRV = 0;
			if( I.irq_state[0] != CLEAR_LINE )
			{
				LOG(("i8085 EI sets INTR\n"));
				I.IREQ |= IM_INTR;
				I.INTR = I8085_INTR;
			}
			if( I.cputype )
			{
				if( I.irq_state[1] != CLEAR_LINE )
				{
					LOG(("i8085 EI sets RST5.5\n"));
					I.IREQ |= IM_RST55;
				}
				if( I.irq_state[2] != CLEAR_LINE )
				{
					LOG(("i8085 EI sets RST6.5\n"));
					I.IREQ |= IM_RST65;
				}
				if( I.irq_state[3] != CLEAR_LINE )
				{
					LOG(("i8085 EI sets RST7.5\n"));
					I.IREQ |= IM_RST75;
				}
				/* find highest priority IREQ flag with
				   IM enabled and schedule for execution */
				if( !(I.IM & IM_RST75) && (I.IREQ & IM_RST75) )
				{
					I.ISRV = IM_RST75;
					I.IRQ2 = ADDR_RST75;
				}
				else
				if( !(I.IM & IM_RST65) && (I.IREQ & IM_RST65) )
				{
					I.ISRV = IM_RST65;
					I.IRQ2 = ADDR_RST65;
				}
				else
				if( !(I.IM & IM_RST55) && (I.IREQ & IM_RST55) )
				{
					I.ISRV = IM_RST55;
					I.IRQ2 = ADDR_RST55;
				}
				else
				if( !(I.IM & IM_INTR) && (I.IREQ & IM_INTR) )
				{
					I.ISRV = IM_INTR;
					I.IRQ2 = I.INTR;
				}
			}
			else
			{
				if( !(I.IM & IM_INTR) && (I.IREQ & IM_INTR) )
				{
					I.ISRV = IM_INTR;
					I.IRQ2 = I.INTR;
				}
			}
			break;
		case 0xfc: i8085_ICount -= 11;	/* CM	nnnn */
			M_CALL( I.AF.b.l & SF );
			break;
		case 0xfd: i8085_ICount -= 4;	/* ???? */
			illegal();
			break;
		case 0xfe: i8085_ICount -= 7;	/* CPI	nn */
			I.XX.b.l = ARG();
			M_CMP(I.XX.b.l);
			break;
		case 0xff: i8085_ICount -= 11;	/* RST	7 */
			M_RST(7);
			break;
	}
}

static void Interrupt(void)
{

	if( I.HALT )		/* if the CPU was halted */
	{
		I.PC.w.l++; 	/* skip HALT instr */
		I.HALT = 0;
	}
	I.IM &= ~IM_IEN;		/* remove general interrupt enable bit */

	if( I.ISRV == IM_INTR )
	{
		LOG(("Interrupt get INTR vector\n"));
		I.IRQ1 = (I.irq_callback)(0);
	}

	if( I.cputype )
	{
		if( I.ISRV == IM_RST55 )
		{
			LOG(("Interrupt get RST5.5 vector\n"));
			I.IRQ1 = (I.irq_callback)(1);
		}

		if( I.ISRV == IM_RST65	)
		{
			LOG(("Interrupt get RST6.5 vector\n"));
			I.IRQ1 = (I.irq_callback)(2);
		}

		if( I.ISRV == IM_RST75 )
		{
			LOG(("Interrupt get RST7.5 vector\n"));
			I.IRQ1 = (I.irq_callback)(3);
		}
	}

	switch( I.IRQ1 & 0xff0000 )
	{
		case 0xcd0000:	/* CALL nnnn */
			i8085_ICount -= 7;
			M_PUSH(PC);
		case 0xc30000:	/* JMP	nnnn */
			i8085_ICount -= 10;
			I.PC.d = I.IRQ1 & 0xffff;
			change_pc16(I.PC.d);
			break;
		default:
			switch( I.IRQ1 )
			{
				case I8085_TRAP:
				case I8085_RST75:
				case I8085_RST65:
				case I8085_RST55:
					M_PUSH(PC);
					if (I.IRQ1 != I8085_RST75)
						I.PC.d = I.IRQ1;
					else
						I.PC.d = 0x3c;
					change_pc16(I.PC.d);
					break;
				default:
					LOG(("i8085 take int $%02x\n", I.IRQ1));
					execute_one(I.IRQ1 & 0xff);
			}
	}
}

int i8085_execute(int cycles)
{

	i8085_ICount = cycles;
	do
	{
		/* interrupts enabled or TRAP pending ? */
		if ( (I.IM & IM_IEN) || (I.IREQ & IM_TRAP) )
		{
			/* copy scheduled to executed interrupt request */
			I.IRQ1 = I.IRQ2;
			/* reset scheduled interrupt request */
			I.IRQ2 = 0;
			/* interrupt now ? */
			if (I.IRQ1) Interrupt();
		}

		/* here we go... */
		execute_one(ROP());

	} while (i8085_ICount > 0);

	return cycles - i8085_ICount;
}

/****************************************************************************
 * Initialise the various lookup tables used by the emulation code
 ****************************************************************************/
static void init_tables (void)
{
	UINT8 zs;
	int i, p;
	for (i = 0; i < 256; i++)
	{
		zs = 0;
		if (i==0) zs |= ZF;
		if (i&128) zs |= SF;
		p = 0;
		if (i&1) ++p;
		if (i&2) ++p;
		if (i&4) ++p;
		if (i&8) ++p;
		if (i&16) ++p;
		if (i&32) ++p;
		if (i&64) ++p;
		if (i&128) ++p;
		ZS[i] = zs;
		ZSP[i] = zs | ((p&1) ? 0 : VF);
	}
}

/****************************************************************************
 * Reset the 8085 emulation
 ****************************************************************************/
void i8085_reset(void *param)
{
	init_tables();
	memset(&I, 0, sizeof(i8085_Regs));
	I.cputype = 1;
	change_pc16(I.PC.d);
}

/****************************************************************************
 * Shut down the CPU emulation
 ****************************************************************************/
void i8085_exit(void)
{
	/* nothing to do */
}

/****************************************************************************
 * Get the current 8085 context
 ****************************************************************************/
unsigned i8085_get_context(void *dst)
{
	if( dst )
		*(i8085_Regs*)dst = I;
	return sizeof(i8085_Regs);
}

/****************************************************************************
 * Set the current 8085 context
 ****************************************************************************/
void i8085_set_context(void *src)
{
	if( src )
	{
		I = *(i8085_Regs*)src;
		change_pc(I.PC.d);
	}
}

/****************************************************************************
 * Get the current 8085 PC
 ****************************************************************************/
unsigned i8085_get_pc(void)
{
	return I.PC.d;
}

/****************************************************************************
 * Set the current 8085 PC
 ****************************************************************************/
void i8085_set_pc(unsigned val)
{
	I.PC.w.l = val;
	change_pc(I.PC.d);
}

/****************************************************************************
 * Get the current 8085 SP
 ****************************************************************************/
unsigned i8085_get_sp(void)
{
	return I.SP.d;
}

/****************************************************************************
 * Set the current 8085 SP
 ****************************************************************************/
void i8085_set_sp(unsigned val)
{
	I.SP.w.l = val;
}

/****************************************************************************
 * Get a specific register
 ****************************************************************************/
unsigned i8085_get_reg(int regnum)
{
	switch( regnum )
	{
		case I8085_PC: return I.PC.w.l;
		case I8085_SP: return I.SP.w.l;
		case I8085_AF: return I.AF.w.l;
		case I8085_BC: return I.BC.w.l;
		case I8085_DE: return I.DE.w.l;
		case I8085_HL: return I.HL.w.l;
		case I8085_IM: return I.IM;
		case I8085_HALT: return I.HALT;
		case I8085_IREQ: return I.IREQ;
		case I8085_ISRV: return I.ISRV;
		case I8085_VECTOR: return I.INTR;
		case I8085_TRAP_STATE: return I.nmi_state;
		case I8085_INTR_STATE: return I.irq_state[I8085_INTR_LINE];
		case I8085_RST55_STATE: return I.irq_state[I8085_RST55_LINE];
		case I8085_RST65_STATE: return I.irq_state[I8085_RST65_LINE];
		case I8085_RST75_STATE: return I.irq_state[I8085_RST75_LINE];
		case REG_PREVIOUSPC: return 0; /* previous pc not supported */
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = I.SP.w.l + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					return RM( offset ) + ( RM( offset+1 ) << 8 );
			}
	}
	return 0;
}

/****************************************************************************
 * Set a specific register
 ****************************************************************************/
void i8085_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case I8085_PC: I.PC.w.l = val; break;
		case I8085_SP: I.SP.w.l = val; break;
		case I8085_AF: I.AF.w.l = val; break;
		case I8085_BC: I.BC.w.l = val; break;
		case I8085_DE: I.DE.w.l = val; break;
		case I8085_HL: I.HL.w.l = val; break;
		case I8085_IM: I.IM = val; break;
		case I8085_HALT: I.HALT = val; break;
		case I8085_IREQ: I.IREQ = val; break;
		case I8085_ISRV: I.ISRV = val; break;
		case I8085_VECTOR: I.INTR = val; break;
		case I8085_TRAP_STATE: I.nmi_state = val; break;
		case I8085_INTR_STATE: I.irq_state[I8085_INTR_LINE] = val; break;
		case I8085_RST55_STATE: I.irq_state[I8085_RST55_LINE] = val; break;
		case I8085_RST65_STATE: I.irq_state[I8085_RST65_LINE] = val; break;
		case I8085_RST75_STATE: I.irq_state[I8085_RST75_LINE] = val; break;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = I.SP.w.l + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
				{
					WM( offset, val&0xff );
					WM( offset+1, (val>>8)&0xff );
				}
			}
	}
}

/****************************************************************************/
/* Set the 8085 SID input signal state										*/
/****************************************************************************/
void i8085_set_SID(int state)
{
	LOG(("i8085: SID %d\n", state));
	if (state)
		I.IM |= IM_SID;
	else
		I.IM &= ~IM_SID;
}

/****************************************************************************/
/* Set a callback to be called at SOD output change 						*/
/****************************************************************************/
void i8085_set_sod_callback(void (*callback)(int state))
{
	I.sod_callback = callback;
}

/****************************************************************************/
/* Set TRAP signal state													*/
/****************************************************************************/
void i8085_set_TRAP(int state)
{
	LOG(("i8085: TRAP %d\n", state));
	if (state)
	{
		I.IREQ |= IM_TRAP;
		if( I.ISRV & IM_TRAP ) return;	/* already servicing TRAP ? */
		I.ISRV = IM_TRAP;				/* service TRAP */
		I.IRQ2 = ADDR_TRAP;
	}
	else
	{
		I.IREQ &= ~IM_TRAP; 			/* remove request for TRAP */
	}
}

/****************************************************************************/
/* Set RST7.5 signal state													*/
/****************************************************************************/
void i8085_set_RST75(int state)
{
	LOG(("i8085: RST7.5 %d\n", state));
	if( state )
	{

		I.IREQ |= IM_RST75; 			/* request RST7.5 */
		if( I.IM & IM_RST75 ) return;	/* if masked, ignore it for now */
		if( !I.ISRV )					/* if no higher priority IREQ is serviced */
		{
			I.ISRV = IM_RST75;			/* service RST7.5 */
			I.IRQ2 = ADDR_RST75;
		}
	}
	/* RST7.5 is reset only by SIM or end of service routine ! */
}

/****************************************************************************/
/* Set RST6.5 signal state													*/
/****************************************************************************/
void i8085_set_RST65(int state)
{
	LOG(("i8085: RST6.5 %d\n", state));
	if( state )
	{
		I.IREQ |= IM_RST65; 			/* request RST6.5 */
		if( I.IM & IM_RST65 ) return;	/* if masked, ignore it for now */
		if( !I.ISRV )					/* if no higher priority IREQ is serviced */
		{
			I.ISRV = IM_RST65;			/* service RST6.5 */
			I.IRQ2 = ADDR_RST65;
		}
	}
	else
	{
		I.IREQ &= ~IM_RST65;			/* remove request for RST6.5 */
	}
}

/****************************************************************************/
/* Set RST5.5 signal state													*/
/****************************************************************************/
void i8085_set_RST55(int state)
{
	LOG(("i8085: RST5.5 %d\n", state));
	if( state )
	{
		I.IREQ |= IM_RST55; 			/* request RST5.5 */
		if( I.IM & IM_RST55 ) return;	/* if masked, ignore it for now */
		if( !I.ISRV )					/* if no higher priority IREQ is serviced */
		{
			I.ISRV = IM_RST55;			/* service RST5.5 */
			I.IRQ2 = ADDR_RST55;
		}
	}
	else
	{
		I.IREQ &= ~IM_RST55;			/* remove request for RST5.5 */
	}
}

/****************************************************************************/
/* Set INTR signal															*/
/****************************************************************************/
void i8085_set_INTR(int state)
{
	LOG(("i8085: INTR %d\n", state));
	if( state )
	{
		I.IREQ |= IM_INTR;				/* request INTR */
		I.INTR = state;
		if( I.IM & IM_INTR ) return;	/* if masked, ignore it for now */
		if( !I.ISRV )					/* if no higher priority IREQ is serviced */
		{
			I.ISRV = IM_INTR;			/* service INTR */
			I.IRQ2 = I.INTR;
		}
	}
	else
	{
		I.IREQ &= ~IM_INTR; 			/* remove request for INTR */
	}
}

void i8085_set_nmi_line(int state)
{
	I.nmi_state = state;
	if( state != CLEAR_LINE )
		i8085_set_TRAP(1);
}

void i8085_set_irq_line(int irqline, int state)
{
	I.irq_state[irqline] = state;
	if (state == CLEAR_LINE)
	{
		if( !(I.IM & IM_IEN) )
		{
			switch (irqline)
			{
				case I8085_INTR_LINE: i8085_set_INTR(0); break;
				case I8085_RST55_LINE: i8085_set_RST55(0); break;
				case I8085_RST65_LINE: i8085_set_RST65(0); break;
				case I8085_RST75_LINE: i8085_set_RST75(0); break;
			}
		}
	}
	else
	{
		if( I.IM & IM_IEN )
		{
			switch( irqline )
			{
				case I8085_INTR_LINE: i8085_set_INTR(1); break;
				case I8085_RST55_LINE: i8085_set_RST55(1); break;
				case I8085_RST65_LINE: i8085_set_RST65(1); break;
				case I8085_RST75_LINE: i8085_set_RST75(1); break;
			}
		}
	}
}

void i8085_set_irq_callback(int (*callback)(int))
{
	I.irq_callback = callback;
}

void i8085_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT16(file, "i8085", cpu, "AF", &I.AF.w.l, 1);
	state_save_UINT16(file, "i8085", cpu, "BC", &I.BC.w.l, 1);
	state_save_UINT16(file, "i8085", cpu, "DE", &I.DE.w.l, 1);
	state_save_UINT16(file, "i8085", cpu, "HL", &I.HL.w.l, 1);
	state_save_UINT16(file, "i8085", cpu, "SP", &I.SP.w.l, 1);
	state_save_UINT16(file, "i8085", cpu, "PC", &I.PC.w.l, 1);
	state_save_UINT8(file, "i8085", cpu, "HALT", &I.HALT, 1);
	state_save_UINT8(file, "i8085", cpu, "IM", &I.IM, 1);
	state_save_UINT8(file, "i8085", cpu, "IREQ", &I.IREQ, 1);
	state_save_UINT8(file, "i8085", cpu, "ISRV", &I.ISRV, 1);
	state_save_UINT32(file, "i8085", cpu, "INTR", &I.INTR, 1);
	state_save_UINT32(file, "i8085", cpu, "IRQ2", &I.IRQ2, 1);
	state_save_UINT32(file, "i8085", cpu, "IRQ1", &I.IRQ1, 1);
	state_save_INT8(file, "i8085", cpu, "NMI_STATE", &I.nmi_state, 1);
	state_save_INT8(file, "i8085", cpu, "IRQ_STATE", I.irq_state, 4);
}

void i8085_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT16(file, "i8085", cpu, "AF", &I.AF.w.l, 1);
	state_load_UINT16(file, "i8085", cpu, "BC", &I.BC.w.l, 1);
	state_load_UINT16(file, "i8085", cpu, "DE", &I.DE.w.l, 1);
	state_load_UINT16(file, "i8085", cpu, "HL", &I.HL.w.l, 1);
	state_load_UINT16(file, "i8085", cpu, "SP", &I.SP.w.l, 1);
	state_load_UINT16(file, "i8085", cpu, "PC", &I.PC.w.l, 1);
	state_load_UINT8(file, "i8085", cpu, "HALT", &I.HALT, 1);
	state_load_UINT8(file, "i8085", cpu, "IM", &I.IM, 1);
	state_load_UINT8(file, "i8085", cpu, "IREQ", &I.IREQ, 1);
	state_load_UINT8(file, "i8085", cpu, "ISRV", &I.ISRV, 1);
	state_load_UINT32(file, "i8085", cpu, "INTR", &I.INTR, 1);
	state_load_UINT32(file, "i8085", cpu, "IRQ2", &I.IRQ2, 1);
	state_load_UINT32(file, "i8085", cpu, "IRQ1", &I.IRQ1, 1);
	state_load_INT8(file, "i8085", cpu, "NMI_STATE", &I.nmi_state, 1);
	state_load_INT8(file, "i8085", cpu, "IRQ_STATE", I.irq_state, 4);
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i8085_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "8085A";
		case CPU_INFO_FAMILY: return "Intel 8080";
		case CPU_INFO_VERSION: return "1.1";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (c) 1999 Juergen Buchmueller, all rights reserved.";
	}
	return "";
}

unsigned i8085_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}


/**************************************************************************
 * 8080 section
 **************************************************************************/
#if (HAS_8080)
void i8080_reset(void *param)
{
	i8085_reset(param);
	I.cputype = 0;
}
void i8080_exit(void) { i8085_exit(); }
int i8080_execute(int cycles) { return i8085_execute(cycles); }
unsigned i8080_get_context(void *dst) { return i8085_get_context(dst); }
void i8080_set_context(void *src) { i8085_set_context(src); }
unsigned i8080_get_pc(void) { return i8085_get_pc(); }
void i8080_set_pc(unsigned val) { i8085_set_pc(val); }
unsigned i8080_get_sp(void) { return i8085_get_sp(); }
void i8080_set_sp(unsigned val) { i8085_set_sp(val); }
unsigned i8080_get_reg(int regnum) { return i8085_get_reg(regnum); }
void i8080_set_reg(int regnum, unsigned val)  { i8085_set_reg(regnum,val); }
void i8080_set_nmi_line(int state)	{ i8085_set_nmi_line(state); }
void i8080_set_irq_line(int irqline, int state)
{
	I.irq_state[irqline] = state;
	if (state == CLEAR_LINE)
	{
		if (!(I.IM & IM_IEN))
			i8085_set_INTR(0);
	}
	else
	{
		if (I.IM & IM_IEN)
			i8085_set_INTR(1);
	}
}
void i8080_set_irq_callback(int (*callback)(int irqline)) { i8085_set_irq_callback(callback); }

void i8080_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT16(file, "i8080", cpu, "AF", &I.AF.w.l, 1);
	state_save_UINT16(file, "i8080", cpu, "BC", &I.BC.w.l, 1);
	state_save_UINT16(file, "i8080", cpu, "DE", &I.DE.w.l, 1);
	state_save_UINT16(file, "i8080", cpu, "HL", &I.HL.w.l, 1);
	state_save_UINT16(file, "i8080", cpu, "SP", &I.SP.w.l, 1);
	state_save_UINT16(file, "i8080", cpu, "PC", &I.PC.w.l, 1);
	state_save_UINT8(file, "i8080", cpu, "HALT", &I.HALT, 1);
	state_save_UINT8(file, "i8080", cpu, "IREQ", &I.IREQ, 1);
	state_save_UINT8(file, "i8080", cpu, "ISRV", &I.ISRV, 1);
	state_save_UINT32(file, "i8080", cpu, "INTR", &I.INTR, 1);
	state_save_UINT32(file, "i8080", cpu, "IRQ2", &I.IRQ2, 1);
	state_save_UINT32(file, "i8080", cpu, "IRQ1", &I.IRQ1, 1);
	state_save_INT8(file, "i8080", cpu, "NMI_STATE", &I.nmi_state, 1);
	state_save_INT8(file, "i8080", cpu, "IRQ_STATE", I.irq_state, 1);
}

void i8080_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT16(file, "i8080", cpu, "AF", &I.AF.w.l, 1);
	state_load_UINT16(file, "i8080", cpu, "BC", &I.BC.w.l, 1);
	state_load_UINT16(file, "i8080", cpu, "DE", &I.DE.w.l, 1);
	state_load_UINT16(file, "i8080", cpu, "HL", &I.HL.w.l, 1);
	state_load_UINT16(file, "i8080", cpu, "SP", &I.SP.w.l, 1);
	state_load_UINT16(file, "i8080", cpu, "PC", &I.PC.w.l, 1);
	state_load_UINT8(file, "i8080", cpu, "HALT", &I.HALT, 1);
	state_load_UINT8(file, "i8080", cpu, "IREQ", &I.IREQ, 1);
	state_load_UINT8(file, "i8080", cpu, "ISRV", &I.ISRV, 1);
	state_load_UINT32(file, "i8080", cpu, "INTR", &I.INTR, 1);
	state_load_UINT32(file, "i8080", cpu, "IRQ2", &I.IRQ2, 1);
	state_load_UINT32(file, "i8080", cpu, "IRQ1", &I.IRQ1, 1);
	state_load_INT8(file, "i8080", cpu, "NMI_STATE", &I.nmi_state, 1);
	state_load_INT8(file, "i8080", cpu, "IRQ_STATE", I.irq_state, 1);
}

const char *i8080_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "8080";
		case CPU_INFO_VERSION: return "1.2";
	}
	return i8085_info(context,regnum);
}

unsigned i8080_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}
#endif

