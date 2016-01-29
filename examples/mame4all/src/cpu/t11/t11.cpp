/*** t11: Portable DEC T-11 emulator ******************************************

	Copyright (C) Aaron Giles 1998

	System dependencies:	long must be at least 32 bits
	                        word must be 16 bit unsigned int
							byte must be 8 bit unsigned int
							long must be more than 16 bits
							arrays up to 65536 bytes must be supported
							machine must be twos complement

*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpuintrf.h"
#include "t11.h"

/* T-11 Registers */
typedef struct
{
	PAIR	ppc;	/* previous program counter */
    PAIR    reg[8];
    PAIR    psw;
    UINT16  op;
    UINT8	wait_state;
    UINT8   *bank[8];
    INT8    irq_state[4];
    int		interrupt_cycles;
    int     (*irq_callback)(int irqline);
} t11_Regs;

static t11_Regs t11;

/* public globals */
int	t11_ICount=50000;

/* register definitions and shortcuts */
#define REGD(x) t11.reg[x].d
#define REGW(x) t11.reg[x].w.l
#define REGB(x) t11.reg[x].b.l
#define SP REGW(6)
#define PC REGW(7)
#define SPD REGD(6)
#define PCD REGD(7)
#define PSW t11.psw.b.l

/* shortcuts for reading opcodes */
INLINE int ROPCODE(void)
{
	int pc = PCD;
	PC += 2;
	return READ_WORD(&t11.bank[pc >> 13][pc & 0x1fff]);
}

/* shortcuts for reading/writing memory bytes */
#define RBYTE(addr)      T11_RDMEM(addr)
#define WBYTE(addr,data) T11_WRMEM((addr), (data))

/* shortcuts for reading/writing memory words */
INLINE int RWORD(int addr)
{
	return T11_RDMEM_WORD(addr & 0xfffe);
}

INLINE void WWORD(int addr, int data)
{
	T11_WRMEM_WORD(addr & 0xfffe, data);
}

/* pushes/pops a value from the stack */
INLINE void PUSH(int val)
{
	SP -= 2;
	WWORD(SPD, val);
}

INLINE int POP(void)
{
	int result = RWORD(SPD);
	SP += 2;
	return result;
}

/* flag definitions */
#define CFLAG 1
#define VFLAG 2
#define ZFLAG 4
#define NFLAG 8

/* extracts flags */
#define GET_C (PSW & CFLAG)
#define GET_V (PSW & VFLAG)
#define GET_Z (PSW & ZFLAG)
#define GET_N (PSW & NFLAG)

/* clears flags */
#define CLR_C (PSW &= ~CFLAG)
#define CLR_V (PSW &= ~VFLAG)
#define CLR_Z (PSW &= ~ZFLAG)
#define CLR_N (PSW &= ~NFLAG)

/* sets flags */
#define SET_C (PSW |= CFLAG)
#define SET_V (PSW |= VFLAG)
#define SET_Z (PSW |= ZFLAG)
#define SET_N (PSW |= NFLAG)

/****************************************************************************
 * Checks active interrupts for a valid one
 ****************************************************************************/
static void t11_check_irqs(void)
{
	int priority = PSW & 0xe0;
	int irq;

	/* loop over IRQs from highest to lowest */
	for (irq = 0; irq < 4; irq++)
	{
	    if (t11.irq_state[irq] != CLEAR_LINE)
		{
			/* get the priority of this interrupt */
			int new_pc = RWORD(0x38 + (irq * 0x10));
			int new_psw = RWORD(0x3a + (irq * 0x10));

			/* if it's greater than the current priority, take it */
			if ((new_psw & 0xe0) > priority)
			{
				if (t11.irq_callback)
					(*t11.irq_callback)(irq);

				/* push the old state, set the new one */
				PUSH(PSW);
				PUSH(PC);
				PCD = new_pc;
				PSW = new_psw;
				priority = new_psw & 0xe0;

				/* count 50 cycles (who knows what it really is) and clear the WAIT flag */
				t11.interrupt_cycles += 50;
				t11.wait_state = 0;
			}
	    }
	}
}

/* includes the static function prototypes and the master opcode table */
#include "t11table.cpp"

/* includes the actual opcode implementations */
#include "t11ops.cpp"

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
unsigned t11_get_context(void *dst)
{
	if( dst )
		*(t11_Regs*)dst = t11;
	return sizeof(t11_Regs);
}

/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void t11_set_context(void *src)
{
	if( src )
		t11 = *(t11_Regs*)src;
	t11_check_irqs();
}

/****************************************************************************
 * Return program counter
 ****************************************************************************/
unsigned t11_get_pc(void)
{
	return PCD;
}

/****************************************************************************
 * Set program counter
 ****************************************************************************/
void t11_set_pc(unsigned val)
{
	PC = val;
}

/****************************************************************************
 * Return stack pointer
 ****************************************************************************/
unsigned t11_get_sp(void)
{
	return SPD;
}

/****************************************************************************
 * Set stack pointer
 ****************************************************************************/
void t11_set_sp(unsigned val)
{
	SP = val;
}

/****************************************************************************
 * Return a specific register
 ****************************************************************************/
