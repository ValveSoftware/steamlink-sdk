/*** m6809: Portable 6809 emulator ******************************************

	Copyright (C) John Butler 1997

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

	History:
991026 HJB:
	Fixed missing calls to cpu_changepc() for the TFR and EXG ocpodes.
	Replaced m6809_slapstic checks by a macro (CHANGE_PC). ESB still
	needs the tweaks.

991024 HJB:
	Tried to improve speed: Using bit7 of cycles1/2 as flag for multi
	byte opcodes is gone, those opcodes now call fetch_effective_address().
	Got rid of the slow/fast flags for stack (S and U) memory accesses.
	Minor changes to use 32 bit values as arguments to memory functions
    and added defines for that purpose (e.g. X = 16bit XD = 32bit).

990312 HJB:
	Added bugfixes according to Aaron's findings.
	Reset only sets CC_II and CC_IF, DP to zero and PC from reset vector.
990311 HJB:
	Added _info functions. Now uses static m6808_Regs struct instead
	of single statics. Changed the 16 bit registers to use the generic
	PAIR union. Registers defined using macros. Split the core into
	four execution loops for M6802, M6803, M6808 and HD63701.
    TST, TSTA and TSTB opcodes reset carry flag.
	Modified the read/write stack handlers to push LSB first then MSB
	and pull MSB first then LSB.

990228 HJB:
	Changed the interrupt handling again. Now interrupts are taken
	either right at the moment the lines are asserted or whenever
	an interrupt is enabled and the corresponding line is still
	asserted. That way the pending_interrupts checks are not
	needed anymore. However, the CWAI and SYNC flags still need
	some flags, so I changed the name to 'int_state'.
	This core also has the code for the old interrupt system removed.

990225 HJB:
	Cleaned up the code here and there, added some comments.
	Slightly changed the SAR opcodes (similiar to other CPU cores).
	Added symbolic names for the flag bits.
	Changed the way CWAI/Interrupt() handle CPU state saving.
	A new flag M6809_STATE in pending_interrupts is used to determine
	if a state save is needed on interrupt entry or already done by CWAI.
	Added M6809_IRQ_LINE and M6809_FIRQ_LINE defines to m6809.h
	Moved the internal interrupt_pending flags from m6809.h to m6809.c
	Changed CWAI cycles2[0x3c] to be 2 (plus all or at least 19 if
	CWAI actually pushes the entire state).
	Implemented undocumented TFR/EXG for undefined source and mixed 8/16
	bit transfers (they should transfer/exchange the constant $ff).
	Removed unused jmp/jsr _slap functions from 6809ops.c,
	m6809_slapstick check moved into the opcode functions.

*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "cpuintrf.h"
#include "state.h"
#include "m6809.h"

INLINE void fetch_effective_address( void );

/* 6809 Registers */
typedef struct
{
	PAIR	pc; 		/* Program counter */
	PAIR	ppc;		/* Previous program counter */
	PAIR	d;			/* Accumulator a and b */
	PAIR	dp; 		/* Direct Page register (page in MSB) */
	PAIR	u, s;		/* Stack pointers */
	PAIR	x, y;		/* Index registers */
    UINT8   cc;
	UINT8	ireg;		/* First opcode */
	UINT8	irq_state[2];
    int     extra_cycles; /* cycles used up by interrupts */
    int     (*irq_callback)(int irqline);
    UINT8   int_state;  /* SYNC and CWAI flags */
    UINT8   nmi_state;
} m6809_Regs;

/* flag bits in the cc register */
#define CC_C    0x01        /* Carry */
#define CC_V    0x02        /* Overflow */
#define CC_Z    0x04        /* Zero */
#define CC_N    0x08        /* Negative */
#define CC_II   0x10        /* Inhibit IRQ */
#define CC_H    0x20        /* Half (auxiliary) carry */
#define CC_IF   0x40        /* Inhibit FIRQ */
#define CC_E    0x80        /* entire state pushed */

/* 6809 registers */
static m6809_Regs m6809;
int m6809_slapstic = 0;

#define pPPC    m6809.ppc
#define pPC 	m6809.pc
#define pU		m6809.u
#define pS		m6809.s
#define pX		m6809.x
#define pY		m6809.y
#define pD		m6809.d

#define	PPC		m6809.ppc.w.l
#define PC  	m6809.pc.w.l
#define PCD 	m6809.pc.d
#define U		m6809.u.w.l
#define UD		m6809.u.d
#define S		m6809.s.w.l
#define SD		m6809.s.d
#define X		m6809.x.w.l
#define XD		m6809.x.d
#define Y		m6809.y.w.l
#define YD		m6809.y.d
#define D   	m6809.d.w.l
#define A   	m6809.d.b.h
#define B		m6809.d.b.l
#define DP		m6809.dp.b.h
#define DPD 	m6809.dp.d
#define CC  	m6809.cc

static PAIR ea;         /* effective address */
#define EA	ea.w.l
#define EAD ea.d

#define CHANGE_PC change_pc16(PCD)

#define M6809_CWAI		8	/* set when CWAI is waiting for an interrupt */
#define M6809_SYNC		16	/* set when SYNC is waiting for an interrupt */
#define M6809_LDS		32	/* set when LDS occured at least once */

#define CHECK_IRQ_LINES 												\
	if( m6809.irq_state[M6809_IRQ_LINE] != CLEAR_LINE ||				\
		m6809.irq_state[M6809_FIRQ_LINE] != CLEAR_LINE )				\
		m6809.int_state &= ~M6809_SYNC; /* clear SYNC flag */			\
	if( m6809.irq_state[M6809_FIRQ_LINE]!=CLEAR_LINE && !(CC & CC_IF) ) \
	{																	\
		/* fast IRQ */													\
		/* HJB 990225: state already saved by CWAI? */					\
		if( m6809.int_state & M6809_CWAI )								\
		{																\
			m6809.int_state &= ~M6809_CWAI;  /* clear CWAI */			\
			m6809.extra_cycles += 7;		 /* subtract +7 cycles */	\
        }                                                               \
		else															\
		{																\
			CC &= ~CC_E;				/* save 'short' state */        \
			PUSHWORD(pPC);												\
			PUSHBYTE(CC);												\
			m6809.extra_cycles += 10;	/* subtract +10 cycles */		\
		}																\
		CC |= CC_IF | CC_II;			/* inhibit FIRQ and IRQ */		\
		PCD=RM16(0xfff6);												\
		CHANGE_PC;														\
		(void)(*m6809.irq_callback)(M6809_FIRQ_LINE);					\
	}																	\
	else																\
	if( m6809.irq_state[M6809_IRQ_LINE]!=CLEAR_LINE && !(CC & CC_II) )	\
	{																	\
		/* standard IRQ */												\
		/* HJB 990225: state already saved by CWAI? */					\
		if( m6809.int_state & M6809_CWAI )								\
		{																\
			m6809.int_state &= ~M6809_CWAI;  /* clear CWAI flag */		\
			m6809.extra_cycles += 7;		 /* subtract +7 cycles */	\
		}																\
		else															\
		{																\
			CC |= CC_E; 				/* save entire state */ 		\
			PUSHWORD(pPC);												\
			PUSHWORD(pU);												\
			PUSHWORD(pY);												\
			PUSHWORD(pX);												\
			PUSHBYTE(DP);												\
			PUSHBYTE(B);												\
			PUSHBYTE(A);												\
			PUSHBYTE(CC);												\
			m6809.extra_cycles += 19;	 /* subtract +19 cycles */		\
		}																\
		CC |= CC_II;					/* inhibit IRQ */				\
		PCD=RM16(0xfff8);												\
		CHANGE_PC;														\
		(void)(*m6809.irq_callback)(M6809_IRQ_LINE);					\
	}

