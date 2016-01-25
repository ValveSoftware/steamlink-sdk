/***************************************************************************

  cpuintrf.c

  Don't you love MS-DOS 8+3 names? That stands for CPU interface.
  Functions needed to interface the CPU emulator with the other parts of
  the emulation.

***************************************************************************/

/*#include <signal.h>*/
#include "driver.h"
#include "timer.h"
#include "state.h"
#include "hiscore.h"

#if (HAS_Z80)
#include "cpu/z80/z80.h"
#endif
#if (HAS_DRZ80)
#include "cpu/z80_drz80/drz80_z80.h"
#endif
#if (HAS_Z80GB)
#include "cpu/z80gb/z80gb.h"
#endif
#if (HAS_8080 || HAS_8085A)
#include "cpu/i8085/i8085.h"
#endif
#if (HAS_M6502 || HAS_M65C02 || HAS_M65SC02 || HAS_M6510 || HAS_M6510T || HAS_M7501 || HAS_M8502 || HAS_N2A03)
#include "cpu/m6502/m6502.h"
#endif
#if (HAS_M4510)
#include "cpu/m6502/m4510.h"
#endif
#if (HAS_M65CE02)
#include "cpu/m6502/m65ce02.h"
#endif
#if (HAS_M6509)
#include "cpu/m6502/m6509.h"
#endif
#if (HAS_H6280)
#include "cpu/h6280/h6280.h"
#endif
#if (HAS_I86)
#include "cpu/i86/i86intf.h"
#endif
#if (HAS_I88)
#include "cpu/i86/i88intf.h"
#endif
#if (HAS_I186)
#include "cpu/i86/i186intf.h"
#endif
#if (HAS_I188)
#include "cpu/i86/i188intf.h"
#endif
#if (HAS_I286)
#include "cpu/i86/i286intf.h"
#endif
#if (HAS_V20 || HAS_V30 || HAS_V33)
#include "cpu/nec/necintrf.h"
#endif
#if (HAS_ARMNEC)
#include "cpu/nec_armnec/armnecintrf.h"
#endif
#if (HAS_I8035 || HAS_I8039 || HAS_I8048 || HAS_N7751)
#include "cpu/i8039/i8039.h"
#endif
#if (HAS_M6800 || HAS_M6801 || HAS_M6802 || HAS_M6803 || HAS_M6808 || HAS_HD63701)
#include "cpu/m6800/m6800.h"
#endif
#if (HAS_M6805 || HAS_M68705 || HAS_HD63705)
#include "cpu/m6805/m6805.h"
#endif
#if (HAS_HD6309 || HAS_M6809)
#include "cpu/m6809/m6809.h"
#endif
#if (HAS_KONAMI)
#include "cpu/konami/konami.h"
#endif
#if (HAS_M68000 || defined HAS_M68010 || HAS_M68020 || HAS_M68EC020)
#include "cpu/m68000/m68000.h"
#endif
#if (HAS_CYCLONE)
#include "cpu/m68000_cyclone/c68000.h"
#endif
#if (HAS_T11)
#include "cpu/t11/t11.h"
#endif
#if (HAS_S2650)
#include "cpu/s2650/s2650.h"
#endif
#if (HAS_TMS34010)
#include "cpu/tms34010/tms34010.h"
#endif
#if (HAS_TMS9900) || (HAS_TMS9940) || (HAS_TMS9980) || (HAS_TMS9985) \
	|| (HAS_TMS9989) || (HAS_TMS9995) || (HAS_TMS99105A) || (HAS_TMS99110A)
#include "cpu/tms9900/tms9900.h"
#endif
#if (HAS_Z8000)
#include "cpu/z8000/z8000.h"
#endif
#if (HAS_TMS320C10)
#include "cpu/tms32010/tms32010.h"
#endif
#if (HAS_CCPU)
#include "cpu/ccpu/ccpu.h"
#endif
#if (HAS_PDP1)
#include "cpu/pdp1/pdp1.h"
#endif
#if (HAS_ADSP2100) || (HAS_ADSP2105)
#include "cpu/adsp2100/adsp2100.h"
#endif
#if (HAS_MIPS)
#include "cpu/mips/mips.h"
#endif
#if (HAS_SC61860)
#include "cpu/sc61860/sc61860.h"
#endif
#if (HAS_ARM)
#include "cpu/arm/arm.h"
#endif


/* these are triggers sent to the timer system for various interrupt events */
#define TRIGGER_TIMESLICE		-1000
#define TRIGGER_INT 			-2000
#define TRIGGER_YIELDTIME		-3000
#define TRIGGER_SUSPENDTIME 	-4000

#define VERBOSE 0

#define SAVE_STATE_TEST 0
#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

struct cpuinfo
{
	struct cpu_interface *intf; 	/* pointer to the interface functions */
	int iloops; 					/* number of interrupts remaining this frame */
	int totalcycles;				/* total CPU cycles executed */
	int vblankint_countdown;		/* number of vblank callbacks left until we interrupt */
	int vblankint_multiplier;		/* number of vblank callbacks per interrupt */
	void *vblankint_timer;			/* reference to elapsed time counter */
	timer_tm vblankint_period;		/* timing period of the VBLANK interrupt */
	void *timedint_timer;			/* reference to this CPU's timer */
	timer_tm timedint_period; 		/* timing period of the timed interrupt */
	void *context;					/* dynamically allocated context buffer */
	int save_context;				/* need to context switch this CPU? yes or no */
} __attribute__ ((__aligned__ (32)));

static struct cpuinfo cpu[MAX_CPU];

static int activecpu,totalcpu;
static int cycles_running;	/* number of cycles that the CPU emulation was requested to run */
					/* (needed by cpu_getfcount) */
static int have_to_reset;

static int interrupt_enable[MAX_CPU];
static int interrupt_vector[MAX_CPU];

static int irq_line_state[MAX_CPU * MAX_IRQ_LINES];
static int irq_line_vector[MAX_CPU * MAX_IRQ_LINES];

static int watchdog_counter;

static void *vblank_timer;
static int vblank_countdown;
static int vblank_multiplier;
static timer_tm vblank_period;

static void *refresh_timer;
static timer_tm refresh_period;

static void *timeslice_timer;
static timer_tm timeslice_period;

static timer_tm scanline_period;

static int usres; /* removed from cpu_run and made global */
static int vblank;
static int current_frame;

static void cpu_generate_interrupt(int cpunum, int (*func)(void), int num);
static void cpu_vblankintcallback(int param);
static void cpu_timedintcallback(int param);
static void cpu_internal_interrupt(int cpunum, int type);
static void cpu_manualnmicallback(int param);
static void cpu_manualirqcallback(int param);
static void cpu_internalintcallback(int param);
static void cpu_manualintcallback(int param);
static void cpu_clearintcallback(int param);
static void cpu_resetcallback(int param);
static void cpu_haltcallback(int param);
static void cpu_timeslicecallback(int param);
static void cpu_vblankreset(void);
static void cpu_vblankcallback(int param);
static void cpu_updatecallback(int param);
static timer_tm cpu_computerate(int value);
static void cpu_inittimers(void);


/* default irq callback handlers */
static int cpu_0_irq_callback(int irqline);
static int cpu_1_irq_callback(int irqline);
static int cpu_2_irq_callback(int irqline);
static int cpu_3_irq_callback(int irqline);
static int cpu_4_irq_callback(int irqline);
static int cpu_5_irq_callback(int irqline);
static int cpu_6_irq_callback(int irqline);
static int cpu_7_irq_callback(int irqline);

/* and a list of them for indexed access */
static int (*cpu_irq_callbacks[MAX_CPU])(int) = {
	cpu_0_irq_callback, cpu_1_irq_callback, cpu_2_irq_callback, cpu_3_irq_callback,
	cpu_4_irq_callback, cpu_5_irq_callback, cpu_6_irq_callback, cpu_7_irq_callback
};

/* and a list of driver interception hooks */
static int (*drv_irq_callbacks[MAX_CPU])(int) = { NULL, };

/* Dummy interfaces for non-CPUs */
static void Dummy_reset(void *param);
static void Dummy_exit(void);
static int Dummy_execute(int cycles);
//static void Dummy_burn(int cycles);
static unsigned Dummy_get_context(void *regs);
static void Dummy_set_context(void *regs);
static unsigned Dummy_get_pc(void);
static void Dummy_set_pc(unsigned val);
static unsigned Dummy_get_sp(void);
static void Dummy_set_sp(unsigned val);
static unsigned Dummy_get_reg(int regnum);
static void Dummy_set_reg(int regnum, unsigned val);
static void Dummy_set_nmi_line(int state);
static void Dummy_set_irq_line(int irqline, int state);
static void Dummy_set_irq_callback(int (*callback)(int irqline));
static int Dummy_ICount;
static const char *Dummy_info(void *context, int regnum);
static unsigned Dummy_dasm(char *buffer, unsigned pc);

/* Convenience macros - not in cpuintrf.h because they shouldn't be used by everyone */
#define RESET(index)					((*cpu[index].intf->reset)(Machine->drv->cpu[index].reset_param))
#define EXECUTE(index,cycles)			((*cpu[index].intf->execute)(cycles))
#define GETCONTEXT(index,context)		((*cpu[index].intf->get_context)(context))
#define SETCONTEXT(index,context)		((*cpu[index].intf->set_context)(context))
#define GETCYCLETBL(index,which)		((*cpu[index].intf->get_cycle_table)(which))
#define SETCYCLETBL(index,which,cnts)	((*cpu[index].intf->set_cycle_table)(which,cnts))
#define GETPC(index)                    ((*cpu[index].intf->get_pc)())
#define SETPC(index,val)				((*cpu[index].intf->set_pc)(val))
#define GETSP(index)					((*cpu[index].intf->get_sp)())
#define SETSP(index,val)				((*cpu[index].intf->set_sp)(val))
#define GETREG(index,regnum)			((*cpu[index].intf->get_reg)(regnum))
#define SETREG(index,regnum,value)		((*cpu[index].intf->set_reg)(regnum,value))
#define SETNMILINE(index,state) 		((*cpu[index].intf->set_nmi_line)(state))
#define SETIRQLINE(index,line,state)	((*cpu[index].intf->set_irq_line)(line,state))
#define SETIRQCALLBACK(index,callback)	((*cpu[index].intf->set_irq_callback)(callback))
#define INTERNAL_INTERRUPT(index,type)	if( cpu[index].intf->internal_interrupt ) ((*cpu[index].intf->internal_interrupt)(type))
#define CPUINFO(index,context,regnum)	((*cpu[index].intf->cpu_info)(context,regnum))
#define CPUDASM(index,buffer,pc)		((*cpu[index].intf->cpu_dasm)(buffer,pc))
#define ICOUNT(index)					(*cpu[index].intf->icount)
#define INT_TYPE_NONE(index)			(cpu[index].intf->no_int)
#define INT_TYPE_IRQ(index) 			(cpu[index].intf->irq_int)
#define INT_TYPE_NMI(index) 			(cpu[index].intf->nmi_int)
#define READMEM(index,offset)			((*cpu[index].intf->memory_read)(offset))
#define WRITEMEM(index,offset,data) 	((*cpu[index].intf->memory_write)(offset,data))
#define SET_OP_BASE(index,pc)			((*cpu[index].intf->set_op_base)(pc))

