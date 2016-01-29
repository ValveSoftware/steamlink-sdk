/*****************************************************************************
 *
 *	 m6509.c
 *	 Portable 6509 emulator V1.0beta1
 *
 *	 Copyright (c) 2000 Peter Trauner, all rights reserved.
 *	 documentation by vice emulator team
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
/*
  2000 March 10 PeT added SO input line

The basic difference is the amount of RAM these machines have been supplied with. The B128 and the CBM *10
models had 128k RAM, the others 256k. This implies some banking scheme, as the 6502 can only address 64k. And
indeed those machines use a 6509, that can address 1 MByte of RAM. It has 2 registers at addresses 0 and 1. The
indirect bank register at address 1 determines the bank (0-15) where the opcodes LDA (zp),Y and STA (zp),Y
take the data from. The exec bank register at address 0 determines the bank where all other read and write
addresses take place.

 vice writes to bank register only with zeropage operand
 0, 1 are bank register in all banks

 lda  (zp),y
 sta  (zp),y

*/

#include <stdio.h>
#include "driver.h"
#include "state.h"
#include "m6509.h"

#include "ops02.h"
#include "ill02.h"
#include "ops09.h"

#define CORE_M6509


#define VERBOSE 0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

typedef struct {
	UINT8	subtype;		/* currently selected cpu sub type */
	void	(**insn)(void); /* pointer to the function pointer table */
	PAIR	ppc;			/* previous program counter */
	/* pc.w.h contains the current page pc_bank.w.h for better speed */
	PAIR	pc; 			/* program counter */
	PAIR	sp; 			/* stack pointer (always 100 - 1FF) */
	PAIR	zp; 			/* zero page address */
	PAIR	ea; 			/* effective address */
	UINT8	a;				/* Accumulator */
	UINT8	x;				/* X index register */
	UINT8	y;				/* Y index register */
	PAIR   pc_bank; 	   /* 4 bits, addressed over address 0 */
	PAIR   ind_bank;	   /* 4 bits, addressed over address 1 */
	UINT8	p;				/* Processor status */
	UINT8	pending_irq;	/* nonzero if an IRQ is pending */
	UINT8	after_cli;		/* pending IRQ and last insn cleared I */
	UINT8	nmi_state;
	UINT8	irq_state;
	UINT8	so_state;
	int 	(*irq_callback)(int irqline);	/* IRQ callback */
}	m6509_Regs;

int m6509_ICount = 0;

static m6509_Regs m6509;

/***************************************************************
 * include the opcode macros, functions and tables
 ***************************************************************/

#include "t6509.c"

READ_HANDLER ( m6509_read_00000 )
{
	return m6509.pc_bank.b.h2;
}

READ_HANDLER ( m6509_read_00001 )
{
	return m6509.ind_bank.b.h2;
}

WRITE_HANDLER ( m6509_write_00000 )
{
	m6509.pc_bank.b.h2=data&0xf;
	m6509.pc.w.h=m6509.pc_bank.w.h;
	change_pc(PCD);
}

WRITE_HANDLER ( m6509_write_00001 )
{
	m6509.ind_bank.b.h2=data&0xf;
}

void m6509_reset (void *param)
{
	m6509.insn = insn6509;

	m6509.pc_bank.d=m6509.ind_bank.d=0;
	m6509.pc_bank.b.h2=m6509.ind_bank.b.h2=0xf; /* cbm500 needs this */
	m6509.pc.w.h=m6509.pc_bank.w.h;
	/* wipe out the rest of the m6509 structure */
	/* read the reset vector into PC */
	PCL = RDMEM(M6509_RST_VEC|PB);
	PCH = RDMEM((M6509_RST_VEC+1)|PB);

	m6509.sp.d = 0x01ff;
	m6509.p = F_T|F_B|F_I|F_Z|(P&F_D);	/* set T, I and Z flags */
	m6509.pending_irq = 0;	/* nonzero if an IRQ is pending */
	m6509.after_cli = 0;	/* pending IRQ and last insn cleared I */
	m6509.irq_callback = NULL;

	change_pc20(PCD);
}

void m6509_exit(void)
{
	/* nothing to do yet */
}

unsigned m6509_get_context (void *dst)
{
	if( dst )
		*(m6509_Regs*)dst = m6509;
	return sizeof(m6509_Regs);
}