/* public globals */
int m6809_ICount=50000;

/* these are re-defined in m6809.h TO RAM, ROM or functions in cpuintrf.c */
#define RM(Addr)		M6809_RDMEM(Addr)
#define WM(Addr,Value)	M6809_WRMEM(Addr,Value)
#define ROP(Addr)		M6809_RDOP(Addr)
#define ROP_ARG(Addr)	M6809_RDOP_ARG(Addr)

/* macros to access memory */
#define IMMBYTE(b)	b = ROP_ARG(PCD); PC++
#define IMMWORD(w)	w.d = (ROP_ARG(PCD)<<8) | ROP_ARG((PCD+1)&0xffff); PC+=2

#define PUSHBYTE(b) --S; WM(SD,b)
#define PUSHWORD(w) --S; WM(SD,w.b.l); --S; WM(SD,w.b.h)
#define PULLBYTE(b) b = RM(SD); S++
#define PULLWORD(w) w = RM(SD)<<8; S++; w |= RM(SD); S++

#define PSHUBYTE(b) --U; WM(UD,b);
#define PSHUWORD(w) --U; WM(UD,w.b.l); --U; WM(UD,w.b.h)
#define PULUBYTE(b) b = RM(UD); U++
#define PULUWORD(w) w = RM(UD)<<8; U++; w |= RM(UD); U++

#define CLR_HNZVC   CC&=~(CC_H|CC_N|CC_Z|CC_V|CC_C)
#define CLR_NZV 	CC&=~(CC_N|CC_Z|CC_V)
#define CLR_HNZC	CC&=~(CC_H|CC_N|CC_Z|CC_C)
#define CLR_NZVC	CC&=~(CC_N|CC_Z|CC_V|CC_C)
#define CLR_Z		CC&=~(CC_Z)
#define CLR_NZC 	CC&=~(CC_N|CC_Z|CC_C)
#define CLR_ZC		CC&=~(CC_Z|CC_C)

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

static UINT8 flags8i[256]=	 /* increment */
{
CC_Z,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
CC_N|CC_V,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N
};
static UINT8 flags8d[256]= /* decrement */
{
CC_Z,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,CC_V,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,
CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N,CC_N
};
#define SET_FLAGS8I(a)		{CC|=flags8i[(a)&0xff];}
#define SET_FLAGS8D(a)		{CC|=flags8d[(a)&0xff];}

/* combos */
#define SET_NZ8(a)			{SET_N8(a);SET_Z(a);}
#define SET_NZ16(a)			{SET_N16(a);SET_Z(a);}
#define SET_FLAGS8(a,b,r)	{SET_N8(r);SET_Z8(r);SET_V8(a,b,r);SET_C8(r);}
#define SET_FLAGS16(a,b,r)	{SET_N16(r);SET_Z16(r);SET_V16(a,b,r);SET_C16(r);}

/* for treating an unsigned byte as a signed word */
#define SIGNED(b) ((UINT16)(b&0x80?b|0xff00:b))
//#define SIGNED(b) ((UINT16)((INT8)b))

/* macros for addressing modes (postbytes have their own code) */
#define DIRECT	EAD = DPD; IMMBYTE(ea.b.l)
#define IMM8	EAD = PCD; PC++
#define IMM16	EAD = PCD; PC+=2
#define EXTENDED IMMWORD(ea)

/* macros to set status flags */
#define SEC CC|=CC_C
#define CLC CC&=~CC_C
#define SEZ CC|=CC_Z
#define CLZ CC&=~CC_Z
#define SEN CC|=CC_N
#define CLN CC&=~CC_N
#define SEV CC|=CC_V
#define CLV CC&=~CC_V
#define SEH CC|=CC_H
#define CLH CC&=~CC_H

/* macros for convenience */
#define DIRBYTE(b) {DIRECT;b=RM(EAD);}
#define DIRWORD(w) {DIRECT;w.d=RM16(EAD);}
#define EXTBYTE(b) {EXTENDED;b=RM(EAD);}
#define EXTWORD(w) {EXTENDED;w.d=RM16(EAD);}

/* macros for branch instructions */
#define BRANCH(f) { 					\
	UINT8 t;							\
	IMMBYTE(t); 						\
	if( f ) 							\
	{									\
		PC += SIGNED(t);				\
		CHANGE_PC;						\
	}									\
}

#define LBRANCH(f) {                    \
	PAIR t; 							\
	IMMWORD(t); 						\
	if( f ) 							\
	{									\
		m6809_ICount -= 1;				\
		PC += t.w.l;					\
		CHANGE_PC;						\
	}									\
}

#define NXORV  ((CC&CC_N)^((CC&CC_V)<<2))

/* macros for setting/getting registers in TFR/EXG instructions */

INLINE UINT32 RM16( UINT32 Addr )
{
	return (RM(Addr) << 8) | (RM((Addr+1)&0xffff));
}

INLINE void WM16( UINT32 Addr, PAIR *p )
{
	WM( Addr, p->b.h );
	WM( (Addr+1)&0xffff, p->b.l );
}

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
unsigned m6809_get_context(void *dst)
{
	if( dst )
		*(m6809_Regs*)dst = m6809;
	return sizeof(m6809_Regs);
}

/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void m6809_set_context(void *src)
{
	if( src )
		m6809 = *(m6809_Regs*)src;
	CHANGE_PC;

    CHECK_IRQ_LINES;
}

/****************************************************************************
 * Return program counter
 ****************************************************************************/
unsigned m6809_get_pc(void)
{
	return PC;
}


/****************************************************************************
 * Set program counter
 ****************************************************************************/
void m6809_set_pc(unsigned val)
{
	PC = val;
	CHANGE_PC;
}


/****************************************************************************
 * Return stack pointer
 ****************************************************************************/
unsigned m6809_get_sp(void)
{
	return S;
}


/****************************************************************************
 * Set stack pointer
 ****************************************************************************/
void m6809_set_sp(unsigned val)
{
	S = val;
}


/****************************************************************************/
/* Return a specific register                                               */
/****************************************************************************/
unsigned m6809_get_reg(int regnum)
{
	switch( regnum )
	{
		case M6809_PC: return PC;
		case M6809_S: return S;
		case M6809_CC: return CC;
		case M6809_U: return U;
		case M6809_A: return A;
		case M6809_B: return B;
		case M6809_X: return X;
		case M6809_Y: return Y;
		case M6809_DP: return DP;
		case M6809_NMI_STATE: return m6809.nmi_state;
		case M6809_IRQ_STATE: return m6809.irq_state[M6809_IRQ_LINE];
		case M6809_FIRQ_STATE: return m6809.irq_state[M6809_FIRQ_LINE];
		case REG_PREVIOUSPC: return PPC;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = S + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					return ( RM( offset ) << 8 ) | RM( offset + 1 );
			}
	}
	return 0;
}