#define CPU_TYPE(index) 				(Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK)
#define CPU_AUDIO(index)				(Machine->drv->cpu[index].cpu_type & CPU_AUDIO_CPU)

#define IFC_INFO(cpu,context,regnum)	((cpuintf[cpu].cpu_info)(context,regnum))

/* most CPUs use this macro */
#define CPU0(cpu,name,nirq,dirq,oc,i0,i1,i2,mem,shift,bits,endian,align,maxinst,MEM) \
	{																			   \
		CPU_##cpu,																   \
		name##_reset, name##_exit, name##_execute, NULL,						   \
		name##_get_context, name##_set_context, NULL, NULL, 					   \
		name##_get_pc, name##_set_pc,											   \
		name##_get_sp, name##_set_sp, name##_get_reg, name##_set_reg,			   \
		name##_set_nmi_line, name##_set_irq_line, name##_set_irq_callback,		   \
		NULL,NULL,NULL, name##_info, name##_dasm,								   \
		nirq, dirq, &name##_ICount, oc, i0, i1, i2,							   \
		cpu_readmem##mem, cpu_writemem##mem, cpu_setOPbase##mem,				   \
		shift, bits, CPU_IS_##endian, align, maxinst,							   \
		ABITS1_##MEM, ABITS2_##MEM, ABITS_MIN_##MEM 							   \
	}

/* CPUs which have _burn, _state_save and _state_load functions */
#define CPU1(cpu,name,nirq,dirq,oc,i0,i1,i2,mem,shift,bits,endian,align,maxinst,MEM)   \
	{																			   \
		CPU_##cpu,																   \
		name##_reset, name##_exit, name##_execute,								   \
		name##_burn,															   \
		name##_get_context, name##_set_context, 								   \
		name##_get_cycle_table, name##_set_cycle_table, 						   \
		name##_get_pc, name##_set_pc,											   \
		name##_get_sp, name##_set_sp, name##_get_reg, name##_set_reg,			   \
		name##_set_nmi_line, name##_set_irq_line, name##_set_irq_callback,		   \
		NULL,name##_state_save,name##_state_load, name##_info, name##_dasm, 	   \
		nirq, dirq, &name##_ICount, oc, i0, i1, i2,							   \
		cpu_readmem##mem, cpu_writemem##mem, cpu_setOPbase##mem,				   \
		shift, bits, CPU_IS_##endian, align, maxinst,							   \
		ABITS1_##MEM, ABITS2_##MEM, ABITS_MIN_##MEM 							   \
	}

/* CPUs which have the _internal_interrupt function */
#define CPU2(cpu,name,nirq,dirq,oc,i0,i1,i2,mem,shift,bits,endian,align,maxinst,MEM)   \
	{																			   \
		CPU_##cpu,																   \
		name##_reset, name##_exit, name##_execute,								   \
		NULL,																	   \
		name##_get_context, name##_set_context, NULL, NULL, 					   \
        name##_get_pc, name##_set_pc,                                              \
		name##_get_sp, name##_set_sp, name##_get_reg, name##_set_reg,			   \
		name##_set_nmi_line, name##_set_irq_line, name##_set_irq_callback,		   \
		name##_internal_interrupt,NULL,NULL, name##_info, name##_dasm,			   \
		nirq, dirq, &name##_ICount, oc, i0, i1, i2,							   \
		cpu_readmem##mem, cpu_writemem##mem, cpu_setOPbase##mem,				   \
		shift, bits, CPU_IS_##endian, align, maxinst,							   \
		ABITS1_##MEM, ABITS2_##MEM, ABITS_MIN_##MEM 							   \
	}																			   \



