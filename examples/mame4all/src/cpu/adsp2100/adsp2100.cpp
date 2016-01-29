/*###################################################################################################
**
**
**		ADSP2100.c
**		Core implementation for the portable Analog ADSP-2100 emulator.
**		Written by Aaron Giles
**
**
**#################################################################################################*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cpuintrf.h"
#include "adsp2100.h"
#include "osdepend.h"

/*###################################################################################################
**	CONSTANTS
**#################################################################################################*/

#define TRACK_HOTSPOTS		0

/* stack depths */
#define	PC_STACK_DEPTH		16
#define CNTR_STACK_DEPTH	4
#define STAT_STACK_DEPTH	4
#define LOOP_STACK_DEPTH	4

/* chip types */
#define CHIP_TYPE_ADSP2100	0
#define CHIP_TYPE_ADSP2105	1


/*###################################################################################################
**	STRUCTURES & TYPEDEFS
**#################################################################################################*/

/* 16-bit registers that can be loaded signed or unsigned */
typedef union
{
	UINT16	u;
	INT16	s;
} ADSPREG16;


/* the SHIFT result register is 32 bits */
typedef union
{
#ifdef LSB_FIRST
	struct { ADSPREG16 sr0, sr1; } srx;
#else
	struct { ADSPREG16 sr1, sr0; } srx;
#endif
	UINT32 sr;
} SHIFTRESULT;


/* the MAC result register is 40 bits */
typedef union
{
#ifdef LSB_FIRST
	struct { ADSPREG16 mr0, mr1, mr2, mrzero; } mrx;
#else
	struct { ADSPREG16 mrzero, mr2, mr1, mr0; } mrx;
#endif
	UINT64 mr;
} MACRESULT;

/* there are two banks of "core" registers */
typedef struct ADSPCORE
{
	/* ALU registers */
	ADSPREG16	ax0, ax1;
	ADSPREG16	ay0, ay1;
	ADSPREG16	ar;
	ADSPREG16	af;

	/* MAC registers */
	ADSPREG16	mx0, mx1;
	ADSPREG16	my0, my1;
	MACRESULT	mr;
	ADSPREG16	mf;

	/* SHIFT registers */
	ADSPREG16	si;
	ADSPREG16	se;
	ADSPREG16	sb;
	SHIFTRESULT	sr;

	/* dummy registers */
	ADSPREG16	zero;
} ADSPCORE;


/* ADSP-2100 Registers */
typedef struct
{
	/* Core registers, 2 banks */
	ADSPCORE	r[2];

	/* Memory addressing registers */
	UINT16		i[8];
	INT16		m[8];
	UINT16		l[8];
	UINT16		lmask[8];
	UINT16		base[8];
	UINT8		px;

	/* other CPU registers */
	UINT16		pc;
	UINT16		ppc;
	UINT16		loop;
	UINT8		loop_condition;
	UINT16		cntr;

	/* status registers */
	UINT8		astat;
	UINT8		sstat;
	UINT8		mstat;
	UINT8		astat_clear;
	UINT8		idle;

	/* stacks */
	UINT32		loop_stack[LOOP_STACK_DEPTH];
	UINT16		cntr_stack[CNTR_STACK_DEPTH];
	UINT16		pc_stack[PC_STACK_DEPTH];
	UINT8		stat_stack[STAT_STACK_DEPTH][3];
	INT8		pc_sp;
	INT8		cntr_sp;
	INT8		stat_sp;
	INT8		loop_sp;

	/* external I/O */
	UINT8		flagout;
	UINT8		flagin;
#if SUPPORT_2101_EXTENSIONS
	UINT8		fl0;
	UINT8		fl1;
	UINT8		fl2;
#endif

	/* interrupt handling */
	UINT8		imask;
	UINT8		icntl;
#if SUPPORT_2101_EXTENSIONS
	UINT16		ifc;
#endif
    UINT8    	irq_state[4];
    UINT8    	irq_latch[4];
    INT16		interrupt_cycles;
    int			(*irq_callback)(int irqline);
} adsp2100_Regs;



/*###################################################################################################
**	PUBLIC GLOBAL VARIABLES
**#################################################################################################*/

int	adsp2100_icount=50000;


/*###################################################################################################
**	PRIVATE GLOBAL VARIABLES
**#################################################################################################*/

static adsp2100_Regs adsp2100;
static ADSPCORE *core;

static int chip_type = CHIP_TYPE_ADSP2100;

static UINT16 *reverse_table = 0;
static UINT16 *mask_table = 0;
static UINT8 *condition_table = 0;

#if SUPPORT_2101_EXTENSIONS
static RX_CALLBACK adsp2105_rx_callback = 0;
static TX_CALLBACK adsp2105_tx_callback = 0;
#endif

#if TRACK_HOTSPOTS
static UINT32 pcbucket[0x4000];
#endif


/*###################################################################################################
**	PRIVATE FUNCTION PROTOTYPES
**#################################################################################################*/

static int create_tables(void);
static void check_irqs(void);


/*###################################################################################################
**	MEMORY ACCESSORS
**#################################################################################################*/

INLINE UINT16 RWORD_DATA(UINT16 addr)
{
	addr = (addr & 0x3fff) << 1;
	return ADSP2100_RDMEM_WORD(ADSP2100_DATA_OFFSET + addr);
}

INLINE void WWORD_DATA(UINT16 addr, UINT16 data)
{
	addr = (addr & 0x3fff) << 1;
	ADSP2100_WRMEM_WORD(ADSP2100_DATA_OFFSET + addr, data);
}

INLINE UINT32 RWORD_PGM(UINT16 addr)
{
	addr = (addr & 0x3fff) << 2;
	return *(UINT32 *)&OP_ROM[ADSP2100_PGM_OFFSET + addr] & 0x00ffffff;
}

INLINE void WWORD_PGM(UINT16 addr, UINT32 data)
{
	addr = (addr & 0x3fff) << 2;
	*(UINT32 *)&OP_ROM[ADSP2100_PGM_OFFSET + addr] = data;
}

#define ROPCODE() RWORD_PGM(adsp2100.pc)


/*###################################################################################################
**	IMPORT CORE UTILITIES
**#################################################################################################*/

#include "2100ops.cpp"



/*###################################################################################################
**	IRQ HANDLING
**#################################################################################################*/

