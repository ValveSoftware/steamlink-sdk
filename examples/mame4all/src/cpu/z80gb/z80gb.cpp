/*************************************************************/
/**                                                         **/
/** 						 z80gb.c						**/
/**                                                         **/
/** This file contains implementation for the GameBoy CPU.  **/
/** See z80gb.h for the relevant definitions. Please, note	**/
/** that this code can not be used to emulate a generic Z80 **/
/** because the GameBoy version of it differs from Z80 in   **/
/** many ways.                                              **/
/**                                                         **/
/** Orginal cpu code (PlayBoy)	Carsten Sorensen	1998	**/
/** MESS modifications			Hans de Goede		1998	**/
/** Adapted to new cpuintrf 	Juergen Buchmueller 2000	**/
/**                                                         **/
/*************************************************************/
#include <stdio.h>
#include <string.h>
#include "z80gb.h"
#include "daa_tab.h"
#include "state.h"

#define FLAG_Z	0x80
#define FLAG_N  0x40
#define FLAG_H  0x20
#define FLAG_C  0x10

/* Nr of cycles to run */
extern int z80gb_ICount;

typedef struct {
	UINT16 AF;
	UINT16 BC;
	UINT16 DE;
	UINT16 HL;

	UINT16 SP;
	UINT16 PC;
	int enable;
	int irq_state;
	int (*irq_callback)(int irqline);
} z80gb_16BitRegs;

#ifdef LSB_FIRST
typedef struct {
	UINT8 F;
	UINT8 A;
	UINT8 C;
	UINT8 B;
	UINT8 E;
	UINT8 D;
	UINT8 L;
	UINT8 H;
} z80gb_8BitRegs;
#else
typedef struct {
	UINT8 A;
	UINT8 F;
	UINT8 B;
	UINT8 C;
	UINT8 D;
	UINT8 E;
	UINT8 H;
	UINT8 L;
} z80gb_8BitRegs;
#endif

typedef union {
	z80gb_16BitRegs w;
	z80gb_8BitRegs b;
} z80gb_regs;

typedef int (*OpcodeEmulator) (void);

static z80gb_regs Regs;
static UINT8 ICycles;
static UINT8 CheckInterrupts;

#define IME     0x01
#define HALTED	0x02

/* Nr of cycles to run */
int z80gb_ICount;

static int Cycles[256] =
{
	 4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,
	 4,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	 8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	 8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	 8,12,12,12,12,16, 8,16, 8, 8,12, 0,12,24, 8,16,
	 8,12,12, 4,12,16, 8,16, 8,16,12, 4,12, 4, 8,16,
	12,12, 8, 4, 4,16, 8,16,16, 4,16, 4, 4, 4, 8,16,
	12,12, 8, 4, 4,16, 8,16,12, 8,16, 4, 4, 4, 8,16
};

static int CyclesCB[256] =
{
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
	 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8
};

/*** Reset Z80 registers: *********************************/
/*** This function can be used to reset the register    ***/
/*** file before starting execution with z80gb_execute()***/
/*** It sets the registers to their initial values.     ***/
/**********************************************************/
void z80gb_reset (void *param)
{
	Regs.w.AF = 0x01B0;
	Regs.w.BC = 0x0013;
	Regs.w.DE = 0x00D8;
	Regs.w.HL = 0x014D;
	Regs.w.SP = 0xFFFE;
	Regs.w.PC = 0x0100;
	Regs.w.enable &= ~IME;

//FIXME, use this in gb_machine_init!     state->TimerShift=32;
	CheckInterrupts = 0;
}

void z80gb_exit(void)
{
}

INLINE void z80gb_ProcessInterrupts (void)
{
	if (CheckInterrupts && (Regs.w.enable & IME))
	{
		UINT8 irq;

		CheckInterrupts = 0;

		irq = ISWITCH & IFLAGS;
		if (irq)
		{
			int irqline = 0;
			//logerror("Z80GB Interrupt IRQ $%02X\n", irq);

			while( irqline < 5 )
			{
				if( irq & (1<<irqline) )
				{
					if( Regs.w.irq_callback )
						(*Regs.w.irq_callback)(irqline);
                    if (Regs.w.enable & HALTED)
					{
						Regs.w.enable &= ~HALTED;
						Regs.w.PC += 1;
					}
					Regs.w.enable &= ~IME;
					IFLAGS &= ~(1 << irqline);
					ICycles += 20;
					Regs.w.SP -= 2;
					mem_WriteWord (Regs.w.SP, Regs.w.PC);
					Regs.w.PC = 0x40 + irqline * 8;
					//logerror("Z80GB Interrupt PC $%04X\n", Regs.w.PC );
					return;
				}
				irqline++;
			}
		}
	}
}

/**********************************************************/
/*** Execute z80gb code for cycles cycles, return nr of ***/
/*** cycles actually executed.                          ***/
/**********************************************************/
int z80gb_execute (int cycles)
{
	UINT8 x;

	z80gb_ICount = cycles;

	do
	{
		ICycles = 0;
		z80gb_ProcessInterrupts ();
		x = mem_ReadByte (Regs.w.PC++);
		ICycles += Cycles[x];
		switch (x)
		{
#include "opc_main.h"
		}
		z80gb_ICount -= ICycles;
		gb_divcount += ICycles;
		if (TIMEFRQ & 0x04)
		{
			gb_timer_count += ICycles;
			if (gb_timer_count & (0xFF00 << gb_timer_shift))
			{
				gb_timer_count = TIMEMOD << gb_timer_shift;
				IFLAGS |= TIM_IFLAG;
				CheckInterrupts = 1;
			}
		}
	} while (z80gb_ICount > 0);

	return cycles - z80gb_ICount;
}

