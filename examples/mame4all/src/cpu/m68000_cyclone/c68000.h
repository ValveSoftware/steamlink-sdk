#ifndef C68000_H
#define C68000_H

extern int *cyclone_cycles;
#define cyclone_ICount (*cyclone_cycles)

#define cyclone_INT_NONE 0							  
#define cyclone_IRQ_1    1
#define cyclone_IRQ_2    2
#define cyclone_IRQ_3    3
#define cyclone_IRQ_4    4
#define cyclone_IRQ_5    5
#define cyclone_IRQ_6    6
#define cyclone_IRQ_7    7
#define cyclone_INT_ACK_AUTOVECTOR    -1
#define cyclone_STOP     0x10

extern void cyclone_reset(void *param);                      
extern int  cyclone_execute(int cycles);                     
extern void cyclone_set_context(void *src);
extern unsigned cyclone_get_context(void *dst);
extern unsigned int cyclone_get_pc(void);                      
extern void cyclone_exit(void);
extern void cyclone_set_pc(unsigned val);
extern unsigned cyclone_get_sp(void);
extern void cyclone_set_sp(unsigned val);
extern unsigned cyclone_get_reg(int regnum);
extern void cyclone_set_reg(int regnum, unsigned val);
extern void cyclone_set_nmi_line(int state);
extern void cyclone_set_irq_line(int irqline, int state);
extern void cyclone_set_irq_callback(int (*callback)(int irqline));
extern const char *cyclone_info(void *context, int regnum);
extern unsigned cyclone_dasm(char *buffer, unsigned pc);

#endif
