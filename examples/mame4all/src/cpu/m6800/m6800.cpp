/*** m6800: Portable 6800 class  emulator *************************************

	m6800.c

	References:

		6809 Simulator V09, By L.C. Benschop, Eidnhoven The Netherlands.

		m6809: Portable 6809 emulator, DS (6809 code in MAME, derived from
			the 6809 Simulator V09)

		6809 Microcomputer Programming & Interfacing with Experiments"
			by Andrew C. Staugaard, Jr.; Howard W. Sams & Co., Inc.

	System dependencies:	UINT16 must be 16 bit unsigned int
							UINT8 must be 8 bit unsigned int
							UINT32 must be more than 16 bits
							arrays up to 65536 bytes must be supported
							machine must be twos complement

History
991031	ZV
	Added NSC-8105 support

990319	HJB
    Fixed wrong LSB/MSB order for push/pull word.
	Subtract .extra_cycles at the beginning/end of the exectuion loops.

990316  HJB
	Renamed to 6800, since that's the basic CPU.
	Added different cycle count tables for M6800/2/8, M6801/3 and HD63701.

990314  HJB
	Also added the M6800 subtype.

990311  HJB
	Added _info functions. Now uses static m6808_Regs struct instead
	of single statics. Changed the 16 bit registers to use the generic
	PAIR union. Registers defined using macros. Split the core into
	four execution loops for M6802, M6803, M6808 and HD63701.
	TST, TSTA and TSTB opcodes reset carry flag.
TODO:
	Verify invalid opcodes for the different CPU types.
	Add proper credits to _info functions.
	Integrate m6808_Flags into the registers (multiple m6808 type CPUs?)

990301	HJB
	Modified the interrupt handling. No more pending interrupt checks.
	WAI opcode saves state, when an interrupt is taken (IRQ or OCI),
	the state is only saved if not already done by WAI.

*****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpuintrf.h"
#include "state.h"
#include "m6800.h"
#include "osdepend.h"

/* 6800 Registers */
typedef struct
{
//	int 	subtype;		/* CPU subtype */
	PAIR	ppc;			/* Previous program counter */
	PAIR	pc; 			/* Program counter */
	PAIR	s;				/* Stack pointer */
	PAIR	x;				/* Index register */
	PAIR	d;				/* Accumulators */
	UINT8	cc; 			/* Condition codes */
	UINT8	wai_state;		/* WAI opcode state ,(or sleep opcode state) */
	UINT8	nmi_state;		/* NMI line state */
	UINT8	irq_state[2];	/* IRQ line state [IRQ1,TIN] */
	UINT8	ic_eddge;		/* InputCapture eddge , b.0=fall,b.1=raise */

	int		(*irq_callback)(int irqline);
	int 	extra_cycles;	/* cycles used for interrupts */
	void	(* const * insn)(void);	/* instruction table */
	const UINT8 *cycles;			/* clock cycle of instruction table */
	/* internal registers */
	UINT8	port1_ddr;
	UINT8	port2_ddr;
	UINT8	port1_data;
	UINT8	port2_data;
	UINT8	tcsr;			/* Timer Control and Status Register */
	UINT8	pending_tcsr;	/* pending IRQ flag for clear IRQflag process */
	UINT8	irq2;			/* IRQ2 flags */
	UINT8	ram_ctrl;
	PAIR	counter;		/* free running counter */
	PAIR	output_compare;	/* output compare       */
	UINT16	input_capture;	/* input capture        */

	PAIR	timer_over;
}   m6800_Regs;

/* 680x registers */
static m6800_Regs m6800;

#define m6801   m6800
#define m6802   m6800
#define m6803	m6800
#define m6808	m6800
#define hd63701 m6800
#define nsc8105 m6800

#define	pPPC	m6800.ppc
#define pPC 	m6800.pc
#define pS		m6800.s
#define pX		m6800.x
#define pD		m6800.d

#define PC		m6800.pc.w.l
#define PCD		m6800.pc.d
#define S		m6800.s.w.l
#define SD		m6800.s.d
#define X		m6800.x.w.l
#define D		m6800.d.w.l
#define A		m6800.d.b.h
#define B		m6800.d.b.l
#define CC		m6800.cc

#define CT		m6800.counter.w.l
#define CTH		m6800.counter.w.h
#define CTD		m6800.counter.d
#define OC		m6800.output_compare.w.l
#define OCH		m6800.output_compare.w.h
#define OCD		m6800.output_compare.d
#define TOH		m6800.timer_over.w.l
#define TOD		m6800.timer_over.d

static PAIR ea; 		/* effective address */
#define EAD ea.d
#define EA	ea.w.l

/* public globals */
int m6800_ICount=50000;

/* point of next timer event */
static UINT32 timer_next;

/* DS -- THESE ARE RE-DEFINED IN m6800.h TO RAM, ROM or FUNCTIONS IN cpuintrf.c */
#define RM				M6800_RDMEM
#define WM				M6800_WRMEM
#define M_RDOP			M6800_RDOP
#define M_RDOP_ARG		M6800_RDOP_ARG

/* macros to access memory */
#define IMMBYTE(b)	b = M_RDOP_ARG(PCD); PC++
#define IMMWORD(w)	w.d = (M_RDOP_ARG(PCD)<<8) | M_RDOP_ARG((PCD+1)&0xffff); PC+=2

#define PUSHBYTE(b) WM(SD,b); --S
#define PUSHWORD(w) WM(SD,w.b.l); --S; WM(SD,w.b.h); --S
#define PULLBYTE(b) S++; b = RM(SD)
#define PULLWORD(w) S++; w.d = RM(SD)<<8; S++; w.d |= RM(SD)

#define MODIFIED_tcsr {	\
	m6800.irq2 = (m6800.tcsr&(m6800.tcsr<<3))&(TCSR_ICF|TCSR_OCF|TCSR_TOF); \
}

#define SET_TIMRE_EVENT {					\
	timer_next = (OCD - CTD < TOD - CTD) ? OCD : TOD;	\
}

/* cleanup high-word of counters */
#define CLEANUP_conters {						\
	OCH -= CTH;									\
	TOH -= CTH;									\
	CTH = 0;									\
	SET_TIMRE_EVENT;							\
}

/* when change freerunningcounter or outputcapture */
#define MODIFIED_counters {						\
	OCH = (OC >= CT) ? CTH : CTH+1;				\
	SET_TIMRE_EVENT;							\
}

/* take interrupt */
#define TAKE_ICI ENTER_INTERRUPT(0xfff6)
#define TAKE_OCI ENTER_INTERRUPT(0xfff4)
#define TAKE_TOI ENTER_INTERRUPT(0xfff2)
#define TAKE_SCI ENTER_INTERRUPT(0xfff0)
#define TAKE_TRAP ENTER_INTERRUPT(0xffee)

/* check IRQ2 (internal irq) */
#define CHECK_IRQ2 {											\
	if(m6800.irq2&(TCSR_ICF|TCSR_OCF|TCSR_TOF))					\
	{															\
		if(m6800.irq2&TCSR_ICF)									\
		{														\
			TAKE_ICI;											\
			if( m6800.irq_callback )							\
				(void)(*m6800.irq_callback)(M6800_TIN_LINE);	\
		}														\
		else if(m6800.irq2&TCSR_OCF)							\
		{														\
			TAKE_OCI;											\
		}														\
		else if(m6800.irq2&TCSR_TOF)							\
		{														\
			TAKE_TOI;											\
		}														\
	}															\
}

/* operate one instruction for */
#define ONE_MORE_INSN() {		\
	UINT8 ireg; 							\
	pPPC = pPC; 							\
	ireg=M_RDOP(PCD);						\
	PC++;									\
	(*m6800.insn[ireg])();					\
	INCREMENT_COUNTER(m6800.cycles[ireg]);	\
}

/* check the IRQ lines for pending interrupts */
#define CHECK_IRQ_LINES() {										\
	if( !(CC & 0x10) )											\
	{															\
		if( m6800.irq_state[M6800_IRQ_LINE] != CLEAR_LINE )		\
		{	/* standard IRQ */									\
			ENTER_INTERRUPT(0xfff8);		\
			if( m6800.irq_callback )							\
				(void)(*m6800.irq_callback)(M6800_IRQ_LINE);	\
		}														\
		else													\
			CHECK_IRQ2;											\
	}															\
}

/* CC masks                       HI NZVC
								7654 3210	*/
#define CLR_HNZVC	CC&=0xd0
#define CLR_NZV 	CC&=0xf1
#define CLR_HNZC	CC&=0xd2
#define CLR_NZVC	CC&=0xf0
#define CLR_Z		CC&=0xfb
#define CLR_NZC 	CC&=0xf2
#define CLR_ZC		CC&=0xfa
#define CLR_C		CC&=0xfe

/* macros for CC -- CC bits affected should be reset before calling */
#define SET_Z(a)		if(!a)SEZ
#define SET_Z8(a)		SET_Z((UINT8)a)
#define SET_Z16(a)		SET_Z((UINT16)a)
#define SET_N8(a)		CC|=((a&0x80)>>4)
#define SET_N16(a)		CC|=((a&0x8000)>>12)
#define SET_H(a,b,r)	CC|=(((a^b^r)&0x10)<<1)
#define SET_C8(a)		CC|=((a&0x100)>>8)
#define SET_C16(a)		CC|=((a&0x10000)>>16)
#define SET_V8(a,b,r)	CC|=(((a^b^r^(r>>1))&0x80)>>6)
#define SET_V16(a,b,r)	CC|=(((a^b^r^(r>>1))&0x8000)>>14)

static const UINT8 flags8i[256]=	 /* increment */
{
0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0a,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};
static const UINT8 flags8d[256]= /* decrement */
{
0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};
#define SET_FLAGS8I(a)		{CC|=flags8i[(a)&0xff];}
#define SET_FLAGS8D(a)		{CC|=flags8d[(a)&0xff];}

/* combos */
#define SET_NZ8(a)			{SET_N8(a);SET_Z(a);}
#define SET_NZ16(a)			{SET_N16(a);SET_Z(a);}
#define SET_FLAGS8(a,b,r)	{SET_N8(r);SET_Z8(r);SET_V8(a,b,r);SET_C8(r);}
#define SET_FLAGS16(a,b,r)	{SET_N16(r);SET_Z16(r);SET_V16(a,b,r);SET_C16(r);}

/* for treating an UINT8 as a signed INT16 */
#define SIGNED(b) ((INT16)(b&0x80?b|0xff00:b))

/* Macros for addressing modes */
#define DIRECT IMMBYTE(EAD)
#define IMM8 EA=PC++
#define IMM16 {EA=PC;PC+=2;}
#define EXTENDED IMMWORD(ea)
#define INDEXED {EA=X+(UINT8)M_RDOP_ARG(PCD);PC++;}

