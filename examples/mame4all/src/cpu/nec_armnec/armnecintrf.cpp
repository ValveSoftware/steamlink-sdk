#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "armnecintrf.h"
#include "armnec.h"

#define VEZ_MEM_SHIFT	11
#define VEZ_PAGE_COUNT	(1 << (20 - VEZ_MEM_SHIFT))

/* structure definition */
typedef struct
{
    struct ArmNec regs; /* ArmNec structure */
	UINT32	int_vector; /* interrupt vector (not used) */
	UINT32	pending_irq; /* pending irq */
	UINT32	nmi_state; /* NMI state */
	UINT32	irq_state; /* IRQ state */
	int     (*irq_callback)(int irqline); /* MAME IRQ callback */
	unsigned char * ppMemRead[VEZ_PAGE_COUNT];
	unsigned char * ppMemWrite[VEZ_PAGE_COUNT];
} armnec_regs;

/* static structure */
static armnec_regs ARMNEC;

/* cycles */
int *armnec_cycles=&ARMNEC.regs.cycles;

/* interrupt types */
#define INT_IRQ 0x01
#define NMI_IRQ 0x02

/* ArmNec required functions */

static int armnec_UnrecognizedCallback(unsigned int a) { return 0; }

static unsigned int armnec_read8(unsigned int Addr)
{
    return ((cpu_readmem20(Addr))&0xff);
}

static unsigned int armnec_read16(unsigned int Addr)
{
	return (((cpu_readmem20(Addr))&0xff)|((cpu_readmem20(Addr+1)&0xff)<<8));
}

static void armnec_write8(unsigned int Addr,unsigned int Value)
{
	cpu_writemem20(Addr,Value);
}

static void armnec_write16(unsigned int Addr,unsigned int Value)
{
	cpu_writemem20(Addr,(unsigned char)Value&0xff);
	cpu_writemem20(Addr+1,((unsigned char)(Value>>8))&0xff);
}

static unsigned int armnec_in8(unsigned int Addr)
{
	return ((cpu_readport(Addr))&0xff);
}

static void armnec_out8(unsigned int Addr, unsigned int Value)
{
	cpu_writeport(Addr,Value);
}

static unsigned int armnec_checkpc(unsigned int ps, unsigned int ip) 
{
	change_pc20((ps << 4) + ip);
    ARMNEC.regs.mem_base=(unsigned int) OP_ROM;
    ARMNEC.regs.pc=ARMNEC.regs.mem_base + ((ps << 4) + ip);
    return (ARMNEC.regs.pc);
}

#define CHECK_PC armnec_checkpc(ARMNEC.regs.sreg[1], ARMNEC.regs.ip)

/* NEC required functions */

void armnec_init(void) { }
void armnec_set_irq_callback(int (*callback)(int irqline)) { ARMNEC.irq_callback = callback; }
unsigned armnec_dasm(char *buffer, unsigned pc) { sprintf( buffer, "$%02X", cpu_readop(pc) ); return 1; }
void armnec_exit(void) { }

void armnec_reset(void *param)
{
    memset(&ARMNEC,0,sizeof(armnec_regs));

	ARMNEC.regs.read8 = armnec_read8;
	ARMNEC.regs.read16 = armnec_read16;
	ARMNEC.regs.write8 = armnec_write8;
	ARMNEC.regs.write16 = armnec_write16;
	ARMNEC.regs.in8 = armnec_in8;
	ARMNEC.regs.out8 = armnec_out8;
	ARMNEC.regs.checkpc = armnec_checkpc;
	ARMNEC.regs.UnrecognizedCallback = armnec_UnrecognizedCallback;
	ARMNEC.regs.ReadMemMap=ARMNEC.ppMemRead;
	ARMNEC.regs.WriteMemMap=ARMNEC.ppMemRead;
    
	ARMNEC.regs.sreg[1] = 0xffff;
	ARMNEC.regs.flags = 0x8000; // MF

	CHECK_PC;
}

unsigned armnec_get_context(void *dst)
{
    if (dst) memcpy (dst,&ARMNEC,sizeof (armnec_regs));
    return sizeof(armnec_regs);
}