unsigned t11_get_reg(int regnum)
{
	switch( regnum )
	{
		case T11_PC: return PCD;
		case T11_SP: return SPD;
		case T11_PSW: return PSW;
		case T11_R0: return REGD(0);
		case T11_R1: return REGD(1);
		case T11_R2: return REGD(2);
		case T11_R3: return REGD(3);
		case T11_R4: return REGD(4);
		case T11_R5: return REGD(5);
		case T11_IRQ0_STATE: return t11.irq_state[T11_IRQ0];
		case T11_IRQ1_STATE: return t11.irq_state[T11_IRQ1];
		case T11_IRQ2_STATE: return t11.irq_state[T11_IRQ2];
		case T11_IRQ3_STATE: return t11.irq_state[T11_IRQ3];
		case T11_BANK0: return (unsigned)(t11.bank[0] - OP_RAM);
		case T11_BANK1: return (unsigned)(t11.bank[1] - OP_RAM);
		case T11_BANK2: return (unsigned)(t11.bank[2] - OP_RAM);
		case T11_BANK3: return (unsigned)(t11.bank[3] - OP_RAM);
		case T11_BANK4: return (unsigned)(t11.bank[4] - OP_RAM);
		case T11_BANK5: return (unsigned)(t11.bank[5] - OP_RAM);
		case T11_BANK6: return (unsigned)(t11.bank[6] - OP_RAM);
		case T11_BANK7: return (unsigned)(t11.bank[7] - OP_RAM);
		case REG_PREVIOUSPC: return t11.ppc.w.l;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = SPD + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					return RWORD( offset );
			}
	}
	return 0;
}

/****************************************************************************
 * Set a specific register
 ****************************************************************************/
void t11_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case T11_PC: PC = val; /* change_pc not needed */ break;
		case T11_SP: SP = val; break;
		case T11_PSW: PSW = val; break;
		case T11_R0: REGW(0) = val; break;
		case T11_R1: REGW(1) = val; break;
		case T11_R2: REGW(2) = val; break;
		case T11_R3: REGW(3) = val; break;
		case T11_R4: REGW(4) = val; break;
		case T11_R5: REGW(5) = val; break;
		case T11_IRQ0_STATE: t11_set_irq_line(T11_IRQ0,val); break;
		case T11_IRQ1_STATE: t11_set_irq_line(T11_IRQ1,val); break;
		case T11_IRQ2_STATE: t11_set_irq_line(T11_IRQ2,val); break;
		case T11_IRQ3_STATE: t11_set_irq_line(T11_IRQ3,val); break;
		case T11_BANK0: t11.bank[0] = &OP_RAM[val]; break;
		case T11_BANK1: t11.bank[1] = &OP_RAM[val]; break;
		case T11_BANK2: t11.bank[2] = &OP_RAM[val]; break;
		case T11_BANK3: t11.bank[3] = &OP_RAM[val]; break;
		case T11_BANK4: t11.bank[4] = &OP_RAM[val]; break;
		case T11_BANK5: t11.bank[5] = &OP_RAM[val]; break;
		case T11_BANK6: t11.bank[6] = &OP_RAM[val]; break;
		case T11_BANK7: t11.bank[7] = &OP_RAM[val]; break;
		default:
			if( regnum < REG_SP_CONTENTS )
			{
				unsigned offset = SPD + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < 0xffff )
					WWORD( offset, val & 0xffff );
			}
    }
}

/****************************************************************************
 * Sets the banking
 ****************************************************************************/
void t11_SetBank(int offset, unsigned char *base)
{
	t11.bank[offset >> 13] = base;
}


void t11_reset(void *param)
{
	int i;

	memset(&t11, 0, sizeof(t11));
	SP = 0x0400;
	PC = 0x8000;
	PSW = 0xe0;

	for (i = 0; i < 8; i++)
		t11.bank[i] = &OP_RAM[i * 0x2000];
	for (i = 0; i < 4; i++)
		t11.irq_state[i] = CLEAR_LINE;
}

void t11_exit(void)
{
	/* nothing to do */
}

void t11_set_nmi_line(int state)
{
	/* T-11 has no dedicated NMI line */
}

void t11_set_irq_line(int irqline, int state)
{
    t11.irq_state[irqline] = state;
    if (state != CLEAR_LINE)
    	t11_check_irqs();
}

void t11_set_irq_callback(int (*callback)(int irqline))
{
	t11.irq_callback = callback;
}


/* execute instructions on this CPU until icount expires */
int t11_execute(int cycles)
{
	t11_ICount = cycles;
	t11_ICount -= t11.interrupt_cycles;
	t11.interrupt_cycles = 0;

	if (t11.wait_state)
	{
		t11_ICount = 0;
		goto getout;
	}

change_pc(0xffff);
	do
	{
		t11.ppc = t11.reg[7];	/* copy PC to previous PC */

		t11.op = ROPCODE();
		(*opcode_table[t11.op >> 3])();

		t11_ICount -= 22;

	} while (t11_ICount > 0);

getout:

	t11_ICount -= t11.interrupt_cycles;
	t11.interrupt_cycles = 0;

	return cycles - t11_ICount;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *t11_info( void *context, int regnum )
{
    switch( regnum )
	{
		case CPU_INFO_NAME: return "T11";
		case CPU_INFO_FAMILY: return "DEC T-11";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) Aaron Giles 1998";
    }
	return "";
}

unsigned t11_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%04X", cpu_readmem16lew_word(pc) );
	return 2;
}