/* macros to set status flags */
#define SEC CC|=0x01
#define CLC CC&=0xfe
#define SEZ CC|=0x04
#define CLZ CC&=0xfb
#define SEN CC|=0x08
#define CLN CC&=0xf7
#define SEV CC|=0x02
#define CLV CC&=0xfd
#define SEH CC|=0x20
#define CLH CC&=0xdf
#define SEI CC|=0x10
#define CLI CC&=~0x10

/* mnemonicos for the Timer Control and Status Register bits */
#define TCSR_OLVL 0x01
#define TCSR_IEDG 0x02
#define TCSR_ETOI 0x04
#define TCSR_EOCI 0x08
#define TCSR_EICI 0x10
#define TCSR_TOF  0x20
#define TCSR_OCF  0x40
#define TCSR_ICF  0x80

#define INCREMENT_COUNTER(amount)	\
{									\
	m6800_ICount -= amount;			\
	CTD += amount;					\
	if( CTD >= timer_next)			\
		check_timer_event();		\
}

#define EAT_CYCLES													\
{																	\
	int cycles_to_eat;												\
																	\
	cycles_to_eat = timer_next - CTD;								\
	if( cycles_to_eat > m6800_ICount) cycles_to_eat = m6800_ICount;	\
	if (cycles_to_eat > 0)											\
	{																\
		INCREMENT_COUNTER(cycles_to_eat);							\
	}																\
}

/* macros for convenience */
#define DIRBYTE(b) {DIRECT;b=RM(EAD);}
#define DIRWORD(w) {DIRECT;w.d=RM16(EAD);}
#define EXTBYTE(b) {EXTENDED;b=RM(EAD);}
#define EXTWORD(w) {EXTENDED;w.d=RM16(EAD);}

#define IDXBYTE(b) {INDEXED;b=RM(EAD);}
#define IDXWORD(w) {INDEXED;w.d=RM16(EAD);}

/* Macros for branch instructions */
#define CHANGE_PC() change_pc16(PCD)
#define BRANCH(f) {IMMBYTE(t);if(f){PC+=SIGNED(t);CHANGE_PC();}}
#define NXORV  ((CC&0x08)^((CC&0x02)<<2))

static const UINT8 cycles_6800[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/  0, 2, 0, 0, 0, 0, 2, 2, 4, 4, 2, 2, 2, 2, 2, 2,
	/*1*/  2, 2, 0, 0, 0, 0, 2, 2, 0, 2, 0, 2, 0, 0, 0, 0,
	/*2*/  4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	/*3*/  4, 4, 4, 4, 4, 4, 4, 4, 0, 5, 0,10, 0, 0, 9,12,
	/*4*/  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2,
	/*5*/  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2,
	/*6*/  7, 0, 0, 7, 7, 0, 7, 7, 7, 7, 7, 0, 7, 7, 4, 7,
	/*7*/  6, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 0, 6, 6, 3, 6,
	/*8*/  2, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 2, 3, 8, 3, 0,
	/*9*/  3, 3, 3, 0, 3, 3, 3, 4, 3, 3, 3, 3, 4, 0, 4, 5,
	/*A*/  5, 5, 5, 0, 5, 5, 5, 6, 5, 5, 5, 5, 6, 8, 6, 7,
	/*B*/  4, 4, 4, 0, 4, 4, 4, 5, 4, 4, 4, 4, 5, 9, 5, 6,
	/*C*/  2, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 2, 0, 0, 3, 0,
	/*D*/  3, 3, 3, 0, 3, 3, 3, 4, 3, 3, 3, 3, 0, 0, 4, 5,
	/*E*/  5, 5, 5, 0, 5, 5, 5, 6, 5, 5, 5, 5, 0, 0, 6, 7,
	/*F*/  4, 4, 4, 0, 4, 4, 4, 5, 4, 4, 4, 4, 0, 0, 5, 6
};

static const UINT8 cycles_6803[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/  0, 2, 0, 0, 3, 3, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2,
	/*1*/  2, 2, 0, 0, 0, 0, 2, 2, 0, 2, 0, 2, 0, 0, 0, 0,
	/*2*/  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/*3*/  3, 3, 4, 4, 3, 3, 3, 3, 5, 5, 3,10, 4,10, 9,12,
	/*4*/  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2,
	/*5*/  2, 0, 0, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2,
	/*6*/  6, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 0, 6, 6, 3, 6,
	/*7*/  6, 0, 0, 6, 6, 0, 6, 6, 6, 6, 6, 0, 6, 6, 3, 6,
	/*8*/  2, 2, 2, 4, 2, 2, 2, 0, 2, 2, 2, 2, 4, 6, 3, 0,
	/*9*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4,
	/*A*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5,
	/*B*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5,
	/*C*/  2, 2, 2, 4, 2, 2, 2, 0, 2, 2, 2, 2, 3, 0, 3, 0,
	/*D*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
	/*E*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*F*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5
};

static const UINT8 cycles_63701[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/  0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/*1*/  1, 1, 0, 0, 0, 0, 1, 1, 2, 2, 4, 1, 0, 0, 0, 0,
	/*2*/  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/*3*/  1, 1, 3, 3, 1, 1, 4, 4, 4, 5, 1,10, 5, 7, 9,12,
	/*4*/  1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1,
	/*5*/  1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1,
	/*6*/  6, 7, 7, 6, 6, 7, 6, 6, 6, 6, 6, 5, 6, 4, 3, 5,
	/*7*/  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 4, 3, 5,
	/*8*/  2, 2, 2, 3, 2, 2, 2, 0, 2, 2, 2, 2, 3, 5, 3, 0,
	/*9*/  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4,
	/*A*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*B*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5,
	/*C*/  2, 2, 2, 3, 2, 2, 2, 0, 2, 2, 2, 2, 3, 0, 3, 0,
	/*D*/  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
	/*E*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	/*F*/  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5
};

static const UINT8 cycles_nsc8105[] =
{
		/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
	/*0*/  0, 0, 2, 0, 0, 2, 0, 2, 4, 2, 4, 2, 2, 2, 2, 2,
	/*1*/  2, 0, 2, 0, 0, 2, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0,
	/*2*/  4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	/*3*/  4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 5,10, 0, 9, 0,12,
	/*4*/  2, 0, 0, 2, 2, 2, 0, 2, 2, 2, 2, 0, 2, 0, 2, 2,
	/*5*/  2, 0, 0, 2, 2, 2, 0, 2, 2, 2, 2, 0, 2, 0, 2, 2,
	/*6*/  7, 0, 0, 7, 7, 7, 0, 7, 7, 7, 7, 0, 7, 4, 7, 7,
	/*7*/  6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6, 0, 6, 3, 6, 6,
	/*8*/  2, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 2, 3, 3, 8, 0,
	/*9*/  3, 3, 3, 0, 3, 3, 3, 4, 3, 3, 3, 3, 4, 4, 0, 5,
	/*A*/  5, 5, 5, 0, 5, 5, 5, 6, 5, 5, 5, 5, 6, 6, 8, 7,
	/*B*/  4, 4, 4, 0, 4, 4, 4, 5, 4, 4, 4, 4, 5, 5, 9, 6,
	/*C*/  2, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 2, 0, 3, 0, 0,
	/*D*/  3, 3, 3, 0, 3, 3, 3, 4, 3, 3, 3, 3, 0, 4, 0, 5,
	/*E*/  5, 5, 5, 0, 5, 5, 5, 6, 5, 5, 5, 5, 0, 6, 0, 7,
	/*F*/  4, 4, 4, 0, 4, 4, 4, 5, 4, 4, 4, 4, 4, 5, 0, 6
};

INLINE UINT32 RM16( UINT32 Addr )
{
	UINT32 result = RM(Addr) << 8;
	return result | RM((Addr+1)&0xffff);
}

INLINE void WM16( UINT32 Addr, PAIR *p )
{
	WM( Addr, p->b.h );
	WM( (Addr+1)&0xffff, p->b.l );
}

/* IRQ enter */
static void ENTER_INTERRUPT(UINT16 irq_vector)
{
	if( m6800.wai_state & (M6800_WAI|M6800_SLP) )
	{
		if( m6800.wai_state & M6800_WAI )
			m6800.extra_cycles += 4;
		m6800.wai_state &= ~(M6800_WAI|M6800_SLP);
	}
	else
	{
		PUSHWORD(pPC);
		PUSHWORD(pX);
		PUSHBYTE(A);
		PUSHBYTE(B);
		PUSHBYTE(CC);
		m6800.extra_cycles += 12;
	}
	SEI;
	PCD = RM16( irq_vector );
	CHANGE_PC();
}

/* check OCI or TOI */
static void check_timer_event(void)
{
	/* OCI */
	if( CTD >= OCD)
	{
		OCH++;	// next IRQ point
		m6800.tcsr |= TCSR_OCF;
		m6800.pending_tcsr |= TCSR_OCF;
		MODIFIED_tcsr;
		if ( !(CC & 0x10) && (m6800.tcsr & TCSR_EOCI))
			TAKE_OCI;
	}
	/* TOI */
	if( CTD >= TOD)
	{
		TOH++;	// next IRQ point
#if 0
		CLEANUP_conters;
#endif
		m6800.tcsr |= TCSR_TOF;
		m6800.pending_tcsr |= TCSR_TOF;
		MODIFIED_tcsr;
		if ( !(CC & 0x10) && (m6800.tcsr & TCSR_ETOI))
			TAKE_TOI;
	}
	/* set next event */
	SET_TIMRE_EVENT;
}

/* include the opcode prototypes and function pointer tables */
#include "6800tbl.cpp"

/* include the opcode functions */
#include "6800ops.cpp"

/****************************************************************************
 * Reset registers to their initial values
 ****************************************************************************/
void m6800_reset(void *param)
{
	SEI;				/* IRQ disabled */
	PCD = RM16( 0xfffe );
	CHANGE_PC();

	/* HJB 990417 set CPU subtype (other reset functions override this) */
//	m6800.subtype   = SUBTYPE_M6800;
	m6800.insn = m6800_insn;
	m6800.cycles = cycles_6800;

	m6800.wai_state = 0;
	m6800.nmi_state = 0;
	m6800.irq_state[M6800_IRQ_LINE] = 0;
	m6800.irq_state[M6800_TIN_LINE] = 0;
	m6800.ic_eddge = 0;

	m6800.port1_ddr = 0x00;
	m6800.port2_ddr = 0x00;
	/* TODO: on reset port 2 should be read to determine the operating mode (bits 0-2) */
	m6800.tcsr = 0x00;
	m6800.pending_tcsr = 0x00;
	m6800.irq2 = 0;
	CTD = 0x0000;
	OCD = 0xffff;
	TOD = 0xffff;
	m6800.ram_ctrl |= 0x40;
}