void m6509_set_context (void *src)
{
	if( src )
	{
		m6509 = *(m6509_Regs*)src;
		change_pc(PCD);
	}
}

/* gets effective PC */
unsigned m6509_get_pc (void)
{
	return PCD;
}

void m6509_set_pc (unsigned val)
{
	PCW = val&0xffff;
	change_pc(PCD);
}

unsigned m6509_get_sp (void)
{
	return S;
}

void m6509_set_sp (unsigned val)
{
	S = val;
}

unsigned m6509_get_reg (int regnum)
{
	switch( regnum )
	{
		case M6509_PC: return m6509.pc.d;
		case M6509_S: return m6509.sp.b.l;
		case M6509_P: return m6509.p;
		case M6509_A: return m6509.a;
		case M6509_X: return m6509.x;
		case M6509_Y: return m6509.y;
		case M6509_PC_BANK: return m6509.pc_bank.b.h2;
		case M6509_IND_BANK: return m6509.ind_bank.b.h2;
		case M6509_EA: return m6509.ea.d;
		case M6509_ZP: return m6509.zp.b.l;
		case M6509_NMI_STATE: return m6509.nmi_state;
		case M6509_IRQ_STATE: return m6509.irq_state;
		case M6509_SO_STATE: return m6509.so_state;
		case REG_PREVIOUSPC: return m6509.ppc.w.l;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = S + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0x1ff )
					return RDMEM( offset ) | ( RDMEM( offset + 1 ) << 8 );
			}
	}
	return 0;
}

void m6509_set_reg (int regnum, unsigned val)
{
	switch( regnum )
	{
		case M6509_PC: m6509.pc.w.l = val; break;
		case M6509_S: m6509.sp.b.l = val; break;
		case M6509_P: m6509.p = val; break;
		case M6509_A: m6509.a = val; break;
		case M6509_X: m6509.x = val; break;
		case M6509_Y: m6509.y = val; break;
		case M6509_PC_BANK: m6509.pc_bank.b.h2 = val; break;
		case M6509_IND_BANK: m6509.ind_bank.b.h2 = val; break;
		case M6509_EA: m6509.ea.d = val; break;
		case M6509_ZP: m6509.zp.b.l = val; break;
		case M6509_NMI_STATE: m6509_set_nmi_line( val ); break;
		case M6509_IRQ_STATE: m6509_set_irq_line( 0, val ); break;
		case M6509_SO_STATE: m6509_set_irq_line( M6509_SET_OVERFLOW, val ); break;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = S + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0x1ff )
				{
					WRMEM( offset, val & 0xfff );
					WRMEM( offset + 1, (val >> 8) & 0xff );
				}
			}
	}
}

INLINE void m6509_take_irq(void)
{
	if( !(P & F_I) )
	{
		EAD = M6509_IRQ_VEC;
		EAWH = PBWH;
		m6509_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P |= F_I;		/* knock out D and set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M6509#%d takes IRQ ($%04x)\n", cpu_getactivecpu(), PCD));
		/* call back the cpuintrf to let it clear the line */
		if (m6509.irq_callback) (*m6509.irq_callback)(0);
		change_pc20(PCD);
	}
	m6509.pending_irq = 0;
}

int m6509_execute(int cycles)
{
	m6509_ICount = cycles;

	change_pc20(PCD);

	do
	{
		UINT8 op;
		PPC = PCD;

		/* if an irq is pending, take it now */
		if( m6509.pending_irq )
			m6509_take_irq();

		op = RDOP();
		(*m6509.insn[op])();

		/* check if the I flag was just reset (interrupts enabled) */
		if( m6509.after_cli )
		{
			LOG(("M6509#%d after_cli was >0", cpu_getactivecpu()));
			m6509.after_cli = 0;
			if (m6509.irq_state != CLEAR_LINE)
			{
				LOG((": irq line is asserted: set pending IRQ\n"));
				m6509.pending_irq = 1;
			}
			else
			{
				LOG((": irq line is clear\n"));
			}
		}
		else
		if( m6509.pending_irq )
			m6509_take_irq();

	} while (m6509_ICount > 0);

	return cycles - m6509_ICount;
}