/****************************************************************************/
/* Set a specific register                                                  */
/****************************************************************************/
void m6809_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case M6809_PC: PC = val; CHANGE_PC; break;
		case M6809_S: S = val; break;
		case M6809_CC: CC = val; CHECK_IRQ_LINES; break;
		case M6809_U: U = val; break;
		case M6809_A: A = val; break;
		case M6809_B: B = val; break;
		case M6809_X: X = val; break;
		case M6809_Y: Y = val; break;
		case M6809_DP: DP = val; break;
		case M6809_NMI_STATE: m6809.nmi_state = val; break;
		case M6809_IRQ_STATE: m6809.irq_state[M6809_IRQ_LINE] = val; break;
		case M6809_FIRQ_STATE: m6809.irq_state[M6809_FIRQ_LINE] = val; break;
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


/****************************************************************************/
/* Reset registers to their initial values									*/
/****************************************************************************/
void m6809_reset(void *param)
{
	m6809.int_state = 0;
	m6809.nmi_state = CLEAR_LINE;
	m6809.irq_state[0] = CLEAR_LINE;
	m6809.irq_state[1] = CLEAR_LINE;

	DPD = 0;			/* Reset direct page register */

    CC |= CC_II;        /* IRQ disabled */
    CC |= CC_IF;        /* FIRQ disabled */

	PCD = RM16(0xfffe);
	CHANGE_PC;
}

void m6809_exit(void)
{
	/* nothing to do ? */
}

/* Generate interrupts */
/****************************************************************************
 * Set NMI line state
 ****************************************************************************/
void m6809_set_nmi_line(int state)
{
	if (m6809.nmi_state == state) return;
	m6809.nmi_state = state;
	if( state == CLEAR_LINE ) return;

	/* if the stack was not yet initialized */
    if( !(m6809.int_state & M6809_LDS) ) return;

    m6809.int_state &= ~M6809_SYNC;
	/* HJB 990225: state already saved by CWAI? */
	if( m6809.int_state & M6809_CWAI )
	{
		m6809.int_state &= ~M6809_CWAI;
		m6809.extra_cycles += 7;	/* subtract +7 cycles next time */
    }
	else
	{
		CC |= CC_E; 				/* save entire state */
		PUSHWORD(pPC);
		PUSHWORD(pU);
		PUSHWORD(pY);
		PUSHWORD(pX);
		PUSHBYTE(DP);
		PUSHBYTE(B);
		PUSHBYTE(A);
		PUSHBYTE(CC);
		m6809.extra_cycles += 19;	/* subtract +19 cycles next time */
	}
	CC |= CC_IF | CC_II;			/* inhibit FIRQ and IRQ */
	PCD = RM16(0xfffc);
	CHANGE_PC;
}

/****************************************************************************
 * Set IRQ line state
 ****************************************************************************/
void m6809_set_irq_line(int irqline, int state)
{
	m6809.irq_state[irqline] = state;
	if (state == CLEAR_LINE) return;
	CHECK_IRQ_LINES;
}

/****************************************************************************
 * Set IRQ vector callback
 ****************************************************************************/
void m6809_set_irq_callback(int (*callback)(int irqline))
{
	m6809.irq_callback = callback;
}

/****************************************************************************
 * Save CPU state
 ****************************************************************************/
static void state_save(void *file, const char *module)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT16(file, module, cpu, "PC", &PC, 1);
	state_save_UINT16(file, module, cpu, "U", &U, 1);
	state_save_UINT16(file, module, cpu, "S", &S, 1);
	state_save_UINT16(file, module, cpu, "X", &X, 1);
	state_save_UINT16(file, module, cpu, "Y", &Y, 1);
	state_save_UINT8(file, module, cpu, "DP", &DP, 1);
	state_save_UINT8(file, module, cpu, "CC", &CC, 1);
	state_save_UINT8(file, module, cpu, "INT", &m6809.int_state, 1);
	state_save_UINT8(file, module, cpu, "NMI", &m6809.nmi_state, 1);
	state_save_UINT8(file, module, cpu, "IRQ", &m6809.irq_state[0], 1);
	state_save_UINT8(file, module, cpu, "FIRQ", &m6809.irq_state[1], 1);
}

/****************************************************************************
 * Load CPU state
 ****************************************************************************/
static void state_load(void *file, const char *module)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT16(file, module, cpu, "PC", &PC, 1);
	state_load_UINT16(file, module, cpu, "U", &U, 1);
	state_load_UINT16(file, module, cpu, "S", &S, 1);
	state_load_UINT16(file, module, cpu, "X", &X, 1);
	state_load_UINT16(file, module, cpu, "Y", &Y, 1);
	state_load_UINT8(file, module, cpu, "DP", &DP, 1);
	state_load_UINT8(file, module, cpu, "CC", &CC, 1);
	state_load_UINT8(file, module, cpu, "INT", &m6809.int_state, 1);
	state_load_UINT8(file, module, cpu, "NMI", &m6809.nmi_state, 1);
	state_load_UINT8(file, module, cpu, "IRQ", &m6809.irq_state[0], 1);
	state_load_UINT8(file, module, cpu, "FIRQ", &m6809.irq_state[1], 1);
}

void m6809_state_save(void *file) { state_save(file, "m6809"); }
void m6809_state_load(void *file) { state_load(file, "m6809"); }

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *m6809_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6809";
		case CPU_INFO_FAMILY: return "Motorola 6809";
		case CPU_INFO_VERSION: return "1.1";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) John Butler 1997";
	}
	return "";
}

unsigned m6809_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

/* includes the actual opcode implementations */
#include "6809ops.h"

