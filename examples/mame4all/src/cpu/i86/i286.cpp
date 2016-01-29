/****************************************************************************
*			  real mode i286 emulator v1.4 by Fabrice Frances				*
*				(initial work based on David Hedley's pcemu)                *
****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "host.h"
#include "cpuintrf.h"
#include "memory.h"
#include "mame.h"


/* All post-i286 CPUs have a 16MB address space */
#define AMASK	I.amask


#define i86_ICount i286_ICount

#include "i286.h"
#include "i286intf.h"

#include "i86time.c"

/***************************************************************************/
/* cpu state                                                               */
/***************************************************************************/
/* I86 registers */
typedef union
{                   /* eight general registers */
    UINT16 w[8];    /* viewed as 16 bits registers */
    UINT8  b[16];   /* or as 8 bit registers */
} i286basicregs;

typedef struct
{
    i286basicregs regs;
	int 	amask;			/* address mask */
    UINT32  pc;
    UINT32  prevpc;
	UINT16	flags;
	UINT16	msw;
	UINT32	base[4];
	UINT16	sregs[4];
	UINT16	limit[4];
	UINT8 rights[4];
	struct {
		UINT32 base;
		UINT16 limit;
	} gdtr, idtr;
	struct {
		UINT16 sel;
		UINT32 base;
		UINT16 limit;
		UINT8 rights;
	} ldtr, tr;
    int     (*irq_callback)(int irqline);
    int     AuxVal, OverVal, SignVal, ZeroVal, CarryVal, DirVal; /* 0 or non-0 valued flags */
    UINT8	ParityVal;
	UINT8	TF, IF; 	/* 0 or 1 valued flags */
	UINT8	int_vector;
	INT8	nmi_state;
	INT8	irq_state;
	int 	extra_cycles;       /* extra cycles for interrupts */
} i286_Regs;

int i286_ICount;

static i286_Regs I;
static unsigned prefix_base;	/* base address of the latest prefix segment */
static char seg_prefix;         /* prefix segment indicator */

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

static UINT8 parity_table[256];

static struct i86_timing cycles;

/***************************************************************************/

#define I286
#define PREFIX(fname) i286##fname
#define PREFIX86(fname) i286##fname
#define PREFIX186(fname) i286##fname
#define PREFIX286(fname) i286##fname

#include "ea.h"
#include "modrm.h"
#include "instr86.h"
#include "instr186.h"
#include "instr286.h"
#include "table286.h"
#include "instr86.c"
#include "instr186.c"
#include "instr286.c"

static void i286_urinit(void)
{
	unsigned int i,j,c;
	BREGS reg_name[8]={ AL, CL, DL, BL, AH, CH, DH, BH };

	for (i = 0;i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1) c++;
		
		parity_table[i] = !(c & 1);
	}
	
	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = reg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = (WREGS) ( (i & 0x38) >> 3) ;
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = (WREGS)( i & 7 );
		Mod_RM.RM.b[i] = (BREGS)reg_name[i & 7];
	}
}

void i286_set_address_mask(offs_t mask)
{
	I.amask=mask;
}

void i286_reset (void *param)
{
	static int urinit=1;

	/* in my docu not all registers are initialized! */
	//memset( &I, 0, sizeof(I) );

	if (urinit) {
		i286_urinit();
		urinit=0;

		/* this function seams to be called as a result of
		   cpu_set_reset_line */
		/* If a reset parameter is given, take it as pointer to an address mask */
		if( param )
			I.amask = *(unsigned*)param;
		else
			I.amask = 0x00ffff;
	}

	I.sregs[CS] = 0xf000;
	I.base[CS] = 0xff0000; 
	/* temporary, until I have the right reset vector working */
	I.base[CS] = I.sregs[CS] << 4;
	I.pc = 0xffff0;
	I.limit[CS]=I.limit[SS]=I.limit[DS]=I.limit[ES]=0xffff;
	I.sregs[DS]=I.sregs[SS]=I.sregs[ES]=0;
	I.base[DS]=I.base[SS]=I.base[ES]=0;
	I.msw=0xfff0;
	I.ZeroVal = I.ParityVal = 1; /* !? */
	I.flags=2;
	I.idtr.base=0;I.idtr.limit=0x3ff;

	CHANGE_PC(I.pc);

}

void i286_exit (void)
{
	/* nothing to do ? */
}

/****************************************************************************/

/* ASG 971222 -- added these interface functions */

unsigned i286_get_context(void *dst)
{
	if( dst )
		*(i286_Regs*)dst = I;
	 return sizeof(i286_Regs);
}

void i286_set_context(void *src)
{
	if( src )
	{
		I = *(i286_Regs*)src;
		if (PM) {
			
		} else {
			I.base[CS] = SegBase(CS);
			I.base[DS] = SegBase(DS);
			I.base[ES] = SegBase(ES);
			I.base[SS] = SegBase(SS);
		}
		CHANGE_PC(I.pc);
	}
}

unsigned i286_get_pc(void)
{
	return I.pc;
}

void i286_set_pc(unsigned val)
{
	if (PM) {
	} else {
		if (val - I.base[CS] >= 0x10000)
		{
			I.base[CS] = val & 0xffff0;
			I.sregs[CS] = I.base[CS] >> 4;
		}
		I.pc = val;
	}
}

