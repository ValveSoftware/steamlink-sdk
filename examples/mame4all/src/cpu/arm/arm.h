/****************************************************************************************************
 *
 *
 *		arm.h
 *		Interface file for the portable ARM emulator.
 *
 *
 ***************************************************************************************************/

#ifndef _ARM_H
#define _ARM_H

#include "driver.h"

/****************************************************************************************************
 *	COMPILE-TIME DEFINITIONS
 ***************************************************************************************************/

/****************************************************************************************************
 *	GLOBAL CONSTANTS
 ***************************************************************************************************/

#define ARM_DATA_OFFSET    0x00000
#define ARM_PGM_OFFSET	   0x10000
#define ARM_SIZE		   0x20000


/****************************************************************************************************
 *	MEMORY MAP MACROS
 ***************************************************************************************************/

#define ADSP_DATA_ADDR_RANGE(start, end) (ARM_DATA_OFFSET + ((start) << 1)), (ARM_DATA_OFFSET + ((end) << 1) + 1)
#define ADSP_PGM_ADDR_RANGE(start, end)  (ARM_PGM_OFFSET + ((start) << 2)), (ARM_PGM_OFFSET + ((end) << 2) + 3)


/****************************************************************************************************
 *	REGISTER ENUMERATION
 ***************************************************************************************************/

enum 
{
	ARM_OP=1, ARM_Q1, ARM_Q2, ARM_PSW,
	ARM_R0, ARM_R1, ARM_R2, ARM_R3, ARM_R4, ARM_R5, ARM_R6, ARM_R7,
	ARM_R8, ARM_R9, ARM_R10, ARM_R11, ARM_R12, ARM_R13, ARM_R14, ARM_R15,
	ARM_FR8, ARM_FR9, ARM_FR10, ARM_FR11, ARM_FR12, ARM_FR13, ARM_FR14,
	ARM_IR13, ARM_IR14, ARM_SR13, ARM_SR14
};
	

/****************************************************************************************************
 *	INTERRUPT CONSTANTS
 ***************************************************************************************************/

#define ARM_INT_NONE   0	/* No interrupt requested */
#define ARM_FIRQ	   1	/* FIRQ */
#define ARM_IRQ 	   2	/* IRQ */
#define ARM_SVC 	   3	/* SVC */

/****************************************************************************************************
 *	PUBLIC GLOBALS
 ***************************************************************************************************/

extern int arm_ICount;

/****************************************************************************************************
 *	PUBLIC FUNCTIONS
 ***************************************************************************************************/

extern void arm_reset(void *param);
extern void arm_exit(void);
extern int arm_execute(int cycles);
extern unsigned arm_get_context(void *dst);
extern void arm_set_context(void *src);
extern unsigned arm_get_pc(void);
extern void arm_set_pc(unsigned val);
extern unsigned arm_get_sp(void);
extern void arm_set_sp(unsigned val);
extern unsigned arm_get_reg(int regnum);
extern void arm_set_reg(int regnum, unsigned val);
extern void arm_set_nmi_line(int state);
extern void arm_set_irq_line(int irqline, int state);
extern void arm_set_irq_callback(int (*callback)(int irqline));
extern const char *arm_info(void *context, int regnum);
extern unsigned arm_dasm(char *buffer, unsigned pc);

#endif /* _ARM_H */

