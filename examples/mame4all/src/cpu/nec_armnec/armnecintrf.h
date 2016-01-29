#ifndef NECARM_H
#define NECARM_H

#include "memory.h"
#include "osd_cpu.h"

/* registers */
enum {
	ARMNEC_IP=1, ARMNEC_AW, ARMNEC_CW, ARMNEC_DW, ARMNEC_BW,
	ARMNEC_SP, ARMNEC_BP, ARMNEC_IX, ARMNEC_IY,
	ARMNEC_FLAGS, ARMNEC_ES, ARMNEC_CS, ARMNEC_SS, ARMNEC_DS,
	ARMNEC_VECTOR, ARMNEC_PENDING, ARMNEC_NMI_STATE, ARMNEC_IRQ_STATE};

/* interrupt states */
#define ARMNEC_INT_NONE 0
#define ARMNEC_NMI_INT 2

/* cycles */
extern int *armnec_cycles;

/* declaration of core functions */
extern void armnec_init(void);
extern void armnec_reset(void *param);
extern void armnec_exit(void);
extern unsigned armnec_get_context(void *dst);
extern void armnec_set_context(void *src);
extern unsigned armnec_get_pc(void);
extern void armnec_set_pc(unsigned val);
extern unsigned armnec_get_sp(void);
extern void armnec_set_sp(unsigned val);
extern unsigned armnec_get_reg(int regnum);
extern void armnec_set_reg(int regnum, unsigned val);
extern void armnec_set_nmi_line(int state);
extern void armnec_set_irq_line(int irqline, int state);
extern void armnec_set_irq_callback(int (*callback)(int irqline));
extern unsigned armnec_dasm(char *buffer, unsigned pc);

/* V30 */
#define armv30_ICount (*armnec_cycles)
#define armv30_init armnec_init
#define armv30_reset armnec_reset
#define armv30_exit armnec_exit
extern int armv30_execute(int cycles);
#define armv30_get_context armnec_get_context
#define armv30_set_context armnec_set_context
#define armv30_get_pc armnec_get_pc
#define armv30_set_pc armnec_set_pc
#define armv30_get_sp armnec_get_sp
#define armv30_set_sp armnec_set_sp
#define armv30_get_reg armnec_get_reg
#define armv30_set_reg armnec_set_reg
#define armv30_set_nmi_line armnec_set_nmi_line
#define armv30_set_irq_line armnec_set_irq_line
#define armv30_set_irq_callback armnec_set_irq_callback
extern const char *armv30_info(void *context, int regnum);
#define armv30_dasm armnec_dasm

/* V33 */
#define armv33_ICount (*armnec_cycles)
#define armv30_init armnec_init
#define armv33_reset armnec_reset
#define armv33_exit armnec_exit
extern int armv33_execute(int cycles);
#define armv33_get_context armnec_get_context
#define armv33_set_context armnec_set_context
#define armv33_get_pc armnec_get_pc
#define armv33_set_pc armnec_set_pc
#define armv33_get_sp armnec_get_sp
#define armv33_set_sp armnec_set_sp
#define armv33_get_reg armnec_get_reg
#define armv33_set_reg armnec_set_reg
#define armv33_set_nmi_line armnec_set_nmi_line
#define armv33_set_irq_line armnec_set_irq_line
#define armv33_set_irq_callback armnec_set_irq_callback
extern const char *armv33_info(void *context, int regnum);
#define armv33_dasm armnec_dasm

#endif