/****************************************************************************
 * Shut down CPU emulatio
 ****************************************************************************/
void m6800_exit(void)
{
	/* nothing to do */
}

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
unsigned m6800_get_context(void *dst)
{
	if( dst )
		*(m6800_Regs*)dst = m6800;
	return sizeof(m6800_Regs);
}


/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void m6800_set_context(void *src)
{
	if( src )
		m6800 = *(m6800_Regs*)src;
	CHANGE_PC();
	CHECK_IRQ_LINES(); /* HJB 990417 */
}


/****************************************************************************
 * Return program counter
 ****************************************************************************/
unsigned m6800_get_pc(void)
{
	return PC;
}


/****************************************************************************
 * Set program counter
 ****************************************************************************/
void m6800_set_pc(unsigned val)
{
	PC = val;
	CHANGE_PC();
}


/****************************************************************************
 * Return stack pointer
 ****************************************************************************/
unsigned m6800_get_sp(void)
{
	return S;
}


/****************************************************************************
 * Set stack pointer
 ****************************************************************************/
void m6800_set_sp(unsigned val)
{
	S = val;
}


/****************************************************************************
 * Return a specific register
 ****************************************************************************/
unsigned m6800_get_reg(int regnum)
{
	switch( regnum )
	{
		case M6800_PC: return m6800.pc.w.l;
		case M6800_S: return m6800.s.w.l;
		case M6800_CC: return m6800.cc;
		case M6800_A: return m6800.d.b.h;
		case M6800_B: return m6800.d.b.l;
		case M6800_X: return m6800.x.w.l;
		case M6800_NMI_STATE: return m6800.nmi_state;
		case M6800_IRQ_STATE: return m6800.irq_state[M6800_IRQ_LINE];
		case REG_PREVIOUSPC: return m6800.ppc.w.l;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = S + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					return ( RM( offset ) << 8 ) | RM( offset+1 );
			}
	}
	return 0;
}


/****************************************************************************
 * Set a specific register
 ****************************************************************************/
void m6800_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case M6800_PC: m6800.pc.w.l = val; break;
		case M6800_S: m6800.s.w.l = val; break;
		case M6800_CC: m6800.cc = val; break;
		case M6800_A: m6800.d.b.h = val; break;
		case M6800_B: m6800.d.b.l = val; break;
		case M6800_X: m6800.x.w.l = val; break;
		case M6800_NMI_STATE: m6800_set_nmi_line(val); break;
		case M6800_IRQ_STATE: m6800_set_irq_line(M6800_IRQ_LINE,val); break;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = S + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
				{
					WM( offset, (val >> 8) & 0xff );
					WM( offset+1, val & 0xff );
				}
			}
	}
}


void m6800_set_nmi_line(int state)
{
	if (m6800.nmi_state == state) return;
	m6800.nmi_state = state;
	if (state == CLEAR_LINE) return;

	/* NMI */
	ENTER_INTERRUPT(0xfffc);
}

void m6800_set_irq_line(int irqline, int state)
{
	int eddge;

	if (m6800.irq_state[irqline] == state) return;
	m6800.irq_state[irqline] = state;

	switch(irqline)
	{
	case M6800_IRQ_LINE:
		if (state == CLEAR_LINE) return;
		break;
	case M6800_TIN_LINE:
		eddge = (state == CLEAR_LINE ) ? 2 : 0;
		if( ((m6800.tcsr&TCSR_IEDG) ^ (state==CLEAR_LINE ? TCSR_IEDG : 0))==0 )
			return;
		/* active edge in */
		m6800.tcsr |= TCSR_ICF;
		m6800.pending_tcsr |= TCSR_ICF;
		m6800.input_capture = CT;
		MODIFIED_tcsr;
		if( !(CC & 0x10) )
			CHECK_IRQ2
		break;
	default:
		return;
	}
	CHECK_IRQ_LINES(); /* HJB 990417 */
}

void m6800_set_irq_callback(int (*callback)(int irqline))
{
	m6800.irq_callback = callback;
}

static void state_save(void *file, const char *module)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT8(file,module,cpu,"A", &m6800.d.b.h, 1);
	state_save_UINT8(file,module,cpu,"B", &m6800.d.b.l, 1);
	state_save_UINT16(file,module,cpu,"PC", &m6800.pc.w.l, 1);
	state_save_UINT16(file,module,cpu,"S", &m6800.s.w.l, 1);
	state_save_UINT16(file,module,cpu,"X", &m6800.x.w.l, 1);
	state_save_UINT8(file,module,cpu,"CC", &m6800.cc, 1);
	state_save_UINT8(file,module,cpu,"NMI_STATE", &m6800.nmi_state, 1);
	state_save_UINT8(file,module,cpu,"IRQ_STATE", &m6800.irq_state[M6800_IRQ_LINE], 1);
	state_save_UINT8(file,module,cpu,"TIN_STATE", &m6800.irq_state[M6800_TIN_LINE], 1);
}

static void state_load(void *file, const char *module)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT8(file,module,cpu,"A", &m6800.d.b.h, 1);
	state_load_UINT8(file,module,cpu,"B", &m6800.d.b.l, 1);
	state_load_UINT16(file,module,cpu,"PC", &m6800.pc.w.l, 1);
	state_load_UINT16(file,module,cpu,"S", &m6800.s.w.l, 1);
	state_load_UINT16(file,module,cpu,"X", &m6800.x.w.l, 1);
	state_load_UINT8(file,module,cpu,"CC", &m6800.cc, 1);
	state_load_UINT8(file,module,cpu,"NMI_STATE", &m6800.nmi_state, 1);
	state_load_UINT8(file,module,cpu,"IRQ_STATE", &m6800.irq_state[M6800_IRQ_LINE], 1);
	state_load_UINT8(file,module,cpu,"TIN_STATE", &m6800.irq_state[M6800_TIN_LINE], 1);
}

void m6800_state_save(void *file) { state_save(file,"m6800"); }
void m6800_state_load(void *file) { state_load(file,"m6800"); }