/* execute instructions on this CPU until icount expires */
int m6809_execute(int cycles)	/* NS 970908 */
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

    m6809_ICount = cycles - m6809.extra_cycles;
	m6809.extra_cycles = 0;

	if (m6809.int_state & (M6809_CWAI | M6809_SYNC))
	{
		m6809_ICount = 0;
	}
	else
	{
		do
		{
			pPPC = pPC;

			m6809.ireg = ROP(PCD);
			PC++;

            goto *a_jump_table[m6809.ireg];

			l_0x00: neg_di();   m6809_ICount-= 6; goto end;
			l_0x03: com_di();   m6809_ICount-= 6; goto end;
			l_0x04: lsr_di();   m6809_ICount-= 6; goto end;
			l_0x06: ror_di();   m6809_ICount-= 6; goto end;
			l_0x07: asr_di();   m6809_ICount-= 6; goto end;
			l_0x08: asl_di();   m6809_ICount-= 6; goto end;
			l_0x09: rol_di();   m6809_ICount-= 6; goto end;
			l_0x0a: dec_di();   m6809_ICount-= 6; goto end;
			l_0x0c: inc_di();   m6809_ICount-= 6; goto end;
			l_0x0d: tst_di();   m6809_ICount-= 6; goto end;
			l_0x0e: jmp_di();   m6809_ICount-= 3; goto end;
			l_0x0f: clr_di();   m6809_ICount-= 6; goto end;
			l_0x10: pref10();					 goto end;
			l_0x11: pref11();					 goto end;
			l_0x13: sync();	   m6809_ICount-= 4; goto end;
			l_0x16: lbra();	   m6809_ICount-= 5; goto end;
			l_0x17: lbsr();	   m6809_ICount-= 9; goto end;
			l_0x19: daa();	   m6809_ICount-= 2; goto end;
			l_0x1a: orcc();	   m6809_ICount-= 3; goto end;
			l_0x1c: andcc();    m6809_ICount-= 3; goto end;
			l_0x1d: sex();	   m6809_ICount-= 2; goto end;
			l_0x1e: exg();	   m6809_ICount-= 8; goto end;
			l_0x1f: tfr();	   m6809_ICount-= 6; goto end;
			l_0x20: bra();	   m6809_ICount-= 3; goto end;
			l_0x21: brn();	   m6809_ICount-= 3; goto end;
			l_0x22: bhi();	   m6809_ICount-= 3; goto end;
			l_0x23: bls();	   m6809_ICount-= 3; goto end;
			l_0x24: bcc();	   m6809_ICount-= 3; goto end;
			l_0x25: bcs();	   m6809_ICount-= 3; goto end;
			l_0x26: bne();	   m6809_ICount-= 3; goto end;
			l_0x27: beq();	   m6809_ICount-= 3; goto end;
			l_0x28: bvc();	   m6809_ICount-= 3; goto end;
			l_0x29: bvs();	   m6809_ICount-= 3; goto end;
			l_0x2a: bpl();	   m6809_ICount-= 3; goto end;
			l_0x2b: bmi();	   m6809_ICount-= 3; goto end;
			l_0x2c: bge();	   m6809_ICount-= 3; goto end;
			l_0x2d: blt();	   m6809_ICount-= 3; goto end;
			l_0x2e: bgt();	   m6809_ICount-= 3; goto end;
			l_0x2f: ble();	   m6809_ICount-= 3; goto end;
			l_0x30: leax();	   m6809_ICount-= 4; goto end;
			l_0x31: leay();	   m6809_ICount-= 4; goto end;
			l_0x32: leas();	   m6809_ICount-= 4; goto end;
			l_0x33: leau();	   m6809_ICount-= 4; goto end;
			l_0x34: pshs();	   m6809_ICount-= 5; goto end;
			l_0x35: puls();	   m6809_ICount-= 5; goto end;
			l_0x36: pshu();	   m6809_ICount-= 5; goto end;
			l_0x37: pulu();	   m6809_ICount-= 5; goto end;
			l_0x39: rts();	   m6809_ICount-= 5; goto end;
			l_0x3a: abx();	   m6809_ICount-= 3; goto end;
			l_0x3b: rti();	   m6809_ICount-= 6; goto end;
			l_0x3c: cwai();	   m6809_ICount-=20; goto end;
			l_0x3d: mul();	   m6809_ICount-=11; goto end;
			l_0x3f: swi();	   m6809_ICount-=19; goto end;
			l_0x40: nega();	   m6809_ICount-= 2; goto end;
			l_0x43: coma();	   m6809_ICount-= 2; goto end;
			l_0x44: lsra();	   m6809_ICount-= 2; goto end;
			l_0x46: rora();	   m6809_ICount-= 2; goto end;
			l_0x47: asra();	   m6809_ICount-= 2; goto end;
			l_0x48: asla();	   m6809_ICount-= 2; goto end;
			l_0x49: rola();	   m6809_ICount-= 2; goto end;
			l_0x4a: deca();	   m6809_ICount-= 2; goto end;
			l_0x4c: inca();	   m6809_ICount-= 2; goto end;
			l_0x4d: tsta();	   m6809_ICount-= 2; goto end;
			l_0x4f: clra();	   m6809_ICount-= 2; goto end;
			l_0x50: negb();	   m6809_ICount-= 2; goto end;
			l_0x53: comb();	   m6809_ICount-= 2; goto end;
			l_0x54: lsrb();	   m6809_ICount-= 2; goto end;
			l_0x56: rorb();	   m6809_ICount-= 2; goto end;
			l_0x57: asrb();	   m6809_ICount-= 2; goto end;
			l_0x58: aslb();	   m6809_ICount-= 2; goto end;
			l_0x59: rolb();	   m6809_ICount-= 2; goto end;
			l_0x5a: decb();	   m6809_ICount-= 2; goto end;
			l_0x5c: incb();	   m6809_ICount-= 2; goto end;
			l_0x5d: tstb();	   m6809_ICount-= 2; goto end;
			l_0x5f: clrb();	   m6809_ICount-= 2; goto end;
			l_0x60: neg_ix();   m6809_ICount-= 6; goto end;
			l_0x63: com_ix();   m6809_ICount-= 6; goto end;
			l_0x64: lsr_ix();   m6809_ICount-= 6; goto end;
			l_0x66: ror_ix();   m6809_ICount-= 6; goto end;
			l_0x67: asr_ix();   m6809_ICount-= 6; goto end;
			l_0x68: asl_ix();   m6809_ICount-= 6; goto end;
			l_0x69: rol_ix();   m6809_ICount-= 6; goto end;
			l_0x6a: dec_ix();   m6809_ICount-= 6; goto end;
			l_0x6c: inc_ix();   m6809_ICount-= 6; goto end;
			l_0x6d: tst_ix();   m6809_ICount-= 6; goto end;
			l_0x6e: jmp_ix();   m6809_ICount-= 3; goto end;
			l_0x6f: clr_ix();   m6809_ICount-= 6; goto end;
			l_0x70: neg_ex();   m6809_ICount-= 7; goto end;
			l_0x73: com_ex();   m6809_ICount-= 7; goto end;
			l_0x74: lsr_ex();   m6809_ICount-= 7; goto end;
			l_0x76: ror_ex();   m6809_ICount-= 7; goto end;
			l_0x77: asr_ex();   m6809_ICount-= 7; goto end;
			l_0x78: asl_ex();   m6809_ICount-= 7; goto end;
			l_0x79: rol_ex();   m6809_ICount-= 7; goto end;
			l_0x7a: dec_ex();   m6809_ICount-= 7; goto end;
			l_0x7c: inc_ex();   m6809_ICount-= 7; goto end;
			l_0x7d: tst_ex();   m6809_ICount-= 7; goto end;
			l_0x7e: jmp_ex();   m6809_ICount-= 4; goto end;
			l_0x7f: clr_ex();   m6809_ICount-= 7; goto end;
			l_0x80: suba_im();  m6809_ICount-= 2; goto end;
			l_0x81: cmpa_im();  m6809_ICount-= 2; goto end;
			l_0x82: sbca_im();  m6809_ICount-= 2; goto end;
			l_0x83: subd_im();  m6809_ICount-= 4; goto end;
			l_0x84: anda_im();  m6809_ICount-= 2; goto end;
			l_0x85: bita_im();  m6809_ICount-= 2; goto end;
			l_0x86: lda_im();   m6809_ICount-= 2; goto end;
			l_0x87: sta_im();   m6809_ICount-= 2; goto end;
			l_0x88: eora_im();  m6809_ICount-= 2; goto end;
			l_0x89: adca_im();  m6809_ICount-= 2; goto end;
			l_0x8a: ora_im();   m6809_ICount-= 2; goto end;
			l_0x8b: adda_im();  m6809_ICount-= 2; goto end;
			l_0x8c: cmpx_im();  m6809_ICount-= 4; goto end;
			l_0x8d: bsr();	   m6809_ICount-= 7; goto end;
			l_0x8e: ldx_im();   m6809_ICount-= 3; goto end;
			l_0x8f: stx_im();   m6809_ICount-= 2; goto end;
			l_0x90: suba_di();  m6809_ICount-= 4; goto end;
			l_0x91: cmpa_di();  m6809_ICount-= 4; goto end;
			l_0x92: sbca_di();  m6809_ICount-= 4; goto end;
			l_0x93: subd_di();  m6809_ICount-= 6; goto end;
			l_0x94: anda_di();  m6809_ICount-= 4; goto end;
			l_0x95: bita_di();  m6809_ICount-= 4; goto end;
			l_0x96: lda_di();   m6809_ICount-= 4; goto end;
			l_0x97: sta_di();   m6809_ICount-= 4; goto end;
			l_0x98: eora_di();  m6809_ICount-= 4; goto end;
			l_0x99: adca_di();  m6809_ICount-= 4; goto end;
			l_0x9a: ora_di();   m6809_ICount-= 4; goto end;
			l_0x9b: adda_di();  m6809_ICount-= 4; goto end;
			l_0x9c: cmpx_di();  m6809_ICount-= 6; goto end;
			l_0x9d: jsr_di();   m6809_ICount-= 7; goto end;
			l_0x9e: ldx_di();   m6809_ICount-= 5; goto end;
			l_0x9f: stx_di();   m6809_ICount-= 5; goto end;
			l_0xa0: suba_ix();  m6809_ICount-= 4; goto end;
			l_0xa1: cmpa_ix();  m6809_ICount-= 4; goto end;
			l_0xa2: sbca_ix();  m6809_ICount-= 4; goto end;
			l_0xa3: subd_ix();  m6809_ICount-= 6; goto end;
			l_0xa4: anda_ix();  m6809_ICount-= 4; goto end;
			l_0xa5: bita_ix();  m6809_ICount-= 4; goto end;
			l_0xa6: lda_ix();   m6809_ICount-= 4; goto end;
			l_0xa7: sta_ix();   m6809_ICount-= 4; goto end;
			l_0xa8: eora_ix();  m6809_ICount-= 4; goto end;
			l_0xa9: adca_ix();  m6809_ICount-= 4; goto end;
			l_0xaa: ora_ix();   m6809_ICount-= 4; goto end;
			l_0xab: adda_ix();  m6809_ICount-= 4; goto end;
			l_0xac: cmpx_ix();  m6809_ICount-= 6; goto end;
			l_0xad: jsr_ix();   m6809_ICount-= 7; goto end;
			l_0xae: ldx_ix();   m6809_ICount-= 5; goto end;
			l_0xaf: stx_ix();   m6809_ICount-= 5; goto end;
			l_0xb0: suba_ex();  m6809_ICount-= 5; goto end;
			l_0xb1: cmpa_ex();  m6809_ICount-= 5; goto end;
			l_0xb2: sbca_ex();  m6809_ICount-= 5; goto end;
			l_0xb3: subd_ex();  m6809_ICount-= 7; goto end;
			l_0xb4: anda_ex();  m6809_ICount-= 5; goto end;
			l_0xb5: bita_ex();  m6809_ICount-= 5; goto end;
			l_0xb6: lda_ex();   m6809_ICount-= 5; goto end;
			l_0xb7: sta_ex();   m6809_ICount-= 5; goto end;
			l_0xb8: eora_ex();  m6809_ICount-= 5; goto end;
			l_0xb9: adca_ex();  m6809_ICount-= 5; goto end;
			l_0xba: ora_ex();   m6809_ICount-= 5; goto end;
			l_0xbb: adda_ex();  m6809_ICount-= 5; goto end;
			l_0xbc: cmpx_ex();  m6809_ICount-= 7; goto end;
			l_0xbd: jsr_ex();   m6809_ICount-= 8; goto end;
			l_0xbe: ldx_ex();   m6809_ICount-= 6; goto end;
			l_0xbf: stx_ex();   m6809_ICount-= 6; goto end;
			l_0xc0: subb_im();  m6809_ICount-= 2; goto end;
			l_0xc1: cmpb_im();  m6809_ICount-= 2; goto end;
			l_0xc2: sbcb_im();  m6809_ICount-= 2; goto end;
			l_0xc3: addd_im();  m6809_ICount-= 4; goto end;
			l_0xc4: andb_im();  m6809_ICount-= 2; goto end;
			l_0xc5: bitb_im();  m6809_ICount-= 2; goto end;
			l_0xc6: ldb_im();   m6809_ICount-= 2; goto end;
			l_0xc7: stb_im();   m6809_ICount-= 2; goto end;
			l_0xc8: eorb_im();  m6809_ICount-= 2; goto end;
			l_0xc9: adcb_im();  m6809_ICount-= 2; goto end;
			l_0xca: orb_im();   m6809_ICount-= 2; goto end;
			l_0xcb: addb_im();  m6809_ICount-= 2; goto end;
			l_0xcc: ldd_im();   m6809_ICount-= 3; goto end;
			l_0xcd: std_im();   m6809_ICount-= 2; goto end;
			l_0xce: ldu_im();   m6809_ICount-= 3; goto end;
			l_0xcf: stu_im();   m6809_ICount-= 3; goto end;
			l_0xd0: subb_di();  m6809_ICount-= 4; goto end;
			l_0xd1: cmpb_di();  m6809_ICount-= 4; goto end;
			l_0xd2: sbcb_di();  m6809_ICount-= 4; goto end;
			l_0xd3: addd_di();  m6809_ICount-= 6; goto end;
			l_0xd4: andb_di();  m6809_ICount-= 4; goto end;
			l_0xd5: bitb_di();  m6809_ICount-= 4; goto end;
			l_0xd6: ldb_di();   m6809_ICount-= 4; goto end;
			l_0xd7: stb_di();   m6809_ICount-= 4; goto end;
			l_0xd8: eorb_di();  m6809_ICount-= 4; goto end;
			l_0xd9: adcb_di();  m6809_ICount-= 4; goto end;
			l_0xda: orb_di();   m6809_ICount-= 4; goto end;
			l_0xdb: addb_di();  m6809_ICount-= 4; goto end;
			l_0xdc: ldd_di();   m6809_ICount-= 5; goto end;
			l_0xdd: std_di();   m6809_ICount-= 5; goto end;
			l_0xde: ldu_di();   m6809_ICount-= 5; goto end;
			l_0xdf: stu_di();   m6809_ICount-= 5; goto end;
			l_0xe0: subb_ix();  m6809_ICount-= 4; goto end;
			l_0xe1: cmpb_ix();  m6809_ICount-= 4; goto end;
			l_0xe2: sbcb_ix();  m6809_ICount-= 4; goto end;
			l_0xe3: addd_ix();  m6809_ICount-= 6; goto end;
			l_0xe4: andb_ix();  m6809_ICount-= 4; goto end;
			l_0xe5: bitb_ix();  m6809_ICount-= 4; goto end;
			l_0xe6: ldb_ix();   m6809_ICount-= 4; goto end;
			l_0xe7: stb_ix();   m6809_ICount-= 4; goto end;
			l_0xe8: eorb_ix();  m6809_ICount-= 4; goto end;
			l_0xe9: adcb_ix();  m6809_ICount-= 4; goto end;
			l_0xea: orb_ix();   m6809_ICount-= 4; goto end;
			l_0xeb: addb_ix();  m6809_ICount-= 4; goto end;
			l_0xec: ldd_ix();   m6809_ICount-= 5; goto end;
			l_0xed: std_ix();   m6809_ICount-= 5; goto end;
			l_0xee: ldu_ix();   m6809_ICount-= 5; goto end;
			l_0xef: stu_ix();   m6809_ICount-= 5; goto end;
			l_0xf0: subb_ex();  m6809_ICount-= 5; goto end;
			l_0xf1: cmpb_ex();  m6809_ICount-= 5; goto end;
			l_0xf2: sbcb_ex();  m6809_ICount-= 5; goto end;
			l_0xf3: addd_ex();  m6809_ICount-= 7; goto end;
			l_0xf4: andb_ex();  m6809_ICount-= 5; goto end;
			l_0xf5: bitb_ex();  m6809_ICount-= 5; goto end;
			l_0xf6: ldb_ex();   m6809_ICount-= 5; goto end;
			l_0xf7: stb_ex();   m6809_ICount-= 5; goto end;
			l_0xf8: eorb_ex();  m6809_ICount-= 5; goto end;
			l_0xf9: adcb_ex();  m6809_ICount-= 5; goto end;
			l_0xfa: orb_ex();   m6809_ICount-= 5; goto end;
			l_0xfb: addb_ex();  m6809_ICount-= 5; goto end;
			l_0xfc: ldd_ex();   m6809_ICount-= 6; goto end;
			l_0xfd: std_ex();   m6809_ICount-= 6; goto end;
			l_0xfe: ldu_ex();   m6809_ICount-= 6; goto end;
			l_0xff: stu_ex();   m6809_ICount-= 6; goto end;

			l_0x01:
			l_0x02:
			l_0x05:
			l_0x0b:
			l_0x12:
			l_0x14:
			l_0x15:
			l_0x18:
			l_0x1b:
			l_0x38:
			l_0x3e:
			l_0x41:
			l_0x42:
			l_0x45:
			l_0x4b:
			l_0x4e:
			l_0x51:
			l_0x52:
			l_0x55:
			l_0x5b:
			l_0x5e:
			l_0x61:
			l_0x62:
			l_0x65:
			l_0x6b:
			l_0x71:
			l_0x72:
			l_0x75:
			l_0x7b:
                m6809_ICount-= 2;

            end: ;

		} while( m6809_ICount > 0 );

        m6809_ICount -= m6809.extra_cycles;
		m6809.extra_cycles = 0;
    }

    return cycles - m6809_ICount;   /* NS 970908 */
}