static void check_irqs(void)
{
	UINT8 check;

	if ( chip_type == CHIP_TYPE_ADSP2105 ) {
		/* check IRQ2 */
		check = (adsp2100.icntl & 4) ? adsp2100.irq_latch[2] : adsp2100.irq_state[2];
		if (check && (adsp2100.imask & 0x20))
		{
			/* clear the latch */
			adsp2100.irq_latch[2] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling */
			adsp2100.pc = 4;
			adsp2100.idle = 0;

			/* mask all other interrupts */
			adsp2100.imask &= ~0x3f;
			return;
		}

		/* check IRQ1 */
		check = (adsp2100.icntl & 2) ? adsp2100.irq_latch[1] : adsp2100.irq_state[1];
		if (check && (adsp2100.imask & 4))
		{
			/* clear the latch */
			adsp2100.irq_latch[1] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling */
			adsp2100.pc = 0x10;
			adsp2100.idle = 0;

			/* mask other interrupts based on the nesting bit */
			if (adsp2100.icntl & 0x10) adsp2100.imask &= ~0x7;
			else adsp2100.imask &= ~0xf;
			return;
		}

		/* check IRQ0 */
		check = (adsp2100.icntl & 1) ? adsp2100.irq_latch[0] : adsp2100.irq_state[0];
		if (check && (adsp2100.imask & 2))
		{
			/* clear the latch */
			adsp2100.irq_latch[0] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling */
			adsp2100.pc = 0x14;
			adsp2100.idle = 0;

			/* mask other interrupts based on the nesting bit */
			if (adsp2100.icntl & 0x10) adsp2100.imask &= ~0x3;
			else adsp2100.imask &= ~0xf;
			return;
		}
	} else {
		/* check IRQ3 */
		check = (adsp2100.icntl & 8) ? adsp2100.irq_latch[3] : adsp2100.irq_state[3];
		if (check && (adsp2100.imask & 8))
		{
			/* clear the latch */
			adsp2100.irq_latch[3] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location 3 & stop idling */
			adsp2100.pc = 3;
			adsp2100.idle = 0;

			/* mask all other interrupts */
			adsp2100.imask &= ~0xf;
			return;
		}

		/* check IRQ2 */
		check = (adsp2100.icntl & 4) ? adsp2100.irq_latch[2] : adsp2100.irq_state[2];
		if (check && (adsp2100.imask & 4))
		{
			/* clear the latch */
			adsp2100.irq_latch[2] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling */
			adsp2100.pc = 2;
			adsp2100.idle = 0;

			/* mask other interrupts based on the nesting bit */
			if (adsp2100.icntl & 0x10) adsp2100.imask &= ~0x7;
			else adsp2100.imask &= ~0xf;
			return;
		}

		/* check IRQ1 */
		check = (adsp2100.icntl & 2) ? adsp2100.irq_latch[1] : adsp2100.irq_state[1];
		if (check && (adsp2100.imask & 2))
		{
			/* clear the latch */
			adsp2100.irq_latch[1] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling */
			adsp2100.pc = 1;
			adsp2100.idle = 0;

			/* mask other interrupts based on the nesting bit */
			if (adsp2100.icntl & 0x10) adsp2100.imask &= ~0x3;
			else adsp2100.imask &= ~0xf;
			return;
		}

		/* check IRQ0 */
		check = (adsp2100.icntl & 1) ? adsp2100.irq_latch[0] : adsp2100.irq_state[0];
		if (check && (adsp2100.imask & 1))
		{
			/* clear the latch */
			adsp2100.irq_latch[0] = 0;

			/* push the PC and the status */
			pc_stack_push();
			stat_stack_push();

			/* vector to location & stop idling*/
			adsp2100.pc = 0;
			adsp2100.idle = 0;

			/* mask other interrupts based on the nesting bit */
			if (adsp2100.icntl & 0x10) adsp2100.imask &= ~0x1;
			else adsp2100.imask &= ~0xf;
			return;
		}
	}
}


void adsp2100_set_nmi_line(int state)
{
	/* no NMI line */
}


void adsp2100_set_irq_line(int irqline, int state)
{
	/* update the latched state */
	if (state != CLEAR_LINE && adsp2100.irq_state[irqline] == CLEAR_LINE)
    	adsp2100.irq_latch[irqline] = 1;

    /* update the absolute state */
    adsp2100.irq_state[irqline] = state;

	/* check for IRQs */
    if (state != CLEAR_LINE)
    	check_irqs();
}


void adsp2100_set_irq_callback(int (*callback)(int irqline))
{
	adsp2100.irq_callback = callback;
}



/*###################################################################################################
**	CONTEXT SWITCHING
**#################################################################################################*/

unsigned adsp2100_get_context(void *dst)
{
	/* copy the context */
	if (dst)
		*(adsp2100_Regs *)dst = adsp2100;

	/* return the context size */
	return sizeof(adsp2100_Regs);
}


void adsp2100_set_context(void *src)
{
	/* copy the context */
	if (src)
		adsp2100 = *(adsp2100_Regs *)src;

	/* check for IRQs */
	check_irqs();
}



/*###################################################################################################
**	SPECIAL REGISTER GETTERS AND SETTERS
**#################################################################################################*/

unsigned adsp2100_get_pc(void)
{
	return adsp2100.pc;
}


void adsp2100_set_pc(unsigned val)
{
	adsp2100.pc = val;
}


unsigned adsp2100_get_sp(void)
{
	return adsp2100.pc_sp;
}


void adsp2100_set_sp(unsigned val)
{
	adsp2100.pc_sp = val;
}



/*###################################################################################################
**	INITIALIZATION AND SHUTDOWN
**#################################################################################################*/

void adsp2100_reset(void *param)
{
	/* ensure that zero is zero */
	adsp2100.r[0].zero.u = adsp2100.r[1].zero.u = 0;

	/* recompute the memory registers with their current values */
	wr_l0(adsp2100.l[0]);  wr_i0(adsp2100.i[0]);
	wr_l1(adsp2100.l[1]);  wr_i1(adsp2100.i[1]);
	wr_l2(adsp2100.l[2]);  wr_i2(adsp2100.i[2]);
	wr_l3(adsp2100.l[3]);  wr_i3(adsp2100.i[3]);
	wr_l4(adsp2100.l[4]);  wr_i4(adsp2100.i[4]);
	wr_l5(adsp2100.l[5]);  wr_i5(adsp2100.i[5]);
	wr_l6(adsp2100.l[6]);  wr_i6(adsp2100.i[6]);
	wr_l7(adsp2100.l[7]);  wr_i7(adsp2100.i[7]);

	/* reset PC and loops */
	switch ( chip_type ) {
		case CHIP_TYPE_ADSP2100:
			adsp2100.pc = 4;
		break;

		case CHIP_TYPE_ADSP2105:
			adsp2100.pc = 0;
		break;

		default:
			/*logerror( "ADSP2100 core: Unknown chip type!. Defaulting to ADSP2100.\n" );*/
			adsp2100.pc = 4;
			chip_type = CHIP_TYPE_ADSP2100;
		break;
	}

	adsp2100.ppc = -1;
	adsp2100.loop = 0;
	adsp2100.loop_condition = 0;

	/* reset status registers */
	adsp2100.astat_clear = ~(CFLAG | VFLAG | NFLAG | ZFLAG);
	adsp2100.mstat = 0;
	adsp2100.sstat = 0;
	adsp2100.idle = 0;

	/* reset stacks */
	adsp2100.pc_sp = 0;
	adsp2100.cntr_sp = 0;
	adsp2100.stat_sp = 0;
	adsp2100.loop_sp = 0;

	/* reset external I/O */
	adsp2100.flagout = 0;
	adsp2100.flagin = 0;
#if SUPPORT_2101_EXTENSIONS
	adsp2100.fl0 = 0;
	adsp2100.fl1 = 0;
	adsp2100.fl2 = 0;
#endif

	/* reset interrupts */
	adsp2100.imask = 0;
	adsp2100.irq_state[0] = CLEAR_LINE;
	adsp2100.irq_state[1] = CLEAR_LINE;
	adsp2100.irq_state[2] = CLEAR_LINE;
	adsp2100.irq_state[3] = CLEAR_LINE;
	adsp2100.irq_latch[0] = CLEAR_LINE;
	adsp2100.irq_latch[1] = CLEAR_LINE;
	adsp2100.irq_latch[2] = CLEAR_LINE;
	adsp2100.irq_latch[3] = CLEAR_LINE;
	adsp2100.interrupt_cycles = 0;

	/* create the tables */
	if (!create_tables())
		exit(-1);
}