/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int m6800_execute(int cycles)
{
	UINT8 ireg;
	m6800_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(m6800.extra_cycles);
	m6800.extra_cycles = 0;

	do
	{
		if( m6800.wai_state & M6800_WAI )
		{
			EAT_CYCLES;
		}
		else
		{
			pPPC = pPC;
			ireg=M_RDOP(PCD);
			PC++;

			switch( ireg )
			{
				case 0x00: illegal(); break;
				case 0x01: nop(); break;
				case 0x02: illegal(); break;
				case 0x03: illegal(); break;
				case 0x04: illegal(); break;
				case 0x05: illegal(); break;
				case 0x06: tap(); break;
				case 0x07: tpa(); break;
				case 0x08: inx(); break;
				case 0x09: dex(); break;
				case 0x0A: CLV; break;
				case 0x0B: SEV; break;
				case 0x0C: CLC; break;
				case 0x0D: SEC; break;
				case 0x0E: cli(); break;
				case 0x0F: sei(); break;
				case 0x10: sba(); break;
				case 0x11: cba(); break;
				case 0x12: illegal(); break;
				case 0x13: illegal(); break;
				case 0x14: illegal(); break;
				case 0x15: illegal(); break;
				case 0x16: tab(); break;
				case 0x17: tba(); break;
				case 0x18: illegal(); break;
				case 0x19: daa(); break;
				case 0x1a: illegal(); break;
				case 0x1b: aba(); break;
				case 0x1c: illegal(); break;
				case 0x1d: illegal(); break;
				case 0x1e: illegal(); break;
				case 0x1f: illegal(); break;
				case 0x20: bra(); break;
				case 0x21: brn(); break;
				case 0x22: bhi(); break;
				case 0x23: bls(); break;
				case 0x24: bcc(); break;
				case 0x25: bcs(); break;
				case 0x26: bne(); break;
				case 0x27: beq(); break;
				case 0x28: bvc(); break;
				case 0x29: bvs(); break;
				case 0x2a: bpl(); break;
				case 0x2b: bmi(); break;
				case 0x2c: bge(); break;
				case 0x2d: blt(); break;
				case 0x2e: bgt(); break;
				case 0x2f: ble(); break;
				case 0x30: tsx(); break;
				case 0x31: ins(); break;
				case 0x32: pula(); break;
				case 0x33: pulb(); break;
				case 0x34: des(); break;
				case 0x35: txs(); break;
				case 0x36: psha(); break;
				case 0x37: pshb(); break;
				case 0x38: illegal(); break;
				case 0x39: rts(); break;
				case 0x3a: illegal(); break;
				case 0x3b: rti(); break;
				case 0x3c: illegal(); break;
				case 0x3d: illegal(); break;
				case 0x3e: wai(); break;
				case 0x3f: swi(); break;
				case 0x40: nega(); break;
				case 0x41: illegal(); break;
				case 0x42: illegal(); break;
				case 0x43: coma(); break;
				case 0x44: lsra(); break;
				case 0x45: illegal(); break;
				case 0x46: rora(); break;
				case 0x47: asra(); break;
				case 0x48: asla(); break;
				case 0x49: rola(); break;
				case 0x4a: deca(); break;
				case 0x4b: illegal(); break;
				case 0x4c: inca(); break;
				case 0x4d: tsta(); break;
				case 0x4e: illegal(); break;
				case 0x4f: clra(); break;
				case 0x50: negb(); break;
				case 0x51: illegal(); break;
				case 0x52: illegal(); break;
				case 0x53: comb(); break;
				case 0x54: lsrb(); break;
				case 0x55: illegal(); break;
				case 0x56: rorb(); break;
				case 0x57: asrb(); break;
				case 0x58: aslb(); break;
				case 0x59: rolb(); break;
				case 0x5a: decb(); break;
				case 0x5b: illegal(); break;
				case 0x5c: incb(); break;
				case 0x5d: tstb(); break;
				case 0x5e: illegal(); break;
				case 0x5f: clrb(); break;
				case 0x60: neg_ix(); break;
				case 0x61: illegal(); break;
				case 0x62: illegal(); break;
				case 0x63: com_ix(); break;
				case 0x64: lsr_ix(); break;
				case 0x65: illegal(); break;
				case 0x66: ror_ix(); break;
				case 0x67: asr_ix(); break;
				case 0x68: asl_ix(); break;
				case 0x69: rol_ix(); break;
				case 0x6a: dec_ix(); break;
				case 0x6b: illegal(); break;
				case 0x6c: inc_ix(); break;
				case 0x6d: tst_ix(); break;
				case 0x6e: jmp_ix(); break;
				case 0x6f: clr_ix(); break;
				case 0x70: neg_ex(); break;
				case 0x71: illegal(); break;
				case 0x72: illegal(); break;
				case 0x73: com_ex(); break;
				case 0x74: lsr_ex(); break;
				case 0x75: illegal(); break;
				case 0x76: ror_ex(); break;
				case 0x77: asr_ex(); break;
				case 0x78: asl_ex(); break;
				case 0x79: rol_ex(); break;
				case 0x7a: dec_ex(); break;
				case 0x7b: illegal(); break;
				case 0x7c: inc_ex(); break;
				case 0x7d: tst_ex(); break;
				case 0x7e: jmp_ex(); break;
				case 0x7f: clr_ex(); break;
				case 0x80: suba_im(); break;
				case 0x81: cmpa_im(); break;
				case 0x82: sbca_im(); break;
				case 0x83: illegal(); break;
				case 0x84: anda_im(); break;
				case 0x85: bita_im(); break;
				case 0x86: lda_im(); break;
				case 0x87: sta_im(); break;
				case 0x88: eora_im(); break;
				case 0x89: adca_im(); break;
				case 0x8a: ora_im(); break;
				case 0x8b: adda_im(); break;
				case 0x8c: cmpx_im(); break;
				case 0x8d: bsr(); break;
				case 0x8e: lds_im(); break;
				case 0x8f: sts_im(); /* orthogonality */ break;
				case 0x90: suba_di(); break;
				case 0x91: cmpa_di(); break;
				case 0x92: sbca_di(); break;
				case 0x93: illegal(); break;
				case 0x94: anda_di(); break;
				case 0x95: bita_di(); break;
				case 0x96: lda_di(); break;
				case 0x97: sta_di(); break;
				case 0x98: eora_di(); break;
				case 0x99: adca_di(); break;
				case 0x9a: ora_di(); break;
				case 0x9b: adda_di(); break;
				case 0x9c: cmpx_di(); break;
				case 0x9d: jsr_di(); break;
				case 0x9e: lds_di(); break;
				case 0x9f: sts_di(); break;
				case 0xa0: suba_ix(); break;
				case 0xa1: cmpa_ix(); break;
				case 0xa2: sbca_ix(); break;
				case 0xa3: illegal(); break;
				case 0xa4: anda_ix(); break;
				case 0xa5: bita_ix(); break;
				case 0xa6: lda_ix(); break;
				case 0xa7: sta_ix(); break;
				case 0xa8: eora_ix(); break;
				case 0xa9: adca_ix(); break;
				case 0xaa: ora_ix(); break;
				case 0xab: adda_ix(); break;
				case 0xac: cmpx_ix(); break;
				case 0xad: jsr_ix(); break;
				case 0xae: lds_ix(); break;
				case 0xaf: sts_ix(); break;
				case 0xb0: suba_ex(); break;
				case 0xb1: cmpa_ex(); break;
				case 0xb2: sbca_ex(); break;
				case 0xb3: illegal(); break;
				case 0xb4: anda_ex(); break;
				case 0xb5: bita_ex(); break;
				case 0xb6: lda_ex(); break;
				case 0xb7: sta_ex(); break;
				case 0xb8: eora_ex(); break;
				case 0xb9: adca_ex(); break;
				case 0xba: ora_ex(); break;
				case 0xbb: adda_ex(); break;
				case 0xbc: cmpx_ex(); break;
				case 0xbd: jsr_ex(); break;
				case 0xbe: lds_ex(); break;
				case 0xbf: sts_ex(); break;
				case 0xc0: subb_im(); break;
				case 0xc1: cmpb_im(); break;
				case 0xc2: sbcb_im(); break;
				case 0xc3: illegal(); break;
				case 0xc4: andb_im(); break;
				case 0xc5: bitb_im(); break;
				case 0xc6: ldb_im(); break;
				case 0xc7: stb_im(); break;
				case 0xc8: eorb_im(); break;
				case 0xc9: adcb_im(); break;
				case 0xca: orb_im(); break;
				case 0xcb: addb_im(); break;
				case 0xcc: illegal(); break;
				case 0xcd: illegal(); break;
				case 0xce: ldx_im(); break;
				case 0xcf: stx_im(); break;
				case 0xd0: subb_di(); break;
				case 0xd1: cmpb_di(); break;
				case 0xd2: sbcb_di(); break;
				case 0xd3: illegal(); break;
				case 0xd4: andb_di(); break;
				case 0xd5: bitb_di(); break;
				case 0xd6: ldb_di(); break;
				case 0xd7: stb_di(); break;
				case 0xd8: eorb_di(); break;
				case 0xd9: adcb_di(); break;
				case 0xda: orb_di(); break;
				case 0xdb: addb_di(); break;
				case 0xdc: illegal(); break;
				case 0xdd: illegal(); break;
				case 0xde: ldx_di(); break;
				case 0xdf: stx_di(); break;
				case 0xe0: subb_ix(); break;
				case 0xe1: cmpb_ix(); break;
				case 0xe2: sbcb_ix(); break;
				case 0xe3: illegal(); break;
				case 0xe4: andb_ix(); break;
				case 0xe5: bitb_ix(); break;
				case 0xe6: ldb_ix(); break;
				case 0xe7: stb_ix(); break;
				case 0xe8: eorb_ix(); break;
				case 0xe9: adcb_ix(); break;
				case 0xea: orb_ix(); break;
				case 0xeb: addb_ix(); break;
				case 0xec: illegal(); break;
				case 0xed: illegal(); break;
				case 0xee: ldx_ix(); break;
				case 0xef: stx_ix(); break;
				case 0xf0: subb_ex(); break;
				case 0xf1: cmpb_ex(); break;
				case 0xf2: sbcb_ex(); break;
				case 0xf3: illegal(); break;
				case 0xf4: andb_ex(); break;
				case 0xf5: bitb_ex(); break;
				case 0xf6: ldb_ex(); break;
				case 0xf7: stb_ex(); break;
				case 0xf8: eorb_ex(); break;
				case 0xf9: adcb_ex(); break;
				case 0xfa: orb_ex(); break;
				case 0xfb: addb_ex(); break;
				case 0xfc: addx_ex(); break;
				case 0xfd: illegal(); break;
				case 0xfe: ldx_ex(); break;
				case 0xff: stx_ex(); break;
			}
			INCREMENT_COUNTER(cycles_6800[ireg]);
		}
	} while( m6800_ICount>0 );

	INCREMENT_COUNTER(m6800.extra_cycles);
	m6800.extra_cycles = 0;

	return cycles - m6800_ICount;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *m6800_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6800";
		case CPU_INFO_FAMILY: return "Motorola 6800";
		case CPU_INFO_VERSION: return "1.1";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "The MAME team.";
	}
	return "";
}

unsigned m6800_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

/****************************************************************************
 * M6801 almost (fully?) equal to the M6803
 ****************************************************************************/
#if (HAS_M6801)
void m6801_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_M6801;
	m6800.insn = m6803_insn;
	m6800.cycles = cycles_6803;
}
void m6801_exit(void) { m6800_exit(); }
int  m6801_execute(int cycles) { return m6803_execute(cycles); }
unsigned m6801_get_context(void *dst) { return m6800_get_context(dst); }
void m6801_set_context(void *src) { m6800_set_context(src); }
unsigned m6801_get_pc(void) { return m6800_get_pc(); }
void m6801_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned m6801_get_sp(void) { return m6800_get_sp(); }
void m6801_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned m6801_get_reg(int regnum) { return m6800_get_reg(regnum); }
void m6801_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void m6801_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void m6801_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void m6801_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void m6801_state_save(void *file) { state_save(file,"m6801"); }
void m6801_state_load(void *file) { state_load(file,"m6801"); }
const char *m6801_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6801";
	}
	return m6800_info(context,regnum);
}
unsigned m6801_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}

#endif

/****************************************************************************
 * M6802 almost (fully?) equal to the M6800
 ****************************************************************************/
#if (HAS_M6802)
void m6802_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_M6802;
}
void m6802_exit(void) { m6800_exit(); }
int  m6802_execute(int cycles) { return m6800_execute(cycles); }
unsigned m6802_get_context(void *dst) { return m6800_get_context(dst); }
void m6802_set_context(void *src) { m6800_set_context(src); }
unsigned m6802_get_pc(void) { return m6800_get_pc(); }
void m6802_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned m6802_get_sp(void) { return m6800_get_sp(); }
void m6802_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned m6802_get_reg(int regnum) { return m6800_get_reg(regnum); }
void m6802_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void m6802_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void m6802_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void m6802_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void m6802_state_save(void *file) { state_save(file,"m6802"); }
void m6802_state_load(void *file) { state_load(file,"m6802"); }
const char *m6802_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6802";
	}
	return m6800_info(context,regnum);
}

unsigned m6802_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}

#endif

/****************************************************************************
 * M6803 almost (fully?) equal to the M6801
 ****************************************************************************/