unsigned i286_get_sp(void)
{
	return I.base[SS] + I.regs.w[SP];
}

void i286_set_sp(unsigned val)
{
	if (PM) {
	} else {
		if( val - I.base[SS] < 0x10000 )
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
}

unsigned i286_get_reg(int regnum)
{
	switch( regnum )
	{
		case I286_IP: return I.pc - I.base[CS];
		case I286_SP: return I.regs.w[SP];
		case I286_FLAGS: CompressFlags(); return I.flags;
		case I286_AX: return I.regs.w[AX];
		case I286_CX: return I.regs.w[CX];
		case I286_DX: return I.regs.w[DX];
		case I286_BX: return I.regs.w[BX];
		case I286_BP: return I.regs.w[BP];
		case I286_SI: return I.regs.w[SI];
		case I286_DI: return I.regs.w[DI];
		case I286_ES: return I.sregs[ES];
		case I286_CS: return I.sregs[CS];
		case I286_SS: return I.sregs[SS];
		case I286_DS: return I.sregs[DS];
		case I286_VECTOR: return I.int_vector;
		case I286_PENDING: return I.irq_state;
		case I286_NMI_STATE: return I.nmi_state;
		case I286_IRQ_STATE: return I.irq_state;
		case REG_PREVIOUSPC: return I.prevpc;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = ((I.base[SS] + I.regs.w[SP]) & I.amask) + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < I.amask )
					return cpu_readmem24( offset ) | ( cpu_readmem24( offset + 1) << 8 );
			}
	}
	return 0;
}

void i286_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case I286_IP: I.pc = I.base[CS] + val; break;
		case I286_SP: I.regs.w[SP] = val; break;
		case I286_FLAGS: I.flags = val; ExpandFlags(val); break;
		case I286_AX: I.regs.w[AX] = val; break;
		case I286_CX: I.regs.w[CX] = val; break;
		case I286_DX: I.regs.w[DX] = val; break;
		case I286_BX: I.regs.w[BX] = val; break;
		case I286_BP: I.regs.w[BP] = val; break;
		case I286_SI: I.regs.w[SI] = val; break;
		case I286_DI: I.regs.w[DI] = val; break;
		case I286_ES: I.sregs[ES] = val; break;
		case I286_CS: I.sregs[CS] = val; break;
		case I286_SS: I.sregs[SS] = val; break;
		case I286_DS: I.sregs[DS] = val; break;
		case I286_VECTOR: I.int_vector = val; break;
		case I286_PENDING: /* obsolete */ break;
		case I286_NMI_STATE: i286_set_nmi_line(val); break;
		case I286_IRQ_STATE: i286_set_irq_line(0,val); break;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = ((I.base[SS] + I.regs.w[SP]) & I.amask) + 2 * (REG_SP_CONTENTS - regnum);
				if( offset < I.amask - 1 )
				{
					cpu_writemem24( offset, val & 0xff );
					cpu_writemem24( offset+1, (val >> 8) & 0xff );
				}
			}
    }
}

void i286_set_nmi_line(int state)
{
	if (I.nmi_state == state)
		return;
	I.nmi_state = state;
	
	/* on a rising edge, signal the NMI */
	if (state != CLEAR_LINE)
		PREFIX(_interrupt)(I86_NMI_INT);
}

void i286_set_irq_line(int irqline, int state)
{
	I.irq_state = state;
	
	/* if the IF is set, signal an interrupt */
	if (state != CLEAR_LINE && I.IF)
		PREFIX(_interrupt)(-1);
}

void i286_set_irq_callback(int (*callback)(int))
{
	I.irq_callback = callback;
}

int i286_execute(int num_cycles)
{
	/* copy over the cycle counts if they're not correct */
	if (cycles.id != 80286)
		cycles = i286_cycles;

	/* adjust for any interrupts that came in */
	i286_ICount = num_cycles;
	i286_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	/* run until we're out */
	while(i286_ICount>0)
	{
		
//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
		printf("[%04x:%04x]=%02x\tF:%04x\tAX=%04x\tBX=%04x\tCX=%04x\tDX=%04x %d%d%d%d%d%d%d%d%d\n",I.sregs[CS],I.pc - I.base[CS],ReadByte(I.pc),I.flags,I.regs.w[AX],I.regs.w[BX],I.regs.w[CX],I.regs.w[DX], I.AuxVal?1:0, I.OverVal?1:0, I.SignVal?1:0, I.ZeroVal?1:0, I.CarryVal?1:0, I.ParityVal?1:0,I.TF, I.IF, I.DirVal<0?1:0);
#endif
		
		seg_prefix=FALSE;
		I.prevpc = I.pc;
		
		TABLE286 // call instruction
    }

	/* adjust for any interrupts that came in */
	i286_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	return num_cycles - i286_ICount;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *i286_info(void *context, int regnum)
{
	switch( regnum )
	{
	case CPU_INFO_NAME: return "I286";
	case CPU_INFO_FAMILY: return "Intel 80286";
	case CPU_INFO_VERSION: return "1.4";
	case CPU_INFO_FILE: return __FILE__;
	case CPU_INFO_CREDITS: return "Real mode i286 emulator v1.4 by Fabrice Frances\n(initial work I.based on David Hedley's pcemu)";
	}
	return "";
}

unsigned i286_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", cpu_readop(pc) );
	return 1;
}
