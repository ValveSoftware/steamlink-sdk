/****************************************************************************
*			  real mode i286 emulator v1.4 by Fabrice Frances				*
*				(initial work based on David Hedley's pcemu)                *
****************************************************************************/
/* 26.March 2000 PeT changed set_irq_line */

#include <stdio.h>
#include <string.h>
#include "host.h"
#include "cpuintrf.h"
#include "memory.h"
#include "mame.h"

#include "i86.h"
#include "i86intf.h"


/* All pre-i286 CPUs have a 1MB address space */
#define AMASK	0xfffff

/* I86 registers */
typedef union
{									   /* eight general registers */
	UINT16 w[8];					   /* viewed as 16 bits registers */
	UINT8 b[16];					   /* or as 8 bit registers */
}
i86basicregs;

typedef struct
{
	i86basicregs regs;
	UINT32 pc;
	UINT32 prevpc;
	UINT32 base[4];
	UINT16 sregs[4];
	UINT16 flags;
	int (*irq_callback) (int irqline);
	int AuxVal, OverVal, SignVal, ZeroVal, CarryVal, DirVal;		/* 0 or non-0 valued flags */
	UINT8 ParityVal;
	UINT8 TF, IF;				   /* 0 or 1 valued flags */
	UINT8 MF;						   /* V30 mode flag */
	UINT8 int_vector;
	INT8 nmi_state;
	INT8 irq_state;
	int extra_cycles;       /* extra cycles for interrupts */
}
i86_Regs;


#include "i86time.cpp"

/***************************************************************************/
/* cpu state                                                               */
/***************************************************************************/

int i86_ICount;

static i86_Regs I;
static unsigned prefix_base;		   /* base address of the latest prefix segment */
static char seg_prefix;				   /* prefix segment indicator */

static UINT8 parity_table[256];

static struct i86_timing cycles;

/* The interrupt number of a pending external interrupt pending NMI is 2.	*/
/* For INTR interrupts, the level is caught on the bus during an INTA cycle */

#define PREFIX(name) i86##name
#define PREFIX86(name) i86##name

#define I86
#include "instr86.h"
#include "ea.h"
#include "modrm.h"
#include "table86.h"

#include "instr86.cpp"
#undef I86

/***************************************************************************/

void i86_reset(void *param)
{
	unsigned int i, j, c;
	BREGS reg_name[8] = {AL, CL, DL, BL, AH, CH, DH, BH};

	memset(&I, 0, sizeof (I));

	I.sregs[CS] = 0xf000;
	I.base[CS] = SegBase(CS);
	I.pc = 0xffff0 & AMASK;

	change_pc20(I.pc);

	for (i = 0; i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1)
				c++;

		parity_table[i] = !(c & 1);
	}

	I.ZeroVal = I.ParityVal = 1;

	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = reg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = (WREGS) ((i & 0x38) >> 3);
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = (WREGS) (i & 7);
		Mod_RM.RM.b[i] = (BREGS) reg_name[i & 7];
	}
}

void i86_exit(void)
{
	/* nothing to do ? */
}

/* ASG 971222 -- added these interface functions */

unsigned i86_get_context(void *dst)
{
	if (dst)
		*(i86_Regs *) dst = I;
	return sizeof (i86_Regs);
}

void i86_set_context(void *src)
{
	if (src)
	{
		I = *(i86_Regs *)src;
		I.base[CS] = SegBase(CS);
		I.base[DS] = SegBase(DS);
		I.base[ES] = SegBase(ES);
		I.base[SS] = SegBase(SS);
		change_pc20(I.pc);
	}
}

unsigned i86_get_pc(void)
{
	return I.pc;
}

void i86_set_pc(unsigned val)
{
	if (val - I.base[CS] >= 0x10000)
	{
		I.base[CS] = val & 0xffff0;
		I.sregs[CS] = I.base[CS] >> 4;
	}
	I.pc = val;
}

unsigned i86_get_sp(void)
{
	return I.base[SS] + I.regs.w[SP];
}

void i86_set_sp(unsigned val)
{
	if (val - I.base[SS] < 0x10000)
	{
		I.regs.w[SP] = val - I.base[SS];
	}
	else
	{
		I.base[SS] = val & 0xffff0;
		I.sregs[SS] = I.base[SS] >> 4;
		I.regs.w[SP] = val & 0x0000f;
	}
}