static int create_tables(void)
{
	int i;

	/* allocate the tables */
	if (!reverse_table)
		reverse_table = (UINT16 *)malloc(0x4000 * sizeof(UINT16));
	if (!mask_table)
		mask_table = (UINT16 *)malloc(0x4000 * sizeof(UINT16));
	if (!condition_table)
		condition_table = (UINT8 *)malloc(0x1000 * sizeof(UINT8));

	/* handle errors */
	if (!reverse_table || !mask_table || !condition_table)
		return 0;

	/* initialize the bit reversing table */
	for (i = 0; i < 0x4000; i++)
	{
		UINT16 data = 0;

		data |= (i >> 13) & 0x0001;
		data |= (i >> 11) & 0x0002;
		data |= (i >> 9)  & 0x0004;
		data |= (i >> 7)  & 0x0008;
		data |= (i >> 5)  & 0x0010;
		data |= (i >> 3)  & 0x0020;
		data |= (i >> 1)  & 0x0040;
		data |= (i << 1)  & 0x0080;
		data |= (i << 3)  & 0x0100;
		data |= (i << 5)  & 0x0200;
		data |= (i << 7)  & 0x0400;
		data |= (i << 9)  & 0x0800;
		data |= (i << 11) & 0x1000;
		data |= (i << 13) & 0x2000;

		reverse_table[i] = data;
	}

	/* initialize the mask table */
	for (i = 0; i < 0x4000; i++)
	{
		     if (i > 0x2000) mask_table[i] = 0x0000;
		else if (i > 0x1000) mask_table[i] = 0x2000;
		else if (i > 0x0800) mask_table[i] = 0x3000;
		else if (i > 0x0400) mask_table[i] = 0x3800;
		else if (i > 0x0200) mask_table[i] = 0x3c00;
		else if (i > 0x0100) mask_table[i] = 0x3e00;
		else if (i > 0x0080) mask_table[i] = 0x3f00;
		else if (i > 0x0040) mask_table[i] = 0x3f80;
		else if (i > 0x0020) mask_table[i] = 0x3fc0;
		else if (i > 0x0010) mask_table[i] = 0x3fe0;
		else if (i > 0x0008) mask_table[i] = 0x3ff0;
		else if (i > 0x0004) mask_table[i] = 0x3ff8;
		else if (i > 0x0002) mask_table[i] = 0x3ffc;
		else if (i > 0x0001) mask_table[i] = 0x3ffe;
		else                 mask_table[i] = 0x3fff;
	}

	/* initialize the condition table */
	for (i = 0; i < 0x100; i++)
	{
		int az = ((i & ZFLAG) != 0);
		int an = ((i & NFLAG) != 0);
		int av = ((i & VFLAG) != 0);
		int ac = ((i & CFLAG) != 0);
		int mv = ((i & MVFLAG) != 0);
		int as = ((i & SFLAG) != 0);

		condition_table[i | 0x000] = az;
		condition_table[i | 0x100] = !az;
		condition_table[i | 0x200] = !((an ^ av) | az);
		condition_table[i | 0x300] = (an ^ av) | az;
		condition_table[i | 0x400] = an ^ av;
		condition_table[i | 0x500] = !(an ^ av);
		condition_table[i | 0x600] = av;
		condition_table[i | 0x700] = !av;
		condition_table[i | 0x800] = ac;
		condition_table[i | 0x900] = !ac;
		condition_table[i | 0xa00] = as;
		condition_table[i | 0xb00] = !as;
		condition_table[i | 0xc00] = mv;
		condition_table[i | 0xd00] = !mv;
		condition_table[i | 0xf00] = 1;
	}
	return 1;
}


void adsp2100_exit(void)
{
	if (reverse_table)
		free(reverse_table);
	reverse_table = NULL;

	if (mask_table)
		free(mask_table);
	mask_table = NULL;

	if (condition_table)
		free(condition_table);
	condition_table = NULL;

#if TRACK_HOTSPOTS
	{
		FILE *log = fopen("adsp.hot", "w");
		while (1)
		{
			int maxindex = 0, i;
			for (i = 1; i < 0x4000; i++)
				if (pcbucket[i] > pcbucket[maxindex])
					maxindex = i;
			if (pcbucket[maxindex] == 0)
				break;
			fprintf(log, "PC=%04X  (%10d hits)\n", maxindex, pcbucket[maxindex]);
			pcbucket[maxindex] = 0;
		}
		fclose(log);
	}
#endif

}



/*###################################################################################################
**	CORE EXECUTION LOOP
**#################################################################################################*/