#if (HAS_M6803)
void m6803_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_M6803;
	m6800.insn = m6803_insn;
	m6800.cycles = cycles_6803;
}
void m6803_exit(void) { m6800_exit(); }
#endif
/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
#if (HAS_M6803||HAS_M6801)
int m6803_execute(int cycles)
{
	UINT8 ireg;
	m6803_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(m6803.extra_cycles);
	m6803.extra_cycles = 0;

	if( m6803.wai_state & M6800_WAI )
	{
		EAT_CYCLES;
		goto getout;
	}

	do
	{

		pPPC = pPC;
		ireg=M_RDOP(PCD);
		PC++;

		switch( ireg )
		{
			case 0x00: illegal(); break;
			case 0x01: nop(); break;
			case 0x02: illegal(); break;
			case 0x03: illegal(); break;
			case 0x04: lsrd(); /* 6803 only */; break;
			case 0x05: asld(); /* 6803 only */; break;
			case 0x06: tap(); break;
			case 0x07: tpa(); break;
			case 0x08: inx(); break;
			case 0x09: dex(); break;
			case 0x0A: CLV; break;
			case 0x0B: SEV; break;
			case 0x0C: CLC; break;
			case 0x0D: SEC; break;
			case 0x0E: cli(); break;
			case 0x0F: sei(); break;
			case 0x10: sba(); break;
			case 0x11: cba(); break;
			case 0x12: illegal(); break;
			case 0x13: illegal(); break;
			case 0x14: illegal(); break;
			case 0x15: illegal(); break;
			case 0x16: tab(); break;
			case 0x17: tba(); break;
			case 0x18: illegal(); break;
			case 0x19: daa(); break;
			case 0x1a: illegal(); break;
			case 0x1b: aba(); break;
			case 0x1c: illegal(); break;
			case 0x1d: illegal(); break;
			case 0x1e: illegal(); break;
			case 0x1f: illegal(); break;
			case 0x20: bra(); break;
			case 0x21: brn(); break;
			case 0x22: bhi(); break;
			case 0x23: bls(); break;
			case 0x24: bcc(); break;
			case 0x25: bcs(); break;
			case 0x26: bne(); break;
			case 0x27: beq(); break;
			case 0x28: bvc(); break;
			case 0x29: bvs(); break;
			case 0x2a: bpl(); break;
			case 0x2b: bmi(); break;
			case 0x2c: bge(); break;
			case 0x2d: blt(); break;
			case 0x2e: bgt(); break;
			case 0x2f: ble(); break;
			case 0x30: tsx(); break;
			case 0x31: ins(); break;
			case 0x32: pula(); break;
			case 0x33: pulb(); break;
			case 0x34: des(); break;
			case 0x35: txs(); break;
			case 0x36: psha(); break;
			case 0x37: pshb(); break;
			case 0x38: pulx(); /* 6803 only */ break;
			case 0x39: rts(); break;
			case 0x3a: abx(); /* 6803 only */ break;
			case 0x3b: rti(); break;
			case 0x3c: pshx(); /* 6803 only */ break;
			case 0x3d: mul(); /* 6803 only */ break;
			case 0x3e: wai(); break;
			case 0x3f: swi(); break;
			case 0x40: nega(); break;
			case 0x41: illegal(); break;
			case 0x42: illegal(); break;
			case 0x43: coma(); break;
			case 0x44: lsra(); break;
			case 0x45: illegal(); break;
			case 0x46: rora(); break;
			case 0x47: asra(); break;
			case 0x48: asla(); break;
			case 0x49: rola(); break;
			case 0x4a: deca(); break;
			case 0x4b: illegal(); break;
			case 0x4c: inca(); break;
			case 0x4d: tsta(); break;
			case 0x4e: illegal(); break;
			case 0x4f: clra(); break;
			case 0x50: negb(); break;
			case 0x51: illegal(); break;
			case 0x52: illegal(); break;
			case 0x53: comb(); break;
			case 0x54: lsrb(); break;
			case 0x55: illegal(); break;
			case 0x56: rorb(); break;
			case 0x57: asrb(); break;
			case 0x58: aslb(); break;
			case 0x59: rolb(); break;
			case 0x5a: decb(); break;
			case 0x5b: illegal(); break;
			case 0x5c: incb(); break;
			case 0x5d: tstb(); break;
			case 0x5e: illegal(); break;
			case 0x5f: clrb(); break;
			case 0x60: neg_ix(); break;
			case 0x61: illegal(); break;
			case 0x62: illegal(); break;
			case 0x63: com_ix(); break;
			case 0x64: lsr_ix(); break;
			case 0x65: illegal(); break;
			case 0x66: ror_ix(); break;
			case 0x67: asr_ix(); break;
			case 0x68: asl_ix(); break;
			case 0x69: rol_ix(); break;
			case 0x6a: dec_ix(); break;
			case 0x6b: illegal(); break;
			case 0x6c: inc_ix(); break;
			case 0x6d: tst_ix(); break;
			case 0x6e: jmp_ix(); break;
			case 0x6f: clr_ix(); break;
			case 0x70: neg_ex(); break;
			case 0x71: illegal(); break;
			case 0x72: illegal(); break;
			case 0x73: com_ex(); break;
			case 0x74: lsr_ex(); break;
			case 0x75: illegal(); break;
			case 0x76: ror_ex(); break;
			case 0x77: asr_ex(); break;
			case 0x78: asl_ex(); break;
			case 0x79: rol_ex(); break;
			case 0x7a: dec_ex(); break;
			case 0x7b: illegal(); break;
			case 0x7c: inc_ex(); break;
			case 0x7d: tst_ex(); break;
			case 0x7e: jmp_ex(); break;
			case 0x7f: clr_ex(); break;
			case 0x80: suba_im(); break;
			case 0x81: cmpa_im(); break;
			case 0x82: sbca_im(); break;
			case 0x83: subd_im(); /* 6803 only */ break;
			case 0x84: anda_im(); break;
			case 0x85: bita_im(); break;
			case 0x86: lda_im(); break;
			case 0x87: sta_im(); break;
			case 0x88: eora_im(); break;
			case 0x89: adca_im(); break;
			case 0x8a: ora_im(); break;
			case 0x8b: adda_im(); break;
			case 0x8c: cpx_im(); /* 6803 difference */ break;
			case 0x8d: bsr(); break;
			case 0x8e: lds_im(); break;
			case 0x8f: sts_im(); /* orthogonality */ break;
			case 0x90: suba_di(); break;
			case 0x91: cmpa_di(); break;
			case 0x92: sbca_di(); break;
			case 0x93: subd_di(); /* 6803 only */ break;
			case 0x94: anda_di(); break;
			case 0x95: bita_di(); break;
			case 0x96: lda_di(); break;
			case 0x97: sta_di(); break;
			case 0x98: eora_di(); break;
			case 0x99: adca_di(); break;
			case 0x9a: ora_di(); break;
			case 0x9b: adda_di(); break;
			case 0x9c: cpx_di(); /* 6803 difference */ break;
			case 0x9d: jsr_di(); break;
			case 0x9e: lds_di(); break;
			case 0x9f: sts_di(); break;
			case 0xa0: suba_ix(); break;
			case 0xa1: cmpa_ix(); break;
			case 0xa2: sbca_ix(); break;
			case 0xa3: subd_ix(); /* 6803 only */ break;
			case 0xa4: anda_ix(); break;
			case 0xa5: bita_ix(); break;
			case 0xa6: lda_ix(); break;
			case 0xa7: sta_ix(); break;
			case 0xa8: eora_ix(); break;
			case 0xa9: adca_ix(); break;
			case 0xaa: ora_ix(); break;
			case 0xab: adda_ix(); break;
			case 0xac: cpx_ix(); /* 6803 difference */ break;
			case 0xad: jsr_ix(); break;
			case 0xae: lds_ix(); break;
			case 0xaf: sts_ix(); break;
			case 0xb0: suba_ex(); break;
			case 0xb1: cmpa_ex(); break;
			case 0xb2: sbca_ex(); break;
			case 0xb3: subd_ex(); /* 6803 only */ break;
			case 0xb4: anda_ex(); break;
			case 0xb5: bita_ex(); break;
			case 0xb6: lda_ex(); break;
			case 0xb7: sta_ex(); break;
			case 0xb8: eora_ex(); break;
			case 0xb9: adca_ex(); break;
			case 0xba: ora_ex(); break;
			case 0xbb: adda_ex(); break;
			case 0xbc: cpx_ex(); /* 6803 difference */ break;
			case 0xbd: jsr_ex(); break;
			case 0xbe: lds_ex(); break;
			case 0xbf: sts_ex(); break;
			case 0xc0: subb_im(); break;
			case 0xc1: cmpb_im(); break;
			case 0xc2: sbcb_im(); break;
			case 0xc3: addd_im(); /* 6803 only */ break;
			case 0xc4: andb_im(); break;
			case 0xc5: bitb_im(); break;
			case 0xc6: ldb_im(); break;
			case 0xc7: stb_im(); break;
			case 0xc8: eorb_im(); break;
			case 0xc9: adcb_im(); break;
			case 0xca: orb_im(); break;
			case 0xcb: addb_im(); break;
			case 0xcc: ldd_im(); /* 6803 only */ break;
			case 0xcd: std_im(); /* 6803 only -- orthogonality */ break;
			case 0xce: ldx_im(); break;
			case 0xcf: stx_im(); break;
			case 0xd0: subb_di(); break;
			case 0xd1: cmpb_di(); break;
			case 0xd2: sbcb_di(); break;
			case 0xd3: addd_di(); /* 6803 only */ break;
			case 0xd4: andb_di(); break;
			case 0xd5: bitb_di(); break;
			case 0xd6: ldb_di(); break;
			case 0xd7: stb_di(); break;
			case 0xd8: eorb_di(); break;
			case 0xd9: adcb_di(); break;
			case 0xda: orb_di(); break;
			case 0xdb: addb_di(); break;
			case 0xdc: ldd_di(); /* 6803 only */ break;
			case 0xdd: std_di(); /* 6803 only */ break;
			case 0xde: ldx_di(); break;
			case 0xdf: stx_di(); break;
			case 0xe0: subb_ix(); break;
			case 0xe1: cmpb_ix(); break;
			case 0xe2: sbcb_ix(); break;
			case 0xe3: addd_ix(); /* 6803 only */ break;
			case 0xe4: andb_ix(); break;
			case 0xe5: bitb_ix(); break;
			case 0xe6: ldb_ix(); break;
			case 0xe7: stb_ix(); break;
			case 0xe8: eorb_ix(); break;
			case 0xe9: adcb_ix(); break;
			case 0xea: orb_ix(); break;
			case 0xeb: addb_ix(); break;
			case 0xec: ldd_ix(); /* 6803 only */ break;
			case 0xed: std_ix(); /* 6803 only */ break;
			case 0xee: ldx_ix(); break;
			case 0xef: stx_ix(); break;
			case 0xf0: subb_ex(); break;
			case 0xf1: cmpb_ex(); break;
			case 0xf2: sbcb_ex(); break;
			case 0xf3: addd_ex(); /* 6803 only */ break;
			case 0xf4: andb_ex(); break;
			case 0xf5: bitb_ex(); break;
			case 0xf6: ldb_ex(); break;
			case 0xf7: stb_ex(); break;
			case 0xf8: eorb_ex(); break;
			case 0xf9: adcb_ex(); break;
			case 0xfa: orb_ex(); break;
			case 0xfb: addb_ex(); break;
			case 0xfc: ldd_ex(); /* 6803 only */ break;
			case 0xfd: std_ex(); /* 6803 only */ break;
			case 0xfe: ldx_ex(); break;
			case 0xff: stx_ex(); break;
		}
		INCREMENT_COUNTER(cycles_6803[ireg]);
	} while( m6803_ICount>0 );

getout:
	INCREMENT_COUNTER(m6803.extra_cycles);
	m6803.extra_cycles = 0;

	return cycles - m6803_ICount;
}
#endif

#if (HAS_M6803)
unsigned m6803_get_context(void *dst) { return m6800_get_context(dst); }
void m6803_set_context(void *src) { m6800_set_context(src); }
unsigned m6803_get_pc(void) { return m6800_get_pc(); }
void m6803_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned m6803_get_sp(void) { return m6800_get_sp(); }
void m6803_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned m6803_get_reg(int regnum) { return m6800_get_reg(regnum); }
void m6803_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void m6803_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void m6803_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void m6803_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void m6803_state_save(void *file) { state_save(file,"m6803"); }
void m6803_state_load(void *file) { state_load(file,"m6803"); }
const char *m6803_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6803";
	}
	return m6800_info(context,regnum);
}

unsigned m6803_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}
#endif

/****************************************************************************
 * M6808 almost (fully?) equal to the M6800
 ****************************************************************************/