unsigned i86_get_reg(int regnum)
{
	switch (regnum)
	{
	case I86_IP:		return I.pc - I.base[CS];
	case I86_SP:		return I.regs.w[SP];
	case I86_FLAGS: 	CompressFlags(); return I.flags;
	case I86_AX:		return I.regs.w[AX];
	case I86_CX:		return I.regs.w[CX];
	case I86_DX:		return I.regs.w[DX];
	case I86_BX:		return I.regs.w[BX];
	case I86_BP:		return I.regs.w[BP];
	case I86_SI:		return I.regs.w[SI];
	case I86_DI:		return I.regs.w[DI];
	case I86_ES:		return I.sregs[ES];
	case I86_CS:		return I.sregs[CS];
	case I86_SS:		return I.sregs[SS];
	case I86_DS:		return I.sregs[DS];
	case I86_VECTOR:	return I.int_vector;
	case I86_PENDING:	return I.irq_state;
	case I86_NMI_STATE: return I.nmi_state;
	case I86_IRQ_STATE: return I.irq_state;
	case REG_PREVIOUSPC:return I.prevpc;
	default:
		if (regnum <= REG_SP_CONTENTS)
		{
			unsigned offset = ((I.base[SS] + I.regs.w[SP]) & AMASK) + 2 * (REG_SP_CONTENTS - regnum);

			if (offset < AMASK)
				return cpu_readmem20(offset) | (cpu_readmem20(offset + 1) << 8);
		}
	}
	return 0;
}

void i86_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
	case I86_IP:		I.pc = I.base[CS] + val;	break;
	case I86_SP:		I.regs.w[SP] = val; 		break;
	case I86_FLAGS: 	I.flags = val;	ExpandFlags(val); break;
	case I86_AX:		I.regs.w[AX] = val; 		break;
	case I86_CX:		I.regs.w[CX] = val; 		break;
	case I86_DX:		I.regs.w[DX] = val; 		break;
	case I86_BX:		I.regs.w[BX] = val; 		break;
	case I86_BP:		I.regs.w[BP] = val; 		break;
	case I86_SI:		I.regs.w[SI] = val; 		break;
	case I86_DI:		I.regs.w[DI] = val; 		break;
	case I86_ES:		I.sregs[ES] = val;	I.base[ES] = SegBase(ES);	break;
	case I86_CS:		I.sregs[CS] = val;	I.base[CS] = SegBase(CS);	break;
	case I86_SS:		I.sregs[SS] = val;	I.base[SS] = SegBase(SS);	break;
	case I86_DS:		I.sregs[DS] = val;	I.base[DS] = SegBase(DS);	break;
	case I86_VECTOR:	I.int_vector = val; 		break;
	case I86_PENDING:								break;
	case I86_NMI_STATE: i86_set_nmi_line(val);		break;
	case I86_IRQ_STATE: i86_set_irq_line(0, val);	break;
	default:
		if (regnum <= REG_SP_CONTENTS)
		{
			unsigned offset = ((I.base[SS] + I.regs.w[SP]) & AMASK) + 2 * (REG_SP_CONTENTS - regnum);

			if (offset < AMASK - 1)
			{
				cpu_writemem20(offset, val & 0xff);
				cpu_writemem20(offset + 1, (val >> 8) & 0xff);
			}
		}
	}
}

void i86_set_nmi_line(int state)
{
	if (I.nmi_state == state)
		return;
	I.nmi_state = state;

	/* on a rising edge, signal the NMI */
	if (state != CLEAR_LINE)
		PREFIX(_interrupt)(I86_NMI_INT);
}

void i86_set_irq_line(int irqline, int state)
{
	I.irq_state = state;

	/* if the IF is set, signal an interrupt */
	if (state != CLEAR_LINE && I.IF)
		PREFIX(_interrupt)(-1);
}

void i86_set_irq_callback(int (*callback) (int))
{
	I.irq_callback = callback;
}

int i86_execute(int num_cycles)
{
	/* copy over the cycle counts if they're not correct */
	if (cycles.id != 8086)
		cycles = i86_cycles;

	/* adjust for any interrupts that came in */
	i86_ICount = num_cycles;
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	/* run until we're out */
	while (i86_ICount > 0)
	{
//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
		logerror("[%04x:%04x]=%02x\tF:%04x\tAX=%04x\tBX=%04x\tCX=%04x\tDX=%04x %d%d%d%d%d%d%d%d%d\n",
				I.sregs[CS], I.pc - I.base[CS], ReadByte(I.pc), I.flags, I.regs.w[AX], I.regs.w[BX], I.regs.w[CX], I.regs.w[DX], I.AuxVal ? 1 : 0, I.OverVal ? 1 : 0,
				I.SignVal ? 1 : 0, I.ZeroVal ? 1 : 0, I.CarryVal ? 1 : 0, I.ParityVal ? 1 : 0, I.TF, I.IF, I.DirVal < 0 ? 1 : 0);
#endif

		seg_prefix = FALSE;
		I.prevpc = I.pc;
		TABLE86;
	}

	/* adjust for any interrupts that came in */
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	return num_cycles - i86_ICount;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i86_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME: 		return "I86";
	case CPU_INFO_FAMILY:		return "Intel 80x86";
	case CPU_INFO_VERSION:		return "1.4";
	case CPU_INFO_FILE: 		return __FILE__;
	case CPU_INFO_CREDITS:		return "Real mode i286 emulator v1.4 by Fabrice Frances\n(initial work I.based on David Hedley's pcemu)";
	}
	return "";
}

unsigned i86_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", cpu_readop(pc));
	return 1;
}

#if (HAS_I88)
/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i88_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "I88";
	}
	return i86_info(context, regnum);
}
#endif