/* execute instructions on this CPU until icount expires */
int adsp2100_execute(int cycles)
{
	/* reset the core */
	mstat_changed();

	/* count cycles and interrupt cycles */
	adsp2100_icount = cycles;
	adsp2100_icount -= adsp2100.interrupt_cycles;
	adsp2100.interrupt_cycles = 0;

	/* core execution loop */
	do
	{
		UINT32 op, temp;

		/* debugging */
		adsp2100.ppc = adsp2100.pc;	/* copy PC to previous PC */

#if TRACK_HOTSPOTS
		pcbucket[adsp2100.pc & 0x3fff]++;
#endif

		/* instruction fetch */
		op = ROPCODE();

		/* advance to the next instruction */
		if (adsp2100.pc != adsp2100.loop)
			adsp2100.pc++;

		/* handle looping */
		else
		{
			/* condition not met, keep looping */
			if (!CONDITION(adsp2100.loop_condition))
				adsp2100.pc = pc_stack_top();

			/* condition met; pop the PC and loop stacks and fall through */
			else
			{
				loop_stack_pop();
				pc_stack_pop_val();
				adsp2100.pc++;
			}
		}

		/* parse the instruction */
		switch ( ( op >> 16 ) & 0xff )
		{
			case 0x00:
				/* 00000000 00000000 00000000  NOP */
				break;
			case 0x02:
				/* 00000010 0000xxxx xxxxxxxx  modify flag out */
				/* 00000010 10000000 00000000  idle */
				/* 00000010 10000000 0000xxxx  idle (n) */
				if (op & 0x008000)
				{
					adsp2100.idle = 1;
					adsp2100_icount = 0;
				}
				else
				{
					if (CONDITION(op & 15))
					{
						switch ((op >> 4) & 3)
						{
							case 1:	adsp2100.flagout = !adsp2100.flagout;
							case 2: adsp2100.flagout = 0;
							case 3: adsp2100.flagout = 1;
						}
#if SUPPORT_2101_EXTENSIONS
						switch ((op >> 6) & 3)
						{
							case 1:	adsp2100.fl0 = !adsp2100.fl0;
							case 2: adsp2100.fl0 = 0;
							case 3: adsp2100.fl0 = 1;
						}
						switch ((op >> 8) & 3)
						{
							case 1:	adsp2100.fl1 = !adsp2100.fl1;
							case 2: adsp2100.fl1 = 0;
							case 3: adsp2100.fl1 = 1;
						}
						switch ((op >> 10) & 3)
						{
							case 1:	adsp2100.fl2 = !adsp2100.fl2;
							case 2: adsp2100.fl2 = 0;
							case 3: adsp2100.fl2 = 1;
						}
#endif
					}
				}
				break;
			case 0x03:
				/* 00000011 xxxxxxxx xxxxxxxx  call or jump on flag in */
				if (op & 0x000002)
				{
					if (adsp2100.flagin)
					{
						if (op & 0x000001)
							pc_stack_push();
						adsp2100.pc = ((op >> 4) & 0x0fff) | ((op << 10) & 0x3000);
					}
				}
				else
				{
					if (!adsp2100.flagin)
					{
						if (op & 0x000001)
							pc_stack_push();
						adsp2100.pc = ((op >> 4) & 0x0fff) | ((op << 10) & 0x3000);
					}
				}
				break;
			case 0x04:
				/* 00000100 00000000 000xxxxx  stack control */
				if (op & 0x000010) pc_stack_pop_val();
				if (op & 0x000008) loop_stack_pop();
				if (op & 0x000004) cntr_stack_pop();
				if (op & 0x000002)
				{
					if (op & 0x000001) stat_stack_pop();
					else stat_stack_push();
				}
				break;
			case 0x05:
				/* 00000101 00000000 00000000  saturate MR */
				if (GET_MV)
				{
					if (core->mr.mrx.mr2.u & 0x80)
						core->mr.mrx.mr2.u = 0xffff, core->mr.mrx.mr1.u = 0x8000, core->mr.mrx.mr0.u = 0x0000;
					else
						core->mr.mrx.mr2.u = 0x0000, core->mr.mrx.mr1.u = 0x7fff, core->mr.mrx.mr0.u = 0xffff;
				}
				break;
			case 0x06:
				/* 00000110 000xxxxx 00000000  DIVS */
				{
					UINT16 xop = (op >> 8) & 7;
					UINT16 yop = (op >> 11) & 3;

					xop = ALU_GETXREG_UNSIGNED(xop);
					yop = ALU_GETYREG_UNSIGNED(yop);

					temp = xop ^ yop;
					adsp2100.astat = (adsp2100.astat & ~QFLAG) | ((temp >> 10) & QFLAG);
					core->af.u = (yop << 1) | (core->ay0.u >> 15);
					core->ay0.u = (core->ay0.u << 1) | (temp >> 15);
				}
				break;
			case 0x07:
				/* 00000111 00010xxx 00000000  DIVQ */
				{
					UINT16 xop = (op >> 8) & 7;
					UINT16 res;

					xop = ALU_GETXREG_UNSIGNED(xop);

					if (GET_Q)
						res = core->af.u + xop;
					else
						res = core->af.u - xop;

					temp = res ^ xop;
					adsp2100.astat = (adsp2100.astat & ~QFLAG) | ((temp >> 10) & QFLAG);
					core->af.u = (res << 1) | (core->ay0.u >> 15);
					core->ay0.u = (core->ay0.u << 1) | ((~temp >> 15) & 0x0001);
				}
				break;
			case 0x08:
				/* 00001000 00000000 0000xxxx  reserved */
				break;
			case 0x09:
				/* 00001001 00000000 000xxxxx  modify address register */
				temp = (op >> 2) & 4;
				modify_address(temp + ((op >> 2) & 3), temp + (op & 3));
				break;
			case 0x0a:
				/* 00001010 00000000 000xxxxx  conditional return */
				if (CONDITION(op & 15))
				{
					pc_stack_pop();

					/* RTI case */
					if (op & 0x000010)
						stat_stack_pop();
				}
				break;
			case 0x0b:
				/* 00001011 00000000 xxxxxxxx  conditional jump (indirect address) */
				if (CONDITION(op & 15))
				{
					if (op & 0x000010)
						pc_stack_push();
					adsp2100.pc = adsp2100.i[4 + ((op >> 6) & 3)] & 0x3fff;
				}
				break;
			case 0x0c:
				/* 00001100 xxxxxxxx xxxxxxxx  mode control */
#if SUPPORT_2101_EXTENSIONS
				if (op & 0x000008) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_GOMODE) | ((op << 5) & MSTAT_GOMODE);
				if (op & 0x002000) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_INTEGER) | ((op >> 8) & MSTAT_INTEGER);
				if (op & 0x008000) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_TIMER) | ((op >> 9) & MSTAT_TIMER);
