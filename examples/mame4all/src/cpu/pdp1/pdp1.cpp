/*
 * Note: Original Java source written by:
 *
 * Barry Silverman mailto:barry@disus.com or mailto:bss@media.mit.edu
 * Vadim Gerasimov mailto:vadim@media.mit.edu
 * (not much was changed, only the IOT stuff and the 64 bit integer shifts)
 *
 * Note: I removed the IOT function to be external, it is
 * set at emulation initiation, the IOT function is part of
 * the machine emulation...
 *
 *
 * for the runnable java applet go to:
 * http://lcs.www.media.mit.edu/groups/el/projects/spacewar/
 *
 * for a complete html version of the pdp1 handbook go to:
 * http://www.dbit.com/~greeng3/pdp1/index.html
 *
 * there is another java simulator (by the same people) which runs the
 * original pdp1 LISP interpreter, go to:
 * http://lcs.www.media.mit.edu/groups/el/projects/pdp1
 *
 * and finally, there is a nice article about SPACEWAR!, go to:
 * http://ars-www.uchicago.edu/~eric/lore/spacewar/spacewar.html
 *
 * following is an extract from the handbook:
 *
 * INTRODUCTION
 *
 * The Programmed Data Processor (PDP-1) is a high speed, solid state digital computer designed to
 * operate with many types of input-output devices with no internal machine changes. It is a single
 * address, single instruction, stored program computer with powerful program features. Five-megacycle
 * circuits, a magnetic core memory and fully parallel processing make possible a computation rate of
 * 100,000 additions per second. The PDP-1 is unusually versatile. It is easy to install, operate and
 * maintain. Conventional 110-volt power is used, neither air conditioning nor floor reinforcement is
 * necessary, and preventive maintenance is provided for by built-in marginal checking circuits.
 *
 * PDP-1 circuits are based on the designs of DEC's highly successful and reliable System Modules.
 * Flip-flops and most switches use saturating transistors. Primary active elements are
 * Micro-Alloy-Diffused transistors.
 *
 * The entire computer occupies only 17 square feet of floor space. It consists of four equipment frames,
 * one of which is used as the operating station.
 *
 * CENTRAL PROCESSOR
 *
 * The Central Processor contains the control, arithmetic and memory addressing elements, and the memory
 * buffer register. The word length is 18 binary digits. Instructions are performed in multiples of the
 * memory cycle time of five microseconds. Add, subtract, deposit, and load, for example, are two-cycle
 * instructions requiring 10 microseconds. Multiplication requires and average of 20 microseconds.
 * Program features include: single address instructions, multiple step indirect addressing and logical
 * arithmetic commands. Console features include: flip-flop indicators grouped for convenient octal
 * reading, six program flags for automatic setting and computer sensing, and six sense switches for
 * manual setting and computer sensing.
 *
 * MEMORY SYSTEM
 *
 * The coincident-current, magnetic core memory of a standard PDP-1 holds 4096 words of 18 bits each.
 * Memory capacity may be readily expanded, in increments of 4096 words, to a maximum of 65,536 words.
 * The read-rewrite time of the memory is five microseconds, the basic computer rate. Driving currents
 * are automatically adjusted to compensate for temperature variations between 50 and 110 degrees
 * Fahrenheit. The core memory storage may be supplemented by up to 24 magnetic tape transports.
 *
 * INPUT-OUTPUT
 *
 * PDP-1 is designed to operate a variety of buffered input-output devices. Standard equipment consistes
 * of a perforated tape reader with a read speed of 400 lines per second, and alphanuermic typewriter for
 * on-line operation in both input and output, and a perforated tape punch (alphanumeric or binary) with
 * a speed of 63 lines per second. A variety of optional equipment is available, including the following:
 *
 *     Precision CRT Display Type 30
 *     Ultra-Precision CRT Display Type 31
 *     Symbol Generator Type 33
 *     Light Pen Type 32
 *     Oscilloscope Display Type 34
 *     Card Punch Control Type 40-1
 *     Card Reader and Control Type 421
 *     Magnetic Tape Transport Type 50
 *     Programmed Magnetic Tape Control Type 51
 *     Automatic Magnetic Tape Control Type 52
 *     Automatic Magnetic Tape Control Type 510
 *     Parallel Drum Type 23
 *     Automatic Line Printer and Control Type 64
 *     18-bit Real Time Clock
 *     18-bit Output Relay Buffer Type 140
 *     Multiplexed A-D Converter Type 138/139
 *
 * All in-out operations are performed through the In-Out Register or through the high speed input-output
 * channels.
 *
 * The PDP-1 is also available with the optional Sequence Break System. This is a multi-channel priority
 * interrupt feature which permits concurrent operation of several in-out devices. A one-channel Sequence
 * Break System is included in the standard PDP-1. Optional Sequence Break Systems consist of 16, 32, 64,
 * 128, and 256 channels.
 *
 * ...
 *
 * BASIC INSTRUCTIONS
 *
 *                                                                    OPER. TIME
 * INSTRUCTION  CODE #  EXPLANATION                                     (usec)
 * ------------------------------------------------------------------------------
 * add Y        40      Add C(Y) to C(AC)                                 10
 * and Y        02      Logical AND C(Y) with C(AC)                       10
 * cal Y        16      Equals jda 100                                    10
 * dac Y        24      Deposit C(AC) in Y                                10
 * dap Y        26      Deposit contents of address part of AC in Y       10
 * dio Y        32      Deposit C(IO) in Y                                10
 * dip Y        30      Deposit contents of instruction part of AC in Y   10
 * div Y        56      Divide                                          40 max
 * dzm Y        34      Deposit zero in Y                                 10
 * idx Y        44      Index (add one) C(Y), leave in Y & AC             10
 * ior Y        04      Inclusive OR C(Y) with C(AC)                      10
 * iot Y        72      In-out transfer, see below
 * isp Y        46      Index and skip if result is positive              10
 * jda Y        17      Equals dac Y and jsp Y+1                          10
 * jmp Y        60      Take next instruction from Y                      5
 * jsp Y        62      Jump to Y and save program counter in AC          5
 * lac Y        20      Load the AC with C(Y)                             10
 * law N        70      Load the AC with the number N                     5
 * law-N        71      Load the AC with the number -N                    5
 * lio Y        22      Load IO with C(Y)                                 10
 * mul Y        54      Multiply                                        25 max
 * opr          76      Operate, see below                                5
 * sad Y        50      Skip next instruction if C(AC) <> C(Y)            10
 * sas Y        52      Skip next instruction if C(AC) = C(Y)             10
 * sft          66      Shift, see below                                  5
 * skp          64      Skip, see below                                   5
 * sub Y        42      Subtract C(Y) from C(AC)                          10
 * xct Y        10      Execute instruction in Y                          5+
 * xor Y        06      Exclusive OR C(Y) with C(AC)                      10
 *
 * OPERATE GROUP
 *
 *                                                                    OPER. TIME
 * INSTRUCTION  CODE #   EXPLANATION                                    (usec)
 * ------------------------------------------------------------------------------
 * cla        760200     Clear AC                                         5
 * clf        76000f     Clear selected Program Flag (f = flag #)         5
 * cli        764000     Clear IO                                         5
 * cma        761000     Complement AC                                    5
 * hlt        760400     Halt                                             5
 * lap        760100     Load AC with Program Counter                     5
 * lat        762200     Load AC from Test Word switches                  5
 * nop        760000     No operation                                     5
 * stf        76001f     Set selected Program Flag                        5
 *
 * IN-OUT TRANSFER GROUP
 *
 * PERFORATED TAPE READER
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * rpa        720001     Read Perforated Tape Alphanumeric
 * rpb        720002     Read Perforated Tape Binary
 * rrb        720030     Read Reader Buffer
 *
 * PERFORATED TAPE PUNCH
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * ppa        720005     Punch Perforated Tape Alphanumeric
 * ppb        720006     Punch Perforated Tape Binary
 *
 * ALPHANUMERIC ON-LINE TYPEWRITER
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * tyo        720003     Type Out
 * tyi        720004     Type In
 *
 * SEQUENCE BREAK SYSTEM TYPE 120
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * esm        720055     Enter Sequence Break Mode
 * lsm        720054     Leave Sequence Break Mode
 * cbs        720056     Clear Sequence Break System
 * dsc        72kn50     Deactivate Sequence Break Channel
 * asc        72kn51     Activate Sequence Break Channel
 * isb        72kn52     Initiate Sequence Break
 * cac        720053     Clear All Channels
 *
 * HIGH SPEED DATA CONTROL TYPE 131
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * swc        72x046     Set Word Counter
 * sia        720346     Set Location Counter
 * sdf        720146     Stop Data Flow
 * rlc        720366     Read Location Counter
 * shr        720446     Set High Speed Channel Request
 *
 * PRECISION CRT DISPLAY TYPE 30
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * dpy        720007     Display One Point
 *
 * SYMBOL GENERATOR TYPE 33
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * gpl        722027     Generator Plot Left
 * gpr        720027     Generator Plot Right
 * glf        722026     Load Format
 * gsp        720026     Space
 * sdb        722007     Load Buffer, No Intensity
 *
 * ULTRA-PRECISION CRT DISPLAY TYPE 31
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * dpp        720407     Display One Point on Ultra Precision CRT
 *
 * CARD PUNCH CONTROL TYPE 40-1
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * lag        720044     Load a Group
 * pac        720043     Punch a Card
 *
 * CARD READER TYPE 421
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * rac        720041     Read Card Alpha
 * rbc        720042     Read Card Binary
 * rcc        720032     Read Card Column
 *
 * PROGRAMMED MAGNETIC TAPE CONTROL TYPE 51
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * msm        720073     Select Mode
 * mcs        720034     Check Status
 * mcb        720070     Clear Buffer
 * mwc        720071     Write a Character
 * mrc        720072     Read Character
 *
 * AUTOMATIC MAGNETIC TAPE CONTROL TYPE 52
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * muf        72ue76     Tape Unit and FinalT
 * mic        72ue75     Initial and Command
 * mrf        72u067     Reset Final
 * mri        72ug66     Reset Initial
 * mes        72u035     Examine States
 * mel        72u036     Examine Location
 * inr        72ur67     Initiate a High Speed Channel Request
 * ccr        72s067     Clear Command Register
 *
 * AUTOMATIC MAGNETIC TAPE CONTROL TYPE 510
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * sfc        720072     Skip if Tape Control Free
 * rsr        720172     Read State Register
 * crf        720272     Clear End-of-Record Flip-Flop
 * cpm        720472     Clear Proceed Mode
 * dur        72xx70     Load Density, Unit, Rewind
 * mtf        73xx71     Load Tape Function Register
 * cgo        720073     Clear Go
 *
 * MULTIPLEXED A-D CONVERTER TYPE 138/139
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * rcb        720031     Read Converter Buffer
 * cad        720040     Convert a Voltage
 * scv        72mm47     Select Multiplexer (1 of 64 Channels)
 * icv        720060     Index Multiplexer
 *
 * AUTOMATIC LINE PRINTER TYPE 64
 *
 * INSTRUCTION  CODE #   EXPLANATION
 * ------------------------------------------------------------------------------
 * clrbuf     722045     Clear Buffer
 * lpb        720045     Load Printer Buffer
 * pas        721x45     Print and Space
 *
 * SKIP GROUP
 *
 *                                                                    OPER. TIME
 * INSTRUCTION  CODE #   EXPLANATION                                    (usec)
 * ------------------------------------------------------------------------------
 * sma        640400     Dkip on minus AC                                 5
 * spa        640200     Skip on plus AC                                  5
 * spi        642000     Skip on plus IO                                  5
 * sza        640100     Skip on ZERO (+0) AC                             5
 * szf        6400f      Skip on ZERO flag                                5
 * szo        641000     Skip on ZERO overflow (and clear overflow)       5
 * szs        6400s0     Skip on ZERO sense switch                        5
 *
 * SHIFT/ROTATE GROUP
 *
 *                                                                      OPER. TIME
 * INSTRUCTION  CODE #   EXPLANATION                                      (usec)
 * ------------------------------------------------------------------------------
 *   ral        661      Rotate AC left                                     5
 *   rar        671      Rotate AC right                                    5
 *   rcl        663      Rotate Combined AC & IO left                       5
 *   rcr        673      Rotate Combined AC & IO right                      5
 *   ril        662      Rotate IO left                                     5
 *   rir        672      Rotate IO right                                    5
 *   sal        665      Shift AC left                                      5
 *   sar        675      Shift AC right                                     5
 *   scl        667      Shift Combined AC & IO left                        5
 *   scr        677      Shift Combined AC & IO right                       5
 *   sil        666      Shift IO left                                      5
 *   sir        676      Shift IO right                                     5
 */

