/**************************************************************************
 *                      Intel 8039 Portable Emulator                      *
 *                                                                        *
 *                   Copyright (C) 1997 by Mirko Buffoni                  *
 *  Based on the original work (C) 1997 by Dan Boris, an 8048 emulator    *
 *     You are not allowed to distribute this software commercially       *
 *        Please, notify me, if you make any changes to this file         *
 **************************************************************************/

#ifndef _I8039_H
#define _I8039_H

#ifndef INLINE
#define INLINE static inline
#endif

#include "osd_cpu.h"

enum { I8039_PC=1, I8039_SP, I8039_PSW, I8039_A, I8039_IRQ_STATE,
	I8039_R0, I8039_R1, I8039_R2, I8039_R3, I8039_R4, I8039_R5, I8039_R6, I8039_R7 };

extern int i8039_ICount;        /* T-state count                          */

/* HJB 01/05/99 changed to positive values to use pending_irq as a flag */
#define I8039_IGNORE_INT    0   /* Ignore interrupt                     */
#define I8039_EXT_INT		1	/* Execute a normal extern interrupt	*/
#define I8039_TIMER_INT 	2	/* Execute a Timer interrupt			*/
#define I8039_COUNT_INT 	4	/* Execute a Counter interrupt			*/

extern void i8039_reset(void *param);			/* Reset processor & registers	*/
extern void i8039_exit(void);					/* Shut down CPU emulation		*/
extern int i8039_execute(int cycles);			/* Execute cycles T-States - returns number of cycles actually run */
extern unsigned i8039_get_context(void *dst);	/* Get registers				*/
extern void i8039_set_context(void *src);		/* Set registers				*/
extern unsigned i8039_get_pc(void); 			/* Get program counter			*/
extern void i8039_set_pc(unsigned val); 		/* Set program counter			*/
extern unsigned i8039_get_sp(void); 			/* Get stack pointer			*/
extern void i8039_set_sp(unsigned val); 		/* Set stack pointer			*/
extern unsigned i8039_get_reg(int regnum);		/* Get specific register	  */
extern void i8039_set_reg(int regnum, unsigned val);    /* Set specific register 	 */
extern void i8039_set_nmi_line(int state);
extern void i8039_set_irq_line(int irqline, int state);
extern void i8039_set_irq_callback(int (*callback)(int irqline));
extern const char *i8039_info(void *context, int regnum);
extern unsigned i8039_dasm(char *buffer, unsigned pc);

/*   This handling of special I/O ports should be better for actual MAME
 *   architecture.  (i.e., define access to ports { I8039_p1, I8039_p1, dkong_out_w })
 */

#if OLDPORTHANDLING
		UINT8	 I8039_port_r(UINT8 port);
		void	 I8039_port_w(UINT8 port, UINT8 data);
		UINT8	 I8039_test_r(UINT8 port);
		void	 I8039_test_w(UINT8 port, UINT8 data);
		UINT8	 I8039_bus_r(void);
		void	 I8039_bus_w(UINT8 data);
#else
        #define  I8039_p0	0x100   /* Not used */
        #define  I8039_p1	0x101
        #define  I8039_p2	0x102
        #define  I8039_p4	0x104
        #define  I8039_p5	0x105
        #define  I8039_p6	0x106
        #define  I8039_p7	0x107
        #define  I8039_t0	0x110
        #define  I8039_t1	0x111
        #define  I8039_bus	0x120
#endif

/**************************************************************************
 * I8035 section
 **************************************************************************/
#if (HAS_I8035)
#define I8035_PC				I8039_PC
#define I8035_SP				I8039_SP
#define I8035_PSW				I8039_PSW
#define I8035_A 				I8039_A
#define I8035_IRQ_STATE 		I8039_IRQ_STATE
#define I8035_R0				I8039_R0
#define I8035_R1				I8039_R1
#define I8035_R2				I8039_R2
#define I8035_R3				I8039_R3
#define I8035_R4				I8039_R4
#define I8035_R5				I8039_R5
#define I8035_R6				I8039_R6
#define I8035_R7				I8039_R7

#define I8035_IGNORE_INT        I8039_IGNORE_INT
#define I8035_EXT_INT           I8039_EXT_INT
#define I8035_TIMER_INT         I8039_TIMER_INT
#define I8035_COUNT_INT         I8039_COUNT_INT
#define I8035_IRQ_STATE 		I8039_IRQ_STATE

#define i8035_ICount            i8039_ICount

extern void i8035_reset(void *param);
extern void i8035_exit(void);
extern int i8035_execute(int cycles);
extern unsigned i8035_get_context(void *dst);
extern void i8035_set_context(void *src);
extern unsigned i8035_get_pc(void);
extern void i8035_set_pc(unsigned val);
extern unsigned i8035_get_sp(void);
extern void i8035_set_sp(unsigned val);
extern unsigned i8035_get_reg(int regnum);
extern void i8035_set_reg(int regnum, unsigned val);
extern void i8035_set_nmi_line(int state);
extern void i8035_set_irq_line(int irqline, int state);
extern void i8035_set_irq_callback(int (*callback)(int irqline));
extern const char *i8035_info(void *context, int regnum);
extern unsigned i8035_dasm(char *buffer, unsigned pc);
#endif

