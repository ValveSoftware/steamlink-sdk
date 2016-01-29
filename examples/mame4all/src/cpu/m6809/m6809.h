/*** m6809: Portable 6809 emulator ******************************************/

#ifndef _M6809_H
#define _M6809_H

#include "memory.h"
#include "osd_cpu.h"

enum {
	M6809_PC=1, M6809_S, M6809_CC ,M6809_A, M6809_B, M6809_U, M6809_X, M6809_Y,
	M6809_DP, M6809_NMI_STATE, M6809_IRQ_STATE, M6809_FIRQ_STATE };

#define M6809_INT_NONE  0   /* No interrupt required */
#define M6809_INT_IRQ	1	/* Standard IRQ interrupt */
#define M6809_INT_FIRQ	2	/* Fast IRQ */
#define M6809_INT_NMI	4	/* NMI */
#define M6809_IRQ_LINE	0	/* IRQ line number */
#define M6809_FIRQ_LINE 1   /* FIRQ line number */

/* PUBLIC GLOBALS */
extern int  m6809_ICount;


/* PUBLIC FUNCTIONS */
extern void m6809_reset(void *param);
extern void m6809_exit(void);
extern int m6809_execute(int cycles);  /* NS 970908 */
extern unsigned m6809_get_context(void *dst);
extern void m6809_set_context(void *src);
extern unsigned m6809_get_pc(void);
extern void m6809_set_pc(unsigned val);
extern unsigned m6809_get_sp(void);
extern void m6809_set_sp(unsigned val);
extern unsigned m6809_get_reg(int regnum);
extern void m6809_set_reg(int regnum, unsigned val);
extern void m6809_set_nmi_line(int state);
extern void m6809_set_irq_line(int irqline, int state);
extern void m6809_set_irq_callback(int (*callback)(int irqline));
extern void m6809_state_save(void *file);
extern void m6809_state_load(void *file);
extern const char *m6809_info(void *context,int regnum);
extern unsigned m6809_dasm(char *buffer, unsigned pc);

/****************************************************************************/
/* For now the 6309 is using the functions of the 6809						*/
/****************************************************************************/
#if (HAS_HD6309)
#define M6309_A 				M6809_A
#define M6309_B 				M6809_B
#define M6309_PC				M6809_PC
#define M6309_S 				M6809_S
#define M6309_U 				M6809_U
#define M6309_X 				M6809_X
#define M6309_Y 				M6809_Y
#define M6309_CC				M6809_CC
#define M6309_DP				M6809_DP
#define M6309_NMI_STATE 		M6809_NMI_STATE
#define M6309_IRQ_STATE 		M6809_IRQ_STATE
#define M6309_FIRQ_STATE		M6809_FIRQ_STATE

#define HD6309_INT_NONE					M6809_INT_NONE
#define HD6309_INT_IRQ					M6809_INT_IRQ
#define HD6309_INT_FIRQ					M6809_INT_FIRQ
#define HD6309_INT_NMI					M6809_INT_NMI
#define M6309_IRQ_LINE					M6809_IRQ_LINE
#define M6309_FIRQ_LINE 				M6809_FIRQ_LINE

#define hd6309_ICount m6809_ICount
#define hd6309_reset m6809_reset
#define hd6309_exit m6809_exit
#define hd6309_execute m6809_execute
#define hd6309_get_context m6809_get_context
#define hd6309_set_context m6809_set_context
#define hd6309_get_pc m6809_get_pc
#define hd6309_set_pc m6809_set_pc
#define hd6309_get_sp m6809_get_sp
#define hd6309_set_sp m6809_set_sp
#define hd6309_get_reg m6809_get_reg
#define hd6309_set_reg m6809_set_reg
#define hd6309_set_nmi_line m6809_set_nmi_line
#define hd6309_set_irq_line m6809_set_irq_line
#define hd6309_set_irq_callback m6809_set_irq_callback
extern void hd6309_state_save(void *file);
extern void hd6309_state_load(void *file);
extern const char *hd6309_info(void *context,int regnum);
extern unsigned hd6309_dasm(char *buffer, unsigned pc);
#endif

/****************************************************************************/
/* Read a byte from given memory location									*/
/****************************************************************************/
/* ASG 971005 -- changed to cpu_readmem16/cpu_writemem16 */
#define M6809_RDMEM(Addr) ((unsigned)cpu_readmem16(Addr))

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
#define M6809_WRMEM(Addr,Value) (cpu_writemem16(Addr,Value))

/****************************************************************************/
/* Z80_RDOP() is identical to Z80_RDMEM() except it is used for reading     */
/* opcodes. In case of system with memory mapped I/O, this function can be  */
/* used to greatly speed up emulation                                       */
/****************************************************************************/
#define M6809_RDOP(Addr) ((unsigned)cpu_readop(Addr))

/****************************************************************************/
/* Z80_RDOP_ARG() is identical to Z80_RDOP() except it is used for reading  */
/* opcode arguments. This difference can be used to support systems that    */
/* use different encoding mechanisms for opcodes and opcode arguments       */
/****************************************************************************/
#define M6809_RDOP_ARG(Addr) ((unsigned)cpu_readop_arg(Addr))

#ifndef FALSE
#    define FALSE 0
#endif
#ifndef TRUE
#    define TRUE (!FALSE)
#endif

#endif /* _M6809_H */
