#ifndef __V33INTRF_H_
#define __V33INTRF_H_

#include "memory.h"
#include "osd_cpu.h"

#include "i86intrf.h"
#include "v30intrf.h"

#define V33_INT_NONE I86_INT_NONE
#define V33_NMI_INT I86_NMI_INT

/* Public variables */
#define v33_ICount i86_ICount

/* Public functions */
#define v33_reset v30_reset
#define v33_exit i86_exit
#define v33_execute v30_execute
#define v33_get_context i86_get_context
#define v33_set_context i86_set_context
#define v33_get_pc i86_get_pc
#define v33_set_pc i86_set_pc
#define v33_get_sp i86_get_sp
#define v33_set_sp i86_set_sp
#define v33_get_reg i86_get_reg
#define v33_set_reg i86_set_reg
#define v33_set_nmi_line i86_set_nmi_line
#define v33_set_irq_line i86_set_irq_line
#define v33_set_irq_callback i86_set_irq_callback
extern const char *v33_info(void *context, int regnum);
#define v33_dasm v30_dasm

#endif