void z80gb_burn(int cycles)
{
    if( cycles > 0 )
    {
        /* NOP takes 4 cycles per instruction */
        int n = (cycles + 3) / 4;
        z80gb_ICount -= 4 * n;
    }
}

/****************************************************************************/
/* Set all registers to given values                                        */
/****************************************************************************/
void z80gb_set_context (void *src)
{
	if( src )
		Regs = *(z80gb_regs *)src;
	change_pc(Regs.w.PC);
}

/****************************************************************************/
/* Get all registers in given buffer                                        */
/****************************************************************************/
unsigned z80gb_get_context (void *dst)
{
	if( dst )
		*(z80gb_regs *)dst = Regs;
	return sizeof(z80gb_regs);
}

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
unsigned z80gb_get_pc (void)
{
	return Regs.w.PC;
}

void z80gb_set_pc (unsigned pc)
{
	Regs.w.PC = pc;
	change_pc(Regs.w.PC);
}

unsigned z80gb_get_sp (void)
{
	return Regs.w.SP;
}

void z80gb_set_sp (unsigned sp)
{
	Regs.w.SP = sp;
}

unsigned z80gb_get_reg (int regnum)
{
	switch( regnum )
	{
	case Z80GB_PC: return Regs.w.PC;
	case Z80GB_SP: return Regs.w.SP;
	case Z80GB_AF: return Regs.w.AF;
	case Z80GB_BC: return Regs.w.BC;
	case Z80GB_DE: return Regs.w.DE;
	case Z80GB_HL: return Regs.w.HL;
	}
	return 0;
}

void z80gb_set_reg (int regnum, unsigned val)
{
	switch( regnum )
	{
	case Z80GB_PC: Regs.w.PC = val; break;
	case Z80GB_SP: Regs.w.SP = val; break;
	case Z80GB_AF: Regs.w.AF = val; break;
	case Z80GB_BC: Regs.w.BC = val; break;
	case Z80GB_DE: Regs.w.DE = val; break;
	case Z80GB_HL: Regs.w.HL = val; break;
    }
}

void z80gb_set_nmi_line(int state)
{
}

void z80gb_set_irq_line (int irqline, int state)
{
	if( Regs.w.irq_state == state )
		return;

	Regs.w.irq_state = state;
	if( state == ASSERT_LINE )
	{
		IFLAGS |= 0x01 << irqline;
		CheckInterrupts = 1;
		//logerror("Z80GB assert irq line %d ($%02X)\n", irqline, IFLAGS);
	}
	else
	{
		IFLAGS &= ~(0x01 << irqline);
		if( IFLAGS == 0 )
			CheckInterrupts = 0;
		//logerror("Z80GB clear irq line %d ($%02X)\n", irqline, IFLAGS);
    }
}

void z80gb_set_irq_callback(int (*irq_callback)(int))
{
	Regs.w.irq_callback = irq_callback;
}

void z80gb_clear_pending_interrupts (void)
{
	IFLAGS = 0;
	CheckInterrupts = 0;
}

/****************************************************************************
 * Save CPU state
 ****************************************************************************/
void z80gb_state_save(void *file)
{
	int cpu = cpu_getactivecpu();
	state_save_UINT16(file, "z80gb", cpu, "AF", &Regs.w.AF, 1);
	state_save_UINT16(file, "z80gb", cpu, "BC", &Regs.w.BC, 1);
	state_save_UINT16(file, "z80gb", cpu, "DE", &Regs.w.DE, 1);
	state_save_UINT16(file, "z80gb", cpu, "HL", &Regs.w.HL, 1);
	state_save_UINT16(file, "z80gb", cpu, "PC", &Regs.w.PC, 1);
	state_save_UINT16(file, "z80gb", cpu, "SP", &Regs.w.SP, 1);
	state_save_INT32(file, "z80gb", cpu, "irq_state", &Regs.w.irq_state, 1);
}

/****************************************************************************
 * Load CPU state
 ****************************************************************************/
void z80gb_state_load(void *file)
{
	int cpu = cpu_getactivecpu();
	state_load_UINT16(file, "z80gb", cpu, "AF", &Regs.w.AF, 1);
	state_load_UINT16(file, "z80gb", cpu, "BC", &Regs.w.BC, 1);
	state_load_UINT16(file, "z80gb", cpu, "DE", &Regs.w.DE, 1);
	state_load_UINT16(file, "z80gb", cpu, "HL", &Regs.w.HL, 1);
	state_load_UINT16(file, "z80gb", cpu, "PC", &Regs.w.PC, 1);
	state_load_UINT16(file, "z80gb", cpu, "SP", &Regs.w.SP, 1);
	state_load_INT32(file, "z80gb", cpu, "irq_state", &Regs.w.irq_state, 1);
}

const char *z80gb_info(void *context, int regnum)
{
    switch( regnum )
	{
		case CPU_INFO_NAME: return "Z80GB";
		case CPU_INFO_FAMILY: return "Nintendo Z80";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) 2000 by The MESS Team.";
	}
	return "";
}

unsigned z80gb_dasm( char *buffer, unsigned pc )
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}