#include <stdio.h>
#include <stdlib.h>
#include "driver.h"
#include "pdp1.h"

#define READ_PDP_18BIT(A) ((signed)cpu_readmem16(A))
#define WRITE_PDP_18BIT(A,V) (cpu_writemem16(A,V))

int intern_iot (int *io, int md);
int (*extern_iot) (int *, int) = intern_iot;
int execute_instruction (int md);

/* PDP1 Registers */
typedef struct
{
	UINT32 pc;
	int ac;
	int io;
	int y;
	int ib;
	int ov;
	int f;
	int flag[8];
	int sense[8];
}
pdp1_Regs;


static pdp1_Regs pdp1;

#define PC		pdp1.pc
#define AC		pdp1.ac
#define IO		pdp1.io
#define Y		pdp1.y
#define IB		pdp1.ib
#define OV		pdp1.ov
#define F		pdp1.f
#define FLAG    pdp1.flag
#define F1		pdp1.flag[1]
#define F2		pdp1.flag[2]
#define F3		pdp1.flag[3]
#define F4		pdp1.flag[4]
#define F5		pdp1.flag[5]
#define F6		pdp1.flag[6]
#define SENSE   pdp1.sense
#define S1		pdp1.sense[1]
#define S2		pdp1.sense[2]
#define S3		pdp1.sense[3]
#define S4		pdp1.sense[4]
#define S5		pdp1.sense[5]
#define S6		pdp1.sense[6]