/* warning the ordering must match the one of the enum in driver.h! */
struct cpu_interface cpuintf[] =
{
	CPU0(DUMMY,    Dummy,	 1,  0,1.00,0,				   -1,			   -1,			   16,	  0,16,LE,1, 1,16	),
#if (HAS_Z80)
	CPU1(Z80,	   z80, 	 1,255,1.00,Z80_IGNORE_INT,    Z80_IRQ_INT,    Z80_NMI_INT,    16,	  0,16,LE,1, 4,16	),
#endif
#if (HAS_DRZ80)
	CPU1(DRZ80,	   drz80, 	 1,255,1.00,DRZ80_IGNORE_INT,    DRZ80_IRQ_INT,    DRZ80_NMI_INT,    16,	  0,16,LE,1, 4,16	),
#endif
#if (HAS_Z80GB)
	CPU0(Z80GB,    z80gb,	 5,255,1.00,Z80GB_IGNORE_INT,  0,			   1,			   16,	  0,16,LE,1, 4,16	),
#endif
#if (HAS_8080)
	CPU0(8080,	   i8080,	 4,255,1.00,I8080_NONE, 	   I8080_INTR,	   I8080_TRAP,	   16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_8085A)
	CPU0(8085A,    i8085,	 4,255,1.00,I8085_NONE, 	   I8085_INTR,	   I8085_TRAP,	   16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M6502)
	CPU0(M6502,    m6502,	 1,  0,1.00,M6502_INT_NONE,    M6502_INT_IRQ,  M6502_INT_NMI,  16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M65C02)
	CPU0(M65C02,   m65c02,	 1,  0,1.00,M65C02_INT_NONE,   M65C02_INT_IRQ, M65C02_INT_NMI, 16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M65SC02)
	CPU0(M65SC02,  m65sc02,  1,  0,1.00,M65SC02_INT_NONE,  M65SC02_INT_IRQ,M65SC02_INT_NMI,16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M65CE02)
	CPU0(M65CE02,  m65ce02,  1,  0,1.00,M65CE02_INT_NONE,  M65CE02_INT_IRQ,M65CE02_INT_NMI,16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M6509)
	CPU0(M6509,    m6509,	 1,  0,1.00,M6509_INT_NONE,    M6509_INT_IRQ,  M6509_INT_NMI,  20,	  0,20,LE,1, 3,20	),
#endif
#if (HAS_M6510)
	CPU0(M6510,    m6510,	 1,  0,1.00,M6510_INT_NONE,    M6510_INT_IRQ,  M6510_INT_NMI,  16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M6510T)
	CPU0(M6510T,   m6510t,	 1,  0,1.00,M6510T_INT_NONE,   M6510T_INT_IRQ, M6510T_INT_NMI, 16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M7501)
	CPU0(M7501,    m7501,	 1,  0,1.00,M7501_INT_NONE,    M7501_INT_IRQ,  M7501_INT_NMI,  16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M8502)
	CPU0(M8502,    m8502,	 1,  0,1.00,M8502_INT_NONE,    M8502_INT_IRQ,  M8502_INT_NMI,  16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_N2A03)
	CPU0(N2A03,    n2a03,	 1,  0,1.00,N2A03_INT_NONE,    N2A03_INT_IRQ,  N2A03_INT_NMI,  16,	  0,16,LE,1, 3,16	),
#endif
#if (HAS_M4510)
	CPU0(M4510,    m4510,	 1,  0,1.00,M4510_INT_NONE,    M4510_INT_IRQ,  M4510_INT_NMI,  20,	  0,20,LE,1, 3,20	),
#endif
#if (HAS_H6280)
	CPU0(H6280,    h6280,	 3,  0,1.00,H6280_INT_NONE,    -1,			   H6280_INT_NMI,  21,	  0,21,LE,1, 3,21	),
#endif
#if (HAS_I86)
	CPU0(I86,	   i86, 	 1,  0,1.00,I86_INT_NONE,	   -1000,		   I86_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_I88)
	CPU0(I88,	   i88, 	 1,  0,1.00,I88_INT_NONE,	   -1000,		   I88_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_I186)
	CPU0(I186,	   i186,	 1,  0,1.00,I186_INT_NONE,	   -1000,		   I186_NMI_INT,   20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_I188)
	CPU0(I188,	   i188,	 1,  0,1.00,I188_INT_NONE,	   -1000,		   I188_NMI_INT,   20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_I286)
	CPU0(I286,	   i286,	 1,  0,1.00,I286_INT_NONE,	   -1000,		   I286_NMI_INT,   24,	  0,24,LE,1, 5,24	),
#endif
#if (HAS_V20)
	CPU0(V20,	   v20, 	 1,  0,1.00,NEC_INT_NONE,	   -1000,		   NEC_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_V30)
	CPU0(V30,	   v30, 	 1,  0,1.00,NEC_INT_NONE,	   -1000,		   NEC_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_V33)
	CPU0(V33,	   v33, 	 1,  0,1.20,NEC_INT_NONE,	   -1000,		   NEC_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_ARMNEC)
	CPU0(ARMV30,   armv30, 	 1,  0,1.00,NEC_INT_NONE,	   -1000,		   NEC_NMI_INT,    20,	  0,20,LE,1, 5,20	),
	CPU0(ARMV33,   armv33, 	 1,  0,1.20,NEC_INT_NONE,	   -1000,		   NEC_NMI_INT,    20,	  0,20,LE,1, 5,20	),
#endif
#if (HAS_I8035)
	CPU0(I8035,    i8035,	 1,  0,1.00,I8035_IGNORE_INT,  I8035_EXT_INT,  -1,			   16,	  0,16,LE,1, 2,16	),
#endif
#if (HAS_I8039)
	CPU0(I8039,    i8039,	 1,  0,1.00,I8039_IGNORE_INT,  I8039_EXT_INT,  -1,			   16,	  0,16,LE,1, 2,16	),
#endif
#if (HAS_I8048)
	CPU0(I8048,    i8048,	 1,  0,1.00,I8048_IGNORE_INT,  I8048_EXT_INT,  -1,			   16,	  0,16,LE,1, 2,16	),
#endif
#if (HAS_N7751)
	CPU0(N7751,    n7751,	 1,  0,1.00,N7751_IGNORE_INT,  N7751_EXT_INT,  -1,			   16,	  0,16,LE,1, 2,16	),
#endif
#if (HAS_M6800)
	CPU0(M6800,    m6800,	 1,  0,1.00,M6800_INT_NONE,    M6800_INT_IRQ,  M6800_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6801)
	CPU0(M6801,    m6801,	 1,  0,1.00,M6801_INT_NONE,    M6801_INT_IRQ,  M6801_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6802)
	CPU0(M6802,    m6802,	 1,  0,1.00,M6802_INT_NONE,    M6802_INT_IRQ,  M6802_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6803)
	CPU0(M6803,    m6803,	 1,  0,1.00,M6803_INT_NONE,    M6803_INT_IRQ,  M6803_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6808)
	CPU0(M6808,    m6808,	 1,  0,1.00,M6808_INT_NONE,    M6808_INT_IRQ,  M6808_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_HD63701)
	CPU0(HD63701,  hd63701,  1,  0,1.00,HD63701_INT_NONE,  HD63701_INT_IRQ,HD63701_INT_NMI,16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_NSC8105)
	CPU0(NSC8105,  nsc8105,  1,  0,1.00,NSC8105_INT_NONE,  NSC8105_INT_IRQ,NSC8105_INT_NMI,16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6805)
	CPU0(M6805,    m6805,	 1,  0,1.00,M6805_INT_NONE,    M6805_INT_IRQ,  -1,			   16,	  0,11,BE,1, 3,16	),
#endif
#if (HAS_M68705)
	CPU0(M68705,   m68705,	 1,  0,1.00,M68705_INT_NONE,   M68705_INT_IRQ, -1,			   16,	  0,11,BE,1, 3,16	),
#endif
#if (HAS_HD63705)
	CPU0(HD63705,  hd63705,  8,  0,1.00,HD63705_INT_NONE,  HD63705_INT_IRQ,-1,			   16,	  0,16,BE,1, 3,16	),
#endif
#if (HAS_HD6309)
	CPU0(HD6309,   hd6309,	 2,  0,1.00,HD6309_INT_NONE,   HD6309_INT_IRQ, HD6309_INT_NMI, 16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M6809)
	CPU0(M6809,    m6809,	 2,  0,1.00,M6809_INT_NONE,    M6809_INT_IRQ,  M6809_INT_NMI,  16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_KONAMI)
	CPU0(KONAMI,   konami,	 2,  0,1.00,KONAMI_INT_NONE,   KONAMI_INT_IRQ, KONAMI_INT_NMI, 16,	  0,16,BE,1, 4,16	),
#endif
#if (HAS_M68000)
	CPU0(M68000,   m68000,	 8, -1,1.00,MC68000_INT_NONE,  -1,			   -1,			   24bew, 0,24,BE,2,10,24BEW),
#endif
#if (HAS_CYCLONE)
	CPU0(CYCLONE,   cyclone, 8, -1,1.00,cyclone_INT_NONE,  -1,			   -1,			   24bew, 0,24,BE,2,10,24BEW),
#endif
#if (HAS_M68010)
	CPU0(M68010,   m68010,	 8, -1,1.00,MC68010_INT_NONE,  -1,			   -1,			   24bew, 0,24,BE,2,10,24BEW),
#endif
#if (HAS_M68EC020)
	CPU0(M68EC020, m68ec020, 8, -1,1.00,MC68EC020_INT_NONE,-1,			   -1,			   24bew, 0,24,BE,2,10,24BEW),
#endif
#if (HAS_M68020)
	CPU0(M68020,   m68020,	 8, -1,1.00,MC68020_INT_NONE,  -1,			   -1,			   24bew, 0,24,BE,2,10,24BEW),
#endif
#if (HAS_T11)
	CPU0(T11,	   t11, 	 4,  0,1.00,T11_INT_NONE,	   -1,			   -1,			   16lew, 0,16,LE,2, 6,16LEW),
#endif
#if (HAS_S2650)
	CPU0(S2650,    s2650,	 2,  0,1.00,S2650_INT_NONE,    -1,			   -1,			   16,	  0,15,LE,1, 3,16	),
#endif
#if (HAS_TMS34010)
	CPU2(TMS34010, tms34010, 2,  0,1.00,TMS34010_INT_NONE, TMS34010_INT1,  -1,			   29,	  3,29,LE,2,10,29	),
#endif
#if (HAS_TMS9900)
	CPU0(TMS9900,  tms9900,  1,  0,1.00,TMS9900_NONE,	   -1,			   -1,			   16bew, 0,16,BE,2, 6,16BEW),
#endif
#if (HAS_TMS9940)
	CPU0(TMS9940,  tms9940,  1,  0,1.00,TMS9940_NONE,	   -1,			   -1,			   16bew, 0,16,BE,2, 6,16BEW),
#endif
#if (HAS_TMS9980)
	CPU0(TMS9980,  tms9980a, 1,  0,1.00,TMS9980A_NONE,	   -1,			   -1,			   16,	  0,16,BE,1, 6,16	),
#endif
#if (HAS_TMS9985)
	CPU0(TMS9985,  tms9985,  1,  0,1.00,TMS9985_NONE,	   -1,			   -1,			   16,	  0,16,BE,1, 6,16	),
#endif
#if (HAS_TMS9989)
	CPU0(TMS9989,  tms9989,  1,  0,1.00,TMS9989_NONE,	   -1,			   -1,			   16,	  0,16,BE,1, 6,16	),
#endif
#if (HAS_TMS9995)
	CPU0(TMS9995,  tms9995,  1,  0,1.00,TMS9995_NONE,	   -1,			   -1,			   16,	  0,16,BE,1, 6,16	),
#endif
#if (HAS_TMS99105A)
	CPU0(TMS99105A,tms99105a,1,  0,1.00,TMS99105A_NONE,    -1,			   -1,			   16bew, 0,16,BE,2, 6,16BEW),
#endif
#if (HAS_TMS99110A)
	CPU0(TMS99110A,tms99110a,1,  0,1.00,TMS99110A_NONE,    -1,			   -1,			   16bew, 0,16,BE,2, 6,16BEW),
#endif
#if (HAS_Z8000)
	CPU0(Z8000,    z8000,	 2,  0,1.00,Z8000_INT_NONE,    Z8000_NVI,	   Z8000_NMI,	   16bew, 0,16,BE,2, 6,16BEW),
#endif
#if (HAS_TMS320C10)
	CPU0(TMS320C10,tms320c10,2,  0,1.00,TMS320C10_INT_NONE,-1,			   -1,			   16,	 -1,16,BE,2, 4,16	),
#endif
#if (HAS_CCPU)
	CPU0(CCPU,	   ccpu,	 2,  0,1.00,0,				   -1,			   -1,			   16,	  0,15,LE,1, 3,16	),
#endif
#if (HAS_PDP1)
	CPU0(PDP1,	   pdp1,	 0,  0,1.00,0,				   -1,			   -1,			   16,	  0,18,LE,1, 3,16	),
#endif
#if (HAS_ADSP2100)
/* IMO we should rename all *_ICount to *_icount - ie. no mixed case */
#define adsp2100_ICount adsp2100_icount
	CPU0(ADSP2100, adsp2100, 4,  0,1.00,ADSP2100_INT_NONE, -1,			   -1,			   16lew,-1,14,LE,2, 4,16LEW),
#endif
#if (HAS_ADSP2105)
/* IMO we should rename all *_ICount to *_icount - ie. no mixed case */
#define adsp2105_ICount adsp2105_icount
	CPU0(ADSP2105, adsp2105, 4,  0,1.00,ADSP2105_INT_NONE, -1,			   -1,			   16lew,-1,14,LE,2, 4,16LEW),
#endif
#if (HAS_MIPS)
	CPU0(MIPS,	   mips,	 8, -1,1.00,MIPS_INT_NONE,	   MIPS_INT_NONE,  MIPS_INT_NONE,  32lew, 0,32,LE,4, 4,32LEW),
#endif
#if (HAS_SC61860)
	#define sc61860_ICount sc61860_icount
	CPU0(SC61860,  sc61860,  1,  0,1.00,-1,				   -1,			   -1,			   16,    0,16,BE,1, 4,16	),
#endif
#if (HAS_ARM)
	CPU0(ARM,	   arm, 	 2,  0,1.00,ARM_INT_NONE,	   ARM_FIRQ,	   ARM_IRQ, 	   26lew, 0,26,LE,4, 4,26LEW),
#endif
};

void cpu_init(void)
{
	int i;

	/* Verify the order of entries in the cpuintf[] array */
	for( i = 0; i < CPU_COUNT; i++ )
	{
		if( cpuintf[i].cpu_num != i )
		{
logerror("CPU #%d [%s] wrong ID %d: check enum CPU_... in src/driver.h!\n", i, cputype_name(i), cpuintf[i].cpu_num);
			exit(1);
		}
	}

	/* count how many CPUs we have to emulate */
	totalcpu = 0;

	while (totalcpu < MAX_CPU)
	{
		if( CPU_TYPE(totalcpu) == CPU_DUMMY ) break;
		totalcpu++;
	}

	/* zap the CPU data structure */
	memset(cpu, 0, sizeof(cpu));

	/* Set up the interface functions */
	for (i = 0; i < MAX_CPU; i++)
		cpu[i].intf = &cpuintf[CPU_TYPE(i)];

	/* reset the timer system */
	timer_init();
	timeslice_timer = refresh_timer = vblank_timer = NULL;
}