INLINE void fetch_effective_address( void )
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

	UINT8 postbyte = ROP_ARG(PCD);
	PC++;

    goto *a_jump_table[postbyte];
    
	l_0x00: EA=X;												m6809_ICount-=1;   return;
	l_0x01: EA=X+1;												m6809_ICount-=1;   return;
	l_0x02: EA=X+2;												m6809_ICount-=1;   return;
	l_0x03: EA=X+3;												m6809_ICount-=1;   return;
	l_0x04: EA=X+4;												m6809_ICount-=1;   return;
	l_0x05: EA=X+5;												m6809_ICount-=1;   return;
	l_0x06: EA=X+6;												m6809_ICount-=1;   return;
	l_0x07: EA=X+7;												m6809_ICount-=1;   return;
	l_0x08: EA=X+8;												m6809_ICount-=1;   return;
	l_0x09: EA=X+9;												m6809_ICount-=1;   return;
	l_0x0a: EA=X+10; 											m6809_ICount-=1;   return;
	l_0x0b: EA=X+11; 											m6809_ICount-=1;   return;
	l_0x0c: EA=X+12; 											m6809_ICount-=1;   return;
	l_0x0d: EA=X+13; 											m6809_ICount-=1;   return;
	l_0x0e: EA=X+14; 											m6809_ICount-=1;   return;
	l_0x0f: EA=X+15; 											m6809_ICount-=1;   return;

	l_0x10: EA=X-16; 											m6809_ICount-=1;   return;
	l_0x11: EA=X-15; 											m6809_ICount-=1;   return;
	l_0x12: EA=X-14; 											m6809_ICount-=1;   return;
	l_0x13: EA=X-13; 											m6809_ICount-=1;   return;
	l_0x14: EA=X-12; 											m6809_ICount-=1;   return;
	l_0x15: EA=X-11; 											m6809_ICount-=1;   return;
	l_0x16: EA=X-10; 											m6809_ICount-=1;   return;
	l_0x17: EA=X-9;												m6809_ICount-=1;   return;
	l_0x18: EA=X-8;												m6809_ICount-=1;   return;
	l_0x19: EA=X-7;												m6809_ICount-=1;   return;
	l_0x1a: EA=X-6;												m6809_ICount-=1;   return;
	l_0x1b: EA=X-5;												m6809_ICount-=1;   return;
	l_0x1c: EA=X-4;												m6809_ICount-=1;   return;
	l_0x1d: EA=X-3;												m6809_ICount-=1;   return;
	l_0x1e: EA=X-2;												m6809_ICount-=1;   return;
	l_0x1f: EA=X-1;												m6809_ICount-=1;   return;

	l_0x20: EA=Y;												m6809_ICount-=1;   return;
	l_0x21: EA=Y+1;												m6809_ICount-=1;   return;
	l_0x22: EA=Y+2;												m6809_ICount-=1;   return;
	l_0x23: EA=Y+3;												m6809_ICount-=1;   return;
	l_0x24: EA=Y+4;												m6809_ICount-=1;   return;
	l_0x25: EA=Y+5;												m6809_ICount-=1;   return;
	l_0x26: EA=Y+6;												m6809_ICount-=1;   return;
	l_0x27: EA=Y+7;												m6809_ICount-=1;   return;
	l_0x28: EA=Y+8;												m6809_ICount-=1;   return;
	l_0x29: EA=Y+9;												m6809_ICount-=1;   return;
	l_0x2a: EA=Y+10; 											m6809_ICount-=1;   return;
	l_0x2b: EA=Y+11; 											m6809_ICount-=1;   return;
	l_0x2c: EA=Y+12; 											m6809_ICount-=1;   return;
	l_0x2d: EA=Y+13; 											m6809_ICount-=1;   return;
	l_0x2e: EA=Y+14; 											m6809_ICount-=1;   return;
	l_0x2f: EA=Y+15; 											m6809_ICount-=1;   return;

	l_0x30: EA=Y-16; 											m6809_ICount-=1;   return;
	l_0x31: EA=Y-15; 											m6809_ICount-=1;   return;
	l_0x32: EA=Y-14; 											m6809_ICount-=1;   return;
	l_0x33: EA=Y-13; 											m6809_ICount-=1;   return;
	l_0x34: EA=Y-12; 											m6809_ICount-=1;   return;
	l_0x35: EA=Y-11; 											m6809_ICount-=1;   return;
	l_0x36: EA=Y-10; 											m6809_ICount-=1;   return;
	l_0x37: EA=Y-9;												m6809_ICount-=1;   return;
	l_0x38: EA=Y-8;												m6809_ICount-=1;   return;
	l_0x39: EA=Y-7;												m6809_ICount-=1;   return;
	l_0x3a: EA=Y-6;												m6809_ICount-=1;   return;
	l_0x3b: EA=Y-5;												m6809_ICount-=1;   return;
	l_0x3c: EA=Y-4;												m6809_ICount-=1;   return;
	l_0x3d: EA=Y-3;												m6809_ICount-=1;   return;
	l_0x3e: EA=Y-2;												m6809_ICount-=1;   return;
	l_0x3f: EA=Y-1;												m6809_ICount-=1;   return;

	l_0x40: EA=U;												m6809_ICount-=1;   return;
	l_0x41: EA=U+1;												m6809_ICount-=1;   return;
	l_0x42: EA=U+2;												m6809_ICount-=1;   return;
	l_0x43: EA=U+3;												m6809_ICount-=1;   return;
	l_0x44: EA=U+4;												m6809_ICount-=1;   return;
	l_0x45: EA=U+5;												m6809_ICount-=1;   return;
	l_0x46: EA=U+6;												m6809_ICount-=1;   return;
	l_0x47: EA=U+7;												m6809_ICount-=1;   return;
	l_0x48: EA=U+8;												m6809_ICount-=1;   return;
	l_0x49: EA=U+9;												m6809_ICount-=1;   return;
	l_0x4a: EA=U+10; 											m6809_ICount-=1;   return;
	l_0x4b: EA=U+11; 											m6809_ICount-=1;   return;
	l_0x4c: EA=U+12; 											m6809_ICount-=1;   return;
	l_0x4d: EA=U+13; 											m6809_ICount-=1;   return;
	l_0x4e: EA=U+14; 											m6809_ICount-=1;   return;
	l_0x4f: EA=U+15; 											m6809_ICount-=1;   return;

	l_0x50: EA=U-16; 											m6809_ICount-=1;   return;
	l_0x51: EA=U-15; 											m6809_ICount-=1;   return;
	l_0x52: EA=U-14; 											m6809_ICount-=1;   return;
	l_0x53: EA=U-13; 											m6809_ICount-=1;   return;
	l_0x54: EA=U-12; 											m6809_ICount-=1;   return;
	l_0x55: EA=U-11; 											m6809_ICount-=1;   return;
	l_0x56: EA=U-10; 											m6809_ICount-=1;   return;
	l_0x57: EA=U-9;												m6809_ICount-=1;   return;
	l_0x58: EA=U-8;												m6809_ICount-=1;   return;
	l_0x59: EA=U-7;												m6809_ICount-=1;   return;
	l_0x5a: EA=U-6;												m6809_ICount-=1;   return;
	l_0x5b: EA=U-5;												m6809_ICount-=1;   return;
	l_0x5c: EA=U-4;												m6809_ICount-=1;   return;
	l_0x5d: EA=U-3;												m6809_ICount-=1;   return;
	l_0x5e: EA=U-2;												m6809_ICount-=1;   return;
	l_0x5f: EA=U-1;												m6809_ICount-=1;   return;

	l_0x60: EA=S;												m6809_ICount-=1;   return;
	l_0x61: EA=S+1;												m6809_ICount-=1;   return;
	l_0x62: EA=S+2;												m6809_ICount-=1;   return;
	l_0x63: EA=S+3;												m6809_ICount-=1;   return;
	l_0x64: EA=S+4;												m6809_ICount-=1;   return;
	l_0x65: EA=S+5;												m6809_ICount-=1;   return;
	l_0x66: EA=S+6;												m6809_ICount-=1;   return;
	l_0x67: EA=S+7;												m6809_ICount-=1;   return;
	l_0x68: EA=S+8;												m6809_ICount-=1;   return;
	l_0x69: EA=S+9;												m6809_ICount-=1;   return;
	l_0x6a: EA=S+10; 											m6809_ICount-=1;   return;
	l_0x6b: EA=S+11; 											m6809_ICount-=1;   return;
	l_0x6c: EA=S+12; 											m6809_ICount-=1;   return;
	l_0x6d: EA=S+13; 											m6809_ICount-=1;   return;
	l_0x6e: EA=S+14; 											m6809_ICount-=1;   return;
	l_0x6f: EA=S+15; 											m6809_ICount-=1;   return;

	l_0x70: EA=S-16; 											m6809_ICount-=1;   return;
	l_0x71: EA=S-15; 											m6809_ICount-=1;   return;
	l_0x72: EA=S-14; 											m6809_ICount-=1;   return;
	l_0x73: EA=S-13; 											m6809_ICount-=1;   return;
	l_0x74: EA=S-12; 											m6809_ICount-=1;   return;
	l_0x75: EA=S-11; 											m6809_ICount-=1;   return;
	l_0x76: EA=S-10; 											m6809_ICount-=1;   return;
	l_0x77: EA=S-9;												m6809_ICount-=1;   return;
	l_0x78: EA=S-8;												m6809_ICount-=1;   return;
	l_0x79: EA=S-7;												m6809_ICount-=1;   return;
	l_0x7a: EA=S-6;												m6809_ICount-=1;   return;
	l_0x7b: EA=S-5;												m6809_ICount-=1;   return;
	l_0x7c: EA=S-4;												m6809_ICount-=1;   return;
	l_0x7d: EA=S-3;												m6809_ICount-=1;   return;
	l_0x7e: EA=S-2;												m6809_ICount-=1;   return;
	l_0x7f: EA=S-1;												m6809_ICount-=1;   return;

	l_0x80: EA=X;	X++;										m6809_ICount-=2;   return;
	l_0x81: EA=X;	X+=2;										m6809_ICount-=3;   return;
	l_0x82: X--; 	EA=X;										m6809_ICount-=2;   return;
	l_0x83: X-=2;	EA=X;										m6809_ICount-=3;   return;
	l_0x84: EA=X;																   return;
	l_0x85: EA=X+SIGNED(B);										m6809_ICount-=1;   return;
	l_0x86: EA=X+SIGNED(A);										m6809_ICount-=1;   return;
	l_0x88: IMMBYTE(EA); 	EA=X+SIGNED(EA);					m6809_ICount-=1;   return; /* this is a hack to make Vectrex work. It should be m6809_ICount-=1. Dunno where the cycle was lost :( */
	l_0x89: IMMWORD(ea); 	EA+=X;								m6809_ICount-=4;   return;
	l_0x8b: EA=X+D;												m6809_ICount-=4;   return;
	l_0x8c: IMMBYTE(EA); 	EA=PC+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0x8d: IMMWORD(ea); 	EA+=PC; 							m6809_ICount-=5;   return;
	l_0x8f: IMMWORD(ea); 										m6809_ICount-=5;   return;

	l_0x90: EA=X;	X++;						EAD=RM16(EAD);	m6809_ICount-=5;   return; /* Indirect ,R+ not in my specs */
	l_0x91: EA=X;	X+=2;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0x92: X--; 	EA=X;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0x93: X-=2;	EA=X;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0x94: EA=X;								EAD=RM16(EAD);	m6809_ICount-=3;   return;
	l_0x95: EA=X+SIGNED(B);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0x96: EA=X+SIGNED(A);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0x98: IMMBYTE(EA); 	EA=X+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0x99: IMMWORD(ea); 	EA+=X;				EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0x9b: EA=X+D;								EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0x9c: IMMBYTE(EA); 	EA=PC+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0x9d: IMMWORD(ea); 	EA+=PC; 			EAD=RM16(EAD);	m6809_ICount-=8;   return;
	l_0x9f: IMMWORD(ea); 						EAD=RM16(EAD);	m6809_ICount-=8;   return;

	l_0xa0: EA=Y;	Y++;										m6809_ICount-=2;   return;
	l_0xa1: EA=Y;	Y+=2;										m6809_ICount-=3;   return;
	l_0xa2: Y--; 	EA=Y;										m6809_ICount-=2;   return;
	l_0xa3: Y-=2;	EA=Y;										m6809_ICount-=3;   return;
	l_0xa4: EA=Y;																   return;
	l_0xa5: EA=Y+SIGNED(B);										m6809_ICount-=1;   return;
	l_0xa6: EA=Y+SIGNED(A);										m6809_ICount-=1;   return;
	l_0xa8: IMMBYTE(EA); 	EA=Y+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xa9: IMMWORD(ea); 	EA+=Y;								m6809_ICount-=4;   return;
	l_0xab: EA=Y+D;												m6809_ICount-=4;   return;
	l_0xac: IMMBYTE(EA); 	EA=PC+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xad: IMMWORD(ea); 	EA+=PC; 							m6809_ICount-=5;   return;
	l_0xaf: IMMWORD(ea); 										m6809_ICount-=5;   return;

	l_0xb0: EA=Y;	Y++;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xb1: EA=Y;	Y+=2;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xb2: Y--; 	EA=Y;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xb3: Y-=2;	EA=Y;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xb4: EA=Y;								EAD=RM16(EAD);	m6809_ICount-=3;   return;
	l_0xb5: EA=Y+SIGNED(B);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xb6: EA=Y+SIGNED(A);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xb8: IMMBYTE(EA); 	EA=Y+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xb9: IMMWORD(ea); 	EA+=Y;				EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xbb: EA=Y+D;								EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xbc: IMMBYTE(EA); 	EA=PC+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xbd: IMMWORD(ea); 	EA+=PC; 			EAD=RM16(EAD);	m6809_ICount-=8;   return;
	l_0xbf: IMMWORD(ea); 						EAD=RM16(EAD);	m6809_ICount-=8;   return;

	l_0xc0: EA=U;			U++;								m6809_ICount-=2;   return;
	l_0xc1: EA=U;			U+=2;								m6809_ICount-=3;   return;
	l_0xc2: U--; 			EA=U;								m6809_ICount-=2;   return;
	l_0xc3: U-=2;			EA=U;								m6809_ICount-=3;   return;
	l_0xc4: EA=U;																   return;
	l_0xc5: EA=U+SIGNED(B);										m6809_ICount-=1;   return;
	l_0xc6: EA=U+SIGNED(A);										m6809_ICount-=1;   return;
	l_0xc8: IMMBYTE(EA); 	EA=U+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xc9: IMMWORD(ea); 	EA+=U;								m6809_ICount-=4;   return;
	l_0xcb: EA=U+D;												m6809_ICount-=4;   return;
	l_0xcc: IMMBYTE(EA); 	EA=PC+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xcd: IMMWORD(ea); 	EA+=PC; 							m6809_ICount-=5;   return;
	l_0xcf: IMMWORD(ea); 										m6809_ICount-=5;   return;

	l_0xd0: EA=U;	U++;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xd1: EA=U;	U+=2;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xd2: U--; 	EA=U;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xd3: U-=2;	EA=U;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xd4: EA=U;								EAD=RM16(EAD);	m6809_ICount-=3;   return;
	l_0xd5: EA=U+SIGNED(B);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xd6: EA=U+SIGNED(A);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xd8: IMMBYTE(EA); 	EA=U+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xd9: IMMWORD(ea); 	EA+=U;				EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xdb: EA=U+D;								EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xdc: IMMBYTE(EA); 	EA=PC+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xdd: IMMWORD(ea); 	EA+=PC; 			EAD=RM16(EAD);	m6809_ICount-=8;   return;
	l_0xdf: IMMWORD(ea); 						EAD=RM16(EAD);	m6809_ICount-=8;   return;

	l_0xe0: EA=S;	S++;										m6809_ICount-=2;   return;
	l_0xe1: EA=S;	S+=2;										m6809_ICount-=3;   return;
	l_0xe2: S--; 	EA=S;										m6809_ICount-=2;   return;
	l_0xe3: S-=2;	EA=S;										m6809_ICount-=3;   return;
	l_0xe4: EA=S;																   return;
	l_0xe5: EA=S+SIGNED(B);										m6809_ICount-=1;   return;
	l_0xe6: EA=S+SIGNED(A);										m6809_ICount-=1;   return;
	l_0xe8: IMMBYTE(EA); 	EA=S+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xe9: IMMWORD(ea); 	EA+=S;								m6809_ICount-=4;   return;
	l_0xeb: EA=S+D;												m6809_ICount-=4;   return;
	l_0xec: IMMBYTE(EA); 	EA=PC+SIGNED(EA);					m6809_ICount-=1;   return;
	l_0xed: IMMWORD(ea); 	EA+=PC; 							m6809_ICount-=5;   return;
	l_0xef: IMMWORD(ea); 										m6809_ICount-=5;   return;

	l_0xf0: EA=S;	S++;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xf1: EA=S;	S+=2;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xf2: S--; 	EA=S;						EAD=RM16(EAD);	m6809_ICount-=5;   return;
	l_0xf3: S-=2;	EA=S;						EAD=RM16(EAD);	m6809_ICount-=6;   return;
	l_0xf4: EA=S;								EAD=RM16(EAD);	m6809_ICount-=3;   return;
	l_0xf5: EA=S+SIGNED(B);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xf6: EA=S+SIGNED(A);						EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xf8: IMMBYTE(EA); 	EA=S+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xf9: IMMWORD(ea); 	EA+=S;				EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xfb: EA=S+D;								EAD=RM16(EAD);	m6809_ICount-=7;   return;
	l_0xfc: IMMBYTE(EA); 	EA=PC+SIGNED(EA);	EAD=RM16(EAD);	m6809_ICount-=4;   return;
	l_0xfd: IMMWORD(ea); 	EA+=PC; 			EAD=RM16(EAD);	m6809_ICount-=8;   return;
	l_0xff: IMMWORD(ea); 						EAD=RM16(EAD);	m6809_ICount-=8;   return;

	l_0x87: 
	l_0x8a:
	l_0x8e:
	l_0x97:
	l_0x9a:
	l_0x9e:
	l_0xa7:
	l_0xaa:
	l_0xae:
	l_0xb7:
	l_0xba:
	l_0xbe:
	l_0xc7:
	l_0xca:
	l_0xce:
	l_0xd7:
	l_0xda:
	l_0xde:
	l_0xe7:
	l_0xea:
	l_0xee:
	l_0xf7:
	l_0xfa:
	l_0xfe:
        EA=0; return; /* ILLEGAL */
}

/****************************************************************************
 * M6309 section
 ****************************************************************************/
#if (HAS_HD6309)

void hd6309_state_save(void *file) { state_save(file, "hd6309"); }
void hd6309_state_load(void *file) { state_load(file, "hd6309"); }
const char *hd6309_info(void *context, int regnum)
{
    switch( regnum )
    {
		case CPU_INFO_NAME: return "HD6309";
    }
    return m6809_info(context,regnum);
}
unsigned hd6309_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}
#endif