/* not only 6 flags/senses, but we start counting at 1 */

/* public globals */
signed int pdp1_ICount = 50000;

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
unsigned pdp1_get_pc (void)
{
	return PC;
}

void pdp1_set_pc (UINT32 newpc)
{
	PC = newpc;
}

unsigned pdp1_get_sp (void)
{
	/* nothing to do */
	return 0;
}

void pdp1_set_sp (UINT32 newsp)
{
	/* nothing to do */
}

void pdp1_set_nmi_line (int state)
{
	/* no NMI line */
}

void pdp1_set_irq_line (int irqline, int state)
{
	/* no IRQ line */
}

void pdp1_set_irq_callback (int (*callback) (int irqline))
{
	/* no IRQ line */
}


void pdp1_reset (void *param)
{
	memset (&pdp1, 0, sizeof (pdp1));
	PC = 4;
	SENSE[5] = 1;					   /* for lisp... typewriter input */
}

void pdp1_exit (void)
{
	/* nothing to do */
}

unsigned pdp1_get_context (void *dst)
{
	if (dst)
		*(pdp1_Regs *) dst = pdp1;
	return sizeof (pdp1_Regs);
}

void pdp1_set_context (void *src)
{
	if (src)
		pdp1 = *(pdp1_Regs *) src;
}