void cpu_run(void)
{
	int i;

	/* determine which CPUs need a context switch */
	for (i = 0; i < totalcpu; i++)
	{
		int j, size;

		/* allocate a context buffer for the CPU */
		size = GETCONTEXT(i,NULL);
		if( size == 0 )
		{
			/* That can't really be true */
logerror("CPU #%d claims to need no context buffer!\n", i);
			/*raise( SIGABRT );*/
		}

		cpu[i].context = malloc( size );
		if( cpu[i].context == NULL )
		{
			/* That's really bad :( */
logerror("CPU #%d failed to allocate context buffer (%d bytes)!\n", i, size);
			/*raise( SIGABRT );*/
		}

		/* Zap the context buffer */
		memset(cpu[i].context, 0, size );


		/* Save if there is another CPU of the same type */
		cpu[i].save_context = 0;

		for (j = 0; j < totalcpu; j++)
			if ( i != j && !strcmp(cpunum_core_file(i),cpunum_core_file(j)) )
				cpu[i].save_context = 1;

		for( j = 0; j < MAX_IRQ_LINES; j++ )
		{
			irq_line_state[i * MAX_IRQ_LINES + j] = CLEAR_LINE;
			irq_line_vector[i * MAX_IRQ_LINES + j] = cpuintf[CPU_TYPE(i)].default_vector;
		}
	}

reset:
	/* read hi scores information from hiscore.dat */
	hs_open(Machine->gamedrv->name);
	hs_init();

	/* initialize the various timers (suspends all CPUs at startup) */
	cpu_inittimers();
	watchdog_counter = -1;

	/* reset sound chips */
	sound_reset();

	/* enable all CPUs (except for audio CPUs if the sound is off) */
	for (i = 0; i < totalcpu; i++)
	{
		if (!CPU_AUDIO(i) || Machine->sample_rate != 0)
		{
			timer_suspendcpu(i, 0, SUSPEND_REASON_RESET);
		}
		else
		{
			timer_suspendcpu(i, 1, SUSPEND_REASON_DISABLE);
		}
	}

	have_to_reset = 0;
	vblank = 0;

logerror("Machine reset\n");

	/* start with interrupts enabled, so the generic routine will work even if */
	/* the machine doesn't have an interrupt enable port */
	for (i = 0;i < MAX_CPU;i++)
	{
		interrupt_enable[i] = 1;
		interrupt_vector[i] = 0xff;
        /* Reset any driver hooks into the IRQ acknowledge callbacks */
        drv_irq_callbacks[i] = NULL;
	}

	/* do this AFTER the above so init_machine() can use cpu_halt() to hold the */
	/* execution of some CPUs, or disable interrupts */
	if (Machine->drv->init_machine) (*Machine->drv->init_machine)();

	/* reset each CPU */
	for (i = 0; i < totalcpu; i++)
	{
		/* swap memory contexts and reset */
		memorycontextswap(i);
		if (cpu[i].save_context) SETCONTEXT(i, cpu[i].context);
		activecpu = i;
		RESET(i);

		/* Set the irq callback for the cpu */
		SETIRQCALLBACK(i,cpu_irq_callbacks[i]);


		/* save the CPU context if necessary */
		if (cpu[i].save_context) GETCONTEXT (i, cpu[i].context);

		/* reset the total number of cycles */
		cpu[i].totalcycles = 0;
	}

	/* reset the globals */
	cpu_vblankreset();
	current_frame = 0;

	/* loop until the user quits */
	usres = 0;
	while (usres == 0)
	{
		int cpunum;

		/* was machine_reset() called? */
		if (have_to_reset)
		{
#ifdef MESS
			if (Machine->drv->stop_machine) (*Machine->drv->stop_machine)();
#endif
			goto reset;
		}
		profiler_mark(PROFILER_EXTRA);

#if SAVE_STATE_TEST
		{
			if( keyboard_pressed_memory(KEYCODE_S) )
			{
				void *s = state_create(Machine->gamedrv->name);
				if( s )
				{
					for( cpunum = 0; cpunum < totalcpu; cpunum++ )
					{
						activecpu = cpunum;
						memorycontextswap(activecpu);
						if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
						/* make sure any bank switching is reset */
						SET_OP_BASE(activecpu, GETPC(activecpu));
						if( cpu[activecpu].intf->cpu_state_save )
							(*cpu[activecpu].intf->cpu_state_save)(s);
					}
					state_close(s);
				}
			}

			if( keyboard_pressed_memory(KEYCODE_L) )
			{
				void *s = state_open(Machine->gamedrv->name);
				if( s )
				{
					for( cpunum = 0; cpunum < totalcpu; cpunum++ )
					{
						activecpu = cpunum;
						memorycontextswap(activecpu);
						if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
						/* make sure any bank switching is reset */
						SET_OP_BASE(activecpu, GETPC(activecpu));
						if( cpu[activecpu].intf->cpu_state_load )
							(*cpu[activecpu].intf->cpu_state_load)(s);
						/* update the contexts */
						if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
					}
					state_close(s);
				}
			}
		}
#endif
		/* ask the timer system to schedule */
		if (timer_schedule_cpu(&cpunum, &cycles_running))
		{
			int ran;


			/* switch memory and CPU contexts */
			activecpu = cpunum;
			memorycontextswap(activecpu);
			if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

			/* make sure any bank switching is reset */
			SET_OP_BASE(activecpu, GETPC(activecpu));

			/* run for the requested number of cycles */
			profiler_mark(PROFILER_CPU1 + cpunum);
			ran = EXECUTE(activecpu, cycles_running);
			profiler_mark(PROFILER_END);

			/* update based on how many cycles we really ran */
			cpu[activecpu].totalcycles += ran;

			/* update the contexts */
			if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
			activecpu = -1;

			/* update the timer with how long we actually ran */
			timer_update_cpu(cpunum, ran);
		}

		profiler_mark(PROFILER_END);
	}

	/* write hi scores to disk - No scores saving if cheat */
	hs_close();

#ifdef MESS
	if (Machine->drv->stop_machine) (*Machine->drv->stop_machine)();
#endif

	/* shut down the CPU cores */
	for (i = 0; i < totalcpu; i++)
	{
		/* if the CPU core defines an exit function, call it now */
		if( cpu[i].intf->exit )
			(*cpu[i].intf->exit)();

		/* free the context buffer for that CPU */
		if( cpu[i].context )
		{
			free( cpu[i].context );
			cpu[i].context = NULL;
		}
	}
	totalcpu = 0;
}




/***************************************************************************

  Use this function to initialize, and later maintain, the watchdog. For
  convenience, when the machine is reset, the watchdog is disabled. If you
  call this function, the watchdog is initialized, and from that point
  onwards, if you don't call it at least once every 2 seconds, the machine
  will be reset.

  The 2 seconds delay is targeted at dondokod, which during boot stays more
  than 1 second without resetting the watchdog.

***************************************************************************/
WRITE_HANDLER( watchdog_reset_w )
{
	if (watchdog_counter == -1) logerror("watchdog armed\n");
	watchdog_counter = 2*Machine->drv->frames_per_second;
}

READ_HANDLER( watchdog_reset_r )
{
	if (watchdog_counter == -1) logerror("watchdog armed\n");
	watchdog_counter = 2*Machine->drv->frames_per_second;
	return 0;
}



/***************************************************************************

  This function resets the machine (the reset will not take place
  immediately, it will be performed at the end of the active CPU's time
  slice)

***************************************************************************/
void machine_reset(void)
{
	/* write hi scores to disk - No scores saving if cheat */
	hs_close();

	have_to_reset = 1;
}



/***************************************************************************

  Use this function to reset a specified CPU immediately

***************************************************************************/
void cpu_set_reset_line(int cpunum,int state)
{
	timer_set(TIME_NOW, (cpunum & 7) | (state << 3), cpu_resetcallback);
}


/***************************************************************************

  Use this function to control the HALT line on a CPU

***************************************************************************/
void cpu_set_halt_line(int cpunum,int state)
{
	timer_set(TIME_NOW, (cpunum & 7) | (state << 3), cpu_haltcallback);
}


/***************************************************************************

  Use this function to install a callback for IRQ acknowledge

***************************************************************************/
void cpu_set_irq_callback(int cpunum, int (*callback)(int))
{
	drv_irq_callbacks[cpunum] = callback;
}


/***************************************************************************

  This function returns CPUNUM current status  (running or halted)

***************************************************************************/
int cpu_getstatus(int cpunum)
{
	if (cpunum >= MAX_CPU) return 0;

	return !timer_iscpususpended(cpunum,
			SUSPEND_REASON_HALT | SUSPEND_REASON_RESET | SUSPEND_REASON_DISABLE);
}



int cpu_getactivecpu(void)
{
	return (activecpu < 0) ? 0 : activecpu;
}

void cpu_setactivecpu(int cpunum)
{
	activecpu = cpunum;
}

int cpu_gettotalcpu(void)
{
	return totalcpu;
}



unsigned cpu_get_pc(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return GETPC(cpunum);
}

void cpu_set_pc(unsigned val)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	SETPC(cpunum,val);
}

unsigned cpu_get_sp(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return GETSP(cpunum);
}

void cpu_set_sp(unsigned val)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	SETSP(cpunum,val);
}

/* these are available externally, for the timer system */
int cycles_currently_ran(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cycles_running - ICOUNT(cpunum);
}

int cycles_left_to_run(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return ICOUNT(cpunum);
}



/***************************************************************************

  Returns the number of CPU cycles since the last reset of the CPU

  IMPORTANT: this value wraps around in a relatively short time.
  For example, for a 6Mhz CPU, it will wrap around in
  2^32/6000000 = 716 seconds = 12 minutes.
  Make sure you don't do comparisons between values returned by this
  function, but only use the difference (which will be correct regardless
  of wraparound).

***************************************************************************/
int cpu_gettotalcycles(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpu[cpunum].totalcycles + cycles_currently_ran();
}



/***************************************************************************

  Returns the number of CPU cycles before the next interrupt handler call

***************************************************************************/
int cpu_geticount(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	int result = TIME_TO_CYCLES(cpunum, cpu[cpunum].vblankint_period - timer_timeelapsed(cpu[cpunum].vblankint_timer));
	return (result < 0) ? 0 : result;
}



/***************************************************************************

  Returns the number of CPU cycles before the end of the current video frame

***************************************************************************/
int cpu_getfcount(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	int result = TIME_TO_CYCLES(cpunum, refresh_period - timer_timeelapsed(refresh_timer));
	return (result < 0) ? 0 : result;
}



/***************************************************************************

  Returns the number of CPU cycles in one video frame

***************************************************************************/
int cpu_getfperiod(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return TIME_TO_CYCLES(cpunum, refresh_period);
}



/***************************************************************************

  Scales a given value by the ratio of fcount / fperiod

***************************************************************************/
int cpu_scalebyfcount(int value)
{
    int result = ( ((INT64)value) * ((INT64)timer_timeelapsed(refresh_timer)) ) / ((INT64)refresh_period);
	if (value >= 0) return (result < value) ? result : value;
	else return (result > value) ? result : value;
}



/***************************************************************************

  Returns the current scanline, or the time until a specific scanline

  Note: cpu_getscanline() counts from 0, 0 being the first visible line. You
  might have to adjust this value to match the hardware, since in many cases
  the first visible line is >0.

***************************************************************************/
int cpu_getscanline(void)
{
    return (int)(timer_timeelapsed(refresh_timer)/scanline_period);
}


timer_tm cpu_getscanlinetime(int scanline)
{
	timer_tm ret;
    timer_tm scantime = ((timer_entry *)refresh_timer)->start + scanline*scanline_period;
    timer_tm abstime = getabsolutetime();
    if (abstime >= scantime) scantime += TIME_IN_HZ(Machine->drv->frames_per_second);
    ret = scantime - abstime;
    if (ret < 1)
    {
        ret = TIME_IN_HZ(Machine->drv->frames_per_second);
    }

	return ret;
}


timer_tm cpu_getscanlineperiod(void)
{
	return scanline_period;
}


/***************************************************************************

  Returns the number of cycles in a scanline

 ***************************************************************************/
int cpu_getscanlinecycles(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return TIME_TO_CYCLES(cpunum, scanline_period);
}


/***************************************************************************

  Returns the number of cycles since the beginning of this frame

 ***************************************************************************/
int cpu_getcurrentcycles(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return TIME_TO_CYCLES(cpunum, timer_timeelapsed(refresh_timer));
}


/***************************************************************************

  Returns the current horizontal beam position in pixels

 ***************************************************************************/
int cpu_gethorzbeampos(void)
{
    /*
	timer_tm elapsed_time = timer_timeelapsed(refresh_timer);
    int scanline = elapsed_time / scanline_period;
	timer_tm time_since_scanline = elapsed_time - scanline * scanline_period;
	*/
	timer_tm time_since_scanline = timer_timeelapsed(refresh_timer) % scanline_period;
    return ( ((INT64)time_since_scanline) * ((INT64)Machine->drv->screen_width) ) / ((INT64)scanline_period);
}


/***************************************************************************

  Returns the number of times the interrupt handler will be called before
  the end of the current video frame. This can be useful to interrupt
  handlers to synchronize their operation. If you call this from outside
  an interrupt handler, add 1 to the result, i.e. if it returns 0, it means
  that the interrupt handler will be called once.

***************************************************************************/
int cpu_getiloops(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpu[cpunum].iloops;
}