#endif
				if (op & 0x000020) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_BANK) | ((op >> 4) & MSTAT_BANK);
				if (op & 0x000080) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_REVERSE) | ((op >> 5) & MSTAT_REVERSE);
				if (op & 0x000200) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_STICKYV) | ((op >> 6) & MSTAT_STICKYV);
				if (op & 0x000800) adsp2100.mstat = (adsp2100.mstat & ~MSTAT_SATURATE) | ((op >> 7) & MSTAT_SATURATE);
				mstat_changed();
				break;
			case 0x0d:
				/* 00001101 0000xxxx xxxxxxxx  internal data move */
				WRITE_REG((op >> 10) & 3, (op >> 4) & 15, READ_REG((op >> 8) & 3, op & 15));
				break;
			case 0x0e:
				/* 00001110 0xxxxxxx xxxxxxxx  conditional shift */
				if (CONDITION(op & 15)) shift_op(op);
				break;
			case 0x0f:
				/* 00001111 0xxxxxxx xxxxxxxx  shift immediate */
				shift_op_imm(op);
				break;
			case 0x10:
				/* 00010000 0xxxxxxx xxxxxxxx  shift with internal data register move */
				shift_op(op);
				temp = READ_REG(0, op & 15);
				WRITE_REG(0, (op >> 4) & 15, temp);
				break;
			case 0x11:
				/* 00010001 xxxxxxxx xxxxxxxx  shift with pgm memory read/write */
				if (op & 0x8000)
				{
					pgm_write_dag2(op, READ_REG(0, (op >> 4) & 15));
					shift_op(op);
				}
				else
				{
					shift_op(op);
					WRITE_REG(0, (op >> 4) & 15, pgm_read_dag2(op));
				}
				break;
			case 0x12:
				/* 00010010 xxxxxxxx xxxxxxxx  shift with data memory read/write DAG1 */
				if (op & 0x8000)
				{
					data_write_dag1(op, READ_REG(0, (op >> 4) & 15));
					shift_op(op);
				}
				else
				{
					shift_op(op);
					WRITE_REG(0, (op >> 4) & 15, data_read_dag1(op));
				}
				break;
			case 0x13:
				/* 00010011 xxxxxxxx xxxxxxxx  shift with data memory read/write DAG2 */
				if (op & 0x8000)
				{
					data_write_dag2(op, READ_REG(0, (op >> 4) & 15));
					shift_op(op);
				}
				else
				{
					shift_op(op);
					WRITE_REG(0, (op >> 4) & 15, data_read_dag2(op));
				}
				break;
			case 0x14: case 0x15: case 0x16: case 0x17:
				/* 000101xx xxxxxxxx xxxxxxxx  do until */
				loop_stack_push(op & 0x3ffff);
				pc_stack_push();
				break;
			case 0x18: case 0x19: case 0x1a: case 0x1b:
				/* 000110xx xxxxxxxx xxxxxxxx  conditional jump (immediate addr) */
				if (CONDITION(op & 15)) adsp2100.pc = (op >> 4) & 0x3fff;
				/* check for a busy loop */
				if ( adsp2100.pc == adsp2100.ppc )
					adsp2100_icount = 0;
				break;
			case 0x1c: case 0x1d: case 0x1e: case 0x1f:
				/* 000111xx xxxxxxxx xxxxxxxx  conditional call (immediate addr) */
				if (CONDITION(op & 15))
				{
					pc_stack_push();
					adsp2100.pc = (op >> 4) & 0x3fff;
				}
				break;
			case 0x20: case 0x21:
				/* 0010000x xxxxxxxx xxxxxxxx  conditional MAC to MR */
				if (CONDITION(op & 15)) mac_op_mr(op);
				break;
			case 0x22: case 0x23:
				/* 0010001x xxxxxxxx xxxxxxxx  conditional ALU to AR */
				if (CONDITION(op & 15)) alu_op_ar(op);
				break;
			case 0x24: case 0x25:
				/* 0010010x xxxxxxxx xxxxxxxx  conditional MAC to MF */
				if (CONDITION(op & 15)) mac_op_mf(op);
				break;
			case 0x26: case 0x27:
				/* 0010011x xxxxxxxx xxxxxxxx  conditional ALU to AF */
				if (CONDITION(op & 15)) alu_op_af(op);
				break;
			case 0x28: case 0x29:
				/* 0010100x xxxxxxxx xxxxxxxx  MAC to MR with internal data register move */
				temp = READ_REG(0, op & 15);
				mac_op_mr(op);
				WRITE_REG(0, (op >> 4) & 15, temp);
				break;
			case 0x2a: case 0x2b:
				/* 0010101x xxxxxxxx xxxxxxxx  ALU to AR with internal data register move */
				temp = READ_REG(0, op & 15);
				alu_op_ar(op);
				WRITE_REG(0, (op >> 4) & 15, temp);
				break;
			case 0x2c: case 0x2d:
				/* 0010110x xxxxxxxx xxxxxxxx  MAC to MF with internal data register move */
				temp = READ_REG(0, op & 15);
				mac_op_mf(op);
				WRITE_REG(0, (op >> 4) & 15, temp);
				break;
			case 0x2e: case 0x2f:
				/* 0010111x xxxxxxxx xxxxxxxx  ALU to AF with internal data register move */
				temp = READ_REG(0, op & 15);
				alu_op_af(op);
				WRITE_REG(0, (op >> 4) & 15, temp);
				break;
			case 0x30: case 0x31: case 0x32: case 0x33:
				/* 001100xx xxxxxxxx xxxxxxxx  load non-data register immediate (group 0) */
				WRITE_REG(0, op & 15, (INT16)(op >> 2) >> 2);
				break;
			case 0x34: case 0x35: case 0x36: case 0x37:
				/* 001101xx xxxxxxxx xxxxxxxx  load non-data register immediate (group 1) */
				WRITE_REG(1, op & 15, (INT16)(op >> 2) >> 2);
				break;
			case 0x38: case 0x39: case 0x3a: case 0x3b:
				/* 001110xx xxxxxxxx xxxxxxxx  load non-data register immediate (group 2) */
				WRITE_REG(2, op & 15, (INT16)(op >> 2) >> 2);
				break;
			case 0x3c: case 0x3d: case 0x3e: case 0x3f:
				/* 001111xx xxxxxxxx xxxxxxxx  load non-data register immediate (group 3) */
				WRITE_REG(3, op & 15, (INT16)(op >> 2) >> 2);
				break;
			case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
			case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
				/* 0100xxxx xxxxxxxx xxxxxxxx  load data register immediate */
				WRITE_REG(0, op & 15, (op >> 4) & 0xffff);
				break;
			case 0x50: case 0x51:
				/* 0101000x xxxxxxxx xxxxxxxx  MAC to MR with pgm memory read */
				mac_op_mr(op);
				WRITE_REG(0, (op >> 4) & 15, pgm_read_dag2(op));
				break;
			case 0x52: case 0x53:
				/* 0101001x xxxxxxxx xxxxxxxx  ALU to AR with pgm memory read */
				alu_op_ar(op);
				WRITE_REG(0, (op >> 4) & 15, pgm_read_dag2(op));
				break;
			case 0x54: case 0x55:
				/* 0101010x xxxxxxxx xxxxxxxx  MAC to MF with pgm memory read */
				mac_op_mf(op);
				WRITE_REG(0, (op >> 4) & 15, pgm_read_dag2(op));
				break;
			case 0x56: case 0x57:
				/* 0101011x xxxxxxxx xxxxxxxx  ALU to AF with pgm memory read */
				alu_op_af(op);
				WRITE_REG(0, (op >> 4) & 15, pgm_read_dag2(op));
				break;
			case 0x58: case 0x59:
				/* 0101100x xxxxxxxx xxxxxxxx  MAC to MR with pgm memory write */
				pgm_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mr(op);
				break;
			case 0x5a: case 0x5b:
				/* 0101101x xxxxxxxx xxxxxxxx  ALU to AR with pgm memory write */
				pgm_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				alu_op_ar(op);
				break;
			case 0x5c: case 0x5d:
				/* 0101110x xxxxxxxx xxxxxxxx  ALU to MR with pgm memory write */
				pgm_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mf(op);
				break;
			case 0x5e: case 0x5f:
				/* 0101111x xxxxxxxx xxxxxxxx  ALU to MF with pgm memory write */
				pgm_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				alu_op_af(op);
				break;
			case 0x60: case 0x61:
				/* 0110000x xxxxxxxx xxxxxxxx  MAC to MR with data memory read DAG1 */
				mac_op_mr(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag1(op));
				break;
			case 0x62: case 0x63:
				/* 0110001x xxxxxxxx xxxxxxxx  ALU to AR with data memory read DAG1 */
				alu_op_ar(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag1(op));
				break;
			case 0x64: case 0x65:
				/* 0110010x xxxxxxxx xxxxxxxx  MAC to MF with data memory read DAG1 */
				mac_op_mf(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag1(op));
				break;
			case 0x66: case 0x67:
				/* 0110011x xxxxxxxx xxxxxxxx  ALU to AF with data memory read DAG1 */
				alu_op_af(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag1(op));
				break;
			case 0x68: case 0x69:
				/* 0110100x xxxxxxxx xxxxxxxx  MAC to MR with data memory write DAG1 */
				data_write_dag1(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mr(op);
				break;
			case 0x6a: case 0x6b:
				/* 0110101x xxxxxxxx xxxxxxxx  ALU to AR with data memory write DAG1 */
				data_write_dag1(op, READ_REG(0, (op >> 4) & 15));
				alu_op_ar(op);
				break;
			case 0x6c: case 0x6d:
				/* 0111110x xxxxxxxx xxxxxxxx  MAC to MF with data memory write DAG1 */
				data_write_dag1(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mf(op);
				break;
			case 0x6e: case 0x6f:
				/* 0111111x xxxxxxxx xxxxxxxx  ALU to AF with data memory write DAG1 */
				data_write_dag1(op, READ_REG(0, (op >> 4) & 15));
				alu_op_af(op);
				break;
			case 0x70: case 0x71:
				/* 0111000x xxxxxxxx xxxxxxxx  MAC to MR with data memory read DAG2 */
				mac_op_mr(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag2(op));
				break;
			case 0x72: case 0x73:
				/* 0111001x xxxxxxxx xxxxxxxx  ALU to AR with data memory read DAG2 */
				alu_op_ar(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag2(op));
				break;
			case 0x74: case 0x75:
				/* 0111010x xxxxxxxx xxxxxxxx  MAC to MF with data memory read DAG2 */
				mac_op_mf(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag2(op));
				break;
			case 0x76: case 0x77:
				/* 0111011x xxxxxxxx xxxxxxxx  ALU to AF with data memory read DAG2 */
				alu_op_af(op);
				WRITE_REG(0, (op >> 4) & 15, data_read_dag2(op));
				break;
			case 0x78: case 0x79:
				/* 0111100x xxxxxxxx xxxxxxxx  MAC to MR with data memory write DAG2 */
				data_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mr(op);
				break;
			case 0x7a: case 0x7b:
				/* 0111101x xxxxxxxx xxxxxxxx  ALU to AR with data memory write DAG2 */
				data_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				alu_op_ar(op);
				break;
			case 0x7c: case 0x7d:
				/* 0111110x xxxxxxxx xxxxxxxx  MAC to MF with data memory write DAG2 */
				data_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				mac_op_mf(op);
				break;
			case 0x7e: case 0x7f:
				/* 0111111x xxxxxxxx xxxxxxxx  ALU to AF with data memory write DAG2 */
				data_write_dag2(op, READ_REG(0, (op >> 4) & 15));
				alu_op_af(op);
				break;
			case 0x80: case 0x81: case 0x82: case 0x83:
				/* 100000xx xxxxxxxx xxxxxxxx  read data memory (immediate addr) to reg group 0 */
				WRITE_REG(0, op & 15, RWORD_DATA((op >> 4) & 0xffff));
				break;
			case 0x84: case 0x85: case 0x86: case 0x87:
				/* 100001xx xxxxxxxx xxxxxxxx  read data memory (immediate addr) to reg group 1 */
				WRITE_REG(1, op & 15, RWORD_DATA((op >> 4) & 0xffff));
				break;
			case 0x88: case 0x89: case 0x8a: case 0x8b:
				/* 100010xx xxxxxxxx xxxxxxxx  read data memory (immediate addr) to reg group 2 */
				WRITE_REG(2, op & 15, RWORD_DATA((op >> 4) & 0xffff));
				break;
			case 0x8c: case 0x8d: case 0x8e: case 0x8f:
				/* 100011xx xxxxxxxx xxxxxxxx  read data memory (immediate addr) to reg group 3 */
				WRITE_REG(3, op & 15, RWORD_DATA((op >> 4) & 0xffff));
				break;
			case 0x90: case 0x91: case 0x92: case 0x93:
				/* 1001xxxx xxxxxxxx xxxxxxxx  write data memory (immediate addr) from reg group 0 */
				WWORD_DATA((op >> 4) & 0x3fff, READ_REG(0, op & 15));
				break;
			case 0x94: case 0x95: case 0x96: case 0x97:
				/* 1001xxxx xxxxxxxx xxxxxxxx  write data memory (immediate addr) from reg group 1 */
				WWORD_DATA((op >> 4) & 0x3fff, READ_REG(1, op & 15));
				break;
			case 0x98: case 0x99: case 0x9a: case 0x9b:
				/* 1001xxxx xxxxxxxx xxxxxxxx  write data memory (immediate addr) from reg group 2 */
				WWORD_DATA((op >> 4) & 0x3fff, READ_REG(2, op & 15));
				break;
			case 0x9c: case 0x9d: case 0x9e: case 0x9f:
				/* 1001xxxx xxxxxxxx xxxxxxxx  write data memory (immediate addr) from reg group 3 */
				WWORD_DATA((op >> 4) & 0x3fff, READ_REG(3, op & 15));
				break;
			case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
			case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
				/* 1010xxxx xxxxxxxx xxxxxxxx  data memory write (immediate) DAG1 */
				data_write_dag1(op, (op >> 4) & 0xffff);
				break;
			case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
				/* 1011xxxx xxxxxxxx xxxxxxxx  data memory write (immediate) DAG2 */
				data_write_dag2(op, (op >> 4) & 0xffff);
				break;
			case 0xc0: case 0xc1:
				/* 1100000x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX0 & pgm read to AY0 */
				mac_op_mr(op);
				core->ax0.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xc2: case 0xc3:
				/* 1100001x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX0 & pgm read to AY0 */
				alu_op_ar(op);
				core->ax0.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xc4: case 0xc5:
				/* 1100010x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX1 & pgm read to AY0 */
				mac_op_mr(op);
				core->ax1.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xc6: case 0xc7:
				/* 1100011x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX1 & pgm read to AY0 */
				alu_op_ar(op);
				core->ax1.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xc8: case 0xc9:
				/* 1100100x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX0 & pgm read to AY0 */
				mac_op_mr(op);
				core->mx0.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xca: case 0xcb:
				/* 1100101x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX0 & pgm read to AY0 */
				alu_op_ar(op);
				core->mx0.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xcc: case 0xcd:
				/* 1100110x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX1 & pgm read to AY0 */
				mac_op_mr(op);
				core->mx1.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xce: case 0xcf:
				/* 1100111x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX1 & pgm read to AY0 */
				alu_op_ar(op);
				core->mx1.u = data_read_dag1(op);
				core->ay0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xd0: case 0xd1:
				/* 1101000x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX0 & pgm read to AY1 */
				mac_op_mr(op);
				core->ax0.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xd2: case 0xd3:
				/* 1101001x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX0 & pgm read to AY1 */
				alu_op_ar(op);
				core->ax0.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xd4: case 0xd5:
				/* 1101010x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX1 & pgm read to AY1 */
				mac_op_mr(op);
				core->ax1.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xd6: case 0xd7:
				/* 1101011x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX1 & pgm read to AY1 */
				alu_op_ar(op);
				core->ax1.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xd8: case 0xd9:
				/* 1101100x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX0 & pgm read to AY1 */
				mac_op_mr(op);
				core->mx0.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xda: case 0xdb:
				/* 1101101x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX0 & pgm read to AY1 */
				alu_op_ar(op);
				core->mx0.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xdc: case 0xdd:
				/* 1101110x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX1 & pgm read to AY1 */
				mac_op_mr(op);
				core->mx1.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xde: case 0xdf:
				/* 1101111x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX1 & pgm read to AY1 */
				alu_op_ar(op);
				core->mx1.u = data_read_dag1(op);
				core->ay1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xe0: case 0xe1:
				/* 1110000x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX0 & pgm read to MY0 */
				mac_op_mr(op);
				core->ax0.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xe2: case 0xe3:
				/* 1110001x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX0 & pgm read to MY0 */
				alu_op_ar(op);
				core->ax0.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xe4: case 0xe5:
				/* 1110010x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX1 & pgm read to MY0 */
				mac_op_mr(op);
				core->ax1.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xe6: case 0xe7:
				/* 1110011x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX1 & pgm read to MY0 */
				alu_op_ar(op);
				core->ax1.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xe8: case 0xe9:
				/* 1110100x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX0 & pgm read to MY0 */
				mac_op_mr(op);
				core->mx0.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xea: case 0xeb:
				/* 1110101x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX0 & pgm read to MY0 */
				alu_op_ar(op);
				core->mx0.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xec: case 0xed:
				/* 1110110x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX1 & pgm read to MY0 */
				mac_op_mr(op);
				core->mx1.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xee: case 0xef:
				/* 1110111x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX1 & pgm read to MY0 */
				alu_op_ar(op);
				core->mx1.u = data_read_dag1(op);
				core->my0.u = pgm_read_dag2(op >> 4);
				break;
			case 0xf0: case 0xf1:
				/* 1111000x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX0 & pgm read to MY1 */
				mac_op_mr(op);
				core->ax0.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xf2: case 0xf3:
				/* 1111001x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX0 & pgm read to MY1 */
				alu_op_ar(op);
				core->ax0.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xf4: case 0xf5:
				/* 1111010x xxxxxxxx xxxxxxxx  MAC to MR with data read to AX1 & pgm read to MY1 */
				mac_op_mr(op);
				core->ax1.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xf6: case 0xf7:
				/* 1111011x xxxxxxxx xxxxxxxx  ALU to AR with data read to AX1 & pgm read to MY1 */
				alu_op_ar(op);
				core->ax1.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xf8: case 0xf9:
				/* 1111100x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX0 & pgm read to MY1 */
				mac_op_mr(op);
				core->mx0.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xfa: case 0xfb:
				/* 1111101x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX0 & pgm read to MY1 */
				alu_op_ar(op);
				core->mx0.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xfc: case 0xfd:
				/* 1111110x xxxxxxxx xxxxxxxx  MAC to MR with data read to MX1 & pgm read to MY1 */
				mac_op_mr(op);
				core->mx1.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
			case 0xfe: case 0xff:
				/* 1111111x xxxxxxxx xxxxxxxx  ALU to AR with data read to MX1 & pgm read to MY1 */
				alu_op_ar(op);
				core->mx1.u = data_read_dag1(op);
				core->my1.u = pgm_read_dag2(op >> 4);
				break;
		}

		adsp2100_icount--;

	} while (adsp2100_icount > 0);

	adsp2100_icount -= adsp2100.interrupt_cycles;
	adsp2100.interrupt_cycles = 0;

	return cycles - adsp2100_icount;
}