unsigned pdp1_get_reg (int regnum)
{
	switch (regnum)
	{
	case PDP1_PC: return PC;
	case PDP1_AC: return AC;
	case PDP1_IO: return IO;
	case PDP1_Y:  return Y;
	case PDP1_IB: return IB;
	case PDP1_OV: return OV;
	case PDP1_F:  return F;
	case PDP1_F1: return F1;
	case PDP1_F2: return F2;
	case PDP1_F3: return F3;
	case PDP1_F4: return F4;
	case PDP1_F5: return F5;
	case PDP1_F6: return F6;
	case PDP1_S1: return S1;
	case PDP1_S2: return S2;
	case PDP1_S3: return S3;
	case PDP1_S4: return S4;
	case PDP1_S5: return S5;
	case PDP1_S6: return S6;
	}
	return 0;
}

void pdp1_set_reg (int regnum, unsigned val)
{
	switch (regnum)
	{
	case PDP1_PC: PC = val; break;
	case PDP1_AC: AC = val; break;
	case PDP1_IO: IO = val; break;
	case PDP1_Y:  Y  = val; break;
	case PDP1_IB: IB = val; break;
	case PDP1_OV: OV = val; break;
	case PDP1_F:  F  = val; break;
	case PDP1_F1: F1 = val; break;
	case PDP1_F2: F2 = val; break;
	case PDP1_F3: F3 = val; break;
	case PDP1_F4: F4 = val; break;
	case PDP1_F5: F5 = val; break;
	case PDP1_F6: F6 = val; break;
	case PDP1_S1: S1 = val; break;
	case PDP1_S2: S2 = val; break;
	case PDP1_S3: S3 = val; break;
	case PDP1_S4: S4 = val; break;
	case PDP1_S5: S5 = val; break;
	case PDP1_S6: S6 = val; break;
	}
}