/***************************************************************************

  Interrupt handling

***************************************************************************/

/***************************************************************************

  These functions are called when a cpu calls the callback sent to it's
  set_irq_callback function. It clears the irq line if the current state
  is HOLD_LINE and returns the interrupt vector for that line.

***************************************************************************/
#define MAKE_IRQ_CALLBACK(num)												\
static int cpu_##num##_irq_callback(int irqline)							\
{																			\
	int vector = irq_line_vector[num * MAX_IRQ_LINES + irqline];			\
    if( irq_line_state[num * MAX_IRQ_LINES + irqline] == HOLD_LINE )        \
	{																		\
		SETIRQLINE(num, irqline, CLEAR_LINE);								\
		irq_line_state[num * MAX_IRQ_LINES + irqline] = CLEAR_LINE; 		\
	}																		\
	LOG(("cpu_##num##_irq_callback(%d) $%04x\n", irqline, vector));         \
	if( drv_irq_callbacks[num] )											\
		return (*drv_irq_callbacks[num])(vector);							\
	return vector;															\
}

MAKE_IRQ_CALLBACK(0)
MAKE_IRQ_CALLBACK(1)
MAKE_IRQ_CALLBACK(2)
MAKE_IRQ_CALLBACK(3)
MAKE_IRQ_CALLBACK(4)
MAKE_IRQ_CALLBACK(5)
MAKE_IRQ_CALLBACK(6)
MAKE_IRQ_CALLBACK(7)

/***************************************************************************

  This function is used to generate internal interrupts (TMS34010)

***************************************************************************/
void cpu_generate_internal_interrupt(int cpunum, int type)
{
	timer_set(TIME_NOW, (cpunum & 7) | (type << 3), cpu_internalintcallback);
}


/***************************************************************************

  Use this functions to set the vector for a irq line of a CPU

***************************************************************************/
void cpu_irq_line_vector_w(int cpunum, int irqline, int vector)
{
	cpunum &= (MAX_CPU - 1);
	irqline &= (MAX_IRQ_LINES - 1);
	if( irqline < cpu[cpunum].intf->num_irqs )
	{
		LOG(("cpu_irq_line_vector_w(%d,%d,$%04x)\n",cpunum,irqline,vector));
		irq_line_vector[cpunum * MAX_IRQ_LINES + irqline] = vector;
		return;
	}
	LOG(("cpu_irq_line_vector_w CPU#%d irqline %d > max irq lines\n", cpunum, irqline));
}

/***************************************************************************

  Use these functions to set the vector (data) for a irq line (offset)
  of CPU #0 to #3

***************************************************************************/
WRITE_HANDLER( cpu_0_irq_line_vector_w ) { cpu_irq_line_vector_w(0, offset, data); }
WRITE_HANDLER( cpu_1_irq_line_vector_w ) { cpu_irq_line_vector_w(1, offset, data); }
WRITE_HANDLER( cpu_2_irq_line_vector_w ) { cpu_irq_line_vector_w(2, offset, data); }
WRITE_HANDLER( cpu_3_irq_line_vector_w ) { cpu_irq_line_vector_w(3, offset, data); }
WRITE_HANDLER( cpu_4_irq_line_vector_w ) { cpu_irq_line_vector_w(4, offset, data); }
WRITE_HANDLER( cpu_5_irq_line_vector_w ) { cpu_irq_line_vector_w(5, offset, data); }
WRITE_HANDLER( cpu_6_irq_line_vector_w ) { cpu_irq_line_vector_w(6, offset, data); }
WRITE_HANDLER( cpu_7_irq_line_vector_w ) { cpu_irq_line_vector_w(7, offset, data); }

/***************************************************************************

  Use this function to set the state the NMI line of a CPU

***************************************************************************/
void cpu_set_nmi_line(int cpunum, int state)
{
	/* don't trigger interrupts on suspended CPUs */
	if (cpu_getstatus(cpunum) == 0) return;

	LOG(("cpu_set_nmi_line(%d,%d)\n",cpunum,state));
	timer_set(TIME_NOW, (cpunum & 7) | (state << 3), cpu_manualnmicallback);
}

/***************************************************************************

  Use this function to set the state of an IRQ line of a CPU
  The meaning of irqline varies between the different CPU types

***************************************************************************/
void cpu_set_irq_line(int cpunum, int irqline, int state)
{
	/* don't trigger interrupts on suspended CPUs */
	if (cpu_getstatus(cpunum) == 0) return;

	LOG(("cpu_set_irq_line(%d,%d,%d)\n",cpunum,irqline,state));
	timer_set(TIME_NOW, (irqline & 7) | ((cpunum & 7) << 3) | (state << 6), cpu_manualirqcallback);
}

/***************************************************************************

  Use this function to cause an interrupt immediately (don't have to wait
  until the next call to the interrupt handler)

***************************************************************************/
void cpu_cause_interrupt(int cpunum,int type)
{
	/* don't trigger interrupts on suspended CPUs */
	if (cpu_getstatus(cpunum) == 0) return;

	timer_set(TIME_NOW, (cpunum & 7) | (type << 3), cpu_manualintcallback);
}



void cpu_clear_pending_interrupts(int cpunum)
{
	timer_set(TIME_NOW, cpunum, cpu_clearintcallback);
}



WRITE_HANDLER( interrupt_enable_w )
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	interrupt_enable[cpunum] = data;

	/* make sure there are no queued interrupts */
	if (data == 0) cpu_clear_pending_interrupts(cpunum);
}



WRITE_HANDLER( interrupt_vector_w )
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_vector[cpunum] != data)
	{
		LOG(("CPU#%d interrupt_vector_w $%02x\n", cpunum, data));
		interrupt_vector[cpunum] = data;

		/* make sure there are no queued interrupts */
		cpu_clear_pending_interrupts(cpunum);
	}
}



int interrupt(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	int val;

	if (interrupt_enable[cpunum] == 0)
		return INT_TYPE_NONE(cpunum);

	val = INT_TYPE_IRQ(cpunum);
	if (val == -1000)
		val = interrupt_vector[cpunum];

	return val;
}



int nmi_interrupt(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;

	if (interrupt_enable[cpunum] == 0)
		return INT_TYPE_NONE(cpunum);

	return INT_TYPE_NMI(cpunum);
}



int m68_level1_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_1;
}
int m68_level2_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_2;
}
int m68_level3_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_3;
}
int m68_level4_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_4;
}
int m68_level5_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_5;
}
int m68_level6_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_6;
}
int m68_level7_irq(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	if (interrupt_enable[cpunum] == 0) return MC68000_INT_NONE;
	return MC68000_IRQ_7;
}



int ignore_interrupt(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return INT_TYPE_NONE(cpunum);
}



/***************************************************************************

  CPU timing and synchronization functions.

***************************************************************************/

/* generate a trigger */
void cpu_trigger(int trigger)
{
	timer_trigger(trigger);
}

/* generate a trigger after a specific period of time */
void cpu_triggertime(timer_tm duration, int trigger)
{
	timer_set(duration, trigger, cpu_trigger);
}



/* burn CPU cycles until a timer trigger */
void cpu_spinuntil_trigger(int trigger)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	timer_suspendcpu_trigger(cpunum, trigger);
}

/* burn CPU cycles until the next interrupt */
void cpu_spinuntil_int(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	cpu_spinuntil_trigger(TRIGGER_INT + cpunum);
}

/* burn CPU cycles until our timeslice is up */
void cpu_spin(void)
{
	cpu_spinuntil_trigger(TRIGGER_TIMESLICE);
}

/* burn CPU cycles for a specific period of time */
void cpu_spinuntil_time(timer_tm duration)
{
	static int timetrig = 0;

	cpu_spinuntil_trigger(TRIGGER_SUSPENDTIME + timetrig);
	cpu_triggertime(duration, TRIGGER_SUSPENDTIME + timetrig);
	timetrig = (timetrig + 1) & 255;
}



/* yield our timeslice for a specific period of time */
void cpu_yielduntil_trigger(int trigger)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	timer_holdcpu_trigger(cpunum, trigger);
}

/* yield our timeslice until the next interrupt */
void cpu_yielduntil_int(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	cpu_yielduntil_trigger(TRIGGER_INT + cpunum);
}

/* yield our current timeslice */
void cpu_yield(void)
{
	cpu_yielduntil_trigger(TRIGGER_TIMESLICE);
}

/* yield our timeslice for a specific period of time */
void cpu_yielduntil_time(timer_tm duration)
{
	static int timetrig = 0;

	cpu_yielduntil_trigger(TRIGGER_YIELDTIME + timetrig);
	cpu_triggertime(duration, TRIGGER_YIELDTIME + timetrig);
	timetrig = (timetrig + 1) & 255;
}



int cpu_getvblank(void)
{
	return vblank;
}


int cpu_getcurrentframe(void)
{
	return current_frame;
}


/***************************************************************************

  Internal CPU event processors.

***************************************************************************/

static void cpu_manualnmicallback(int param)
{
	int cpunum, state, oldactive;
	cpunum = param & 7;
	state = param >> 3;

	/* swap to the CPU's context */
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	LOG(("cpu_manualnmicallback %d,%d\n",cpunum,state));

	switch (state)
	{
		case PULSE_LINE:
			SETNMILINE(cpunum,ASSERT_LINE);
			SETNMILINE(cpunum,CLEAR_LINE);
			break;
		case HOLD_LINE:
		case ASSERT_LINE:
			SETNMILINE(cpunum,ASSERT_LINE);
			break;
		case CLEAR_LINE:
			SETNMILINE(cpunum,CLEAR_LINE);
			break;
		default:
			logerror("cpu_manualnmicallback cpu #%d unknown state %d\n", cpunum, state);
	}
	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);

	/* generate a trigger to unsuspend any CPUs waiting on the interrupt */
	if (state != CLEAR_LINE)
		timer_trigger(TRIGGER_INT + cpunum);
}

static void cpu_manualirqcallback(int param)
{
	int cpunum, irqline, state, oldactive;

	irqline = param & 7;
	cpunum = (param >> 3) & 7;
	state = param >> 6;

	/* swap to the CPU's context */
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	LOG(("cpu_manualirqcallback %d,%d,%d\n",cpunum,irqline,state));

	irq_line_state[cpunum * MAX_IRQ_LINES + irqline] = state;
	switch (state)
	{
		case PULSE_LINE:
			SETIRQLINE(cpunum,irqline,ASSERT_LINE);
			SETIRQLINE(cpunum,irqline,CLEAR_LINE);
			break;
		case HOLD_LINE:
		case ASSERT_LINE:
			SETIRQLINE(cpunum,irqline,ASSERT_LINE);
			break;
		case CLEAR_LINE:
			SETIRQLINE(cpunum,irqline,CLEAR_LINE);
			break;
		default:
			logerror("cpu_manualirqcallback cpu #%d, line %d, unknown state %d\n", cpunum, irqline, state);
	}

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);

	/* generate a trigger to unsuspend any CPUs waiting on the interrupt */
	if (state != CLEAR_LINE)
		timer_trigger(TRIGGER_INT + cpunum);
}