void armnec_set_context(void *src)
{
    if (src) memcpy (&ARMNEC, src, sizeof (armnec_regs));
    CHECK_PC;
}

unsigned armnec_get_pc(void)
{ 
    return ((ARMNEC.regs.sreg[1] << 4) + ARMNEC.regs.ip);
}

void armnec_set_pc(unsigned val)
{
	if( val - (ARMNEC.regs.sreg[1]<<4) < 0x10000 )
	{
		ARMNEC.regs.ip = val - (ARMNEC.regs.sreg[1]);
	}
	else
	{
		ARMNEC.regs.sreg[1] = val >> 4;
		ARMNEC.regs.ip = val & 0x0000f;
	}
	CHECK_PC;
}

unsigned armnec_get_sp(void)
{
    unsigned short *regs=(unsigned short *)ARMNEC.regs.reg;
   	return (ARMNEC.regs.sreg[2]<<4) + regs[4];
}

void armnec_set_sp(unsigned val)
{
    unsigned short *regs=(unsigned short *)ARMNEC.regs.reg;
	if( val - (ARMNEC.regs.sreg[2]<<4) < 0x10000 )
	{
		regs[4] = val - (ARMNEC.regs.sreg[2]<<4);
	}
	else
	{
		ARMNEC.regs.sreg[2] = val >> 4;
		regs[4] = val & 0x0000f;
	}
}

unsigned armnec_get_reg(int regnum)
{
    unsigned short *regs=(unsigned short *)ARMNEC.regs.reg;

	switch( regnum )
	{
		case ARMNEC_IP: return ARMNEC.regs.ip;
		case ARMNEC_FLAGS: return ARMNEC.regs.flags;
        case ARMNEC_AW: return regs[0];
		case ARMNEC_CW: return regs[1];
		case ARMNEC_DW: return regs[2];
		case ARMNEC_BW: return regs[3];
		case ARMNEC_SP: return regs[4];
		case ARMNEC_BP: return regs[5];
		case ARMNEC_IX: return regs[6];
		case ARMNEC_IY: return regs[7];
		case ARMNEC_ES: return ARMNEC.regs.sreg[0];
		case ARMNEC_CS: return ARMNEC.regs.sreg[1];
		case ARMNEC_SS: return ARMNEC.regs.sreg[2];
		case ARMNEC_DS: return ARMNEC.regs.sreg[3];
		case ARMNEC_VECTOR: return ARMNEC.int_vector;
		case ARMNEC_PENDING: return ARMNEC.pending_irq;
		case ARMNEC_NMI_STATE: return ARMNEC.nmi_state;
		case ARMNEC_IRQ_STATE: return ARMNEC.irq_state;
		case REG_PREVIOUSPC: return 0;	/* not supported */
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = armnec_get_sp() + 2 * (REG_SP_CONTENTS - regnum);
				return cpu_readmem20( offset ) | ( cpu_readmem20( offset + 1) << 8 );
			}
	}
	return 0;
}

void armnec_set_reg(int regnum, unsigned val)
{
    unsigned short *regs=(unsigned short *)ARMNEC.regs.reg;

	switch( regnum )
	{
		case ARMNEC_IP: ARMNEC.regs.ip = val; break;
		case ARMNEC_FLAGS: ARMNEC.regs.flags = val; break;
        case ARMNEC_AW: regs[0] = val; break;
		case ARMNEC_CW: regs[1] = val; break;
		case ARMNEC_DW: regs[2] = val; break;
		case ARMNEC_BW: regs[3] = val; break;
		case ARMNEC_SP: regs[4] = val; break;
		case ARMNEC_BP: regs[5] = val; break;
		case ARMNEC_IX: regs[6] = val; break;
		case ARMNEC_IY: regs[7] = val; break;
		case ARMNEC_ES: ARMNEC.regs.sreg[0] = val; break;
		case ARMNEC_CS: ARMNEC.regs.sreg[1] = val; break;
		case ARMNEC_SS: ARMNEC.regs.sreg[2] = val; break;
		case ARMNEC_DS: ARMNEC.regs.sreg[3] = val; break;
		case ARMNEC_VECTOR: ARMNEC.int_vector = val; break;
		case ARMNEC_PENDING: ARMNEC.pending_irq = val; break;
		case ARMNEC_NMI_STATE: armnec_set_nmi_line(val); break;
		case ARMNEC_IRQ_STATE: armnec_set_irq_line(0,val); break;
		default:
			if( regnum <= REG_SP_CONTENTS )
			{
				unsigned offset = armnec_get_sp() + 2 * (REG_SP_CONTENTS - regnum);
				cpu_writemem20( offset, val & 0xff );
				cpu_writemem20( offset+1, (val >> 8) & 0xff );
			}
    }
}

