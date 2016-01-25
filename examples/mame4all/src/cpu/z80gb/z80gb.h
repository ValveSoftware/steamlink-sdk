#ifndef z80gb_H
#define z80gb_H
#include "cpuintrf.h"
#include "osd_cpu.h"
#include "mess/machine/gb.h"

extern int z80gb_ICount;

enum {
	Z80GB_PC=1, Z80GB_SP, Z80GB_AF, Z80GB_BC, Z80GB_DE, Z80GB_HL,
	Z80GB_IRQ_STATE
};

#define Z80GB_IGNORE_INT  -1    /* Ignore interrupt */

/*** Reset Z80 registers: *********************************/
/*** This function can be used to reset the register    ***/
/*** file before starting execution with Z80(). It sets ***/
/*** the registers to their initial values.             ***/
/**********************************************************/
extern void z80gb_reset (void *param);

/*** Shutdown CPU core:   *********************************/
/*** Could be used to free dynamically allocated memory ***/
/**********************************************************/
extern void z80gb_exit (void);

/**********************************************************/
/*** Execute z80gb code for cycles cycles, return nr of ***/
/*** cycles actually executed.                          ***/
/**********************************************************/
extern int z80gb_execute(int cycles);

/*** Burn CPU cycles: *************************************/
/*** called whenever the MAME core want's to bur cycles ***/
/**********************************************************/
extern void z80gb_burn(int cycles);

/****************************************************************************/
/* Set all registers to given values                                        */
/****************************************************************************/
extern void z80gb_set_context (void *src);

/****************************************************************************/
/* Get all registers in given buffer, return size of the context structure	*/
/****************************************************************************/
extern unsigned z80gb_get_context (void *dst);

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
extern unsigned z80gb_get_pc (void);

/****************************************************************************/
/* Set program counter														*/
/****************************************************************************/
extern void z80gb_set_pc (unsigned val);

/****************************************************************************/
/* Return stack pointer 													*/
/****************************************************************************/
extern unsigned z80gb_get_sp (void);

/****************************************************************************/
/* Set stack pointer														*/
/****************************************************************************/
extern void z80gb_set_sp (unsigned val);

/****************************************************************************/
/* Return register contents 												*/
/****************************************************************************/
extern unsigned z80gb_get_reg (int regnum);

/****************************************************************************/
/* Set register contents													*/
/****************************************************************************/
extern void z80gb_set_reg (int regnum, unsigned val);

/****************************************************************************/
/* MAME interrupt interfacing												*/
/****************************************************************************/
extern void z80gb_set_nmi_line(int state);
extern void z80gb_set_irq_line(int irqline, int state);
extern void z80gb_set_irq_callback(int (*irq_callback)(int));

/****************************************************************************/
/* State saving/loading 													*/
/****************************************************************************/
extern void z80gb_state_save(void *file);
extern void z80gb_state_load(void *file);

/****************************************************************************/
/* Information                                                              */
/****************************************************************************/
extern const char *z80gb_info(void *context, int regnum);

/****************************************************************************/
/* Disassembler 															*/
/****************************************************************************/
extern unsigned z80gb_dasm(char *buffer, unsigned pc);

/****************************************************************************/
/* Memory functions                                                         */
/****************************************************************************/

#define mem_ReadByte(A)    ((UINT8)cpu_readmem16(A))
#define mem_WriteByte(A,V) (cpu_writemem16(A,V))

INLINE UINT16 mem_ReadWord (UINT32 address)
{
	UINT16 value = (UINT16) mem_ReadByte ((address + 1) & 0xffff) << 8;
	value |= mem_ReadByte (address);
	return value;
}

INLINE void mem_WriteWord (UINT32 address, UINT16 value)
{
  mem_WriteByte (address, value & 0xFF);
  mem_WriteByte ((address + 1) & 0xffff, value >> 8);
}

#endif