/* execute instructions on this CPU until icount expires */
int pdp1_execute (int cycles)
{
	int word18;

	pdp1_ICount = cycles;

	do
	{

		word18 = READ_PDP_18BIT (PC++);
/*
		logerror("PC:0%06o ",PC-1);
		logerror("I:0%06o  ",word18);
		logerror("1:%i "    ,FLAG[1]);
		logerror("2:%i "    ,FLAG[2]);
		logerror("3:%i "    ,FLAG[3]);
		logerror("4:%i "    ,FLAG[4]);
		logerror("5:%i "    ,FLAG[5]);
		logerror("6:%i \n"  ,FLAG[6]);
*/
		pdp1_ICount -= execute_instruction (word18);

	}
	while (pdp1_ICount > 0);

	return cycles - pdp1_ICount;
}

unsigned pdp1_dasm (char *buffer, unsigned pc)
{
	sprintf (buffer, "0%06o", READ_PDP_18BIT (pc));
	return 1;
}

static int etime = 0;
INLINE void ea (void)
{
	while (1)
	{
		if (IB == 0)
			return;

		etime += 5;
		IB = (READ_PDP_18BIT (Y) >> 12) & 1;
		Y = READ_PDP_18BIT (Y) & 07777;
		if (etime > 100)
		{
			logerror("Massive indirection (>20), assuming emulator fault... bye at:\n");
			logerror("PC:0%06o Y=0%06o\n", PC - 1, Y);
			exit (1);
		}
	}
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/
const char *pdp1_info (void *context, int regnum)
{
	switch (regnum)
	{
	case CPU_INFO_NAME: return "PDP1";
	case CPU_INFO_FAMILY: return "DEC PDP-1";
	case CPU_INFO_VERSION: return "1.1";
	case CPU_INFO_FILE: return __FILE__;
	case CPU_INFO_CREDITS: return
			"Brian Silverman (original Java Source)\n"
			"Vadim Gerasimov (original Java Source)\n"
			"Chris Salomon (MESS driver)\n";
	}
	return "";
}


int execute_instruction (int md)
{
	etime = 0;
	Y = md & 07777;
	IB = (md >> 12) & 1;			   /* */
	switch (md >> 13)
	{
	case AND:
		ea ();
		AC &= READ_PDP_18BIT (Y);
		etime += 10;
		break;
	case IOR:
		ea ();
		AC |= READ_PDP_18BIT (Y);
		etime += 10;
		break;
	case XOR:
		ea ();
		AC ^= READ_PDP_18BIT (Y);
		etime += 10;
		break;
	case XCT:
		ea ();
		etime += 5 + execute_instruction (READ_PDP_18BIT (Y));
		break;
	case CALJDA:
		{
			int target = (IB == 0) ? 64 : Y;

			WRITE_PDP_18BIT (target, AC);
			AC = (OV << 17) + PC;
			PC = target + 1;
			etime += 10;
			break;
		}
	case LAC:
		ea ();
		AC = READ_PDP_18BIT (Y);
		etime += 10;
		break;
	case LIO:
		ea ();
		IO = READ_PDP_18BIT (Y);
		etime += 10;
		break;
	case DAC:
		ea ();
		WRITE_PDP_18BIT (Y, AC);
		etime += 10;
		break;
	case DAP:
		ea ();
		WRITE_PDP_18BIT (Y, (READ_PDP_18BIT (Y) & 0770000) + (AC & 07777));
		etime += 10;
		break;
	case DIO:
		ea ();
		WRITE_PDP_18BIT (Y, IO);
		etime += 10;
		break;
	case DZM:
		ea ();
		WRITE_PDP_18BIT (Y, 0);
		etime += 10;
		break;
	case ADD:
		ea ();
		AC = AC + READ_PDP_18BIT (Y);
		OV = AC >> 18;
		AC = (AC + OV) & 0777777;
		if (AC == 0777777)
			AC = 0;
		etime += 10;
		break;
	case SUB:
		{
			int diffsigns;

			ea ();
			diffsigns = ((AC >> 17) ^ (READ_PDP_18BIT (Y) >> 17)) == 1;
			AC = AC + (READ_PDP_18BIT (Y) ^ 0777777);
			AC = (AC + (AC >> 18)) & 0777777;
			if (AC == 0777777)
				AC = 0;
			if (diffsigns && (READ_PDP_18BIT (Y) >> 17 == AC >> 17))
				OV = 1;
			etime += 10;
			break;
		}
	case IDX:
		ea ();
		AC = READ_PDP_18BIT (Y) + 1;
		if (AC == 0777777)
			AC = 0;
		WRITE_PDP_18BIT (Y, AC);
		etime += 10;
		break;
	case ISP:
		ea ();
		AC = READ_PDP_18BIT (Y) + 1;
		if (AC == 0777777)
			AC = 0;
		WRITE_PDP_18BIT (Y, AC);
		if ((AC & 0400000) == 0)
			PC++;
		etime += 10;
		break;
	case SAD:
		ea ();
		if (AC != READ_PDP_18BIT (Y))
			PC++;
		etime += 10;
		break;
	case SAS:
		ea ();
		if (AC == READ_PDP_18BIT (Y))
			PC++;
		etime += 10;
		break;
	case MUS:
		ea ();
		if ((IO & 1) == 1)
		{
			AC = AC + READ_PDP_18BIT (Y);
			AC = (AC + (AC >> 18)) & 0777777;
			if (AC == 0777777)
				AC = 0;
		}
		IO = (IO >> 1 | AC << 17) & 0777777;
		AC >>= 1;
		etime += 10;
		break;
	case DIS:
		{
			int acl;

			ea ();
			acl = AC >> 17;
			AC = (AC << 1 | IO >> 17) & 0777777;
			IO = ((IO << 1 | acl) & 0777777) ^ 1;
			if ((IO & 1) == 1)
			{
				AC = AC + (READ_PDP_18BIT (Y) ^ 0777777);
				AC = (AC + (AC >> 18)) & 0777777;
			}
			else
			{
				AC = AC + 1 + READ_PDP_18BIT (Y);
				AC = (AC + (AC >> 18)) & 0777777;
			}
			if (AC == 0777777)
				AC = 0;
			etime += 10;
			break;
		}
	case JMP:
		ea ();
		PC = Y;
		etime += 5;
		break;
	case JSP:
		ea ();
		AC = (OV << 17) + PC;
		PC = Y;
		etime += 5;
		break;
	case SKP:
		{
			int cond = (((Y & 0100) == 0100) && (AC == 0)) ||
			(((Y & 0200) == 0200) && (AC >> 17 == 0)) ||
			(((Y & 0400) == 0400) && (AC >> 17 == 1)) ||
			(((Y & 01000) == 01000) && (OV == 0)) ||
			(((Y & 02000) == 02000) && (IO >> 17 == 0)) ||
			(((Y & 7) != 0) && !FLAG[Y & 7]) ||
			(((Y & 070) != 0) && !SENSE[(Y & 070) >> 3]) ||
			((Y & 070) == 010);

			if (IB == 0)
			{
				if (cond)
					PC++;
			}
			else
			{
				if (!cond)
					PC++;
			}
			if ((Y & 01000) == 01000)
				OV = 0;
			etime += 5;
			break;
		}
	case SFT:
		{
			int nshift = 0;
			int mask = md & 0777;

			while (mask != 0)
			{
				nshift += mask & 1;
				mask = mask >> 1;
			}
			switch ((md >> 9) & 017)
			{
				int i;

			case 1:
				for (i = 0; i < nshift; i++)
					AC = (AC << 1 | AC >> 17) & 0777777;
				break;
			case 2:
				for (i = 0; i < nshift; i++)
					IO = (IO << 1 | IO >> 17) & 0777777;
				break;
			case 3:
				for (i = 0; i < nshift; i++)
				{
					int tmp = AC;

					AC = (AC << 1 | IO >> 17) & 0777777;
					IO = (IO << 1 | tmp >> 17) & 0777777;
				}
				break;
			case 5:
				for (i = 0; i < nshift; i++)
					AC = ((AC << 1 | AC >> 17) & 0377777) + (AC & 0400000);
				break;
			case 6:
				for (i = 0; i < nshift; i++)
					IO = ((IO << 1 | IO >> 17) & 0377777) + (IO & 0400000);
				break;
			case 7:
				for (i = 0; i < nshift; i++)
				{
					int tmp = AC;

					AC = ((AC << 1 | IO >> 17) & 0377777) + (AC & 0400000);		/* shouldn't that be IO?, no it is the sign! */
					IO = (IO << 1 | tmp >> 17) & 0777777;
				}
				break;
			case 9:
				for (i = 0; i < nshift; i++)
					AC = (AC >> 1 | AC << 17) & 0777777;
				break;
			case 10:
				for (i = 0; i < nshift; i++)
					IO = (IO >> 1 | IO << 17) & 0777777;
				break;
			case 11:
				for (i = 0; i < nshift; i++)
				{
					int tmp = AC;

					AC = (AC >> 1 | IO << 17) & 0777777;
					IO = (IO >> 1 | tmp << 17) & 0777777;
				}
				break;
			case 13:
				for (i = 0; i < nshift; i++)
					AC = (AC >> 1) + (AC & 0400000);
				break;
			case 14:
				for (i = 0; i < nshift; i++)
					IO = (IO >> 1) + (IO & 0400000);
				break;
			case 15:
				for (i = 0; i < nshift; i++)
				{
					int tmp = AC;

					AC = (AC >> 1) + (AC & 0400000);	/* shouldn't that be IO, no it is the sign */
					IO = (IO >> 1 | tmp << 17) & 0777777;
				}
				break;
			default:
				logerror("Undefined shift: ");
				logerror("0%06o at ", md);
				logerror("0%06o\n", PC - 1);
				exit (1);
			}
			etime += 5;
			break;
		}
	case LAW:
		AC = (IB == 0) ? Y : Y ^ 0777777;
		etime += 5;
		break;
	case IOT:
		etime += extern_iot (&IO, md);
		break;
	case OPR:
		{
			int nflag;
			int state;
			int i;

			etime += 5;
			if ((Y & 0200) == 0200)
				AC = 0;
			if ((Y & 04000) == 04000)
				IO = 0;
			if ((Y & 01000) == 01000)
				AC ^= 0777777;
			if ((Y & 0400) == 0400)
			{
				/* ignored till I emulate the extention switches... with
				 * continue...
				 */
				//logerror("PDP1 Program executed HALT: at ");
				//logerror("0%06o\n", PC - 1);
				//logerror("HALT ignored...\n");
				/* exit(1); */
			}
			nflag = Y & 7;
			if (nflag < 1)			   /* was 2 */
				break;
			state = (Y & 010) == 010;
			if (nflag == 7)
			{
				for (i = 1; i < 7; i++)		/* was 2 */
				{
					FLAG[i] = state;
				}
				break;
			}
			FLAG[nflag] = state;
			F = 0;
			for (i = 1; i < 7; i++)
			{
				F += (1 << (i - 1)) * (FLAG[i] != 0);
			}
			break;
		}
	default:
		logerror("Undefined instruction: ");
		logerror("0%06o at ", md);
		logerror("0%06o\n", PC - 1);
		exit (1);
	}
	return etime;
}

int intern_iot (int *iio, int md)
{
	logerror("No external IOT function given (IO=0%06o) -> EXIT(1) invoked in PDP1\\PDP1.C\n", *iio);
	exit (1);
}
