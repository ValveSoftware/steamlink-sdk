#ifndef _PDP1_H
#define _PDP1_H

#include "memory.h"
#include "osd_cpu.h"

enum {
	PDP1_PC=1, PDP1_AC, PDP1_IO, PDP1_Y, PDP1_IB, PDP1_OV, PDP1_F,
	PDP1_F1, PDP1_F2, PDP1_F3, PDP1_F4, PDP1_F5, PDP1_F6,
	PDP1_S1, PDP1_S2, PDP1_S3, PDP1_S4, PDP1_S5, PDP1_S6
};

#ifndef INLINE
#define INLINE static inline
#endif

/* PUBLIC FUNCTIONS */
extern unsigned pdp1_get_pc(void);
extern void pdp1_set_pc(UINT32 newpc);
extern unsigned pdp1_get_sp(void);
extern void pdp1_set_sp(UINT32 newsp);
extern unsigned pdp1_get_context (void *dst);
extern void pdp1_set_context (void *src);
extern unsigned pdp1_get_reg (int regnum);
extern void pdp1_set_reg (int regnum, unsigned val);
extern void pdp1_set_nmi_line(int state);
extern void pdp1_set_irq_line(int irqline, int state);
extern void pdp1_set_irq_callback(int (*callback)(int irqline));
extern void pdp1_reset(void *param);
extern void pdp1_exit (void);
extern int pdp1_execute(int cycles);
extern const char *pdp1_info(void *context, int regnum);
extern unsigned pdp1_dasm(char *buffer, unsigned pc);

extern int pdp1_ICount;
extern int (* extern_iot)(int *, int);

#define READ_PDP_18BIT(A) ((signed)cpu_readmem16(A))
#define WRITE_PDP_18BIT(A,V) (cpu_writemem16(A,V))

#define AND 001
#define IOR 002
#define XOR 003
#define XCT 004
#define CALJDA 007
#define LAC 010
#define LIO 011
#define DAC 012
#define DAP 013
#define DIO 015
#define DZM 016
#define ADD 020
#define SUB 021
#define IDX 022
#define ISP 023
#define SAD 024
#define SAS 025
#define MUS 026
#define DIS 027
#define JMP 030
#define JSP 031
#define SKP 032
#define SFT 033
#define LAW 034
#define IOT 035
#define OPR 037

#endif /* _PDP1_H */