static void cpu_internal_interrupt(int cpunum, int type)
{
	int oldactive = activecpu;

	/* swap to the CPU's context */
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	INTERNAL_INTERRUPT(cpunum, type);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);

	/* generate a trigger to unsuspend any CPUs waiting on the interrupt */
	timer_trigger(TRIGGER_INT + cpunum);
}

static void cpu_internalintcallback(int param)
{
	int type = param >> 3;
	int cpunum = param & 7;

	LOG(("CPU#%d internal interrupt type $%04x\n", cpunum, type));
	/* generate the interrupt */
	cpu_internal_interrupt(cpunum, type);
}

static void cpu_generate_interrupt(int cpunum, int (*func)(void), int num)
{
	int oldactive = activecpu;

	/* don't trigger interrupts on suspended CPUs */
	if (cpu_getstatus(cpunum) == 0) return;

	/* swap to the CPU's context */
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	/* cause the interrupt, calling the function if it exists */
	if (func) num = (*func)();

	/* wrapper for the new interrupt system */
	if (num != INT_TYPE_NONE(cpunum))
	{
		LOG(("CPU#%d interrupt type $%04x: ", cpunum, num));
		/* is it the NMI type interrupt of that CPU? */
		if (num == INT_TYPE_NMI(cpunum))
		{

			LOG(("NMI\n"));
			cpu_manualnmicallback(cpunum | (PULSE_LINE << 3) );

		}
		else
		{
			int irq_line=0;

			switch (CPU_TYPE(cpunum))
			{
#if (HAS_8085A)
			case CPU_8085A:
				switch (num)
				{
				case I8085_RST55:		irq_line = 1; break;
				case I8085_RST65:		irq_line = 2; break;
				case I8085_RST75:		irq_line = 3; break;
				}
				break;
#endif
#if (HAS_H6280)
			case CPU_H6280:
				switch (num)
				{
				case H6280_INT_IRQ2:	irq_line = 1; break;
				case H6280_INT_TIMER:	irq_line = 2; break;
				}
				break;
#endif
#if (HAS_HD6309)
			case CPU_HD6309:
			    if (num==HD6309_INT_FIRQ) irq_line = 1;
			    break;
#endif
#if (HAS_M6809)
			case CPU_M6809:
			    if (num==M6809_INT_FIRQ) irq_line = 1;
				break;
#endif
#if (HAS_KONAMI)
				case CPU_KONAMI:
				if (num==KONAMI_INT_FIRQ) irq_line = 1;
				break;
#endif
#if (HAS_M68000)
			case CPU_M68000:
				switch (num)
				{
				case MC68000_IRQ_1: 	irq_line = 1; break;
				case MC68000_IRQ_2: 	irq_line = 2; break;
				case MC68000_IRQ_3: 	irq_line = 3; break;
				case MC68000_IRQ_4: 	irq_line = 4; break;
				case MC68000_IRQ_5: 	irq_line = 5; break;
				case MC68000_IRQ_6: 	irq_line = 6; break;
				case MC68000_IRQ_7: 	irq_line = 7; break;
				}
				/* until now only auto vector interrupts supported */
				num = MC68000_INT_ACK_AUTOVECTOR;
				break;
#endif
#if (HAS_CYCLONE)
			case CPU_CYCLONE:
				switch (num)
				{
				case cyclone_IRQ_1: 	irq_line = 1; break;
				case cyclone_IRQ_2: 	irq_line = 2; break;
				case cyclone_IRQ_3: 	irq_line = 3; break;
				case cyclone_IRQ_4: 	irq_line = 4; break;
				case cyclone_IRQ_5: 	irq_line = 5; break;
				case cyclone_IRQ_6: 	irq_line = 6; break;
				case cyclone_IRQ_7: 	irq_line = 7; break;
				}
				/* until now only auto vector interrupts supported */
				num = cyclone_INT_ACK_AUTOVECTOR;
				break;
#endif
#if (HAS_M68010)
			case CPU_M68010:
				switch (num)
				{
				case MC68010_IRQ_1: 	irq_line = 1; break;
				case MC68010_IRQ_2: 	irq_line = 2; break;
				case MC68010_IRQ_3: 	irq_line = 3; break;
				case MC68010_IRQ_4: 	irq_line = 4; break;
				case MC68010_IRQ_5: 	irq_line = 5; break;
				case MC68010_IRQ_6: 	irq_line = 6; break;
				case MC68010_IRQ_7: 	irq_line = 7; break;
				}
				/* until now only auto vector interrupts supported */
				num = MC68000_INT_ACK_AUTOVECTOR;
				break;
#endif
#if (HAS_M68020)
			case CPU_M68020:
				switch (num)
				{
				case MC68020_IRQ_1: 	irq_line = 1; break;
				case MC68020_IRQ_2: 	irq_line = 2; break;
				case MC68020_IRQ_3: 	irq_line = 3; break;
				case MC68020_IRQ_4: 	irq_line = 4; break;
				case MC68020_IRQ_5: 	irq_line = 5; break;
				case MC68020_IRQ_6: 	irq_line = 6; break;
				case MC68020_IRQ_7: 	irq_line = 7; break;
				}
				/* until now only auto vector interrupts supported */
				num = MC68000_INT_ACK_AUTOVECTOR;
				break;
#endif
#if (HAS_M68EC020)
			case CPU_M68EC020:
				switch (num)
				{
				case MC68EC020_IRQ_1:	irq_line = 1; break;
				case MC68EC020_IRQ_2:	irq_line = 2; break;
				case MC68EC020_IRQ_3:	irq_line = 3; break;
				case MC68EC020_IRQ_4:	irq_line = 4; break;
				case MC68EC020_IRQ_5:	irq_line = 5; break;
				case MC68EC020_IRQ_6:	irq_line = 6; break;
				case MC68EC020_IRQ_7:	irq_line = 7; break;
				}
				/* until now only auto vector interrupts supported */
				num = MC68000_INT_ACK_AUTOVECTOR;
				break;
#endif
#if (HAS_T11)
			case CPU_T11:
				switch (num)
				{
				case T11_IRQ0:			irq_line = 0; break;
				case T11_IRQ1:			irq_line = 1; break;
				case T11_IRQ2:			irq_line = 2; break;
				case T11_IRQ3:			irq_line = 3; break;
				}
				break;
#endif
#if (HAS_TMS34010)
			case CPU_TMS34010:
			    if (num==TMS34010_INT2) irq_line = 1;
				break;
#endif
#if (HAS_Z8000)
                if (num==Z8000_VI) irq_line = 1;
				break;
#endif
#if (HAS_TMS320C10)
			case CPU_TMS320C10:
			    if (num==TMS320C10_ACTIVE_BIO) irq_line = 1;
				break;
#endif
#if (HAS_ADSP2100)
			case CPU_ADSP2100:
				switch (num)
				{
				case ADSP2100_IRQ1: 		irq_line = 1; break;
				case ADSP2100_IRQ2: 		irq_line = 2; break;
				case ADSP2100_IRQ3: 		irq_line = 3; break;
				}
				break;
#endif
			}
			LOG(("IRQ %d\n",irq_line));
			cpu_irq_line_vector_w(cpunum, irq_line, num);
			cpu_manualirqcallback(irq_line | (cpunum << 3) | (HOLD_LINE << 6) );
		}
	}

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);

	/* trigger already generated by cpu_manualirqcallback or cpu_manualnmicallback */
}

static void cpu_clear_interrupts(int cpunum)
{
	int oldactive = activecpu;
	int i;

	/* swap to the CPU's context */
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	/* clear NMI line */
	SETNMILINE(activecpu,CLEAR_LINE);

	/* clear all IRQ lines */
	for (i = 0; i < cpu[activecpu].intf->num_irqs; i++)
		SETIRQLINE(activecpu,i,CLEAR_LINE);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);
}


static void cpu_reset_cpu(int cpunum)
{
	int oldactive = activecpu;

	/* swap to the CPU's context */
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	/* reset the CPU */
	RESET(cpunum);

	/* Set the irq callback for the cpu */
	SETIRQCALLBACK(cpunum,cpu_irq_callbacks[cpunum]);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0) memorycontextswap(activecpu);
}

/***************************************************************************

  Interrupt callback. This is called once per CPU interrupt by either the
  VBLANK handler or by the CPU's own timer directly, depending on whether
  or not the CPU's interrupts are synced to VBLANK.

***************************************************************************/
static void cpu_vblankintcallback(int param)
{
	if (Machine->drv->cpu[param].vblank_interrupt)
		cpu_generate_interrupt(param, Machine->drv->cpu[param].vblank_interrupt, 0);

	/* update the counters */
	cpu[param].iloops--;
}


static void cpu_timedintcallback(int param)
{
	/* bail if there is no routine */
	if (!Machine->drv->cpu[param].timed_interrupt)
		return;

	/* generate the interrupt */
	cpu_generate_interrupt(param, Machine->drv->cpu[param].timed_interrupt, 0);
}


static void cpu_manualintcallback(int param)
{
	int intnum = param >> 3;
	int cpunum = param & 7;

	/* generate the interrupt */
	cpu_generate_interrupt(cpunum, 0, intnum);
}


static void cpu_clearintcallback(int param)
{
	/* clear the interrupts */
	cpu_clear_interrupts(param);
}


static void cpu_resetcallback(int param)
{
	int state = param >> 3;
	int cpunum = param & 7;

	/* reset the CPU */
	if (state == PULSE_LINE)
		cpu_reset_cpu(cpunum);
	else if (state == ASSERT_LINE)
	{
/* ASG - do we need this?		cpu_reset_cpu(cpunum);*/
		timer_suspendcpu(cpunum, 1, SUSPEND_REASON_RESET);	/* halt cpu */
	}
	else if (state == CLEAR_LINE)
	{
		if (timer_iscpususpended(cpunum, SUSPEND_REASON_RESET))
			cpu_reset_cpu(cpunum);
		timer_suspendcpu(cpunum, 0, SUSPEND_REASON_RESET);	/* restart cpu */
	}
}


static void cpu_haltcallback(int param)
{
	int state = param >> 3;
	int cpunum = param & 7;

	/* reset the CPU */
	if (state == ASSERT_LINE)
		timer_suspendcpu(cpunum, 1, SUSPEND_REASON_HALT);	/* halt cpu */
	else if (state == CLEAR_LINE)
		timer_suspendcpu(cpunum, 0, SUSPEND_REASON_HALT);	/* restart cpu */
}



/***************************************************************************

  VBLANK reset. Called at the start of emulation and once per VBLANK in
  order to update the input ports and reset the interrupt counter.

***************************************************************************/
static void cpu_vblankreset(void)
{
	int i;

	/* read hi scores from disk */
	hs_update();

	/* read keyboard & update the status of the input ports */
	update_input_ports();

	/* reset the cycle counters */
	for (i = 0; i < totalcpu; i++)
	{
		if (!timer_iscpususpended(i, SUSPEND_ANY_REASON))
			cpu[i].iloops = Machine->drv->cpu[i].vblank_interrupts_per_frame - 1;
		else
			cpu[i].iloops = -1;
	}
}