/*###################################################################################################
**	REGISTER SNOOP
**#################################################################################################*/

unsigned adsp2100_get_reg(int regnum)
{
	switch (regnum)
	{
		case ADSP2100_PC: return adsp2100.pc;

		case ADSP2100_AX0: return core->ax0.u;
		case ADSP2100_AX1: return core->ax1.u;
		case ADSP2100_AY0: return core->ay0.u;
		case ADSP2100_AY1: return core->ay1.u;
		case ADSP2100_AR: return core->ar.u;
		case ADSP2100_AF: return core->af.u;

		case ADSP2100_MX0: return core->mx0.u;
		case ADSP2100_MX1: return core->mx1.u;
		case ADSP2100_MY0: return core->my0.u;
		case ADSP2100_MY1: return core->my1.u;
		case ADSP2100_MR0: return core->mr.mrx.mr0.u;
		case ADSP2100_MR1: return core->mr.mrx.mr1.u;
		case ADSP2100_MR2: return core->mr.mrx.mr2.u;
		case ADSP2100_MF: return core->mf.u;

		case ADSP2100_SI: return core->si.u;
		case ADSP2100_SE: return core->se.u;
		case ADSP2100_SB: return core->sb.u;
		case ADSP2100_SR0: return core->sr.srx.sr0.u;
		case ADSP2100_SR1: return core->sr.srx.sr1.u;

		case ADSP2100_I0: return adsp2100.i[0];
		case ADSP2100_I1: return adsp2100.i[1];
		case ADSP2100_I2: return adsp2100.i[2];
		case ADSP2100_I3: return adsp2100.i[3];
		case ADSP2100_I4: return adsp2100.i[4];
		case ADSP2100_I5: return adsp2100.i[5];
		case ADSP2100_I6: return adsp2100.i[6];
		case ADSP2100_I7: return adsp2100.i[7];

		case ADSP2100_L0: return adsp2100.l[0];
		case ADSP2100_L1: return adsp2100.l[1];
		case ADSP2100_L2: return adsp2100.l[2];
		case ADSP2100_L3: return adsp2100.l[3];
		case ADSP2100_L4: return adsp2100.l[4];
		case ADSP2100_L5: return adsp2100.l[5];
		case ADSP2100_L6: return adsp2100.l[6];
		case ADSP2100_L7: return adsp2100.l[7];

		case ADSP2100_M0: return adsp2100.m[0];
		case ADSP2100_M1: return adsp2100.m[1];
		case ADSP2100_M2: return adsp2100.m[2];
		case ADSP2100_M3: return adsp2100.m[3];
		case ADSP2100_M4: return adsp2100.m[4];
		case ADSP2100_M5: return adsp2100.m[5];
		case ADSP2100_M6: return adsp2100.m[6];
		case ADSP2100_M7: return adsp2100.m[7];

		case ADSP2100_PX: return adsp2100.px;
		case ADSP2100_CNTR: return adsp2100.cntr;
		case ADSP2100_ASTAT: return adsp2100.astat;
		case ADSP2100_SSTAT: return adsp2100.sstat;
		case ADSP2100_MSTAT: return adsp2100.mstat;

		case ADSP2100_PCSP: return adsp2100.pc_sp;
		case ADSP2100_CNTRSP: return adsp2100.cntr_sp;
		case ADSP2100_STATSP: return adsp2100.stat_sp;
		case ADSP2100_LOOPSP: return adsp2100.loop_sp;

		case ADSP2100_IMASK: return adsp2100.imask;
		case ADSP2100_ICNTL: return adsp2100.icntl;
		case ADSP2100_IRQSTATE0: return adsp2100.irq_state[0];
		case ADSP2100_IRQSTATE1: return adsp2100.irq_state[1];
		case ADSP2100_IRQSTATE2: return adsp2100.irq_state[2];
		case ADSP2100_IRQSTATE3: return adsp2100.irq_state[3];

		case ADSP2100_FLAGIN: return adsp2100.flagin;
		case ADSP2100_FLAGOUT: return adsp2100.flagout;
#if SUPPORT_2101_EXTENSIONS
		case ADSP2100_FL0: return adsp2100.fl0;
		case ADSP2100_FL1: return adsp2100.fl1;
		case ADSP2100_FL2: return adsp2100.fl2;
#endif
		case REG_PREVIOUSPC: return adsp2100.ppc;
		default:
			if (regnum <= REG_SP_CONTENTS)
			{
				unsigned offset = REG_SP_CONTENTS - regnum;
				if (offset < PC_STACK_DEPTH)
					return adsp2100.pc_stack[offset];
			}
	}
	return 0;
}