void armnec_set_nmi_line(int state)
{
	if( ARMNEC.nmi_state == state ) return;
    ARMNEC.nmi_state = state;
	if (state != CLEAR_LINE)
	{
		ARMNEC.pending_irq |= NMI_IRQ;
	}
}

void armnec_set_irq_line(int irqline, int state)
{
	ARMNEC.irq_state = state;
	if (state == CLEAR_LINE)
	{
		if (!(ARMNEC.regs.flags & 0x0200))
			ARMNEC.pending_irq &= ~INT_IRQ;
	}
	else
	{
		if (ARMNEC.regs.flags & 0x0200)
			ARMNEC.pending_irq |= INT_IRQ;
	}
}

static void armv30_interrupt(void)
{
	if( ARMNEC.pending_irq & NMI_IRQ )
	{
        ArmV30Irq(&ARMNEC.regs, ARMNEC_NMI_INT);
    	CHECK_PC;
		ARMNEC.pending_irq &= ~NMI_IRQ;
	}
	else if( ARMNEC.pending_irq )
	{
        ArmV30Irq(&ARMNEC.regs,(*ARMNEC.irq_callback)(0));
    	CHECK_PC;
		ARMNEC.irq_state = CLEAR_LINE;
		ARMNEC.pending_irq &= ~INT_IRQ;
	}
}

static void armv33_interrupt(void)
{
	if( ARMNEC.pending_irq & NMI_IRQ )
	{
        ArmV33Irq(&ARMNEC.regs, ARMNEC_NMI_INT);
    	CHECK_PC;
		ARMNEC.pending_irq &= ~NMI_IRQ;
	}
	else if( ARMNEC.pending_irq )
	{
        ArmV33Irq(&ARMNEC.regs,(*ARMNEC.irq_callback)(0));
    	CHECK_PC;
		ARMNEC.irq_state = CLEAR_LINE;
		ARMNEC.pending_irq &= ~INT_IRQ;
	}
}

int armv30_execute(int cycles)
{
    int exec=0;
	ARMNEC.regs.cycles = cycles;
	if (ARMNEC.pending_irq) armv30_interrupt();
    exec=ArmV30Run(&ARMNEC.regs);
	CHECK_PC;
	return (cycles-exec);
}

int armv33_execute(int cycles)
{
    int exec=0;
	ARMNEC.regs.cycles = cycles;
	if (ARMNEC.pending_irq) armv33_interrupt();
    exec=ArmV33Run(&ARMNEC.regs);
	CHECK_PC;
	return (cycles-exec);
}

const char *armv30_info(void *context, int regnum)
{
    switch( regnum )
    {
        case CPU_INFO_NAME: return "ArmNec V30";
        case CPU_INFO_FAMILY: return "NEC V-Series";
        case CPU_INFO_VERSION: return "1.0";
        case CPU_INFO_FILE: return __FILE__;
        case CPU_INFO_CREDITS: return "ArmNec emulator by Oopsware";
    }
    return "";
}

const char *armv33_info(void *context, int regnum)
{
    switch( regnum )
    {
        case CPU_INFO_NAME: return "ArmNec V33";
        case CPU_INFO_FAMILY: return "NEC V-Series";
        case CPU_INFO_VERSION: return "1.0";
        case CPU_INFO_FILE: return __FILE__;
        case CPU_INFO_CREDITS: return "ArmNec emulator by Oopsware";
    }
    return "";
}