/***************************************************************************

  VBLANK callback. This is called 'vblank_multipler' times per frame to
  service VBLANK-synced interrupts and to begin the screen update process.

***************************************************************************/
static void cpu_firstvblankcallback(int param)
{
	/* now that we're synced up, pulse from here on out */
	vblank_timer = timer_pulse(vblank_period, param, cpu_vblankcallback);

	/* but we need to call the standard routine as well */
	cpu_vblankcallback(param);
}

/* note that calling this with param == -1 means count everything, but call no subroutines */
static void cpu_vblankcallback(int param)
{
	int i;

	/* loop over CPUs */
	for (i = 0; i < totalcpu; i++)
	{
		/* if the interrupt multiplier is valid */
		if (cpu[i].vblankint_multiplier != -1)
		{
			/* decrement; if we hit zero, generate the interrupt and reset the countdown */
			if (!--cpu[i].vblankint_countdown)
			{
				if (param != -1)
					cpu_vblankintcallback(i);
				cpu[i].vblankint_countdown = cpu[i].vblankint_multiplier;
				timer_reset(cpu[i].vblankint_timer, TIME_NEVER);
			}
		}

		/* else reset the VBLANK timer if this is going to be a real VBLANK */
		else if (vblank_countdown == 1)
			timer_reset(cpu[i].vblankint_timer, TIME_NEVER);
	}

	/* is it a real VBLANK? */
	if (!--vblank_countdown)
	{
		/* do we update the screen now? */
		if (!(Machine->drv->video_attributes & VIDEO_UPDATE_AFTER_VBLANK))
			usres = updatescreen();

		/* Set the timer to update the screen */
		timer_set(TIME_IN_USEC(Machine->drv->vblank_duration), 0, cpu_updatecallback);
		vblank = 1;

		/* reset the globals */
		cpu_vblankreset();

		/* reset the counter */
		vblank_countdown = vblank_multiplier;
	}
}


/***************************************************************************

  Video update callback. This is called a game-dependent amount of time
  after the VBLANK in order to trigger a video update.

***************************************************************************/
static void cpu_updatecallback(int param)
{
	/* update the screen if we didn't before */
	if (Machine->drv->video_attributes & VIDEO_UPDATE_AFTER_VBLANK)
		usres = updatescreen();
	vblank = 0;

	/* update IPT_VBLANK input ports */
	inputport_vblank_end();

	/* check the watchdog */
	if (watchdog_counter > 0)
	{
		if (--watchdog_counter == 0)
		{
logerror("reset caused by the watchdog\n");
			machine_reset();
		}
	}

	current_frame++;

	/* reset the refresh timer */
	timer_reset(refresh_timer, TIME_NEVER);
}


/***************************************************************************

  Converts an integral timing rate into a period. Rates can be specified
  as follows:

		rate > 0	   -> 'rate' cycles per frame
		rate == 0	   -> 0
		rate >= -10000 -> 'rate' cycles per second
		rate < -10000  -> 'rate' nanoseconds

***************************************************************************/
static timer_tm cpu_computerate(int value)
{
	/* values equal to zero are zero */
	if (value <= 0)
		return 0;

	/* values above between 0 and 50000 are in Hz */
	if (value < 50000)
		return TIME_IN_HZ(value);

	/* values greater than 50000 are in nanoseconds */
	else
		return TIME_IN_NSEC(value);
}


static void cpu_timeslicecallback(int param)
{
	timer_trigger(TRIGGER_TIMESLICE);
}


/***************************************************************************

  Initializes all the timers used by the CPU system.

***************************************************************************/
static void cpu_inittimers(void)
{
	timer_tm first_time;
	int i, max, ipf;

/*
#ifdef MAME_FASTSOUND
{
    extern int fast_sound;
    if (fast_sound)
    {
        // M72 sound hack
        if ((strcmp(Machine->gamedrv->name,"bchopper")==0) || (strcmp(Machine->gamedrv->name,"mrheli")==0) || (strcmp(Machine->gamedrv->name,"nspirit")==0) ||
            (strcmp(Machine->gamedrv->name,"nspiritj")==0) || (strcmp(Machine->gamedrv->name,"imgfight")==0) || (strcmp(Machine->gamedrv->name,"loht")==0) ||
            (strcmp(Machine->gamedrv->name,"xmultipl")==0) || (strcmp(Machine->gamedrv->name,"dbreed")==0) || (strcmp(Machine->gamedrv->name,"rtype2")==0) ||
            (strcmp(Machine->gamedrv->name,"rtype2j")==0) || (strcmp(Machine->gamedrv->name,"majtitle")==0) || (strcmp(Machine->gamedrv->name,"hharry")==0) ||
            (strcmp(Machine->gamedrv->name,"hharryu")==0) || (strcmp(Machine->gamedrv->name,"dkgensan")==0) || (strcmp(Machine->gamedrv->name,"kengo")==0) ||
            (strcmp(Machine->gamedrv->name,"poundfor")==0) || (strcmp(Machine->gamedrv->name,"poundfou")==0) || (strcmp(Machine->gamedrv->name,"airduel")==0) ||
            (strcmp(Machine->gamedrv->name,"gallop")==0))
        {
            int *ptr=(int *)&Machine->drv->cpu[1].vblank_interrupts_per_frame;
            *ptr=1;
        }
    }
}
#endif
*/

	/* remove old timers */
	if (timeslice_timer)
		timer_remove(timeslice_timer);
	if (refresh_timer)
		timer_remove(refresh_timer);
	if (vblank_timer)
		timer_remove(vblank_timer);

	/* allocate a dummy timer at the minimum frequency to break things up */
	ipf = Machine->drv->cpu_slices_per_frame;
	if (ipf <= 0)
		ipf = 1;
	timeslice_period = TIME_IN_HZ(Machine->drv->frames_per_second * ipf);
	timeslice_timer = timer_pulse(timeslice_period, 0, cpu_timeslicecallback);

	/* allocate an infinite timer to track elapsed time since the last refresh */
	refresh_period = TIME_IN_HZ(Machine->drv->frames_per_second);
	refresh_timer = timer_set(TIME_NEVER, 0, NULL);

	/* while we're at it, compute the scanline times */
	if (Machine->drv->vblank_duration)
		scanline_period = (refresh_period - TIME_IN_USEC(Machine->drv->vblank_duration)) /
				(timer_tm)(Machine->visible_area.max_y - Machine->visible_area.min_y + 1);
	else
		scanline_period = refresh_period / (timer_tm)Machine->drv->screen_height;

	/*
	 *		The following code finds all the CPUs that are interrupting in sync with the VBLANK
	 *		and sets up the VBLANK timer to run at the minimum number of cycles per frame in
	 *		order to service all the synced interrupts
	 */

	/* find the CPU with the maximum interrupts per frame */
	max = 1;
	for (i = 0; i < totalcpu; i++)
	{
		ipf = Machine->drv->cpu[i].vblank_interrupts_per_frame;
		if (ipf > max)
			max = ipf;
	}

	/* now find the LCD with the rest of the CPUs (brute force - these numbers aren't huge) */
	vblank_multiplier = max;
	while (1)
	{
		for (i = 0; i < totalcpu; i++)
		{
			ipf = Machine->drv->cpu[i].vblank_interrupts_per_frame;
			if (ipf > 0 && (vblank_multiplier % ipf) != 0)
				break;
		}
		if (i == totalcpu)
			break;
		vblank_multiplier += max;
	}

	/* initialize the countdown timers and intervals */
	for (i = 0; i < totalcpu; i++)
	{
		ipf = Machine->drv->cpu[i].vblank_interrupts_per_frame;
		if (ipf > 0)
			cpu[i].vblankint_countdown = cpu[i].vblankint_multiplier = vblank_multiplier / ipf;
		else
			cpu[i].vblankint_countdown = cpu[i].vblankint_multiplier = -1;
	}

	/* allocate a vblank timer at the frame rate * the LCD number of interrupts per frame */
	vblank_period = TIME_IN_HZ(Machine->drv->frames_per_second * vblank_multiplier);
	vblank_timer = timer_pulse(vblank_period, 0, cpu_vblankcallback);
	vblank_countdown = vblank_multiplier;

	/*
	 *		The following code creates individual timers for each CPU whose interrupts are not
	 *		synced to the VBLANK, and computes the typical number of cycles per interrupt
	 */

	/* start the CPU interrupt timers */
	for (i = 0; i < totalcpu; i++)
	{
		ipf = Machine->drv->cpu[i].vblank_interrupts_per_frame;

		/* remove old timers */
		if (cpu[i].vblankint_timer)
			timer_remove(cpu[i].vblankint_timer);
		if (cpu[i].timedint_timer)
			timer_remove(cpu[i].timedint_timer);

		/* compute the average number of cycles per interrupt */
		if (ipf <= 0)
			ipf = 1;
		cpu[i].vblankint_period = TIME_IN_HZ(Machine->drv->frames_per_second * ipf);
		cpu[i].vblankint_timer = timer_set(TIME_NEVER, 0, NULL);

		/* see if we need to allocate a CPU timer */
		ipf = Machine->drv->cpu[i].timed_interrupts_per_second;
		if (ipf)
		{
			cpu[i].timedint_period = cpu_computerate(ipf);
			cpu[i].timedint_timer = timer_pulse(cpu[i].timedint_period, i, cpu_timedintcallback);
		}
	}

	/* note that since we start the first frame on the refresh, we can't pulse starting
	   immediately; instead, we back up one VBLANK period, and inch forward until we hit
	   positive time. That time will be the time of the first VBLANK timer callback */
	timer_remove(vblank_timer);

	first_time = -TIME_IN_USEC(Machine->drv->vblank_duration) + vblank_period;
	while (first_time < 0)
	{
		cpu_vblankcallback(-1);
		first_time += vblank_period;
	}
	vblank_timer = timer_set(first_time, 0, cpu_firstvblankcallback);
}


/* AJP 981016 */
int cpu_is_saving_context(int _activecpu)
{
	return (cpu[_activecpu].save_context);
}


/* JB 971019 */
void* cpu_getcontext(int _activecpu)
{
	return cpu[_activecpu].context;
}


/***************************************************************************
  Retrieve or set the entire context of the active CPU
***************************************************************************/

unsigned cpu_get_context(void *context)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return GETCONTEXT(cpunum,context);
}

void cpu_set_context(void *context)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	SETCONTEXT(cpunum,context);
}

/***************************************************************************
  Retrieve or set a cycle counts lookup table for the active CPU
***************************************************************************/

void *cpu_get_cycle_table(int which)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return GETCYCLETBL(cpunum,which);
}

void cpu_set_cycle_tbl(int which, void *new_table)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	SETCYCLETBL(cpunum,which,new_table);
}

/***************************************************************************
  Retrieve or set the value of a specific register of the active CPU
***************************************************************************/