#if (HAS_M6808)
void m6808_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_M6808;
}
void m6808_exit(void) { m6800_exit(); }
int  m6808_execute(int cycles) { return m6800_execute(cycles); }
unsigned m6808_get_context(void *dst) { return m6800_get_context(dst); }
void m6808_set_context(void *src) { m6800_set_context(src); }
unsigned m6808_get_pc(void) { return m6800_get_pc(); }
void m6808_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned m6808_get_sp(void) { return m6800_get_sp(); }
void m6808_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned m6808_get_reg(int regnum) { return m6800_get_reg(regnum); }
void m6808_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void m6808_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void m6808_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void m6808_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void m6808_state_save(void *file) { state_save(file,"m6808"); }
void m6808_state_load(void *file) { state_load(file,"m6808"); }
const char *m6808_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6808";
	}
	return m6800_info(context,regnum);
}

unsigned m6808_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}
#endif

/****************************************************************************
 * HD63701 similiar to the M6800
 ****************************************************************************/
#if (HAS_HD63701)
void hd63701_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_HD63701;
	m6800.insn = hd63701_insn;
	m6800.cycles = cycles_63701;
}
void hd63701_exit(void) { m6800_exit(); }
/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int hd63701_execute(int cycles)
{
	UINT8 ireg;
	hd63701_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(hd63701.extra_cycles);
	hd63701.extra_cycles = 0;

	if( hd63701.wai_state & (HD63701_WAI|HD63701_SLP) )
	{
		EAT_CYCLES;
		goto getout;
	}

	do
	{
		pPPC = pPC;
		ireg=M_RDOP(PCD);
		PC++;

		switch( ireg )
		{
			case 0x00: trap(); break;
			case 0x01: nop(); break;
			case 0x02: trap(); break;
			case 0x03: trap(); break;
			case 0x04: lsrd(); /* 6803 only */; break;
			case 0x05: asld(); /* 6803 only */; break;
			case 0x06: tap(); break;
			case 0x07: tpa(); break;
			case 0x08: inx(); break;
			case 0x09: dex(); break;
			case 0x0A: CLV; break;
			case 0x0B: SEV; break;
			case 0x0C: CLC; break;
			case 0x0D: SEC; break;
			case 0x0E: cli(); break;
			case 0x0F: sei(); break;
			case 0x10: sba(); break;
			case 0x11: cba(); break;
			case 0x12: undoc1(); break;
			case 0x13: undoc2(); break;
			case 0x14: trap(); break;
			case 0x15: trap(); break;
			case 0x16: tab(); break;
			case 0x17: tba(); break;
			case 0x18: xgdx(); /* HD63701YO only */; break;
			case 0x19: daa(); break;
			case 0x1a: slp(); break;
			case 0x1b: aba(); break;
			case 0x1c: trap(); break;
			case 0x1d: trap(); break;
			case 0x1e: trap(); break;
			case 0x1f: trap(); break;
			case 0x20: bra(); break;
			case 0x21: brn(); break;
			case 0x22: bhi(); break;
			case 0x23: bls(); break;
			case 0x24: bcc(); break;
			case 0x25: bcs(); break;
			case 0x26: bne(); break;
			case 0x27: beq(); break;
			case 0x28: bvc(); break;
			case 0x29: bvs(); break;
			case 0x2a: bpl(); break;
			case 0x2b: bmi(); break;
			case 0x2c: bge(); break;
			case 0x2d: blt(); break;
			case 0x2e: bgt(); break;
			case 0x2f: ble(); break;
			case 0x30: tsx(); break;
			case 0x31: ins(); break;
			case 0x32: pula(); break;
			case 0x33: pulb(); break;
			case 0x34: des(); break;
			case 0x35: txs(); break;
			case 0x36: psha(); break;
			case 0x37: pshb(); break;
			case 0x38: pulx(); /* 6803 only */ break;
			case 0x39: rts(); break;
			case 0x3a: abx(); /* 6803 only */ break;
			case 0x3b: rti(); break;
			case 0x3c: pshx(); /* 6803 only */ break;
			case 0x3d: mul(); /* 6803 only */ break;
			case 0x3e: wai(); break;
			case 0x3f: swi(); break;
			case 0x40: nega(); break;
			case 0x41: trap(); break;
			case 0x42: trap(); break;
			case 0x43: coma(); break;
			case 0x44: lsra(); break;
			case 0x45: trap(); break;
			case 0x46: rora(); break;
			case 0x47: asra(); break;
			case 0x48: asla(); break;
			case 0x49: rola(); break;
			case 0x4a: deca(); break;
			case 0x4b: trap(); break;
			case 0x4c: inca(); break;
			case 0x4d: tsta(); break;
			case 0x4e: trap(); break;
			case 0x4f: clra(); break;
			case 0x50: negb(); break;
			case 0x51: trap(); break;
			case 0x52: trap(); break;
			case 0x53: comb(); break;
			case 0x54: lsrb(); break;
			case 0x55: trap(); break;
			case 0x56: rorb(); break;
			case 0x57: asrb(); break;
			case 0x58: aslb(); break;
			case 0x59: rolb(); break;
			case 0x5a: decb(); break;
			case 0x5b: trap(); break;
			case 0x5c: incb(); break;
			case 0x5d: tstb(); break;
			case 0x5e: trap(); break;
			case 0x5f: clrb(); break;
			case 0x60: neg_ix(); break;
			case 0x61: aim_ix(); /* HD63701YO only */; break;
			case 0x62: oim_ix(); /* HD63701YO only */; break;
			case 0x63: com_ix(); break;
			case 0x64: lsr_ix(); break;
			case 0x65: eim_ix(); /* HD63701YO only */; break;
			case 0x66: ror_ix(); break;
			case 0x67: asr_ix(); break;
			case 0x68: asl_ix(); break;
			case 0x69: rol_ix(); break;
			case 0x6a: dec_ix(); break;
			case 0x6b: tim_ix(); /* HD63701YO only */; break;
			case 0x6c: inc_ix(); break;
			case 0x6d: tst_ix(); break;
			case 0x6e: jmp_ix(); break;
			case 0x6f: clr_ix(); break;
			case 0x70: neg_ex(); break;
			case 0x71: aim_di(); /* HD63701YO only */; break;
			case 0x72: oim_di(); /* HD63701YO only */; break;
			case 0x73: com_ex(); break;
			case 0x74: lsr_ex(); break;
			case 0x75: eim_di(); /* HD63701YO only */; break;
			case 0x76: ror_ex(); break;
			case 0x77: asr_ex(); break;
			case 0x78: asl_ex(); break;
			case 0x79: rol_ex(); break;
			case 0x7a: dec_ex(); break;
			case 0x7b: tim_di(); /* HD63701YO only */; break;
			case 0x7c: inc_ex(); break;
			case 0x7d: tst_ex(); break;
			case 0x7e: jmp_ex(); break;
			case 0x7f: clr_ex(); break;
			case 0x80: suba_im(); break;
			case 0x81: cmpa_im(); break;
			case 0x82: sbca_im(); break;
			case 0x83: subd_im(); /* 6803 only */ break;
			case 0x84: anda_im(); break;
			case 0x85: bita_im(); break;
			case 0x86: lda_im(); break;
			case 0x87: sta_im(); break;
			case 0x88: eora_im(); break;
			case 0x89: adca_im(); break;
			case 0x8a: ora_im(); break;
			case 0x8b: adda_im(); break;
			case 0x8c: cpx_im(); /* 6803 difference */ break;
			case 0x8d: bsr(); break;
			case 0x8e: lds_im(); break;
			case 0x8f: sts_im(); /* orthogonality */ break;
			case 0x90: suba_di(); break;
			case 0x91: cmpa_di(); break;
			case 0x92: sbca_di(); break;
			case 0x93: subd_di(); /* 6803 only */ break;
			case 0x94: anda_di(); break;
			case 0x95: bita_di(); break;
			case 0x96: lda_di(); break;
			case 0x97: sta_di(); break;
			case 0x98: eora_di(); break;
			case 0x99: adca_di(); break;
			case 0x9a: ora_di(); break;
			case 0x9b: adda_di(); break;
			case 0x9c: cpx_di(); /* 6803 difference */ break;
			case 0x9d: jsr_di(); break;
			case 0x9e: lds_di(); break;
			case 0x9f: sts_di(); break;
			case 0xa0: suba_ix(); break;
			case 0xa1: cmpa_ix(); break;
			case 0xa2: sbca_ix(); break;
			case 0xa3: subd_ix(); /* 6803 only */ break;
			case 0xa4: anda_ix(); break;
			case 0xa5: bita_ix(); break;
			case 0xa6: lda_ix(); break;
			case 0xa7: sta_ix(); break;
			case 0xa8: eora_ix(); break;
			case 0xa9: adca_ix(); break;
			case 0xaa: ora_ix(); break;
			case 0xab: adda_ix(); break;
			case 0xac: cpx_ix(); /* 6803 difference */ break;
			case 0xad: jsr_ix(); break;
			case 0xae: lds_ix(); break;
			case 0xaf: sts_ix(); break;
			case 0xb0: suba_ex(); break;
			case 0xb1: cmpa_ex(); break;
			case 0xb2: sbca_ex(); break;
			case 0xb3: subd_ex(); /* 6803 only */ break;
			case 0xb4: anda_ex(); break;
			case 0xb5: bita_ex(); break;
			case 0xb6: lda_ex(); break;
			case 0xb7: sta_ex(); break;
			case 0xb8: eora_ex(); break;
			case 0xb9: adca_ex(); break;
			case 0xba: ora_ex(); break;
			case 0xbb: adda_ex(); break;
			case 0xbc: cpx_ex(); /* 6803 difference */ break;
			case 0xbd: jsr_ex(); break;
			case 0xbe: lds_ex(); break;
			case 0xbf: sts_ex(); break;
			case 0xc0: subb_im(); break;
			case 0xc1: cmpb_im(); break;
			case 0xc2: sbcb_im(); break;
			case 0xc3: addd_im(); /* 6803 only */ break;
			case 0xc4: andb_im(); break;
			case 0xc5: bitb_im(); break;
			case 0xc6: ldb_im(); break;
			case 0xc7: stb_im(); break;
			case 0xc8: eorb_im(); break;
			case 0xc9: adcb_im(); break;
			case 0xca: orb_im(); break;
			case 0xcb: addb_im(); break;
			case 0xcc: ldd_im(); /* 6803 only */ break;
			case 0xcd: std_im(); /* 6803 only -- orthogonality */ break;
			case 0xce: ldx_im(); break;
			case 0xcf: stx_im(); break;
			case 0xd0: subb_di(); break;
			case 0xd1: cmpb_di(); break;
			case 0xd2: sbcb_di(); break;
			case 0xd3: addd_di(); /* 6803 only */ break;
			case 0xd4: andb_di(); break;
			case 0xd5: bitb_di(); break;
			case 0xd6: ldb_di(); break;
			case 0xd7: stb_di(); break;
			case 0xd8: eorb_di(); break;
			case 0xd9: adcb_di(); break;
			case 0xda: orb_di(); break;
			case 0xdb: addb_di(); break;
			case 0xdc: ldd_di(); /* 6803 only */ break;
			case 0xdd: std_di(); /* 6803 only */ break;
			case 0xde: ldx_di(); break;
			case 0xdf: stx_di(); break;
			case 0xe0: subb_ix(); break;
			case 0xe1: cmpb_ix(); break;
			case 0xe2: sbcb_ix(); break;
			case 0xe3: addd_ix(); /* 6803 only */ break;
			case 0xe4: andb_ix(); break;
			case 0xe5: bitb_ix(); break;
			case 0xe6: ldb_ix(); break;
			case 0xe7: stb_ix(); break;
			case 0xe8: eorb_ix(); break;
			case 0xe9: adcb_ix(); break;
			case 0xea: orb_ix(); break;
			case 0xeb: addb_ix(); break;
			case 0xec: ldd_ix(); /* 6803 only */ break;
			case 0xed: std_ix(); /* 6803 only */ break;
			case 0xee: ldx_ix(); break;
			case 0xef: stx_ix(); break;
			case 0xf0: subb_ex(); break;
			case 0xf1: cmpb_ex(); break;
			case 0xf2: sbcb_ex(); break;
			case 0xf3: addd_ex(); /* 6803 only */ break;
			case 0xf4: andb_ex(); break;
			case 0xf5: bitb_ex(); break;
			case 0xf6: ldb_ex(); break;
			case 0xf7: stb_ex(); break;
			case 0xf8: eorb_ex(); break;
			case 0xf9: adcb_ex(); break;
			case 0xfa: orb_ex(); break;
			case 0xfb: addb_ex(); break;
			case 0xfc: ldd_ex(); /* 6803 only */ break;
			case 0xfd: std_ex(); /* 6803 only */ break;
			case 0xfe: ldx_ex(); break;
			case 0xff: stx_ex(); break;
		}
		INCREMENT_COUNTER(cycles_63701[ireg]);
	} while( hd63701_ICount>0 );

getout:
	INCREMENT_COUNTER(hd63701.extra_cycles);
	hd63701.extra_cycles = 0;

	return cycles - hd63701_ICount;
}

