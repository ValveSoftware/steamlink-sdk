/*###################################################################################################
**
**
**		ADSP2100.h
**		Interface file for the portable Analog ADSP-2100 emulator.
**		Written by Aaron Giles
**
**
**#################################################################################################*/

#ifndef _ADSP2100_H
#define _ADSP2100_H

#include "memory.h"
#include "osd_cpu.h"


/*###################################################################################################
**	COMPILE-TIME DEFINITIONS
**#################################################################################################*/

/* turn this on to support 2101 and later CPUs (not tested at all!) */
#define SUPPORT_2101_EXTENSIONS 1


/*###################################################################################################
**	GLOBAL CONSTANTS
**#################################################################################################*/

#define ADSP2100_DATA_OFFSET	0x00000
#define ADSP2100_PGM_OFFSET		0x10000
#define ADSP2100_SIZE			0x20000

/* transmit and receive data callbacks types */
#if SUPPORT_2101_EXTENSIONS
typedef INT32 (*RX_CALLBACK)( int port );
typedef void  (*TX_CALLBACK)( int port, INT32 data );
#endif


/*###################################################################################################
**	MEMORY MAP MACROS
**#################################################################################################*/

#define ADSP_DATA_ADDR_RANGE(start, end) (ADSP2100_DATA_OFFSET + ((start) << 1)), (ADSP2100_DATA_OFFSET + ((end) << 1) + 1)
#define ADSP_PGM_ADDR_RANGE(start, end)  (ADSP2100_PGM_OFFSET + ((start) << 2)), (ADSP2100_PGM_OFFSET + ((end) << 2) + 3)


/*###################################################################################################
**	REGISTER ENUMERATION
**#################################################################################################*/

enum 
{
	ADSP2100_PC=1, 
	ADSP2100_AX0, ADSP2100_AX1, ADSP2100_AY0, ADSP2100_AY1, ADSP2100_AR, ADSP2100_AF,
	ADSP2100_MX0, ADSP2100_MX1, ADSP2100_MY0, ADSP2100_MY1, ADSP2100_MR0, ADSP2100_MR1, ADSP2100_MR2, ADSP2100_MF,
	ADSP2100_SI, ADSP2100_SE, ADSP2100_SB, ADSP2100_SR0, ADSP2100_SR1,
	ADSP2100_I0, ADSP2100_I1, ADSP2100_I2, ADSP2100_I3, ADSP2100_I4, ADSP2100_I5, ADSP2100_I6, ADSP2100_I7,
	ADSP2100_L0, ADSP2100_L1, ADSP2100_L2, ADSP2100_L3, ADSP2100_L4, ADSP2100_L5, ADSP2100_L6, ADSP2100_L7,
	ADSP2100_M0, ADSP2100_M1, ADSP2100_M2, ADSP2100_M3, ADSP2100_M4, ADSP2100_M5, ADSP2100_M6, ADSP2100_M7,
	ADSP2100_PX, ADSP2100_CNTR, ADSP2100_ASTAT, ADSP2100_SSTAT, ADSP2100_MSTAT,
	ADSP2100_PCSP, ADSP2100_CNTRSP, ADSP2100_STATSP, ADSP2100_LOOPSP,
	ADSP2100_IMASK, ADSP2100_ICNTL, ADSP2100_IRQSTATE0, ADSP2100_IRQSTATE1, ADSP2100_IRQSTATE2, ADSP2100_IRQSTATE3,
	ADSP2100_FLAGIN, ADSP2100_FLAGOUT
#if SUPPORT_2101_EXTENSIONS
	, ADSP2100_FL0, ADSP2100_FL1, ADSP2100_FL2 
#endif
};
	

/*###################################################################################################
**	INTERRUPT CONSTANTS
**#################################################################################################*/

#define ADSP2100_INT_NONE	-1		/* No interrupt requested */
#define ADSP2100_IRQ0		0		/* IRQ0 */
#define ADSP2100_IRQ1		1		/* IRQ1 */
#define ADSP2100_IRQ2		2		/* IRQ2 */
#define ADSP2100_IRQ3		3		/* IRQ3 */


/*###################################################################################################
**	PUBLIC GLOBALS
**#################################################################################################*/

extern int adsp2100_icount;


/*###################################################################################################
**	PUBLIC FUNCTIONS
**#################################################################################################*/

extern void adsp2100_reset(void *param);
extern void adsp2100_exit(void);
extern int adsp2100_execute(int cycles);    /* NS 970908 */
extern unsigned adsp2100_get_context(void *dst);
extern void adsp2100_set_context(void *src);
extern unsigned adsp2100_get_pc(void);
extern void adsp2100_set_pc(unsigned val);
extern unsigned adsp2100_get_sp(void);
extern void adsp2100_set_sp(unsigned val);
extern unsigned adsp2100_get_reg(int regnum);
extern void adsp2100_set_reg(int regnum, unsigned val);
extern void adsp2100_set_nmi_line(int state);
extern void adsp2100_set_irq_line(int irqline, int state);
extern void adsp2100_set_irq_callback(int (*callback)(int irqline));
extern const char *adsp2100_info(void *context, int regnum);
extern unsigned adsp2100_dasm(char *buffer, unsigned pc);


/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
#define ADSP2100_RDMEM(A) ((unsigned)cpu_readmem16lew(A))
#define ADSP2100_RDMEM_WORD(A) ((unsigned)cpu_readmem16lew_word(A))

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
#define ADSP2100_WRMEM(A,V) (cpu_writemem16lew(A,V))
#define ADSP2100_WRMEM_WORD(A,V) (cpu_writemem16lew_word(A,V))

#if (HAS_ADSP2105)
/**************************************************************************
 * ADSP2105 section
 **************************************************************************/

#define adsp2105_icount adsp2100_icount

#define ADSP2105_INT_NONE	-1		/* No interrupt requested */
#define ADSP2105_IRQ0		0		/* IRQ0 */
#define ADSP2105_IRQ1		1		/* IRQ1 */
#define ADSP2105_IRQ2		2		/* IRQ2 */

extern void adsp2105_reset(void *param);
extern void adsp2105_exit(void);
extern int adsp2105_execute(int cycles);    /* NS 970908 */
extern unsigned adsp2105_get_context(void *dst);
extern void adsp2105_set_context(void *src);
extern unsigned adsp2105_get_pc(void);
extern void adsp2105_set_pc(unsigned val);
extern unsigned adsp2105_get_sp(void);
extern void adsp2105_set_sp(unsigned val);
extern unsigned adsp2105_get_reg(int regnum);
extern void adsp2105_set_reg(int regnum, unsigned val);
extern void adsp2105_set_nmi_line(int state);
extern void adsp2105_set_irq_line(int irqline, int state);
extern void adsp2105_set_irq_callback(int (*callback)(int irqline));
extern const char *adsp2105_info(void *context, int regnum);
extern unsigned adsp2105_dasm(char *buffer, unsigned pc);

#if SUPPORT_2101_EXTENSIONS
extern void adsp2105_set_rx_callback( RX_CALLBACK cb );
extern void adsp2105_set_tx_callback( TX_CALLBACK cb );
#endif
#endif

#endif /* _ADSP2100_H */