unsigned cpu_get_reg(int regnum)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return GETREG(cpunum,regnum);
}

void cpu_set_reg(int regnum, unsigned val)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	SETREG(cpunum,regnum,val);
}

/***************************************************************************

  Get various CPU information

***************************************************************************/

/***************************************************************************
  Returns the number of address bits for the active CPU
***************************************************************************/
unsigned cpu_address_bits(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpuintf[CPU_TYPE(cpunum)].address_bits;
}

/***************************************************************************
  Returns the address bit mask for the active CPU
***************************************************************************/
unsigned cpu_address_mask(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return (1 << cpuintf[CPU_TYPE(cpunum)].address_bits) - 1;
}

/***************************************************************************
  Returns the address shift factor for the active CPU
***************************************************************************/
int cpu_address_shift(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpuintf[CPU_TYPE(cpunum)].address_shift;
}

/***************************************************************************
  Returns the endianess for the active CPU
***************************************************************************/
unsigned cpu_endianess(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpuintf[CPU_TYPE(cpunum)].endianess;
}

/***************************************************************************
  Returns the code align unit for the active CPU (1 byte, 2 word, ...)
***************************************************************************/
unsigned cpu_align_unit(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpuintf[CPU_TYPE(cpunum)].align_unit;
}

/***************************************************************************
  Returns the max. instruction length for the active CPU
***************************************************************************/
unsigned cpu_max_inst_len(void)
{
	int cpunum = (activecpu < 0) ? 0 : activecpu;
	return cpuintf[CPU_TYPE(cpunum)].max_inst_len;
}

/***************************************************************************
  Returns the name for the active CPU
***************************************************************************/
const char *cpu_name(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_NAME);
	return "";
}

/***************************************************************************
  Returns the family name for the active CPU
***************************************************************************/
const char *cpu_core_family(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_FAMILY);
	return "";
}

/***************************************************************************
  Returns the version number for the active CPU
***************************************************************************/
const char *cpu_core_version(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_VERSION);
	return "";
}

/***************************************************************************
  Returns the core filename for the active CPU
***************************************************************************/
const char *cpu_core_file(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_FILE);
	return "";
}

/***************************************************************************
  Returns the credits for the active CPU
***************************************************************************/
const char *cpu_core_credits(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_CREDITS);
	return "";
}

/***************************************************************************
  Returns a dissassembled instruction for the active CPU
***************************************************************************/
unsigned cpu_dasm(char *buffer, unsigned pc)
{
	if( activecpu >= 0 )
		return CPUDASM(activecpu,buffer,pc);
	return 0;
}

/***************************************************************************
  Returns a flags (state, condition codes) string for the active CPU
***************************************************************************/
const char *cpu_flags(void)
{
	if( activecpu >= 0 )
		return CPUINFO(activecpu,NULL,CPU_INFO_FLAGS);
	return "";
}

/***************************************************************************
  Returns the number of address bits for a specific CPU type
***************************************************************************/
unsigned cputype_address_bits(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return cpuintf[cpu_type].address_bits;
	return 0;
}

/***************************************************************************
  Returns the address bit mask for a specific CPU type
***************************************************************************/
unsigned cputype_address_mask(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return (1 << cpuintf[cpu_type].address_bits) - 1;
	return 0;
}

/***************************************************************************
  Returns the address shift factor for a specific CPU type
***************************************************************************/
int cputype_address_shift(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return cpuintf[cpu_type].address_shift;
	return 0;
}

/***************************************************************************
  Returns the endianess for a specific CPU type
***************************************************************************/
unsigned cputype_endianess(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return cpuintf[cpu_type].endianess;
	return 0;
}

/***************************************************************************
  Returns the code align unit for a speciific CPU type (1 byte, 2 word, ...)
***************************************************************************/
unsigned cputype_align_unit(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return cpuintf[cpu_type].align_unit;
	return 0;
}

/***************************************************************************
  Returns the max. instruction length for a specific CPU type
***************************************************************************/
unsigned cputype_max_inst_len(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return cpuintf[cpu_type].max_inst_len;
	return 0;
}

/***************************************************************************
  Returns the name for a specific CPU type
***************************************************************************/
const char *cputype_name(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return IFC_INFO(cpu_type,NULL,CPU_INFO_NAME);
	return "";
}

/***************************************************************************
  Returns the family name for a specific CPU type
***************************************************************************/
const char *cputype_core_family(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return IFC_INFO(cpu_type,NULL,CPU_INFO_FAMILY);
	return "";
}

/***************************************************************************
  Returns the version number for a specific CPU type
***************************************************************************/
const char *cputype_core_version(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return IFC_INFO(cpu_type,NULL,CPU_INFO_VERSION);
	return "";
}

/***************************************************************************
  Returns the core filename for a specific CPU type
***************************************************************************/
const char *cputype_core_file(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return IFC_INFO(cpu_type,NULL,CPU_INFO_FILE);
	return "";
}

/***************************************************************************
  Returns the credits for a specific CPU type
***************************************************************************/
const char *cputype_core_credits(int cpu_type)
{
	cpu_type &= ~CPU_FLAGS_MASK;
	if( cpu_type < CPU_COUNT )
		return IFC_INFO(cpu_type,NULL,CPU_INFO_CREDITS);
	return "";
}

/***************************************************************************
  Returns the number of address bits for a specific CPU number
***************************************************************************/
unsigned cpunum_address_bits(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_address_bits(CPU_TYPE(cpunum));
	return 0;
}

/***************************************************************************
  Returns the address bit mask for a specific CPU number
***************************************************************************/
unsigned cpunum_address_mask(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_address_mask(CPU_TYPE(cpunum));
	return 0;
}

/***************************************************************************
  Returns the endianess for a specific CPU number
***************************************************************************/
unsigned cpunum_endianess(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_endianess(CPU_TYPE(cpunum));
	return 0;
}

/***************************************************************************
  Returns the code align unit for the active CPU (1 byte, 2 word, ...)
***************************************************************************/
unsigned cpunum_align_unit(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_align_unit(CPU_TYPE(cpunum));
	return 0;
}

/***************************************************************************
  Returns the max. instruction length for a specific CPU number
***************************************************************************/
unsigned cpunum_max_inst_len(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_max_inst_len(CPU_TYPE(cpunum));
	return 0;
}

/***************************************************************************
  Returns the name for a specific CPU number
***************************************************************************/
const char *cpunum_name(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_name(CPU_TYPE(cpunum));
	return "";
}

/***************************************************************************
  Returns the family name for a specific CPU number
***************************************************************************/
const char *cpunum_core_family(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_core_family(CPU_TYPE(cpunum));
	return "";
}

/***************************************************************************
  Returns the core version for a specific CPU number
***************************************************************************/
const char *cpunum_core_version(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_core_version(CPU_TYPE(cpunum));
	return "";
}

/***************************************************************************
  Returns the core filename for a specific CPU number
***************************************************************************/
const char *cpunum_core_file(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_core_file(CPU_TYPE(cpunum));
	return "";
}

/***************************************************************************
  Returns the credits for a specific CPU number
***************************************************************************/
const char *cpunum_core_credits(int cpunum)
{
	if( cpunum < totalcpu )
		return cputype_core_credits(CPU_TYPE(cpunum));
	return "";
}

/***************************************************************************
  Return a register value for a specific CPU number of the running machine
***************************************************************************/
unsigned cpunum_get_reg(int cpunum, int regnum)
{
	int oldactive;
	unsigned val = 0;

	if( cpunum == activecpu )
		return cpu_get_reg( regnum );

	/* swap to the CPU's context */
	if (activecpu >= 0)
		if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	val = GETREG(activecpu,regnum);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0)
	{
		memorycontextswap(activecpu);
		if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
	}

	return val;
}

/***************************************************************************
  Set a register value for a specific CPU number of the running machine
***************************************************************************/
void cpunum_set_reg(int cpunum, int regnum, unsigned val)
{
	int oldactive;

	if( cpunum == activecpu )
	{
		cpu_set_reg( regnum, val );
		return;
	}

	/* swap to the CPU's context */
	if (activecpu >= 0)
		if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	SETREG(activecpu,regnum,val);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0)
	{
		memorycontextswap(activecpu);
		if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
	}
}

/***************************************************************************
  Return a dissassembled instruction for a specific CPU
***************************************************************************/
unsigned cpunum_dasm(int cpunum,char *buffer,unsigned pc)
{
	unsigned result;
	int oldactive;

	if( cpunum == activecpu )
		return cpu_dasm(buffer,pc);

	/* swap to the CPU's context */
	if (activecpu >= 0)
		if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	result = CPUDASM(activecpu,buffer,pc);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0)
	{
		memorycontextswap(activecpu);
		if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
	}

	return result;
}

/***************************************************************************
  Return a flags (state, condition codes) string for a specific CPU
***************************************************************************/
const char *cpunum_flags(int cpunum)
{
	const char *result;
	int oldactive;

	if( cpunum == activecpu )
		return cpu_flags();

	/* swap to the CPU's context */
	if (activecpu >= 0)
		if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	oldactive = activecpu;
	activecpu = cpunum;
	memorycontextswap(activecpu);
	if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);

	result = CPUINFO(activecpu,NULL,CPU_INFO_FLAGS);

	/* update the CPU's context */
	if (cpu[activecpu].save_context) GETCONTEXT(activecpu, cpu[activecpu].context);
	activecpu = oldactive;
	if (activecpu >= 0)
	{
		memorycontextswap(activecpu);
		if (cpu[activecpu].save_context) SETCONTEXT(activecpu, cpu[activecpu].context);
	}

	return result;
}

/***************************************************************************

  Dummy interfaces for non-CPUs

***************************************************************************/
static void Dummy_reset(void *param) { }
static void Dummy_exit(void) { }
static int Dummy_execute(int cycles) { return cycles; }
//static void Dummy_burn(int cycles) { }
static unsigned Dummy_get_context(void *regs) { return 0; }
static void Dummy_set_context(void *regs) { }
static unsigned Dummy_get_pc(void) { return 0; }
static void Dummy_set_pc(unsigned val) { }
static unsigned Dummy_get_sp(void) { return 0; }
static void Dummy_set_sp(unsigned val) { }
static unsigned Dummy_get_reg(int regnum) { return 0; }
static void Dummy_set_reg(int regnum, unsigned val) { }
static void Dummy_set_nmi_line(int state) { }
static void Dummy_set_irq_line(int irqline, int state) { }
static void Dummy_set_irq_callback(int (*callback)(int irqline)) { }

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
static const char *Dummy_info(void *context, int regnum)
{
	if( !context && regnum )
		return "";

	switch (regnum)
	{
		case CPU_INFO_NAME: return "Dummy";
		case CPU_INFO_FAMILY: return "no CPU";
		case CPU_INFO_VERSION: return "0.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "The MAME team.";
	}
	return "";
}

static unsigned Dummy_dasm(char *buffer, unsigned pc)
{
	strcpy(buffer, "???");
	return 1;
}

