#ifndef __I188INTR_H_
#define __I188INTR_H_

#include "memory.h"
#include "osd_cpu.h"

#include "i186intf.h"

#define I188_INT_NONE I86_INT_NONE
#define I188_NMI_INT I86_NMI_INT

/* Public variables */
#define i188_ICount i86_ICount

/* Public functions */
#define i188_reset i86_reset
#define i188_exit i86_exit
#define i188_execute i186_execute
#define i188_get_context i86_get_context
#define i188_set_context i86_set_context
#define i188_get_pc i86_get_pc
#define i188_set_pc i86_set_pc
#define i188_get_sp i86_get_sp
#define i188_set_sp i86_set_sp
#define i188_get_reg i86_get_reg
#define i188_set_reg i86_set_reg
#define i188_set_nmi_line i86_set_nmi_line
#define i188_set_irq_line i86_set_irq_line
#define i188_set_irq_callback i86_set_irq_callback
extern const char *i188_info(void *context, int regnum);
#define i188_dasm i186_dasm

#endif
