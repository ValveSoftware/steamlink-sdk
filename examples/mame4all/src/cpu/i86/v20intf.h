/* ASG 971222 -- rewrote this interface */
#ifndef __V20INTRF_H_
#define __V20INTRF_H_

#include "memory.h"
#include "osd_cpu.h"

#include "i86intrf.h"
#include "v30intrf.h"

#define V20_INT_NONE I86_INT_NONE
#define V20_NMI_INT I86_NMI_INT

/* Public variables */
#define v20_ICount i86_ICount

/* Public functions */
#define v20_reset v30_reset
#define v20_exit i86_exit
#define v20_execute v30_execute
#define v20_get_context i86_get_context
#define v20_set_context i86_set_context
#define v20_get_pc i86_get_pc
#define v20_set_pc i86_set_pc
#define v20_get_sp i86_get_sp
#define v20_set_sp i86_set_sp
#define v20_get_reg i86_get_reg
#define v20_set_reg i86_set_reg
#define v20_set_nmi_line i86_set_nmi_line
#define v20_set_irq_line i86_set_irq_line
#define v20_set_irq_callback i86_set_irq_callback
extern const char *v20_info(void *context, int regnum);
#define v20_dasm v30_dasm

#endif