#if (HAS_I186 || HAS_I188)

#include "i186intf.h"

#undef PREFIX
#define PREFIX(name) i186##name
#define PREFIX186(name) i186##name

#define I186
#include "instr186.h"
#include "table186.h"

#include "instr86.cpp"
#include "instr186.cpp"
#undef I186

int i186_execute(int num_cycles)
{
	/* copy over the cycle counts if they're not correct */
	if (cycles.id != 80186)
		cycles = i186_cycles;

	/* adjust for any interrupts that came in */
	i86_ICount = num_cycles;
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	/* run until we're out */
	while (i86_ICount > 0)
	{
#ifdef VERBOSE_DEBUG
		printf("[%04x:%04x]=%02x\tAX=%04x\tBX=%04x\tCX=%04x\tDX=%04x\n", I.sregs[CS], I.pc, ReadByte(I.pc), I.regs.w[AX],
			   I.regs.w[BX], I.regs.w[CX], I.regs.w[DX]);
#endif

		seg_prefix = FALSE;
		I.prevpc = I.pc;
		TABLE186;
	}

	/* adjust for any interrupts that came in */
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	return num_cycles - i86_ICount;
}

#if (HAS_I188)
/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i188_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "I188";
	}
	return i186_info(context, regnum);
}
#endif

#if (HAS_I186)
/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i186_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "I186";
	}
	return i86_info(context, regnum);
}
#endif

unsigned i186_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", cpu_readop(pc));
	return 1;
}

#endif

#if defined(INCLUDE_V20) && (HAS_V20 || HAS_V30 || HAS_V33)

#include "v30.h"
#include "v30intf.h"

/* compile the opcode emulating instruction new with v20 timing value */
/* MF flag -> opcode must be compiled new */
#undef PREFIX
#define PREFIX(name) v30##name
#undef PREFIX86
#define PREFIX86(name) v30##name
#undef PREFIX186
#define PREFIX186(name) v30##name
#define PREFIXV30(name) v30##name

#define nec_ICount i86_ICount

static UINT16 bytes[] =
{
	   1,	 2,    4,	 8,
	  16,	32,   64,  128,
	 256,  512, 1024, 2048,
	4096, 8192,16384,32768	/*,65536 */
};

#undef IncWordReg
#undef DecWordReg
#define V20
#include "instr86.h"
#include "instr186.h"
#include "instrv30.h"
#include "tablev30.h"

static void v30_interrupt(unsigned int_num, BOOLEAN md_flag)
{
	unsigned dest_seg, dest_off;

#if 0
	logerror("PC=%05x : NEC Interrupt %02d", cpu_get_pc(), int_num);
#endif

	v30_pushf();
	I.TF = I.IF = 0;
	if (md_flag)
		SetMD(0);					   /* clear Mode-flag = start 8080 emulation mode */

	if (int_num == -1)
	{
		int_num = (*I.irq_callback) (0);
/*		logerror(" (indirect ->%02d) ",int_num); */
	}

	dest_off = ReadWord(int_num * 4);
	dest_seg = ReadWord(int_num * 4 + 2);

	PUSH(I.sregs[CS]);
	PUSH(I.pc - I.base[CS]);
	I.sregs[CS] = (WORD) dest_seg;
	I.base[CS] = SegBase(CS);
	I.pc = (I.base[CS] + dest_off) & AMASK;
	change_pc20(I.pc);
/*	logerror("=%06x\n",cpu_get_pc()); */
}

void v30_trap(void)
{
	(*v30_instruction[FETCHOP])();
	v30_interrupt(1, 0);
}


#include "instr86.cpp"
#include "instr186.cpp"
#include "instrv30.cpp"
#undef V20

void v30_reset(void *param)
{
	i86_reset(param);
	SetMD(1);
}

int v30_execute(int num_cycles)
{
	/* copy over the cycle counts if they're not correct */
	if (cycles.id != 30)
		cycles = v30_cycles;

	/* adjust for any interrupts that came in */
	i86_ICount = num_cycles;
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	/* run until we're out */
	while (i86_ICount > 0)
	{
#ifdef VERBOSE_DEBUG
		printf("[%04x:%04x]=%02x\tAX=%04x\tBX=%04x\tCX=%04x\tDX=%04x\n", I.sregs[CS], I.pc, ReadByte(I.pc), I.regs.w[AX],
			   I.regs.w[BX], I.regs.w[CX], I.regs.w[DX]);
#endif

		seg_prefix = FALSE;
		I.prevpc = I.pc;
		TABLEV30;
	}

	/* adjust for any interrupts that came in */
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	return num_cycles - i86_ICount;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *v30_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "V30";
	}
	return i86_info(context, regnum);
}

#if (HAS_V20)
/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *v20_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "V20";
	}
	return v30_info(context, regnum);
}
#endif

#if (HAS_V33)
/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *v33_info(void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME:
		return "V33";
	}
	return v30_info(context, regnum);
}
#endif

unsigned v30_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", cpu_readop(pc));
	return 1;
}

#endif
