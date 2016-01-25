/*****************************************************************************
 *
 *	 m6502.c
 *	 Portable 6502/65c02/65sc02/6510/n2a03 emulator V1.2
 *
 *	 Copyright (c) 1998,1999,2000 Juergen Buchmueller, all rights reserved.
 *	 65sc02 core Copyright (c) 2000 Peter Trauner.
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
/* 2.February 2000 PeT added 65sc02 subtype */
/* 10.March   2000 PeT added 6502 set overflow input line */

#include <stdio.h>
#include "driver.h"
#include "state.h"
#include "m6502.h"
#include "ops02.h"
#include "ill02.h"


#define VERBOSE 0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

/****************************************************************************
 * The 6502 registers.
 ****************************************************************************/
typedef struct
{
	UINT8	subtype;		/* currently selected cpu sub type */
	void	(**insn)(void); /* pointer to the function pointer table */
	PAIR	ppc;			/* previous program counter */
	PAIR	pc; 			/* program counter */
	PAIR	sp; 			/* stack pointer (always 100 - 1FF) */
	PAIR	zp; 			/* zero page address */
	PAIR	ea; 			/* effective address */
	UINT8	a;				/* Accumulator */
	UINT8	x;				/* X index register */
	UINT8	y;				/* Y index register */
	UINT8	p;				/* Processor status */
	UINT8	pending_irq;	/* nonzero if an IRQ is pending */
	UINT8	after_cli;		/* pending IRQ and last insn cleared I */
	UINT8	nmi_state;
	UINT8	irq_state;
	UINT8   so_state;
	int 	(*irq_callback)(int irqline);	/* IRQ callback */
}	m6502_Regs;

int m6502_ICount = 0;

static m6502_Regs m6502;

/***************************************************************
 * include the opcode macros, functions and tables
 ***************************************************************/
#include "t6502.cpp"

#if (HAS_M6510)
#include "t6510.cpp"
#endif

#include "opsc02.h"

#if (HAS_M65C02)
#include "t65c02.cpp"
#endif

#if (HAS_M65SC02)
#include "t65sc02.cpp"
#endif

#if (HAS_N2A03)
#include "tn2a03.cpp"
#endif

/*****************************************************************************
 *
 *		6502 CPU interface functions
 *
 *****************************************************************************/

void m6502_reset(void *param)
{
	m6502.subtype = SUBTYPE_6502;
	m6502.insn = insn6502;

	/* wipe out the rest of the m6502 structure */
	/* read the reset vector into PC */
	PCL = RDMEM(M6502_RST_VEC);
	PCH = RDMEM(M6502_RST_VEC+1);

	m6502.sp.d = 0x01ff;	/* stack pointer starts at page 1 offset FF */
	m6502.p = F_T|F_I|F_Z|F_B|(P&F_D);	/* set T, I and Z flags */
	m6502.pending_irq = 0;	/* nonzero if an IRQ is pending */
	m6502.after_cli = 0;	/* pending IRQ and last insn cleared I */
	m6502.irq_callback = NULL;

	change_pc16(PCD);
}

void m6502_exit(void)
{
	/* nothing to do yet */
}

unsigned m6502_get_context (void *dst)
{
	if( dst )
		*(m6502_Regs*)dst = m6502;
	return sizeof(m6502_Regs);
}

void m6502_set_context (void *src)
{
	if( src )
	{
		m6502 = *(m6502_Regs*)src;
		change_pc(PCD);
	}
}

unsigned m6502_get_pc (void)
{
	return PCD;
}

void m6502_set_pc (unsigned val)
{
	PCW = val;
	change_pc(PCD);
}

unsigned m6502_get_sp (void)
{
	return S;
}

void m6502_set_sp (unsigned val)
{
	S = val;
}