/**************************************************************************
 * I8048 section
 **************************************************************************/
#if (HAS_I8048)
#define I8048_PC				I8039_PC
#define I8048_SP				I8039_SP
#define I8048_PSW				I8039_PSW
#define I8048_A 				I8039_A
#define I8048_IRQ_STATE 		I8039_IRQ_STATE
#define I8048_R0				I8039_R0
#define I8048_R1				I8039_R1
#define I8048_R2				I8039_R2
#define I8048_R3				I8039_R3
#define I8048_R4				I8039_R4
#define I8048_R5				I8039_R5
#define I8048_R6				I8039_R6
#define I8048_R7				I8039_R7

#define I8048_IGNORE_INT        I8039_IGNORE_INT
#define I8048_EXT_INT           I8039_EXT_INT
#define I8048_TIMER_INT         I8039_TIMER_INT
#define I8048_COUNT_INT         I8039_COUNT_INT

#define i8048_ICount            i8039_ICount

extern void i8048_reset(void *param);
extern void i8048_exit(void);
extern int i8048_execute(int cycles);
extern unsigned i8048_get_context(void *dst);
extern void i8048_set_context(void *src);
extern unsigned i8048_get_pc(void);
extern void i8048_set_pc(unsigned val);
extern unsigned i8048_get_sp(void);
extern void i8048_set_sp(unsigned val);
extern unsigned i8048_get_reg(int regnum);
extern void i8048_set_reg(int regnum, unsigned val);
extern void i8048_set_nmi_line(int state);
extern void i8048_set_irq_line(int irqline, int state);
extern void i8048_set_irq_callback(int (*callback)(int irqline));
const char *i8048_info(void *context, int regnum);
extern unsigned i8048_dasm(char *buffer, unsigned pc);
#endif

/**************************************************************************
 * N7751 section
 **************************************************************************/
#if (HAS_N7751)
#define N7751_PC				I8039_PC
#define N7751_SP				I8039_SP
#define N7751_PSW				I8039_PSW
#define N7751_A 				I8039_A
#define N7751_IRQ_STATE 		I8039_IRQ_STATE
#define N7751_R0				I8039_R0
#define N7751_R1				I8039_R1
#define N7751_R2				I8039_R2
#define N7751_R3				I8039_R3
#define N7751_R4				I8039_R4
#define N7751_R5				I8039_R5
#define N7751_R6				I8039_R6
#define N7751_R7				I8039_R7

#define N7751_IGNORE_INT        I8039_IGNORE_INT
#define N7751_EXT_INT           I8039_EXT_INT
#define N7751_TIMER_INT         I8039_TIMER_INT
#define N7751_COUNT_INT         I8039_COUNT_INT

#define n7751_ICount            i8039_ICount

extern void n7751_reset(void *param);
extern void n7751_exit(void);
extern int n7751_execute(int cycles);
extern unsigned n7751_get_context(void *dst);
extern void n7751_set_context(void *src);
extern unsigned n7751_get_pc(void);
extern void n7751_set_pc(unsigned val);
extern unsigned n7751_get_sp(void);
extern void n7751_set_sp(unsigned val);
extern unsigned n7751_get_reg(int regnum);
extern void n7751_set_reg(int regnum, unsigned val);
extern void n7751_set_nmi_line(int state);
extern void n7751_set_irq_line(int irqline, int state);
extern void n7751_set_irq_callback(int (*callback)(int irqline));
extern const char *n7751_info(void *context, int regnum);
extern unsigned n7751_dasm(char *buffer, unsigned pc);
#endif

#include "memory.h"

/*
 *	 Input a UINT8 from given I/O port
 */
#define I8039_In(Port) ((UINT8)cpu_readport(Port))


/*
 *	 Output a UINT8 to given I/O port
 */
#define I8039_Out(Port,Value) (cpu_writeport(Port,Value))


/*
 *	 Read a UINT8 from given memory location
 */
#define I8039_RDMEM(A) ((unsigned)cpu_readmem16(A))


/*
 *	 Write a UINT8 to given memory location
 */
#define I8039_WRMEM(A,V) (cpu_writemem16(A,V))


/*
 *   I8039_RDOP() is identical to I8039_RDMEM() except it is used for reading
 *   opcodes. In case of system with memory mapped I/O, this function can be
 *   used to greatly speed up emulation
 */
#define I8039_RDOP(A) ((unsigned)cpu_readop(A))


/*
 *   I8039_RDOP_ARG() is identical to I8039_RDOP() except it is used for reading
 *   opcode arguments. This difference can be used to support systems that
 *   use different encoding mechanisms for opcodes and opcode arguments
 */
#define I8039_RDOP_ARG(A) ((unsigned)cpu_readop_arg(A))

#endif  /* _I8039_H */