void m6509_set_nmi_line(int state)
{
	if (m6509.nmi_state == state) return;
	m6509.nmi_state = state;
	if( state != CLEAR_LINE )
	{
		LOG(( "M6509#%d set_nmi_line(ASSERT)\n", cpu_getactivecpu()));
		EAD = M6509_NMI_VEC;
		EAWH = PBWH;
		m6509_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P |= F_I;		/* knock out D and set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M6509#%d takes NMI ($%04x)\n", cpu_getactivecpu(), PCD));
		change_pc20(PCD);
	}
}

void m6509_set_irq_line(int irqline, int state)
{
	if( irqline == M6509_SET_OVERFLOW )
	{
		if( m6509.so_state && !state )
		{
			LOG(( "M6509#%d set overflow\n", cpu_getactivecpu()));
			P|=F_V;
		}
		m6509.so_state=state;
		return;
	}
	m6509.irq_state = state;
	if( state != CLEAR_LINE )
	{
		LOG(( "M6509#%d set_irq_line(ASSERT)\n", cpu_getactivecpu()));
		m6509.pending_irq = 1;
	}
}

void m6509_set_irq_callback(int (*callback)(int))
{
	m6509.irq_callback = callback;
}

void m6509_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	/* insn is set at restore since it's a pointer */
	state_save_UINT16(file,"m6509",cpu,"PC",&m6509.pc.w.l,2);
	state_save_UINT8(file,"m6509",cpu,"SP",&m6509.sp.b.l,1);
	state_save_UINT8(file,"m6509",cpu,"P",&m6509.p,1);
	state_save_UINT8(file,"m6509",cpu,"A",&m6509.a,1);
	state_save_UINT8(file,"m6509",cpu,"X",&m6509.x,1);
	state_save_UINT8(file,"m6509",cpu,"Y",&m6509.y,1);
	state_save_UINT8(file,"m6509",cpu,"PB",&m6509.pc_bank.b.h2,1);
	state_save_UINT8(file,"m6509",cpu,"IB",&m6509.ind_bank.b.h2,1);
	state_save_UINT8(file,"m6509",cpu,"PENDING",&m6509.pending_irq,1);
	state_save_UINT8(file,"m6509",cpu,"AFTER_CLI",&m6509.after_cli,1);
	state_save_UINT8(file,"m6509",cpu,"NMI_STATE",&m6509.nmi_state,1);
	state_save_UINT8(file,"m6509",cpu,"IRQ_STATE",&m6509.irq_state,1);
	state_save_UINT8(file,"m6509",cpu,"SO_STATE",&m6509.so_state,1);
}

void m6509_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	m6509.insn = insn6509;
	state_load_UINT16(file,"m6509",cpu,"PC",&m6509.pc.w.l,2);
	state_load_UINT8(file,"m6509",cpu,"SP",&m6509.sp.b.l,1);
	state_load_UINT8(file,"m6509",cpu,"P",&m6509.p,1);
	state_load_UINT8(file,"m6509",cpu,"A",&m6509.a,1);
	state_load_UINT8(file,"m6509",cpu,"X",&m6509.x,1);
	state_load_UINT8(file,"m6509",cpu,"Y",&m6509.y,1);
	state_load_UINT8(file,"m6509",cpu,"PB",&m6509.pc_bank.b.h2,1);
	state_load_UINT8(file,"m6509",cpu,"IB",&m6509.ind_bank.b.h2,1);
	state_load_UINT8(file,"m6509",cpu,"PENDING",&m6509.pending_irq,1);
	state_load_UINT8(file,"m6509",cpu,"AFTER_CLI",&m6509.after_cli,1);
	state_load_UINT8(file,"m6509",cpu,"NMI_STATE",&m6509.nmi_state,1);
	state_load_UINT8(file,"m6509",cpu,"IRQ_STATE",&m6509.irq_state,1);
	state_load_UINT8(file,"m6509",cpu,"SO_STATE",&m6509.so_state,1);
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *m6509_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6509";
		case CPU_INFO_FAMILY: return "MOS Technology 6509";
		case CPU_INFO_VERSION: return "1.0beta";
		case CPU_INFO_CREDITS:
			return "Copyright (c) 1998 Juergen Buchmueller\n"
				"Copyright (c) 2000 Peter Trauner\n"
				"all rights reserved.";
		case CPU_INFO_FILE: return __FILE__;
	}
	return buffer[which];
}

unsigned m6509_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}