/*###################################################################################################
**	REGISTER MODIFY
**#################################################################################################*/

void adsp2100_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
		case ADSP2100_PC: adsp2100.pc = val; break;

		case ADSP2100_AX0: wr_ax0(val); break;
		case ADSP2100_AX1: wr_ax1(val); break;
		case ADSP2100_AY0: wr_ay0(val); break;
		case ADSP2100_AY1: wr_ay1(val); break;
		case ADSP2100_AR: wr_ar(val); break;
		case ADSP2100_AF: core->af.u = val; break;

		case ADSP2100_MX0: wr_mx0(val); break;
		case ADSP2100_MX1: wr_mx1(val); break;
		case ADSP2100_MY0: wr_my0(val); break;
		case ADSP2100_MY1: wr_my1(val); break;
		case ADSP2100_MR0: wr_mr0(val); break;
		case ADSP2100_MR1: wr_mr1(val); break;
		case ADSP2100_MR2: wr_mr2(val); break;
		case ADSP2100_MF: core->mf.u = val; break;

		case ADSP2100_SI: wr_si(val); break;
		case ADSP2100_SE: wr_se(val); break;
		case ADSP2100_SB: wr_sb(val); break;
		case ADSP2100_SR0: wr_sr0(val); break;
		case ADSP2100_SR1: wr_sr1(val); break;

		case ADSP2100_I0: wr_i0(val); break;
		case ADSP2100_I1: wr_i1(val); break;
		case ADSP2100_I2: wr_i2(val); break;
		case ADSP2100_I3: wr_i3(val); break;
		case ADSP2100_I4: wr_i4(val); break;
		case ADSP2100_I5: wr_i5(val); break;
		case ADSP2100_I6: wr_i6(val); break;
		case ADSP2100_I7: wr_i7(val); break;

		case ADSP2100_L0: wr_l0(val); break;
		case ADSP2100_L1: wr_l1(val); break;
		case ADSP2100_L2: wr_l2(val); break;
		case ADSP2100_L3: wr_l3(val); break;
		case ADSP2100_L4: wr_l4(val); break;
		case ADSP2100_L5: wr_l5(val); break;
		case ADSP2100_L6: wr_l6(val); break;
		case ADSP2100_L7: wr_l7(val); break;

		case ADSP2100_M0: wr_m0(val); break;
		case ADSP2100_M1: wr_m1(val); break;
		case ADSP2100_M2: wr_m2(val); break;
		case ADSP2100_M3: wr_m3(val); break;
		case ADSP2100_M4: wr_m4(val); break;
		case ADSP2100_M5: wr_m5(val); break;
		case ADSP2100_M6: wr_m6(val); break;
		case ADSP2100_M7: wr_m7(val); break;

		case ADSP2100_PX: wr_px(val); break;
		case ADSP2100_CNTR: adsp2100.cntr = val; break;
		case ADSP2100_ASTAT: wr_astat(val); break;
		case ADSP2100_SSTAT: wr_sstat(val); break;
		case ADSP2100_MSTAT: wr_mstat(val); break;

		case ADSP2100_PCSP: adsp2100.pc_sp = val; break;
		case ADSP2100_CNTRSP: adsp2100.cntr_sp = val; break;
		case ADSP2100_STATSP: adsp2100.stat_sp = val; break;
		case ADSP2100_LOOPSP: adsp2100.loop_sp = val; break;

		case ADSP2100_IMASK: wr_imask(val); break;
		case ADSP2100_ICNTL: wr_icntl(val); break;
		case ADSP2100_IRQSTATE0: adsp2100.irq_state[0] = val; break;
		case ADSP2100_IRQSTATE1: adsp2100.irq_state[1] = val; break;
		case ADSP2100_IRQSTATE2: adsp2100.irq_state[2] = val; break;
		case ADSP2100_IRQSTATE3: adsp2100.irq_state[3] = val; break;

		case ADSP2100_FLAGIN: adsp2100.flagin = val; break;
		case ADSP2100_FLAGOUT: adsp2100.flagout = val; break;