unsigned hd63701_get_context(void *dst) { return m6800_get_context(dst); }
void hd63701_set_context(void *src) { m6800_set_context(src); }
unsigned hd63701_get_pc(void) { return m6800_get_pc(); }
void hd63701_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned hd63701_get_sp(void) { return m6800_get_sp(); }
void hd63701_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned hd63701_get_reg(int regnum) { return m6800_get_reg(regnum); }
void hd63701_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void hd63701_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void hd63701_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void hd63701_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void hd63701_state_save(void *file) { state_save(file,"hd63701"); }
void hd63701_state_load(void *file) { state_load(file,"hd63701"); }
const char *hd63701_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "HD63701";
	}
	return m6800_info(context,regnum);
}

/*
	if change_pc() direccted these areas ,Call hd63701_trap_pc().
	'mode' is selected by the sense of p2.0,p2.1,and p2.3 at reset timming.
	mode 0,1,2,4,6 : $0000-$001f
	mode 5         : $0000-$001f,$0200-$efff
	mode 7         : $0000-$001f,$0100-$efff
*/
void hd63701_trap_pc(void)
{
	TAKE_TRAP;
}

READ_HANDLER( hd63701_internal_registers_r )
{
	return m6803_internal_registers_r(offset);
}

WRITE_HANDLER( hd63701_internal_registers_w )
{
	m6803_internal_registers_w(offset,data);
}

unsigned hd63701_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}
#endif

/****************************************************************************
 * NSC-8105 similiar to the M6800, but the opcodes are scrambled and there
 * is at least one new opcode ($fc)
 ****************************************************************************/
#if (HAS_NSC8105)
void nsc8105_reset(void *param)
{
	m6800_reset(param);
//	m6800.subtype = SUBTYPE_NSC8105;
	m6800.insn = nsc8105_insn;
	m6800.cycles = cycles_nsc8105;
}
void nsc8105_exit(void) { m6800_exit(); }
/****************************************************************************
 * Execute cycles CPU cycles. Return number of cycles really executed
 ****************************************************************************/
int nsc8105_execute(int cycles)
{
	UINT8 ireg;
	nsc8105_ICount = cycles;

	CLEANUP_conters;
	INCREMENT_COUNTER(nsc8105.extra_cycles);
	nsc8105.extra_cycles = 0;

	if( nsc8105.wai_state & NSC8105_WAI )
	{
		EAT_CYCLES;
		goto getout;
	}

	do
	{
		pPPC = pPC;
		ireg=M_RDOP(PCD);
		PC++;

		switch( ireg )
		{
			case 0x00: illegal(); break;
			case 0x01: illegal(); break;
			case 0x02: nop(); break;
			case 0x03: illegal(); break;
			case 0x04: illegal(); break;
			case 0x05: tap(); break;
			case 0x06: illegal(); break;
			case 0x07: tpa(); break;
			case 0x08: inx(); break;
			case 0x09: CLV; break;
			case 0x0a: dex(); break;
			case 0x0b: SEV; break;
			case 0x0c: CLC; break;
			case 0x0d: cli(); break;
			case 0x0e: SEC; break;
			case 0x0f: sei(); break;
			case 0x10: sba(); break;
			case 0x11: illegal(); break;
			case 0x12: cba(); break;
			case 0x13: illegal(); break;
			case 0x14: illegal(); break;
			case 0x15: tab(); break;
			case 0x16: illegal(); break;
			case 0x17: tba(); break;
			case 0x18: illegal(); break;
			case 0x19: illegal(); break;
			case 0x1a: daa(); break;
			case 0x1b: aba(); break;
			case 0x1c: illegal(); break;
			case 0x1d: illegal(); break;
			case 0x1e: illegal(); break;
			case 0x1f: illegal(); break;
			case 0x20: bra(); break;
			case 0x21: bhi(); break;
			case 0x22: brn(); break;
			case 0x23: bls(); break;
			case 0x24: bcc(); break;
			case 0x25: bne(); break;
			case 0x26: bcs(); break;
			case 0x27: beq(); break;
			case 0x28: bvc(); break;
			case 0x29: bpl(); break;
			case 0x2a: bvs(); break;
			case 0x2b: bmi(); break;
			case 0x2c: bge(); break;
			case 0x2d: bgt(); break;
			case 0x2e: blt(); break;
			case 0x2f: ble(); break;
			case 0x30: tsx(); break;
			case 0x31: pula(); break;
			case 0x32: ins(); break;
			case 0x33: pulb(); break;
			case 0x34: des(); break;
			case 0x35: psha(); break;
			case 0x36: txs(); break;
			case 0x37: pshb(); break;
			case 0x38: illegal(); break;
			case 0x39: illegal(); break;
			case 0x3a: rts(); break;
			case 0x3b: rti(); break;
			case 0x3c: illegal(); break;
			case 0x3d: wai(); break;
			case 0x3e: illegal(); break;
			case 0x3f: swi(); break;
			case 0x40: suba_im(); break;
			case 0x41: sbca_im(); break;
			case 0x42: cmpa_im(); break;
			case 0x43: illegal(); break;
			case 0x44: anda_im(); break;
			case 0x45: lda_im(); break;
			case 0x46: bita_im(); break;
			case 0x47: sta_im(); break;
			case 0x48: eora_im(); break;
			case 0x49: ora_im(); break;
			case 0x4a: adca_im(); break;
			case 0x4b: adda_im(); break;
			case 0x4c: cmpx_im(); break;
			case 0x4d: lds_im(); break;
			case 0x4e: bsr(); break;
			case 0x4f: sts_im(); /* orthogonality */ break;
			case 0x50: suba_di(); break;
			case 0x51: sbca_di(); break;
			case 0x52: cmpa_di(); break;
			case 0x53: illegal(); break;
			case 0x54: anda_di(); break;
			case 0x55: lda_di(); break;
			case 0x56: bita_di(); break;
			case 0x57: sta_di(); break;
			case 0x58: eora_di(); break;
			case 0x59: ora_di(); break;
			case 0x5a: adca_di(); break;
			case 0x5b: adda_di(); break;
			case 0x5c: cmpx_di(); break;
			case 0x5d: lds_di(); break;
			case 0x5e: jsr_di(); break;
			case 0x5f: sts_di(); break;
			case 0x60: suba_ix(); break;
			case 0x61: sbca_ix(); break;
			case 0x62: cmpa_ix(); break;
			case 0x63: illegal(); break;
			case 0x64: anda_ix(); break;
			case 0x65: lda_ix(); break;
			case 0x66: bita_ix(); break;
			case 0x67: sta_ix(); break;
			case 0x68: eora_ix(); break;
			case 0x69: ora_ix(); break;
			case 0x6a: adca_ix(); break;
			case 0x6b: adda_ix(); break;
			case 0x6c: cmpx_ix(); break;
			case 0x6d: lds_ix(); break;
			case 0x6e: jsr_ix(); break;
			case 0x6f: sts_ix(); break;
			case 0x70: suba_ex(); break;
			case 0x71: sbca_ex(); break;
			case 0x72: cmpa_ex(); break;
			case 0x73: illegal(); break;
			case 0x74: anda_ex(); break;
			case 0x75: lda_ex(); break;
			case 0x76: bita_ex(); break;
			case 0x77: sta_ex(); break;
			case 0x78: eora_ex(); break;
			case 0x79: ora_ex(); break;
			case 0x7a: adca_ex(); break;
			case 0x7b: adda_ex(); break;
			case 0x7c: cmpx_ex(); break;
			case 0x7d: lds_ex(); break;
			case 0x7e: jsr_ex(); break;
			case 0x7f: sts_ex(); break;
			case 0x80: nega(); break;
			case 0x81: illegal(); break;
			case 0x82: illegal(); break;
			case 0x83: coma(); break;
			case 0x84: lsra(); break;
			case 0x85: rora(); break;
			case 0x86: illegal(); break;
			case 0x87: asra(); break;
			case 0x88: asla(); break;
			case 0x89: deca(); break;
			case 0x8a: rola(); break;
			case 0x8b: illegal(); break;
			case 0x8c: inca(); break;
			case 0x8d: illegal(); break;
			case 0x8e: tsta(); break;
			case 0x8f: clra(); break;
			case 0x90: negb(); break;
			case 0x91: illegal(); break;
			case 0x92: illegal(); break;
			case 0x93: comb(); break;
			case 0x94: lsrb(); break;
			case 0x95: rorb(); break;
			case 0x96: illegal(); break;
			case 0x97: asrb(); break;
			case 0x98: aslb(); break;
			case 0x99: decb(); break;
			case 0x9a: rolb(); break;
			case 0x9b: illegal(); break;
			case 0x9c: incb(); break;
			case 0x9d: illegal(); break;
			case 0x9e: tstb(); break;
			case 0x9f: clrb(); break;
			case 0xa0: neg_ix(); break;
			case 0xa1: illegal(); break;
			case 0xa2: illegal(); break;
			case 0xa3: com_ix(); break;
			case 0xa4: lsr_ix(); break;
			case 0xa5: ror_ix(); break;
			case 0xa6: illegal(); break;
			case 0xa7: asr_ix(); break;
			case 0xa8: asl_ix(); break;
			case 0xa9: dec_ix(); break;
			case 0xaa: rol_ix(); break;
			case 0xab: illegal(); break;
			case 0xac: inc_ix(); break;
			case 0xad: jmp_ix(); break;
			case 0xae: tst_ix(); break;
			case 0xaf: clr_ix(); break;
			case 0xb0: neg_ex(); break;
			case 0xb1: illegal(); break;
			case 0xb2: illegal(); break;
			case 0xb3: com_ex(); break;
			case 0xb4: lsr_ex(); break;
			case 0xb5: ror_ex(); break;
			case 0xb6: illegal(); break;
			case 0xb7: asr_ex(); break;
			case 0xb8: asl_ex(); break;
			case 0xb9: dec_ex(); break;
			case 0xba: rol_ex(); break;
			case 0xbb: illegal(); break;
			case 0xbc: inc_ex(); break;
			case 0xbd: jmp_ex(); break;
			case 0xbe: tst_ex(); break;
			case 0xbf: clr_ex(); break;
			case 0xc0: subb_im(); break;
			case 0xc1: sbcb_im(); break;
			case 0xc2: cmpb_im(); break;
			case 0xc3: illegal(); break;
			case 0xc4: andb_im(); break;
			case 0xc5: ldb_im(); break;
			case 0xc6: bitb_im(); break;
			case 0xc7: stb_im(); break;
			case 0xc8: eorb_im(); break;
			case 0xc9: orb_im(); break;
			case 0xca: adcb_im(); break;
			case 0xcb: addb_im(); break;
			case 0xcc: illegal(); break;
			case 0xcd: ldx_im(); break;
			case 0xce: illegal(); break;
			case 0xcf: stx_im(); break;
			case 0xd0: subb_di(); break;
			case 0xd1: sbcb_di(); break;
			case 0xd2: cmpb_di(); break;
			case 0xd3: illegal(); break;
			case 0xd4: andb_di(); break;
			case 0xd5: ldb_di(); break;
			case 0xd6: bitb_di(); break;
			case 0xd7: stb_di(); break;
			case 0xd8: eorb_di(); break;
			case 0xd9: orb_di(); break;
			case 0xda: adcb_di(); break;
			case 0xdb: addb_di(); break;
			case 0xdc: illegal(); break;
			case 0xdd: ldx_di(); break;
			case 0xde: illegal(); break;
			case 0xdf: stx_di(); break;
			case 0xe0: subb_ix(); break;
			case 0xe1: sbcb_ix(); break;
			case 0xe2: cmpb_ix(); break;
			case 0xe3: illegal(); break;
			case 0xe4: andb_ix(); break;
			case 0xe5: ldb_ix(); break;
			case 0xe6: bitb_ix(); break;
			case 0xe7: stb_ix(); break;
			case 0xe8: eorb_ix(); break;
			case 0xe9: orb_ix(); break;
			case 0xea: adcb_ix(); break;
			case 0xeb: addb_ix(); break;
			case 0xec: illegal(); break;
			case 0xed: ldx_ix(); break;
			case 0xee: illegal(); break;
			case 0xef: stx_ix(); break;
			case 0xf0: subb_ex(); break;
			case 0xf1: sbcb_ex(); break;
			case 0xf2: cmpb_ex(); break;
			case 0xf3: illegal(); break;
			case 0xf4: andb_ex(); break;
			case 0xf5: ldb_ex(); break;
			case 0xf6: bitb_ex(); break;
			case 0xf7: stb_ex(); break;
			case 0xf8: eorb_ex(); break;
			case 0xf9: orb_ex(); break;
			case 0xfa: adcb_ex(); break;
			case 0xfb: addb_ex(); break;
			case 0xfc: addx_ex(); break;
			case 0xfd: ldx_ex(); break;
			case 0xfe: illegal(); break;
			case 0xff: stx_ex(); break;
		}
		INCREMENT_COUNTER(cycles_nsc8105[ireg]);
	} while( nsc8105_ICount>0 );

getout:
	INCREMENT_COUNTER(nsc8105.extra_cycles);
	nsc8105.extra_cycles = 0;

	return cycles - nsc8105_ICount;
}