unsigned m6502_get_reg (int regnum)
{
	switch( regnum )
	{
		case M6502_PC: return m6502.pc.w.l;
		case M6502_S: return m6502.sp.b.l;
		case M6502_P: return m6502.p;
		case M6502_A: return m6502.a;
		case M6502_X: return m6502.x;
		case M6502_Y: return m6502.y;
		case M6502_EA: return m6502.ea.w.l;
		case M6502_ZP: return m6502.zp.w.l;
		case M6502_NMI_STATE: return m6502.nmi_state;
		case M6502_IRQ_STATE: return m6502.irq_state;
		case M6502_SO_STATE: return m6502.so_state;
		case M6502_SUBTYPE: return m6502.subtype;
		case REG_PREVIOUSPC: return m6502.ppc.w.l;
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

void m6502_set_reg (int regnum, unsigned val)
{
	switch( regnum )
	{
		case M6502_PC: m6502.pc.w.l = val; break;
		case M6502_S: m6502.sp.b.l = val; break;
		case M6502_P: m6502.p = val; break;
		case M6502_A: m6502.a = val; break;
		case M6502_X: m6502.x = val; break;
		case M6502_Y: m6502.y = val; break;
		case M6502_EA: m6502.ea.w.l = val; break;
		case M6502_ZP: m6502.zp.w.l = val; break;
		case M6502_NMI_STATE: m6502_set_nmi_line( val ); break;
		case M6502_IRQ_STATE: m6502_set_irq_line( 0, val ); break;
		case M6502_SO_STATE: m6502_set_irq_line( M6502_SET_OVERFLOW, val ); break;
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

INLINE void m6502_take_irq(void)
{
	if( !(P & F_I) )
	{
		EAD = M6502_IRQ_VEC;
		m6502_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P |= F_I;		/* set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M6502#%d takes IRQ ($%04x)\n", cpu_getactivecpu(), PCD));
		/* call back the cpuintrf to let it clear the line */
		if (m6502.irq_callback) (*m6502.irq_callback)(0);
		change_pc16(PCD);
	}
	m6502.pending_irq = 0;
}

int m6502_execute(int cycles)
{
	m6502_ICount = cycles;

	change_pc16(PCD);

	do
	{
		UINT8 op;
		PPC = PCD;

#if 1
		/* if an irq is pending, take it now */
		if( m6502.pending_irq )
			m6502_take_irq();

		op = RDOP();
		(*m6502.insn[op])();
#else
		/* thought as irq request while executing sei */
        /* sei sets I flag on the stack*/
		op = RDOP();

		/* if an irq is pending, take it now */
		if( m6502.pending_irq && (op == 0x78) )
			m6502_take_irq();

		(*m6502.insn[op])();
#endif

		/* check if the I flag was just reset (interrupts enabled) */
		if( m6502.after_cli )
		{
			LOG(("M6502#%d after_cli was >0", cpu_getactivecpu()));
			m6502.after_cli = 0;
			if (m6502.irq_state != CLEAR_LINE)
			{
				LOG((": irq line is asserted: set pending IRQ\n"));
				m6502.pending_irq = 1;
			}
			else
			{
				LOG((": irq line is clear\n"));
			}
		}
		else
		if( m6502.pending_irq )
			m6502_take_irq();

	} while (m6502_ICount > 0);

	return cycles - m6502_ICount;
}

void m6502_set_nmi_line(int state)
{
	if (m6502.nmi_state == state) return;
	m6502.nmi_state = state;
	if( state != CLEAR_LINE )
	{
		LOG(( "M6502#%d set_nmi_line(ASSERT)\n", cpu_getactivecpu()));
		EAD = M6502_NMI_VEC;
		m6502_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P |= F_I;		/* set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M6502#%d takes NMI ($%04x)\n", cpu_getactivecpu(), PCD));
		change_pc16(PCD);
	}
}

void m6502_set_irq_line(int irqline, int state)
{
	if( irqline == M6502_SET_OVERFLOW )
	{
		if( m6502.so_state && !state )
		{
			LOG(( "M6502#%d set overflow\n", cpu_getactivecpu()));
			P|=F_V;
		}
		m6502.so_state=state;
		return;
	}
	m6502.irq_state = state;
	if( state != CLEAR_LINE )
	{
		LOG(( "M6502#%d set_irq_line(ASSERT)\n", cpu_getactivecpu()));
		m6502.pending_irq = 1;
	}
}

void m6502_set_irq_callback(int (*callback)(int))
{
	m6502.irq_callback = callback;
}

void m6502_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT8(file,"m6502",cpu,"TYPE",&m6502.subtype,1);
	/* insn is set at restore since it's a pointer */
	state_save_UINT16(file,"m6502",cpu,"PC",&m6502.pc.w.l,2);
	state_save_UINT16(file,"m6502",cpu,"SP",&m6502.sp.w.l,2);
	state_save_UINT8(file,"m6502",cpu,"P",&m6502.p,1);
	state_save_UINT8(file,"m6502",cpu,"A",&m6502.a,1);
	state_save_UINT8(file,"m6502",cpu,"X",&m6502.x,1);
	state_save_UINT8(file,"m6502",cpu,"Y",&m6502.y,1);
	state_save_UINT8(file,"m6502",cpu,"PENDING",&m6502.pending_irq,1);
	state_save_UINT8(file,"m6502",cpu,"AFTER_CLI",&m6502.after_cli,1);
	state_save_UINT8(file,"m6502",cpu,"NMI_STATE",&m6502.nmi_state,1);
	state_save_UINT8(file,"m6502",cpu,"IRQ_STATE",&m6502.irq_state,1);
	state_save_UINT8(file,"m6502",cpu,"SO_STATE",&m6502.so_state,1);
}

void m6502_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT8(file,"m6502",cpu,"TYPE",&m6502.subtype,1);
	/* insn is set at restore since it's a pointer */
	switch (m6502.subtype)
	{
#if (HAS_M65C02)
		case SUBTYPE_65C02:
			m6502.insn = insn65c02;
			break;
#endif
#if (HAS_M65SC02)
		case SUBTYPE_65SC02:
			m6502.insn = insn65sc02;
			break;
#endif
#if (HAS_M6510)
		case SUBTYPE_6510:
			m6502.insn = insn6510;
			break;
#endif
		default:
			m6502.insn = insn6502;
			break;
	}
	state_load_UINT16(file,"m6502",cpu,"PC",&m6502.pc.w.l,2);
	state_load_UINT16(file,"m6502",cpu,"SP",&m6502.sp.w.l,2);
	state_load_UINT8(file,"m6502",cpu,"P",&m6502.p,1);
	state_load_UINT8(file,"m6502",cpu,"A",&m6502.a,1);
	state_load_UINT8(file,"m6502",cpu,"X",&m6502.x,1);
	state_load_UINT8(file,"m6502",cpu,"Y",&m6502.y,1);
	state_load_UINT8(file,"m6502",cpu,"PENDING",&m6502.pending_irq,1);
	state_load_UINT8(file,"m6502",cpu,"AFTER_CLI",&m6502.after_cli,1);
	state_load_UINT8(file,"m6502",cpu,"NMI_STATE",&m6502.nmi_state,1);
	state_load_UINT8(file,"m6502",cpu,"IRQ_STATE",&m6502.irq_state,1);
	state_load_UINT8(file,"m6502",cpu,"SO_STATE",&m6502.so_state,1);
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *m6502_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6502";
		case CPU_INFO_FAMILY: return "Motorola 6502";
		case CPU_INFO_VERSION: return "1.2";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (c) 1998 Juergen Buchmueller, all rights reserved.";
	}
	return "";
}

unsigned m6502_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

/****************************************************************************
 * 65C02 section
 ****************************************************************************/
#if (HAS_M65C02)

void m65c02_reset (void *param)
{
	m6502_reset(param);
	P &=~F_D;
	m6502.subtype = SUBTYPE_65C02;
	m6502.insn = insn65c02;
}

void m65c02_exit  (void) { m6502_exit(); }

INLINE void m65c02_take_irq(void)
{
	if( !(P & F_I) )
	{
		EAD = M6502_IRQ_VEC;
		m6502_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P = (P & ~F_D) | F_I;		/* knock out D and set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M65c02#%d takes IRQ ($%04x)\n", cpu_getactivecpu(), PCD));
		/* call back the cpuintrf to let it clear the line */
		if (m6502.irq_callback) (*m6502.irq_callback)(0);
		change_pc16(PCD);
	}
	m6502.pending_irq = 0;
}

int m65c02_execute(int cycles)
{
	m6502_ICount = cycles;

	change_pc16(PCD);

	do
	{
		UINT8 op;
		PPC = PCD;

		op = RDOP();
		(*m6502.insn[op])();

		/* if an irq is pending, take it now */
		if( m6502.pending_irq )
			m65c02_take_irq();


		/* check if the I flag was just reset (interrupts enabled) */
		if( m6502.after_cli )
		{
			LOG(("M6502#%d after_cli was >0", cpu_getactivecpu()));
			m6502.after_cli = 0;
			if (m6502.irq_state != CLEAR_LINE)
			{
				LOG((": irq line is asserted: set pending IRQ\n"));
				m6502.pending_irq = 1;
			}
			else
			{
				LOG((": irq line is clear\n"));
			}
		}
		else
		if( m6502.pending_irq )
			m65c02_take_irq();

	} while (m6502_ICount > 0);

	return cycles - m6502_ICount;
}

unsigned m65c02_get_context (void *dst) { return m6502_get_context(dst); }
void m65c02_set_context (void *src) { m6502_set_context(src); }
unsigned m65c02_get_pc (void) { return m6502_get_pc(); }
void m65c02_set_pc (unsigned val) { m6502_set_pc(val); }
unsigned m65c02_get_sp (void) { return m6502_get_sp(); }
void m65c02_set_sp (unsigned val) { m6502_set_sp(val); }
unsigned m65c02_get_reg (int regnum) { return m6502_get_reg(regnum); }
void m65c02_set_reg (int regnum, unsigned val) { m6502_set_reg(regnum,val); }

void m65c02_set_nmi_line(int state)
{
	if (m6502.nmi_state == state) return;
	m6502.nmi_state = state;
	if( state != CLEAR_LINE )
	{
		LOG(( "M6502#%d set_nmi_line(ASSERT)\n", cpu_getactivecpu()));
		EAD = M6502_NMI_VEC;
		m6502_ICount -= 7;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P = (P & ~F_D) | F_I;		/* knock out D and set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		LOG(("M6502#%d takes NMI ($%04x)\n", cpu_getactivecpu(), PCD));
		change_pc16(PCD);
	}
}

void m65c02_set_irq_line(int irqline, int state) { m6502_set_irq_line(irqline,state); }
void m65c02_set_irq_callback(int (*callback)(int irqline)) { m6502_set_irq_callback(callback); }
void m65c02_state_save(void *file) { m6502_state_save(file); }
void m65c02_state_load(void *file) { m6502_state_load(file); }
const char *m65c02_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M65C02";
		case CPU_INFO_VERSION: return "1.2";
	}
	return m6502_info(context,regnum);
}
unsigned m65c02_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

#endif

/****************************************************************************
 * 65SC02 section
 ****************************************************************************/
#if (HAS_M65SC02)

void m65sc02_reset (void *param)
{
	m6502_reset(param);
	m6502.subtype = SUBTYPE_65SC02;
	m6502.insn = insn65sc02;
}
void m65sc02_exit  (void) { m6502_exit(); }
int  m65sc02_execute(int cycles) { return m65c02_execute(cycles); }
unsigned m65sc02_get_context (void *dst) { return m6502_get_context(dst); }
void m65sc02_set_context (void *src) { m6502_set_context(src); }
unsigned m65sc02_get_pc (void) { return m6502_get_pc(); }
void m65sc02_set_pc (unsigned val) { m6502_set_pc(val); }
unsigned m65sc02_get_sp (void) { return m6502_get_sp(); }
void m65sc02_set_sp (unsigned val) { m6502_set_sp(val); }
unsigned m65sc02_get_reg (int regnum) { return m6502_get_reg(regnum); }
void m65sc02_set_reg (int regnum, unsigned val) { m6502_set_reg(regnum,val); }
void m65sc02_set_nmi_line(int state) { m6502_set_nmi_line(state); }
void m65sc02_set_irq_line(int irqline, int state) { m6502_set_irq_line(irqline,state); }
void m65sc02_set_irq_callback(int (*callback)(int irqline)) { m6502_set_irq_callback(callback); }
void m65sc02_state_save(void *file) { m6502_state_save(file); }
void m65sc02_state_load(void *file) { m6502_state_load(file); }
const char *m65sc02_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M65SC02";
		case CPU_INFO_FAMILY: return "Metal Oxid Semiconductor MOS 6502";
		case CPU_INFO_VERSION: return "1.0beta";
		case CPU_INFO_CREDITS:
			return "Copyright (c) 1998 Juergen Buchmueller\n"
				"Copyright (c) 2000 Peter Trauner\n"
				"all rights reserved.";
	}
	return m6502_info(context,regnum);
}
unsigned m65sc02_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}

#endif

/****************************************************************************
 * 2A03 section
 ****************************************************************************/
#if (HAS_N2A03)

void n2a03_reset (void *param)
{
	m6502_reset(param);
	m6502.subtype = SUBTYPE_2A03;
	m6502.insn = insn2a03;
}
void n2a03_exit  (void) { m6502_exit(); }
int  n2a03_execute(int cycles) { return m65c02_execute(cycles); }
unsigned n2a03_get_context (void *dst) { return m6502_get_context(dst); }
void n2a03_set_context (void *src) { m6502_set_context(src); }
unsigned n2a03_get_pc (void) { return m6502_get_pc(); }
void n2a03_set_pc (unsigned val) { m6502_set_pc(val); }
unsigned n2a03_get_sp (void) { return m6502_get_sp(); }
void n2a03_set_sp (unsigned val) { m6502_set_sp(val); }
unsigned n2a03_get_reg (int regnum) { return m6502_get_reg(regnum); }
void n2a03_set_reg (int regnum, unsigned val) { m6502_set_reg(regnum,val); }
void n2a03_set_nmi_line(int state) { m6502_set_nmi_line(state); }
void n2a03_set_irq_line(int irqline, int state) { m6502_set_irq_line(irqline,state); }
void n2a03_set_irq_callback(int (*callback)(int irqline)) { m6502_set_irq_callback(callback); }
void n2a03_state_save(void *file) { m6502_state_save(file); }
void n2a03_state_load(void *file) { m6502_state_load(file); }
const char *n2a03_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "N2A03";
		case CPU_INFO_VERSION: return "1.0";
	}
	return m6502_info(context,regnum);
}