#if SUPPORT_2101_EXTENSIONS
		case ADSP2100_FL0: adsp2100.fl0 = val; break;
		case ADSP2100_FL1: adsp2100.fl1 = val; break;
		case ADSP2100_FL2: adsp2100.fl2 = val; break;
#endif
		default:
			if (regnum <= REG_SP_CONTENTS)
			{
				unsigned offset = REG_SP_CONTENTS - regnum;
				if (offset < PC_STACK_DEPTH)
					adsp2100.pc_stack[offset] = val;
			}
    }
}

/*###################################################################################################
**	DEBUGGER STRINGS
**#################################################################################################*/

const char *adsp2100_info( void *context, int regnum )
{
    switch( regnum )
	{
		case CPU_INFO_NAME: return "ADSP2100";
		case CPU_INFO_FAMILY: return "ADSP2100";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) Aaron Giles 1999";
    }
	return "";
}

unsigned adsp2100_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%06X", RWORD_PGM(pc));
	return 1;
}

#if (HAS_ADSP2105)
/**************************************************************************
 * ADSP2105 section
 **************************************************************************/

void adsp2105_reset(void *param)
{
	chip_type = CHIP_TYPE_ADSP2105;
	adsp2100_reset(param);
}

void adsp2105_exit(void)
{
	adsp2100_exit();
#if SUPPORT_2101_EXTENSIONS
	adsp2105_rx_callback = 0;
	adsp2105_tx_callback = 0;
#endif
}
int adsp2105_execute(int cycles) { return adsp2100_execute(cycles); }
unsigned adsp2105_get_context(void *dst) { return adsp2100_get_context(dst); }
void adsp2105_set_context(void *src)  { adsp2100_set_context(src); }
unsigned adsp2105_get_pc(void) { return adsp2100_get_pc(); }
void adsp2105_set_pc(unsigned val) { adsp2100_set_pc(val); }
unsigned adsp2105_get_sp(void) { return adsp2100_get_sp(); }
void adsp2105_set_sp(unsigned val) { adsp2100_set_sp(val); }
unsigned adsp2105_get_reg(int regnum) { return adsp2100_get_reg(regnum); }
void adsp2105_set_reg(int regnum, unsigned val) { adsp2100_set_reg(regnum,val); }
void adsp2105_set_nmi_line(int state) { adsp2100_set_nmi_line(state); }
void adsp2105_set_irq_line(int irqline, int state) { adsp2100_set_irq_line(irqline,state); }
void adsp2105_set_irq_callback(int (*callback)(int irqline)) { adsp2100_set_irq_callback(callback); }
const char *adsp2105_info(void *context, int regnum)
{
	switch( regnum )
    {
		case CPU_INFO_NAME: return "ADSP2105";
		case CPU_INFO_VERSION: return "1.0";
	}
	return adsp2100_info(context,regnum);
}

unsigned adsp2105_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%06X", RWORD_PGM(pc));
	return 1;
}

#if SUPPORT_2101_EXTENSIONS
void adsp2105_set_rx_callback( RX_CALLBACK cb ) {
	adsp2105_rx_callback = cb;
}

void adsp2105_set_tx_callback( TX_CALLBACK cb ) {
	adsp2105_tx_callback = cb;
}
#endif

#endif