unsigned nsc8105_get_context(void *dst) { return m6800_get_context(dst); }
void nsc8105_set_context(void *src) { m6800_set_context(src); }
unsigned nsc8105_get_pc(void) { return m6800_get_pc(); }
void nsc8105_set_pc(unsigned val) { m6800_set_pc(val); }
unsigned nsc8105_get_sp(void) { return m6800_get_sp(); }
void nsc8105_set_sp(unsigned val) { m6800_set_sp(val); }
unsigned nsc8105_get_reg(int regnum) { return m6800_get_reg(regnum); }
void nsc8105_set_reg(int regnum, unsigned val) { m6800_set_reg(regnum,val); }
void nsc8105_set_nmi_line(int state) { m6800_set_nmi_line(state); }
void nsc8105_set_irq_line(int irqline, int state) { m6800_set_irq_line(irqline,state); }
void nsc8105_set_irq_callback(int (*callback)(int irqline)) { m6800_set_irq_callback(callback); }
void nsc8105_state_save(void *file) { state_save(file,"nsc8105"); }
void nsc8105_state_load(void *file) { state_load(file,"nsc8105"); }
const char *nsc8105_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "NSC8105";
	}
	return m6800_info(context,regnum);
}

unsigned nsc8105_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readmem16(pc) );
	return 1;
}
#endif


#if (HAS_M6803||HAS_HD63701)

READ_HANDLER( m6803_internal_registers_r )
{
	switch (offset)
	{
		case 0x00:
			return m6800.port1_ddr;
		case 0x01:
			return m6800.port2_ddr;
		case 0x02:
			return (cpu_readport(M6803_PORT1) & (m6800.port1_ddr ^ 0xff))
					| (m6800.port1_data & m6800.port1_ddr);
		case 0x03:
			return (cpu_readport(M6803_PORT2) & (m6800.port2_ddr ^ 0xff))
					| (m6800.port2_data & m6800.port2_ddr);
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			return 0;
		case 0x08:
			m6800.pending_tcsr = 0;
			return m6800.tcsr;
		case 0x09:
			if(!(m6800.pending_tcsr&TCSR_TOF))
			{
				m6800.tcsr &= ~TCSR_TOF;
				MODIFIED_tcsr;
			}
			return m6800.counter.b.h;
		case 0x0a:
			return m6800.counter.b.l;
		case 0x0b:
			if(!(m6800.pending_tcsr&TCSR_OCF))
			{
				m6800.tcsr &= ~TCSR_OCF;
				MODIFIED_tcsr;
			}
			return m6800.output_compare.b.h;
		case 0x0c:
			if(!(m6800.pending_tcsr&TCSR_OCF))
			{
				m6800.tcsr &= ~TCSR_OCF;
				MODIFIED_tcsr;
			}
			return m6800.output_compare.b.l;
		case 0x0d:
			if(!(m6800.pending_tcsr&TCSR_ICF))
			{
				m6800.tcsr &= ~TCSR_ICF;
				MODIFIED_tcsr;
			}
			return (m6800.input_capture >> 0) & 0xff;
		case 0x0e:
			return (m6800.input_capture >> 8) & 0xff;
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			return 0;
		case 0x14:
			return m6800.ram_ctrl;
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		default:
			return 0;
	}
}

WRITE_HANDLER( m6803_internal_registers_w )
{
	static int latch09;

	switch (offset)
	{
		case 0x00:
			if (m6800.port1_ddr != data)
			{
				m6800.port1_ddr = data;
				if(m6800.port1_ddr == 0xff)
					cpu_writeport(M6803_PORT1,m6800.port1_data);
				else
					cpu_writeport(M6803_PORT1,(m6800.port1_data & m6800.port1_ddr)
						| (cpu_readport(M6803_PORT1) & (m6800.port1_ddr ^ 0xff)));
			}
			break;
		case 0x01:
			if (m6800.port2_ddr != data)
			{
				m6800.port2_ddr = data;
				if(m6800.port2_ddr == 0xff)
					cpu_writeport(M6803_PORT2,m6800.port2_data);
				else
					cpu_writeport(M6803_PORT2,(m6800.port2_data & m6800.port2_ddr)
						| (cpu_readport(M6803_PORT2) & (m6800.port2_ddr ^ 0xff)));
			}
			break;
		case 0x02:
			m6800.port1_data = data;
			if(m6800.port1_ddr == 0xff)
				cpu_writeport(M6803_PORT1,m6800.port1_data);
			else
				cpu_writeport(M6803_PORT1,(m6800.port1_data & m6800.port1_ddr)
					| (cpu_readport(M6803_PORT1) & (m6800.port1_ddr ^ 0xff)));
			break;
		case 0x03:
			m6800.port2_data = data;
			m6800.port2_ddr = data;
			if(m6800.port2_ddr == 0xff)
				cpu_writeport(M6803_PORT2,m6800.port2_data);
			else
				cpu_writeport(M6803_PORT2,(m6800.port2_data & m6800.port2_ddr)
					| (cpu_readport(M6803_PORT2) & (m6800.port2_ddr ^ 0xff)));
			break;
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			break;
		case 0x08:
			m6800.tcsr = data;
			m6800.pending_tcsr &= m6800.tcsr;
			MODIFIED_tcsr;
			if( !(CC & 0x10) )
				CHECK_IRQ2;
			break;
		case 0x09:
			latch09 = data & 0xff;	/* 6301 only */
			CT  = 0xfff8;
			TOH = CTH;
			MODIFIED_counters;
			break;
		case 0x0a:	/* 6301 only */
			CT = (latch09 << 8) | (data & 0xff);
			TOH = CTH;
			MODIFIED_counters;
			break;
		case 0x0b:
			if( m6800.output_compare.b.h != data)
			{
				m6800.output_compare.b.h = data;
				MODIFIED_counters;
			}
			break;
		case 0x0c:
			if( m6800.output_compare.b.l != data)
			{
				m6800.output_compare.b.l = data;
				MODIFIED_counters;
			}
			break;
		case 0x0d:
		case 0x0e:
			break;
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			break;
		case 0x14:
			m6800.ram_ctrl = data;
			break;
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		default:
			break;
	}
}
#endif