/* The N2A03 is integrally tied to its PSG (they're on the same die).
   Bit 7 of address $4011 (the PSG's DPCM control register), when set,
   causes an IRQ to be generated.  This function allows the IRQ to be called
   from the PSG core when such an occasion arises. */
void n2a03_irq(void)
{
  m65c02_take_irq();
}

unsigned n2a03_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}
#endif

/****************************************************************************
 * 6510 section
 ****************************************************************************/
#if (HAS_M6510)
void m6510_reset (void *param)
{
	m6502_reset(param);
	m6502.subtype = SUBTYPE_6510;
	m6502.insn = insn6510;
}
void m6510_exit  (void) { m6502_exit(); }
int  m6510_execute(int cycles) { return m6502_execute(cycles); }
unsigned m6510_get_context (void *dst) { return m6502_get_context(dst); }
void m6510_set_context (void *src) { m6502_set_context(src); }
unsigned m6510_get_pc (void) { return m6502_get_pc(); }
void m6510_set_pc (unsigned val) { m6502_set_pc(val); }
unsigned m6510_get_sp (void) { return m6502_get_sp(); }
void m6510_set_sp (unsigned val) { m6502_set_sp(val); }
unsigned m6510_get_reg (int regnum) { return m6502_get_reg(regnum); }
void m6510_set_reg (int regnum, unsigned val) { m6502_set_reg(regnum,val); }
void m6510_set_nmi_line(int state) { m6502_set_nmi_line(state); }
void m6510_set_irq_line(int irqline, int state) { m6502_set_irq_line(irqline,state); }
void m6510_set_irq_callback(int (*callback)(int irqline)) { m6502_set_irq_callback(callback); }
void m6510_state_save(void *file) { m6502_state_save(file); }
void m6510_state_load(void *file) { m6502_state_load(file); }
const char *m6510_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6510";
		case CPU_INFO_VERSION: return "1.2";
	}
	return m6502_info(context,regnum);
}

unsigned m6510_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}
#endif

#if (HAS_M6510T)
const char *m6510t_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M6510T";
	}
	return m6510_info(context,regnum);
}
#endif

#if (HAS_M7501)
const char *m7501_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M7501";
	}
	return m6510_info(context,regnum);
}
#endif

#if (HAS_M8502)
const char *m8502_info(void *context, int regnum)
{
	switch( regnum )
	{
		case CPU_INFO_NAME: return "M8502";
	}
	return m6510_info(context,regnum);
}
#endif
