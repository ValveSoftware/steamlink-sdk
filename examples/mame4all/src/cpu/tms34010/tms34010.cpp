/*###################################################################################################
**
**	TMS34010: Portable Texas Instruments TMS34010 emulator
**
**	Copyright (C) Alex Pasadyn/Zsolt Vasvari 1998
**	 Parts based on code by Aaron Giles
**
**#################################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include "driver.h"
#include "osd_cpu.h"
#include "cpuintrf.h"
#include "tms34010.h"
#include "34010ops.h"

/************************************

New 34020 ops:

	0000 1100 000R dddd = ADDXYI IL,Rd
	iiii iiii iiii iiii
	iiii iiii iiii iiii
	
	0000 0000 1111 00SD = BLMOVE S,D
	
	0000 0110 0000 0000 = CEXEC S,c,ID,L
	cccc cccc S000 0000
	iiic cccc cccc cccc

	1101 1000 0ccc cccS = CEXEC S,c,ID
	iiic cccc cccc cccc
	
	0000 1000 1111 0010 = CLIP
	
	0000 0110 011R dddd = CMOVCG Rd1,Rd2,S,c,ID
	cccc cccc S00R dddd
	iiic cccc cccc cccc
	
	0000 0110 101R dddd = CMOVCM *Rd+,n,S,c,ID
	cccc cccc S00n nnnn
	iiic cccc cccc cccc
	
	0000 0110 110R dddd = CMOVCM -*Rd,n,S,c,ID
	cccc cccc S00n nnnn
	iiic cccc cccc cccc
	
	0000 0110 0110 0000 = CMOVCS c,ID
	cccc cccc 0000 0001
	iiic cccc cccc cccc
	
	0000 0110 001R ssss = CMOVGC Rs,c,ID
	cccc cccc 0000 0000
	iiic cccc cccc cccc
	
	0000 0110 010R ssss = CMOVGC Rs1,Rs2,S,c,ID
	cccc cccc S00R ssss
	iiic cccc cccc cccc
	
	0000 0110 100n nnnn = CMOVMC *Rs+,n,S,c,ID
	cccc cccc S00R ssss
	iiic cccc cccc cccc
	
	0000 1000 001n nnnn = CMOVMC -*Rs,n,S,c,ID
	cccc cccc S00R ssss
	iiic cccc cccc cccc
	
	0000 0110 111R dddd = CMOVMC *Rs+,Rd,S,c,ID
	cccc cccc S00R ssss
	iiic cccc cccc cccc
	
	0011 01kk kkkR dddd = CMPK k,Rd
	
	0000 1010 100R dddd = CVDXYL Rd
	
	0000 1010 011R dddd = CVMXYL Rd
	
	1110 101s sssR dddd = CVSXYL Rs,Rd
	
	0000 0010 101R dddd = EXGPS Rd
	
	1101 1110 Z001 1010 = FLINE Z
	
	0000 1010 1011 1011 = FPIXEQ
	
	0000 1010 1101 1011 = FPIXNE
	
	0000 0010 110R dddd = GETPS Rd
	
	0000 0000 0100 0000 = IDLE
	
	0000 1100 0101 0111 = LINIT
	
	0000 0000 1000 0000 = MWAIT
	
	0000 1010 0011 0111 = PFILL XY
	
	0000 1110 0001 0111 = PIXBLT L,M,L
	
	0000 1000 0110 0000 = RETM
	
	0111 101s sssR dddd = RMO Rs,Rd
	
	0000 0010 100R dddd = RPIX Rd
	
	0000 0010 0111 0011 = SETCDP
	
	0000 0010 1111 1011 = SETCMP
	
	0000 0010 0101 0001 = SETCSP
	
	0111 111s sssR dddd = SWAPF *Rs,Rd,0
	
	0000 1110 1111 1010 = TFILL XY
	
	0000 1000 0000 1111 = TRAPL
	
	0000 1000 0101 0111 = VBLT B,L
	
	0000 1010 0101 0111 = VFILL L
	
	0000 1010 0000 0000 = VLCOL

************************************/


/*###################################################################################################
**	DEBUG STATE & STRUCTURES
**#################################################################################################*/

#define VERBOSE 			0
#define LOG_CONTROL_REGS	0
#define LOG_GRAPHICS_OPS	0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

/*###################################################################################################
**	CORE STATE
**#################################################################################################*/

/* TMS34010 State */
typedef struct
{
#if LSB_FIRST
	INT16 x;
	INT16 y;
#else
	INT16 y;
	INT16 x;
#endif
} XY;

typedef struct
{
	UINT32 op;
	UINT32 pc;
	UINT32 st;					/* Only here so we can display it in the debug window */
	union						/* The register files are interleaved, so */
	{							/* that the SP occupies the same location in both */
		INT32 Bregs[241];   	/* Only every 16th entry is actually used */
		XY BregsXY[241];
		struct
		{
			INT32 unused[225];
			union
			{
				INT32 Aregs[16];
				XY AregsXY[16];
			} a;
		} a;
	} regs;
	UINT32 nflag;
	UINT32 cflag;
	UINT32 notzflag;  /* So we can just do an assignment to set it */
	UINT32 vflag;
	UINT32 pflag;
	UINT32 ieflag;
	UINT32 fe0flag;
	UINT32 fe1flag;
	UINT32 fw[2];
	UINT32 fw_inc[2];  /* Same as fw[], except when fw = 0, fw_inc = 32 */
	UINT16 IOregs[32];
	UINT32 reset_deferred;
	UINT32 is_34020;
	mem_write_handler f0_write;
	mem_write_handler f1_write;
	mem_write_handler pixel_write;
	mem_read_handler f0_read;
	mem_read_handler f1_read;
	mem_read_handler pixel_read;
	UINT32 transparency;
	UINT32 window_checking;
	 INT32 (*raster_op)(INT32 newpix, INT32 oldpix);
	UINT32 xytolshiftcount1;
	UINT32 xytolshiftcount2;
	UINT16 *shiftreg;
	int (*irq_callback)(int irqline);
	int last_update_vcount;
	struct tms34010_config *config;
} tms34010_regs;



/*###################################################################################################
**	GLOBAL VARIABLES
**#################################################################################################*/

/* public globals */
int	tms34010_ICount;

/* internal state */
static tms34010_regs 	state;
static tms34010_regs *	host_interface_context;
static UINT8 			host_interface_cpu;
static void *			dpyint_timer[MAX_CPU];		  /* Display interrupt timer */
static void *			vsblnk_timer[MAX_CPU];		  /* VBLANK start timer */

/* default configuration */
static struct tms34010_config default_config =
{
	0,					/* don't halt on reset */
	NULL,				/* no interrupt callback */
	NULL,				/* no shiftreg functions */
	NULL				/* no shiftreg functions */
};

static void check_interrupt(void);

static timer_tm interval=0;

/*###################################################################################################
**	FUNCTION TABLES
**#################################################################################################*/

#include "34010fld.cpp"

extern WRITE_HANDLER( wfield_01 );
extern WRITE_HANDLER( wfield_02 );
extern WRITE_HANDLER( wfield_03 );
extern WRITE_HANDLER( wfield_04 );
extern WRITE_HANDLER( wfield_05 );
extern WRITE_HANDLER( wfield_06 );
extern WRITE_HANDLER( wfield_07 );
extern WRITE_HANDLER( wfield_08 );
extern WRITE_HANDLER( wfield_09 );
extern WRITE_HANDLER( wfield_10 );
extern WRITE_HANDLER( wfield_11 );
extern WRITE_HANDLER( wfield_12 );
extern WRITE_HANDLER( wfield_13 );
extern WRITE_HANDLER( wfield_14 );
extern WRITE_HANDLER( wfield_15 );
extern WRITE_HANDLER( wfield_16 );
extern WRITE_HANDLER( wfield_17 );
extern WRITE_HANDLER( wfield_18 );
extern WRITE_HANDLER( wfield_19 );
extern WRITE_HANDLER( wfield_20 );
extern WRITE_HANDLER( wfield_21 );
extern WRITE_HANDLER( wfield_22 );
extern WRITE_HANDLER( wfield_23 );
extern WRITE_HANDLER( wfield_24 );
extern WRITE_HANDLER( wfield_25 );
extern WRITE_HANDLER( wfield_26 );
extern WRITE_HANDLER( wfield_27 );
extern WRITE_HANDLER( wfield_28 );
extern WRITE_HANDLER( wfield_29 );
extern WRITE_HANDLER( wfield_30 );
extern WRITE_HANDLER( wfield_31 );
extern WRITE_HANDLER( wfield_32 );

static mem_write_handler wfield_functions[32] =
{
	wfield_32, wfield_01, wfield_02, wfield_03, wfield_04, wfield_05,
	wfield_06, wfield_07, wfield_08, wfield_09, wfield_10, wfield_11,
	wfield_12, wfield_13, wfield_14, wfield_15, wfield_16, wfield_17,
	wfield_18, wfield_19, wfield_20, wfield_21, wfield_22, wfield_23,
	wfield_24, wfield_25, wfield_26, wfield_27, wfield_28, wfield_29,
	wfield_30, wfield_31
};

extern READ_HANDLER( rfield_z_01 );
extern READ_HANDLER( rfield_z_02 );
extern READ_HANDLER( rfield_z_03 );
extern READ_HANDLER( rfield_z_04 );
extern READ_HANDLER( rfield_z_05 );
extern READ_HANDLER( rfield_z_06 );
extern READ_HANDLER( rfield_z_07 );
extern READ_HANDLER( rfield_z_08 );
extern READ_HANDLER( rfield_z_09 );
extern READ_HANDLER( rfield_z_10 );
extern READ_HANDLER( rfield_z_11 );
extern READ_HANDLER( rfield_z_12 );
extern READ_HANDLER( rfield_z_13 );
extern READ_HANDLER( rfield_z_14 );
extern READ_HANDLER( rfield_z_15 );
extern READ_HANDLER( rfield_z_16 );
extern READ_HANDLER( rfield_z_17 );
extern READ_HANDLER( rfield_z_18 );
extern READ_HANDLER( rfield_z_19 );
extern READ_HANDLER( rfield_z_20 );
extern READ_HANDLER( rfield_z_21 );
extern READ_HANDLER( rfield_z_22 );
extern READ_HANDLER( rfield_z_23 );
extern READ_HANDLER( rfield_z_24 );
extern READ_HANDLER( rfield_z_25 );
extern READ_HANDLER( rfield_z_26 );
extern READ_HANDLER( rfield_z_27 );
extern READ_HANDLER( rfield_z_28 );
extern READ_HANDLER( rfield_z_29 );
extern READ_HANDLER( rfield_z_30 );
extern READ_HANDLER( rfield_z_31 );
extern READ_HANDLER( rfield_32 );

static mem_read_handler rfield_functions_z[32] =
{
	rfield_32  , rfield_z_01, rfield_z_02, rfield_z_03, rfield_z_04, rfield_z_05,
	rfield_z_06, rfield_z_07, rfield_z_08, rfield_z_09, rfield_z_10, rfield_z_11,
	rfield_z_12, rfield_z_13, rfield_z_14, rfield_z_15, rfield_z_16, rfield_z_17,
	rfield_z_18, rfield_z_19, rfield_z_20, rfield_z_21, rfield_z_22, rfield_z_23,
	rfield_z_24, rfield_z_25, rfield_z_26, rfield_z_27, rfield_z_28, rfield_z_29,
	rfield_z_30, rfield_z_31
};

extern READ_HANDLER( rfield_s_01 );
extern READ_HANDLER( rfield_s_02 );
extern READ_HANDLER( rfield_s_03 );
extern READ_HANDLER( rfield_s_04 );
extern READ_HANDLER( rfield_s_05 );
extern READ_HANDLER( rfield_s_06 );
extern READ_HANDLER( rfield_s_07 );
extern READ_HANDLER( rfield_s_08 );
extern READ_HANDLER( rfield_s_09 );
extern READ_HANDLER( rfield_s_10 );
extern READ_HANDLER( rfield_s_11 );
extern READ_HANDLER( rfield_s_12 );
extern READ_HANDLER( rfield_s_13 );
extern READ_HANDLER( rfield_s_14 );
extern READ_HANDLER( rfield_s_15 );
extern READ_HANDLER( rfield_s_16 );
extern READ_HANDLER( rfield_s_17 );
extern READ_HANDLER( rfield_s_18 );
extern READ_HANDLER( rfield_s_19 );
extern READ_HANDLER( rfield_s_20 );
extern READ_HANDLER( rfield_s_21 );
extern READ_HANDLER( rfield_s_22 );
extern READ_HANDLER( rfield_s_23 );
extern READ_HANDLER( rfield_s_24 );
extern READ_HANDLER( rfield_s_25 );
extern READ_HANDLER( rfield_s_26 );
extern READ_HANDLER( rfield_s_27 );
extern READ_HANDLER( rfield_s_28 );
extern READ_HANDLER( rfield_s_29 );
extern READ_HANDLER( rfield_s_30 );
extern READ_HANDLER( rfield_s_31 );

static mem_read_handler rfield_functions_s[32] =
{
	rfield_32  , rfield_s_01, rfield_s_02, rfield_s_03, rfield_s_04, rfield_s_05,
	rfield_s_06, rfield_s_07, rfield_s_08, rfield_s_09, rfield_s_10, rfield_s_11,
	rfield_s_12, rfield_s_13, rfield_s_14, rfield_s_15, rfield_s_16, rfield_s_17,
	rfield_s_18, rfield_s_19, rfield_s_20, rfield_s_21, rfield_s_22, rfield_s_23,
	rfield_s_24, rfield_s_25, rfield_s_26, rfield_s_27, rfield_s_28, rfield_s_29,
	rfield_s_30, rfield_s_31
};



/*###################################################################################################
**	MACROS
**#################################################################################################*/

/* context finder */
#define FINDCONTEXT(_cpu) (cpu_is_saving_context(_cpu) ? cpu_getcontext(_cpu) : &state)

/* register definitions and shortcuts */
#define PC				(state.pc)
#define ST				(state.st)
#define N_FLAG			(state.nflag)
#define NOTZ_FLAG		(state.notzflag)
#define C_FLAG			(state.cflag)
#define V_FLAG			(state.vflag)
#define P_FLAG			(state.pflag)
#define IE_FLAG			(state.ieflag)
#define FE0_FLAG		(state.fe0flag)
#define FE1_FLAG		(state.fe1flag)

/* register file access */
#define AREG(i)			(state.regs.a.a.Aregs[i])
#define AREG_XY(i)		(state.regs.a.a.AregsXY[i])
#define AREG_X(i)		(state.regs.a.a.AregsXY[i].x)
#define AREG_Y(i)		(state.regs.a.a.AregsXY[i].y)
#define BREG(i)			(state.regs.Bregs[i])
#define BREG_XY(i)		(state.regs.BregsXY[i])
#define BREG_X(i)		(state.regs.BregsXY[i].x)
#define BREG_Y(i)		(state.regs.BregsXY[i].y)
#define SP				AREG(15)
#define FW(i)			(state.fw[i])
#define FW_INC(i)		(state.fw_inc[i])
#define BINDEX(i)		((i) << 4)

/* opcode decode helpers */
#define ASRCREG			((state.op >> 5) & 0x0f)
#define ADSTREG			(state.op & 0x0f)
#define BSRCREG			((state.op & 0x1e0) >> 1)
#define BDSTREG			((state.op & 0x0f) << 4)
#define SKIP_WORD		(PC += (2 << 3))
#define SKIP_LONG		(PC += (4 << 3))
#define PARAM_K			((state.op >> 5) & 0x1f)
#define PARAM_N			(state.op & 0x1f)
#define PARAM_REL8		((INT8)state.op)

/* memory I/O */
#define WFIELD0(a,b)	state.f0_write(a,b)
#define WFIELD1(a,b)	state.f1_write(a,b)
#define RFIELD0(a)		state.f0_read(a)
#define RFIELD1(a)		state.f1_read(a)
#define WPIXEL(a,b)		state.pixel_write(a,b)
#define RPIXEL(a)		state.pixel_read(a)

/* Implied Operands */
#define SADDR			BREG(BINDEX(0))
#define SADDR_X			BREG_X(BINDEX(0))
#define SADDR_Y			BREG_Y(BINDEX(0))
#define SADDR_XY		BREG_XY(BINDEX(0))
#define SPTCH			BREG(BINDEX(1))
#define DADDR			BREG(BINDEX(2))
#define DADDR_X			BREG_X(BINDEX(2))
#define DADDR_Y			BREG_Y(BINDEX(2))
#define DADDR_XY		BREG_XY(BINDEX(2))
#define DPTCH			BREG(BINDEX(3))
#define OFFSET			BREG(BINDEX(4))
#define WSTART_X		BREG_X(BINDEX(5))
#define WSTART_Y		BREG_Y(BINDEX(5))
#define WEND_X			BREG_X(BINDEX(6))
#define WEND_Y			BREG_Y(BINDEX(6))
#define DYDX_X			BREG_X(BINDEX(7))
#define DYDX_Y			BREG_Y(BINDEX(7))
#define COLOR0			BREG(BINDEX(8))
#define COLOR1			BREG(BINDEX(9))
#define COUNT			BREG(BINDEX(10))
#define INC1_X			BREG_X(BINDEX(11))
#define INC1_Y			BREG_Y(BINDEX(11))
#define INC2_X			BREG_X(BINDEX(12))
#define INC2_Y			BREG_Y(BINDEX(12))
#define PATTRN			BREG(BINDEX(13))
#define TEMP			BREG(BINDEX(14))



/*###################################################################################################
**	INLINE SHORTCUTS
**#################################################################################################*/

/* set the field widths - shortcut */
INLINE void SET_FW(void)
{
	FW_INC(0) = FW(0) ? FW(0) : 0x20;
	FW_INC(1) = FW(1) ? FW(1) : 0x20;

	state.f0_write = wfield_functions[FW(0)];
	state.f1_write = wfield_functions[FW(1)];

	if (FE0_FLAG)
		state.f0_read  = rfield_functions_s[FW(0)];	/* Sign extend */
	else
		state.f0_read  = rfield_functions_z[FW(0)];	/* Zero extend */

	if (FE1_FLAG)
		state.f1_read  = rfield_functions_s[FW(1)];	/* Sign extend */
	else
		state.f1_read  = rfield_functions_z[FW(1)];	/* Zero extend */
}

/* Intialize Status to 0x0010 */
INLINE void RESET_ST(void)
{
	N_FLAG = C_FLAG = V_FLAG = P_FLAG = IE_FLAG = FE0_FLAG = FE1_FLAG = 0;
	NOTZ_FLAG = 1;
	FW(0) = 0x10;
	FW(1) = 0;
	SET_FW();
}

/* Combine indiviual flags into the Status Register */
INLINE UINT32 GET_ST(void)
{
    UINT32 ret=0;
    
    if (N_FLAG) ret|=0x80000000;
    if (C_FLAG) ret|=0x40000000;
    if (!NOTZ_FLAG) ret|=0x20000000;
    if (V_FLAG) ret|=0x10000000;
    if (P_FLAG) ret|=0x02000000;
    if (IE_FLAG) ret|=0x00200000;
    if (FE1_FLAG) ret|=0x00000800;
    ret|=(FW(1) << 6);
    if (FE0_FLAG) ret|=0x00000020;
    ret|=FW(0);
    return ret;
    
    /*
	return (     N_FLAG ? 0x80000000 : 0) |
		   (     C_FLAG ? 0x40000000 : 0) |
		   (  NOTZ_FLAG ? 0 : 0x20000000) |
		   (     V_FLAG ? 0x10000000 : 0) |
		   (     P_FLAG ? 0x02000000 : 0) |
		   (    IE_FLAG ? 0x00200000 : 0) |
		   (   FE1_FLAG ? 0x00000800 : 0) |
		   (FW(1) << 6)                   |
		   (   FE0_FLAG ? 0x00000020 : 0) |
		   FW(0);
	*/
}

/* Break up Status Register into indiviual flags */
INLINE void SET_ST(UINT32 st)
{
	N_FLAG    =    st & 0x80000000;
	C_FLAG    =    st & 0x40000000;
	NOTZ_FLAG =  !(st & 0x20000000);
	V_FLAG    =    st & 0x10000000;
	P_FLAG    =    st & 0x02000000;
	IE_FLAG   =    st & 0x00200000;
	FE1_FLAG  =    st & 0x00000800;
	FW(1)     =   (st >> 6) & 0x1f;
	FE0_FLAG  =    st & 0x00000020;
	FW(0)     =    st & 0x1f;
	SET_FW();

	/* interrupts might have been enabled, check it */
	check_interrupt();
}

/* shortcuts for reading opcodes */
INLINE UINT32 ROPCODE(void)
{
	UINT32 pc = TOBYTE(PC);
	PC += 2 << 3;
	return cpu_readop16(pc);
}

INLINE INT16 PARAM_WORD(void)
{
	UINT32 pc = TOBYTE(PC);
	PC += 2 << 3;
	return cpu_readop_arg16(pc);
}

INLINE INT32 PARAM_LONG(void)
{
	UINT32 pc = TOBYTE(PC);
	PC += 4 << 3;
	return (UINT16)cpu_readop_arg16(pc) | (cpu_readop_arg16(pc + 2) << 16);
}

INLINE INT16 PARAM_WORD_NO_INC(void)
{
	return cpu_readop_arg16(TOBYTE(PC));
}

INLINE INT32 PARAM_LONG_NO_INC(void)
{
	UINT32 pc = TOBYTE(PC);
	return (UINT16)cpu_readop_arg16(pc) | (cpu_readop_arg16(pc + 2) << 16);
}

/* read memory byte */
INLINE READ_HANDLER( RBYTE )
{
	UINT32 ret;
	RFIELDMAC_8;
	return ret;
}

/* write memory byte */
INLINE WRITE_HANDLER( WBYTE )
{
	WFIELDMAC_8;
}

/* read memory long */
INLINE READ_HANDLER( RLONG )
{
	RFIELDMAC_32;
}

/* write memory long */
INLINE WRITE_HANDLER( WLONG )
{
	WFIELDMAC_32;
}

/* pushes/pops a value from the stack */
INLINE void PUSH(UINT32 data)
{
	SP -= 0x20;
	TMS34010_WRMEM_DWORD(TOBYTE(SP), data);
}

INLINE INT32 POP(void)
{
	INT32 ret = TMS34010_RDMEM_DWORD(TOBYTE(SP));
	SP += 0x20;
	return ret;
}



/*###################################################################################################
**	PIXEL READS
**#################################################################################################*/

#define RP(m1,m2)  											\
	/* TODO: Plane masking */								\
	return (TMS34010_RDMEM_WORD(TOBYTE(offset & 0xfffffff0)) >> (offset & m1)) & m2;

static READ_HANDLER( read_pixel_1 ) { RP(0x0f,0x01) }
static READ_HANDLER( read_pixel_2 ) { RP(0x0e,0x03) }
static READ_HANDLER( read_pixel_4 ) { RP(0x0c,0x0f) }
static READ_HANDLER( read_pixel_8 ) { RP(0x08,0xff) }
static READ_HANDLER( read_pixel_16 )
{
	/* TODO: Plane masking */
	return TMS34010_RDMEM_WORD(TOBYTE(offset & 0xfffffff0));
}

/* Shift register read */
static READ_HANDLER( read_pixel_shiftreg )
{
	if (state.config->to_shiftreg)
		state.config->to_shiftreg(offset, &state.shiftreg[0]);
	/*else
		logerror("To ShiftReg function not set. PC = %08X\n", PC);*/
	return state.shiftreg[0];
}



/*###################################################################################################
**	PIXEL WRITES
**#################################################################################################*/

/* No Raster Op + No Transparency */
#define WP(m1,m2)  																			\
	UINT32 a = TOBYTE(offset & 0xfffffff0);													\
	UINT32 pix = TMS34010_RDMEM_WORD(a);													\
	UINT32 shiftcount = offset & m1;														\
																							\
	/* TODO: plane masking */																\
	data &= m2;																				\
	pix = (pix & ~(m2 << shiftcount)) | (data << shiftcount);								\
	TMS34010_WRMEM_WORD(a, pix);															\

/* No Raster Op + Transparency */
#define WP_T(m1,m2)  																		\
	/* TODO: plane masking */																\
	data &= m2;																				\
	if (data)																				\
	{																						\
		UINT32 a = TOBYTE(offset & 0xfffffff0);												\
		UINT32 pix = TMS34010_RDMEM_WORD(a);												\
		UINT32 shiftcount = offset & m1;													\
																							\
		/* TODO: plane masking */															\
		pix = (pix & ~(m2 << shiftcount)) | (data << shiftcount);							\
		TMS34010_WRMEM_WORD(a, pix);														\
	}						  																\

/* Raster Op + No Transparency */
#define WP_R(m1,m2)  																		\
	UINT32 a = TOBYTE(offset & 0xfffffff0);													\
	UINT32 pix = TMS34010_RDMEM_WORD(a);													\
	UINT32 shiftcount = offset & m1;														\
																							\
	/* TODO: plane masking */																\
	data = state.raster_op(data & m2, (pix >> shiftcount) & m2) & m2;						\
	pix = (pix & ~(m2 << shiftcount)) | (data << shiftcount);								\
	TMS34010_WRMEM_WORD(a, pix);															\

/* Raster Op + Transparency */
#define WP_R_T(m1,m2)  																		\
	UINT32 a = TOBYTE(offset & 0xfffffff0);													\
	UINT32 pix = TMS34010_RDMEM_WORD(a);													\
	UINT32 shiftcount = offset & m1;														\
																							\
	/* TODO: plane masking */																\
	data = state.raster_op(data & m2, (pix >> shiftcount) & m2) & m2;						\
	if (data)																				\
	{																						\
		pix = (pix & ~(m2 << shiftcount)) | (data << shiftcount);							\
		TMS34010_WRMEM_WORD(a, pix);														\
	}						  																\


/* No Raster Op + No Transparency */
static WRITE_HANDLER( write_pixel_1 ) { WP(0x0f, 0x01); }
static WRITE_HANDLER( write_pixel_2 ) { WP(0x0e, 0x03); }
static WRITE_HANDLER( write_pixel_4 ) { WP(0x0c, 0x0f); }
static WRITE_HANDLER( write_pixel_8 ) { WP(0x08, 0xff); }
static WRITE_HANDLER( write_pixel_16 )
{
	/* TODO: plane masking */
	TMS34010_WRMEM_WORD(TOBYTE(offset & 0xfffffff0), data);
}

/* No Raster Op + Transparency */
static WRITE_HANDLER( write_pixel_t_1 ) { WP_T(0x0f, 0x01); }
static WRITE_HANDLER( write_pixel_t_2 ) { WP_T(0x0e, 0x03); }
static WRITE_HANDLER( write_pixel_t_4 ) { WP_T(0x0c, 0x0f); }
static WRITE_HANDLER( write_pixel_t_8 ) { WP_T(0x08, 0xff); }
static WRITE_HANDLER( write_pixel_t_16 )
{
	/* TODO: plane masking */
	if (data)
		TMS34010_WRMEM_WORD(TOBYTE(offset & 0xfffffff0), data);
}

/* Raster Op + No Transparency */
static WRITE_HANDLER( write_pixel_r_1 ) { WP_R(0x0f, 0x01); }
static WRITE_HANDLER( write_pixel_r_2 ) { WP_R(0x0e, 0x03); }
static WRITE_HANDLER( write_pixel_r_4 ) { WP_R(0x0c, 0x0f); }
static WRITE_HANDLER( write_pixel_r_8 ) { WP_R(0x08, 0xff); }
static WRITE_HANDLER( write_pixel_r_16 )
{
	/* TODO: plane masking */
	UINT32 a = TOBYTE(offset & 0xfffffff0);
	TMS34010_WRMEM_WORD(a, state.raster_op(data, TMS34010_RDMEM_WORD(a)));
}

/* Raster Op + Transparency */
static WRITE_HANDLER( write_pixel_r_t_1 ) { WP_R_T(0x0f,0x01); }
static WRITE_HANDLER( write_pixel_r_t_2 ) { WP_R_T(0x0e,0x03); }
static WRITE_HANDLER( write_pixel_r_t_4 ) { WP_R_T(0x0c,0x0f); }
static WRITE_HANDLER( write_pixel_r_t_8 ) { WP_R_T(0x08,0xff); }
static WRITE_HANDLER( write_pixel_r_t_16 )
{
	/* TODO: plane masking */
	UINT32 a = TOBYTE(offset & 0xfffffff0);
	data = state.raster_op(data, TMS34010_RDMEM_WORD(a));

	if (data)
		TMS34010_WRMEM_WORD(a, data);
}

/* Shift register write */
static WRITE_HANDLER( write_pixel_shiftreg )
{
	if (state.config->from_shiftreg)
		state.config->from_shiftreg(offset, &state.shiftreg[0]);
	/*else
		logerror("From ShiftReg function not set. PC = %08X\n", PC);*/
}



/*###################################################################################################
**	RASTER OPS
**#################################################################################################*/

/* Raster operations */
static INT32 raster_op_1(INT32 newpix, INT32 oldpix)  { return newpix & oldpix; }
static INT32 raster_op_2(INT32 newpix, INT32 oldpix)  { return newpix & ~oldpix; }
static INT32 raster_op_3(INT32 newpix, INT32 oldpix)  { return 0; }
static INT32 raster_op_4(INT32 newpix, INT32 oldpix)  { return newpix | ~oldpix; }
static INT32 raster_op_5(INT32 newpix, INT32 oldpix)  { return ~(newpix ^ oldpix); }
static INT32 raster_op_6(INT32 newpix, INT32 oldpix)  { return ~oldpix; }
static INT32 raster_op_7(INT32 newpix, INT32 oldpix)  { return ~(newpix | oldpix); }
static INT32 raster_op_8(INT32 newpix, INT32 oldpix)  { return newpix | oldpix; }
static INT32 raster_op_9(INT32 newpix, INT32 oldpix)  { return oldpix; }
static INT32 raster_op_10(INT32 newpix, INT32 oldpix) { return newpix ^ oldpix; }
static INT32 raster_op_11(INT32 newpix, INT32 oldpix) { return ~newpix & oldpix; }
static INT32 raster_op_12(INT32 newpix, INT32 oldpix) { return 0xffff; }
static INT32 raster_op_13(INT32 newpix, INT32 oldpix) { return ~newpix | oldpix; }
static INT32 raster_op_14(INT32 newpix, INT32 oldpix) { return ~(newpix & oldpix); }
static INT32 raster_op_15(INT32 newpix, INT32 oldpix) { return ~newpix; }
static INT32 raster_op_16(INT32 newpix, INT32 oldpix) { return newpix + oldpix; }
static INT32 raster_op_17(INT32 newpix, INT32 oldpix)
{
	INT32 max = (UINT32)0xffffffff >> (32 - IOREG(REG_PSIZE));
	INT32 res = newpix + oldpix;
	return (res > max) ? max : res;
}
static INT32 raster_op_18(INT32 newpix, INT32 oldpix) { return oldpix - newpix; }
static INT32 raster_op_19(INT32 newpix, INT32 oldpix) { return (oldpix > newpix) ? oldpix - newpix : 0; }
static INT32 raster_op_20(INT32 newpix, INT32 oldpix) { return (oldpix > newpix) ? oldpix : newpix; }
static INT32 raster_op_21(INT32 newpix, INT32 oldpix) { return (oldpix > newpix) ? newpix : oldpix; }



/*###################################################################################################
**	OPCODE TABLE & IMPLEMENTATIONS
**#################################################################################################*/

/* includes the static function prototypes and the master opcode table */
#include "34010tbl.cpp"

/* includes the actual opcode implementations */
#include "34010ops.cpp"
#include "34010gfx.cpp"



/*###################################################################################################
**	Internal interrupt check
*#################################################################################################*/

/* Generate pending interrupts. Do NOT inline this function on DJGPP,
   it causes a slowdown */
static void check_interrupt(void)
{
	int vector = 0;
	int irqline = -1;
	int irq;

	/* early out if no interrupts pending */
	irq = IOREG(REG_INTPEND);
	if (!irq)
		return;

	/* check for NMI first */
	if (irq & TMS34010_NMI)
	{
		LOG(("TMS34010#%d takes NMI\n", cpu_getactivecpu()));
		
		/* ack the NMI */
		IOREG(REG_INTPEND) &= ~TMS34010_NMI;

		/* handle NMI mode bit */
		if (!(IOREG(REG_HSTCTLH) & 0x0200))
		{
			PUSH(PC);
			PUSH(GET_ST());
		}
		
		/* leap to the vector */
		RESET_ST();
		PC = RLONG(0xfffffee0);
		change_pc29(PC);
		return;
	}

	/* early out if everything else is disabled */
	irq &= IOREG(REG_INTENB);
	if (!IE_FLAG || !irq)
		return;

	/* host interrupt */
	if (irq & TMS34010_HI)
	{
		LOG(("TMS34010#%d takes HI\n", cpu_getactivecpu()));
		vector = 0xfffffec0;
	}
	
	/* display interrupt */
	else if (irq & TMS34010_DI)
	{
		LOG(("TMS34010#%d takes DI\n", cpu_getactivecpu()));
		vector = 0xfffffea0;
	}
	
	/* window violation interrupt */
	else if (irq & TMS34010_WV)
	{
		LOG(("TMS34010#%d takes WV\n", cpu_getactivecpu()));
		vector = 0xfffffe80;
	}
	
	/* external 1 interrupt */
	else if (irq & TMS34010_INT1)
	{
		LOG(("TMS34010#%d takes INT1\n", cpu_getactivecpu()));
		vector = 0xffffffc0;
		irqline = 0;
	}

	/* external 2 interrupt */
	else if (irq & TMS34010_INT2)
	{
		LOG(("TMS34010#%d takes INT2\n", cpu_getactivecpu()));
		vector = 0xffffffa0;
		irqline = 1;
	}

	/* if we took something, generate it */
	if (vector)
	{
		PUSH(PC);
		PUSH(GET_ST());
		RESET_ST();
		PC = RLONG(vector);
		change_pc29(PC);

		/* call the callback for externals */
		if (irqline >= 0)
			(void)(*state.irq_callback)(irqline);
	}
}



/*###################################################################################################
**	Reset the CPU emulation
**#################################################################################################*/

void tms34010_reset(void *param)
{
	struct tms34010_config *config = param ? (struct tms34010_config *)param : (struct tms34010_config *)&default_config;

	/* zap the state and copy in the config pointer */
	memset(&state, 0, sizeof(state));
	state.is_34020 = 0;
	state.config = config;

	/* allocate the shiftreg */
	state.shiftreg = (UINT16*)malloc(SHIFTREG_SIZE);

	/* fetch the initial PC and reset the state */
	PC = RLONG(0xffffffe0);
	change_pc29(PC)
	RESET_ST();

	/* HALT the CPU if requested, and remember to re-read the starting PC */
	/* the first time we are run */
	state.reset_deferred = config->halt_on_reset;
	if (config->halt_on_reset)
		tms34010_io_register_w(REG_HSTCTLH * 2, 0x8000);

	/* reset the timers and the host interface (but only the first time) */
	host_interface_context = NULL;

    interval = TIME_IN_HZ(Machine->drv->frames_per_second);
}

void tms34020_reset(void *param)
{
	tms34010_reset(param);
	state.is_34020 = 1;
}



/*###################################################################################################
**	Shut down the CPU emulation
**#################################################################################################*/

void tms34010_exit(void)
{
	int i;
	
	/* clear out the timers */
	for (i = 0; i < MAX_CPU; i++)
		dpyint_timer[i] = vsblnk_timer[i] = NULL;
	
	/* free memory */
	if (state.shiftreg)
		free(state.shiftreg);
	state.shiftreg = NULL;
}

void tms34020_exit(void)
{
	tms34010_exit();
}



/*###################################################################################################
**	Get all registers in given buffer
**#################################################################################################*/

unsigned tms34010_get_context(void *dst)
{
	if (dst)
		*(tms34010_regs *)dst = state;
	return sizeof(tms34010_regs);
}

unsigned tms34020_get_context(void *dst)
{
	return tms34010_get_context(dst);
}



/*###################################################################################################
**	Set all registers to given values
**#################################################################################################*/

void tms34010_set_context(void *src)
{
	if (src)
		state = *(tms34010_regs *)src;
	change_pc29(PC)
	check_interrupt();
}

void tms34020_set_context(void *src)
{
	tms34010_set_context(src);
}



/*###################################################################################################
**	Return program counter
**#################################################################################################*/

unsigned tms34010_get_pc(void)
{
	return PC;
}

unsigned tms34020_get_pc(void)
{
	return tms34010_get_pc();
}



/*###################################################################################################
**	Set program counter
**#################################################################################################*/

void tms34010_set_pc(unsigned val)
{
	PC = val;
	change_pc29(PC)
}

void tms34020_set_pc(unsigned val)
{
	tms34010_set_pc(val);
}



/*###################################################################################################
**	Return stack pointer
**#################################################################################################*/

unsigned tms34010_get_sp(void)
{
	return SP;
}

unsigned tms34020_get_sp(void)
{
	return tms34010_get_sp();
}



/*###################################################################################################
**	Set stack pointer
**#################################################################################################*/

void tms34010_set_sp(unsigned val)
{
	SP = val;
}

void tms34020_set_sp(unsigned val)
{
	tms34010_set_sp(val);
}



/*###################################################################################################
**	Return a specific register
**#################################################################################################*/

unsigned tms34010_get_reg(int regnum)
{
	switch (regnum)
	{
		case TMS34010_PC:  return PC;
		case TMS34010_SP:  return SP;
		case TMS34010_ST:  return ST;
		case TMS34010_A0:  return AREG(0);
		case TMS34010_A1:  return AREG(1);
		case TMS34010_A2:  return AREG(2);
		case TMS34010_A3:  return AREG(3);
		case TMS34010_A4:  return AREG(4);
		case TMS34010_A5:  return AREG(5);
		case TMS34010_A6:  return AREG(6);
		case TMS34010_A7:  return AREG(7);
		case TMS34010_A8:  return AREG(8);
		case TMS34010_A9:  return AREG(9);
		case TMS34010_A10: return AREG(10);
		case TMS34010_A11: return AREG(11);
		case TMS34010_A12: return AREG(12);
		case TMS34010_A13: return AREG(13);
		case TMS34010_A14: return AREG(14);
		case TMS34010_B0:  return BREG(BINDEX(0));
		case TMS34010_B1:  return BREG(BINDEX(1));
		case TMS34010_B2:  return BREG(BINDEX(2));
		case TMS34010_B3:  return BREG(BINDEX(3));
		case TMS34010_B4:  return BREG(BINDEX(4));
		case TMS34010_B5:  return BREG(BINDEX(5));
		case TMS34010_B6:  return BREG(BINDEX(6));
		case TMS34010_B7:  return BREG(BINDEX(7));
		case TMS34010_B8:  return BREG(BINDEX(8));
		case TMS34010_B9:  return BREG(BINDEX(9));
		case TMS34010_B10: return BREG(BINDEX(10));
		case TMS34010_B11: return BREG(BINDEX(11));
		case TMS34010_B12: return BREG(BINDEX(12));
		case TMS34010_B13: return BREG(BINDEX(13));
		case TMS34010_B14: return BREG(BINDEX(14));
/* TODO: return contents of [SP + wordsize * (CPU_SP_CONTENTS-regnum)] */
		default:
			if (regnum <= REG_SP_CONTENTS)
			{
				unsigned offset = SP + 4 * (REG_SP_CONTENTS - regnum);
				return cpu_readmem29_dword(TOBYTE(offset));
			}
	}
	return 0;
}

unsigned tms34020_get_reg(int regnum)
{
	return tms34010_get_reg(regnum);
}



/*###################################################################################################
**	Set a specific register
**#################################################################################################*/

void tms34010_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
		case TMS34010_PC:  PC = val; break;
		case TMS34010_SP:  SP = val; break;
		case TMS34010_ST:  ST = val; break;
		case TMS34010_A0:  AREG(0) = val; break;
		case TMS34010_A1:  AREG(1) = val; break;
		case TMS34010_A2:  AREG(2) = val; break;
		case TMS34010_A3:  AREG(3) = val; break;
		case TMS34010_A4:  AREG(4) = val; break;
		case TMS34010_A5:  AREG(5) = val; break;
		case TMS34010_A6:  AREG(6) = val; break;
		case TMS34010_A7:  AREG(7) = val; break;
		case TMS34010_A8:  AREG(8) = val; break;
		case TMS34010_A9:  AREG(9) = val; break;
		case TMS34010_A10: AREG(10) = val; break;
		case TMS34010_A11: AREG(11) = val; break;
		case TMS34010_A12: AREG(12) = val; break;
		case TMS34010_A13: AREG(13) = val; break;
		case TMS34010_A14: AREG(14) = val; break;
		case TMS34010_B0:  BREG(BINDEX(0)) = val; break;
		case TMS34010_B1:  BREG(BINDEX(1)) = val; break;
		case TMS34010_B2:  BREG(BINDEX(2)) = val; break;
		case TMS34010_B3:  BREG(BINDEX(3)) = val; break;
		case TMS34010_B4:  BREG(BINDEX(4)) = val; break;
		case TMS34010_B5:  BREG(BINDEX(5)) = val; break;
		case TMS34010_B6:  BREG(BINDEX(6)) = val; break;
		case TMS34010_B7:  BREG(BINDEX(7)) = val; break;
		case TMS34010_B8:  BREG(BINDEX(8)) = val; break;
		case TMS34010_B9:  BREG(BINDEX(9)) = val; break;
		case TMS34010_B10: BREG(BINDEX(10)) = val; break;
		case TMS34010_B11: BREG(BINDEX(11)) = val; break;
		case TMS34010_B12: BREG(BINDEX(12)) = val; break;
		case TMS34010_B13: BREG(BINDEX(13)) = val; break;
		case TMS34010_B14: BREG(BINDEX(14)) = val; break;
/* TODO: set contents of [SP + wordsize * (CPU_SP_CONTENTS-regnum)] */
		default:
			if (regnum <= REG_SP_CONTENTS)
			{
				unsigned offset = SP + 4 * (REG_SP_CONTENTS - regnum);
				cpu_writemem29_word(TOBYTE(offset), val); /* ??? */
			}
	}
}

void tms34020_set_reg(int regnum, unsigned val)
{
	tms34010_set_reg(regnum, val);
}



/*###################################################################################################
**	Set NMI line state
**#################################################################################################*/

void tms34010_set_nmi_line(int linestate)
{
	/* Does not apply: the NMI is an internal interrupt for the TMS34010 */
}

void tms34020_set_nmi_line(int linestate)
{
	tms34010_set_nmi_line(linestate);
}



/*###################################################################################################
**	Set IRQ line state
**#################################################################################################*/

void tms34010_set_irq_line(int irqline, int linestate)
{
	LOG(("TMS34010#%d set irq line %d state %d\n", cpu_getactivecpu(), irqline, linestate));

	/* set the pending interrupt */
	switch (irqline)
	{
		case 0:
			if (linestate != CLEAR_LINE)
				IOREG(REG_INTPEND) |= TMS34010_INT1;
			else
				IOREG(REG_INTPEND) &= ~TMS34010_INT1;
			break;
		case 1:
			if (linestate != CLEAR_LINE)
				IOREG(REG_INTPEND) |= TMS34010_INT2;
			else
				IOREG(REG_INTPEND) &= ~TMS34010_INT2;
			break;
	}
	check_interrupt();
}

void tms34020_set_irq_line(int irqline, int linestate)
{
	tms34010_set_irq_line(irqline, linestate);
}



/*###################################################################################################
**	Set IRQ callback
**#################################################################################################*/

void tms34010_set_irq_callback(int (*callback)(int irqline))
{
	state.irq_callback = callback;
}

void tms34020_set_irq_callback(int (*callback)(int irqline))
{
	tms34010_set_irq_callback(callback);
}



/*###################################################################################################
**	Generate internal interrupt
*#################################################################################################*/

void tms34010_internal_interrupt(int type)
{
	LOG(("TMS34010#%d set internal interrupt $%04x\n", cpu_getactivecpu(), type));
	IOREG(REG_INTPEND) |= type;
	check_interrupt();
}

void tms34020_internal_interrupt(int type)
{
	tms34010_internal_interrupt(type);
}



/*###################################################################################################
**	Execute
*#################################################################################################*/

int tms34010_execute(int cycles)
{
	/* Get out if CPU is halted. Absolutely no interrupts must be taken!!! */
	if (IOREG(REG_HSTCTLH) & 0x8000)
		return cycles;

	/* if the CPU's reset was deferred, do it now */
	if (state.reset_deferred)
	{
		state.reset_deferred = 0;
		PC = RLONG(0xffffffe0);
	}

	/* execute starting now */
	tms34010_ICount = cycles;
	change_pc29(PC)
	do
	{
		state.op = ROPCODE();
		(*opcode_table[state.op >> 4])();

		state.op = ROPCODE();
		(*opcode_table[state.op >> 4])();

		state.op = ROPCODE();
		(*opcode_table[state.op >> 4])();

		state.op = ROPCODE();
		(*opcode_table[state.op >> 4])();

	} while (tms34010_ICount > 0);

	return cycles - tms34010_ICount;
}

int tms34020_execute(int cycles)
{
	return tms34010_execute(cycles);
}



/*###################################################################################################
**	Return a formatted string for a register
*#################################################################################################*/

const char *tms34010_info(void *context, int regnum)
{
	switch (regnum)
	{
		case CPU_INFO_NAME: return "TMS34010";
		case CPU_INFO_FAMILY: return "Texas Instruments 34010";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (C) Alex Pasadyn/Zsolt Vasvari 1998\nParts based on code by Aaron Giles";
	}
	return "";
}

const char *tms34020_info(void *context, int regnum)
{
	switch (regnum)
	{
		case CPU_INFO_NAME: 	return "TMS34020";
		case CPU_INFO_FAMILY: 	return "Texas Instruments 34020";
		default:				return tms34010_info(context, regnum);
	}
}



/*###################################################################################################
**	Disassemble
*#################################################################################################*/

unsigned tms34010_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%04X", cpu_readop16(pc>>3) );
	return 2;
}

unsigned tms34020_dasm(char *buffer, unsigned pc)
{
	return tms34010_dasm(buffer, pc);
}



/*###################################################################################################
**	PIXEL OPS
**#################################################################################################*/

static mem_write_handler pixel_write_ops[4][5] =
{
	{ write_pixel_1,     write_pixel_2,     write_pixel_4,     write_pixel_8,     write_pixel_16     },
	{ write_pixel_r_1,   write_pixel_r_2,   write_pixel_r_4,   write_pixel_r_8,   write_pixel_r_16   },
	{ write_pixel_t_1,   write_pixel_t_2,   write_pixel_t_4,   write_pixel_t_8,   write_pixel_t_16   },
	{ write_pixel_r_t_1, write_pixel_r_t_2, write_pixel_r_t_4, write_pixel_r_t_8, write_pixel_r_t_16 }
};

static mem_read_handler pixel_read_ops[5] =
{
	read_pixel_1,        read_pixel_2,      read_pixel_4,      read_pixel_8,      read_pixel_16
};


static void set_pixel_function(tms34010_regs *context)
{
	UINT32 i1,i2;

	if (CONTEXT_IOREG(context, REG_DPYCTL) & 0x0800)
	{
		/* Shift Register Transfer */
		context->pixel_write = write_pixel_shiftreg;
		context->pixel_read  = read_pixel_shiftreg;
		return;
	}

	switch (CONTEXT_IOREG(context, REG_PSIZE))
	{
		default:
		case 0x01: i2 = 0; break;
		case 0x02: i2 = 1; break;
		case 0x04: i2 = 2; break;
		case 0x08: i2 = 3; break;
		case 0x10: i2 = 4; break;
	}

	if (context->transparency)
		i1 = context->raster_op ? 3 : 2;
	else
		i1 = context->raster_op ? 1 : 0;

	context->pixel_write = pixel_write_ops[i1][i2];
	context->pixel_read  = pixel_read_ops [i2];
}



/*###################################################################################################
**	RASTER OPS
**#################################################################################################*/

static INT32 (*raster_ops[32]) (INT32 newpix, INT32 oldpix) =
{
	           0, raster_op_1 , raster_op_2 , raster_op_3,
	raster_op_4 , raster_op_5 , raster_op_6 , raster_op_7,
	raster_op_8 , raster_op_9 , raster_op_10, raster_op_11,
	raster_op_12, raster_op_13, raster_op_14, raster_op_15,
	raster_op_16, raster_op_17, raster_op_18, raster_op_19,
	raster_op_20, raster_op_21,            0,            0,
	           0,            0,            0,            0,
	           0,            0,            0,            0,
};


static void set_raster_op(tms34010_regs *context)
{
	context->raster_op = raster_ops[(IOREG(REG_CONTROL) >> 10) & 0x1f];
}



/*###################################################################################################
**	VIDEO TIMING HELPERS
**#################################################################################################*/

INLINE int scanline_to_vcount(tms34010_regs *context, int scanline)
{
	if (Machine->visible_area.min_y == 0)
		scanline += CONTEXT_IOREG(context, REG_VEBLNK);
	if (scanline > CONTEXT_IOREG(context, REG_VTOTAL))
		scanline -= CONTEXT_IOREG(context, REG_VTOTAL);
	return scanline;
}


INLINE int vcount_to_scanline(tms34010_regs *context, int vcount)
{
	if (Machine->visible_area.min_y == 0)
		vcount -= CONTEXT_IOREG(context, REG_VEBLNK);
	if (vcount < 0)
		vcount += CONTEXT_IOREG(context, REG_VTOTAL);
	if (vcount > Machine->visible_area.max_y)
		vcount = 0;
	return vcount;
}


static void update_display_address(tms34010_regs *context, int vcount)
{
	UINT32 dpyadr = CONTEXT_IOREG(context, REG_DPYADR) & 0xfffc;
	UINT32 dpytap = CONTEXT_IOREG(context, REG_DPYTAP) & 0x3fff;
	INT32 dudate = CONTEXT_IOREG(context, REG_DPYCTL) & 0x03fc;
	int org = CONTEXT_IOREG(context, REG_DPYCTL) & 0x0400;
	int scans = (CONTEXT_IOREG(context, REG_DPYSTRT) & 3) + 1;

	/* anytime during VBLANK is effectively the start of the next frame */
	if (vcount >= CONTEXT_IOREG(context, REG_VSBLNK) || vcount <= CONTEXT_IOREG(context, REG_VEBLNK))
		context->last_update_vcount = vcount = CONTEXT_IOREG(context, REG_VEBLNK);

	/* otherwise, compute the updated address */
	else
	{
		int rows = vcount - context->last_update_vcount;
		if (rows < 0) rows += CONTEXT_IOREG(context, REG_VCOUNT);
		dpyadr -= rows * dudate / scans;
		CONTEXT_IOREG(context, REG_DPYADR) = dpyadr | (CONTEXT_IOREG(context, REG_DPYADR) & 0x0003);
		context->last_update_vcount = vcount;
	}

	/* now compute the actual address */
	if (org == 0) dpyadr ^= 0xfffc;
	dpyadr <<= 8;
	dpyadr |= dpytap << 4;

	/* callback */
	if (context->config->display_addr_changed)
	{
		if (org != 0) dudate = -dudate;
		(*context->config->display_addr_changed)(dpyadr & 0x00ffffff, (dudate << 8) / scans, vcount_to_scanline(context, vcount));
	}
}


static void vsblnk_callback(int cpunum)
{
	/* reset timer for next frame */
	//float interval = TIME_IN_HZ(Machine->drv->frames_per_second);
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);
	vsblnk_timer[cpunum] = timer_set(interval, cpunum, vsblnk_callback);
	CONTEXT_IOREG(context, REG_DPYADR) = CONTEXT_IOREG(context, REG_DPYSTRT);
	update_display_address(context, CONTEXT_IOREG(context, REG_VSBLNK));
}


static void dpyint_callback(int cpunum)
{
	/* reset timer for next frame */
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);
	//float interval = TIME_IN_HZ(Machine->drv->frames_per_second);
	dpyint_timer[cpunum] = timer_set(interval, cpunum, dpyint_callback);
	cpu_generate_internal_interrupt(cpunum, TMS34010_DI);

	/* allow a callback so we can update before they are likely to do nasty things */
	if (context->config->display_int_callback)
		(*context->config->display_int_callback)(vcount_to_scanline(context, CONTEXT_IOREG(context, REG_DPYINT)));
}


static void update_timers(int cpunum, tms34010_regs *context)
{
	int dpyint = CONTEXT_IOREG(context, REG_DPYINT);
	int vsblnk = CONTEXT_IOREG(context, REG_VSBLNK);

	/* remove any old timers */
	if (dpyint_timer[cpunum])
		timer_remove(dpyint_timer[cpunum]);
	if (vsblnk_timer[cpunum])
		timer_remove(vsblnk_timer[cpunum]);

	/* set new timers */
	dpyint_timer[cpunum] = timer_set(cpu_getscanlinetime(vcount_to_scanline(context, dpyint)), cpunum, dpyint_callback);
	vsblnk_timer[cpunum] = timer_set(cpu_getscanlinetime(vcount_to_scanline(context, vsblnk)), cpunum, vsblnk_callback);
}



/*###################################################################################################
**	I/O REGISTER WRITES
**#################################################################################################*/

/*
static const char *ioreg_name[] =
{
	"HESYNC", "HEBLNK", "HSBLNK", "HTOTAL",
	"VESYNC", "VEBLNK", "VSBLNK", "VTOTAL",
	"DPYCTL", "DPYSTART", "DPYINT", "CONTROL",
	"HSTDATA", "HSTADRL", "HSTADRH", "HSTCTLL",
	"HSTCTLH", "INTENB", "INTPEND", "CONVSP",
	"CONVDP", "PSIZE", "PMASK", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "DPYTAP",
	"HCOUNT", "VCOUNT", "DPYADR", "REFCNT"
};
*/

static void common_io_register_w(int cpunum, tms34010_regs *context, int reg, int data)
{
	int oldreg, newreg;

	/* Set register */
	reg >>= 1;
	oldreg = CONTEXT_IOREG(context, reg);
	CONTEXT_IOREG(context, reg) = data;

	switch (reg)
	{
		case REG_DPYINT:
			if (data != oldreg || !dpyint_timer[cpunum])
				update_timers(cpunum, context);
			break;

		case REG_VSBLNK:
			if (data != oldreg || !vsblnk_timer[cpunum])
				update_timers(cpunum, context);
			break;

		case REG_VEBLNK:
			if (data != oldreg)
				update_timers(cpunum, context);
			break;

		case REG_CONTROL:
			context->transparency = data & 0x20;
			context->window_checking = (data >> 6) & 0x03;
			set_raster_op(context);
			set_pixel_function(context);
			break;

		case REG_PSIZE:
			set_pixel_function(context);

			switch (data)
			{
				default:
				case 0x01: context->xytolshiftcount2 = 0; break;
				case 0x02: context->xytolshiftcount2 = 1; break;
				case 0x04: context->xytolshiftcount2 = 2; break;
				case 0x08: context->xytolshiftcount2 = 3; break;
				case 0x10: context->xytolshiftcount2 = 4; break;
			}
			break;

		case REG_PMASK:
			/*if (data) logerror("Plane masking not supported. PC=%08X\n", cpu_get_pc());*/
			break;

		case REG_DPYCTL:
			set_pixel_function(context);
			if ((oldreg ^ data) & 0x03fc)
				update_display_address(context, scanline_to_vcount(context, cpu_getscanline()));
			break;

		case REG_DPYADR:
			if (data != oldreg)
			{
				context->last_update_vcount = scanline_to_vcount(context, cpu_getscanline());
				update_display_address(context, context->last_update_vcount);
			}
			break;

		case REG_DPYSTRT:
			if (data != oldreg)
				update_display_address(context, scanline_to_vcount(context, cpu_getscanline()));
			break;

		case REG_DPYTAP:
			if ((oldreg ^ data) & 0x3fff)
				update_display_address(context, scanline_to_vcount(context, cpu_getscanline()));
			break;

		case REG_HSTCTLH:
			/* if the CPU is halting itself, stop execution right away */
			if ((data & 0x8000) && context == &state)
				tms34010_ICount = 0;
			cpu_set_halt_line(cpunum, (data & 0x8000) ? ASSERT_LINE : CLEAR_LINE);

			/* NMI issued? */
			if (data & 0x0100)
				cpu_generate_internal_interrupt(cpunum, TMS34010_NMI);
			break;

		case REG_HSTCTLL:
			/* the TMS34010 can change MSGOUT, can set INTOUT, and can clear INTIN */
			if (cpunum == cpu_getactivecpu())
			{
				newreg = (oldreg & 0xff8f) | (data & 0x0070);
				newreg |= data & 0x0080;
				newreg &= data | ~0x0008;
			}

			/* the host can change MSGIN, can set INTIN, and can clear INTOUT */
			else
			{
				newreg = (oldreg & 0xfff8) | (data & 0x0007);
				newreg &= data | ~0x0080;
				newreg |= data & 0x0008;
			}
			CONTEXT_IOREG(context, reg) = newreg;

			/* output interrupt? */
			if (!(oldreg & 0x0080) && (newreg & 0x0080))
			{
				//logerror("CPU#%d Output int = 1\n", cpunum);
				if (context->config->output_int)
					(*context->config->output_int)(1);
			}
			else if ((oldreg & 0x0080) && !(newreg & 0x0080))
			{
				//logerror("CPU#%d Output int = 0\n", cpunum);
				if (context->config->output_int)
					(*context->config->output_int)(0);
			}

			/* input interrupt? (should really be state-based, but the functions don't exist!) */
			if (!(oldreg & 0x0008) && (newreg & 0x0008))
			{
				//logerror("CPU#%d Input int = 1\n", cpunum);
				cpu_generate_internal_interrupt(cpunum, TMS34010_HI);
			}
			else if ((oldreg & 0x0008) && !(newreg & 0x0008))
			{
				//logerror("CPU#%d Input int = 0\n", cpunum);
				CONTEXT_IOREG(context, REG_INTPEND) &= ~TMS34010_HI;
			}
			break;

		case REG_CONVDP:
			context->xytolshiftcount1 = ~data & 0x0f;
			break;

		case REG_INTENB:
			if (CONTEXT_IOREG(context, REG_INTENB) & CONTEXT_IOREG(context, REG_INTPEND))
				check_interrupt();
			break;
	}

	/*if (LOG_CONTROL_REGS)
		logerror("CPU#%d: %s = %04X (%d)\n", cpunum, ioreg_name[reg], CONTEXT_IOREG(context, reg), cpu_getscanline());*/
}

WRITE_HANDLER( tms34010_io_register_w )
{
	if (!host_interface_context)
		common_io_register_w(cpu_getactivecpu(), &state, offset, data);
	else
		common_io_register_w(host_interface_cpu, host_interface_context, offset, data);
}



/*###################################################################################################
**	I/O REGISTER READS
**#################################################################################################*/

static int common_io_register_r(int cpunum, tms34010_regs *context, int reg)
{
	int result, total;

	reg >>= 1;
	/*if (LOG_CONTROL_REGS)
		logerror("CPU#%d: read %s\n", cpunum, ioreg_name[reg]);*/

	switch (reg)
	{
		case REG_VCOUNT:
			return scanline_to_vcount(context, cpu_getscanline());

		case REG_HCOUNT:

			/* scale the horizontal position from screen width to HTOTAL */
			result = cpu_gethorzbeampos();
			total = CONTEXT_IOREG(context, REG_HTOTAL);
			result = result * total / Machine->drv->screen_width;

			/* offset by the HBLANK end */
			result += CONTEXT_IOREG(context, REG_HEBLNK);

			/* wrap around */
			if (result > total)
				result -= total;
			return result;

		case REG_DPYADR:
			update_display_address(context, scanline_to_vcount(context, cpu_getscanline()));
			break;
	}

	return CONTEXT_IOREG(context, reg);
}


READ_HANDLER( tms34010_io_register_r )
{
	if (!host_interface_context)
		return common_io_register_r(cpu_getactivecpu(), &state, offset);
	else
		return common_io_register_r(host_interface_cpu, host_interface_context, offset);
}



/*###################################################################################################
**	UTILITY FUNCTIONS
**#################################################################################################*/

int tms34010_io_display_blanked(int cpu)
{
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpu);
	return (!(context->IOregs[REG_DPYCTL] & 0x8000));
}


int tms34010_get_DPYSTRT(int cpu)
{
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpu);
	return context->IOregs[REG_DPYSTRT];
}



/*###################################################################################################
**	SAVE STATE
**#################################################################################################*/

void tms34010_state_save(int cpunum, void *f)
{
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);
	osd_fwrite(f,context,sizeof(state));
	osd_fwrite(f,&tms34010_ICount,sizeof(tms34010_ICount));
	osd_fwrite(f,state.shiftreg,sizeof(SHIFTREG_SIZE));
}


void tms34010_state_load(int cpunum, void *f)
{
	/* Don't reload the following */
	UINT16 *shiftreg_save = state.shiftreg;
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);

	osd_fread(f,context,sizeof(state));
	osd_fread(f,&tms34010_ICount,sizeof(tms34010_ICount));
	change_pc29(PC);
	SET_FW();
	tms34010_io_register_w(REG_DPYINT<<1,IOREG(REG_DPYINT));
	set_raster_op(&state);
	set_pixel_function(&state);

	state.shiftreg      = shiftreg_save;

	osd_fread(f,state.shiftreg,sizeof(SHIFTREG_SIZE));
}



/*###################################################################################################
**	HOST INTERFACE WRITES
**#################################################################################################*/

void tms34010_host_w(int cpunum, int reg, int data)
{
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);
	const struct cpu_interface *interface;
	unsigned int addr;
	int oldcpu;

	switch (reg)
	{
		/* upper 16 bits of the address */
		case TMS34010_HOST_ADDRESS_H:
			CONTEXT_IOREG(context, REG_HSTADRH) = data;
			break;

		/* lower 16 bits of the address */
		case TMS34010_HOST_ADDRESS_L:
			CONTEXT_IOREG(context, REG_HSTADRL) = data & 0xfff0;
			break;

		/* actual data */
		case TMS34010_HOST_DATA:

			/* swap to the target cpu */
			oldcpu = cpu_getactivecpu();
			memorycontextswap(cpunum);

			/* write to the address */
			host_interface_cpu = cpunum;
			host_interface_context = context;
			addr = (CONTEXT_IOREG(context, REG_HSTADRH) << 16) | CONTEXT_IOREG(context, REG_HSTADRL);
			TMS34010_WRMEM_WORD(TOBYTE(addr), data);
			host_interface_context = NULL;

			/* optional postincrement */
			if (CONTEXT_IOREG(context, REG_HSTCTLH) & 0x0800)
			{
				addr += 0x10;
				CONTEXT_IOREG(context, REG_HSTADRH) = addr >> 16;
				CONTEXT_IOREG(context, REG_HSTADRL) = (UINT16)addr;
			}

			/* swap back */
			memorycontextswap(oldcpu);
			interface = &cpuintf[Machine->drv->cpu[oldcpu].cpu_type & ~CPU_FLAGS_MASK];
			(*interface->set_op_base)((*interface->get_pc)());
			break;

		/* control register */
		case TMS34010_HOST_CONTROL:
			common_io_register_w(cpunum, context, REG_HSTCTLH * 2, data & 0xff00);
			common_io_register_w(cpunum, context, REG_HSTCTLL * 2, data & 0x00ff);
			break;

		/* error case */
		default:
			//logerror("tms34010_host_control_w called on invalid register %d\n", reg);
			break;
	}
}



/*###################################################################################################
**	HOST INTERFACE READS
**#################################################################################################*/

int tms34010_host_r(int cpunum, int reg)
{
	tms34010_regs *context = (tms34010_regs *)FINDCONTEXT(cpunum);
	const struct cpu_interface *interface;
	unsigned int addr;
	int oldcpu, result;

	switch (reg)
	{
		/* upper 16 bits of the address */
		case TMS34010_HOST_ADDRESS_H:
			return CONTEXT_IOREG(context, REG_HSTADRH);

		/* lower 16 bits of the address */
		case TMS34010_HOST_ADDRESS_L:
			return CONTEXT_IOREG(context, REG_HSTADRL);

		/* actual data */
		case TMS34010_HOST_DATA:

			/* swap to the target cpu */
			oldcpu = cpu_getactivecpu();
			memorycontextswap(cpunum);

			/* read from the address */
			host_interface_cpu = cpunum;
			host_interface_context = context;
			addr = (CONTEXT_IOREG(context, REG_HSTADRH) << 16) | CONTEXT_IOREG(context, REG_HSTADRL);
			result = TMS34010_RDMEM_WORD(TOBYTE(addr));
			host_interface_context = NULL;

			/* optional postincrement (it says preincrement, but data is preloaded, so it
			   is effectively a postincrement */
			if (CONTEXT_IOREG(context, REG_HSTCTLH) & 0x1000)
			{
				addr += 0x10;
				CONTEXT_IOREG(context, REG_HSTADRH) = addr >> 16;
				CONTEXT_IOREG(context, REG_HSTADRL) = (UINT16)addr;
			}

			/* swap back */
			memorycontextswap(oldcpu);
			interface = &cpuintf[Machine->drv->cpu[oldcpu].cpu_type & ~CPU_FLAGS_MASK];
			(*interface->set_op_base)((*interface->get_pc)());
			return result;

		/* control register */
		case TMS34010_HOST_CONTROL:
			return (CONTEXT_IOREG(context, REG_HSTCTLH) & 0xff00) | (CONTEXT_IOREG(context, REG_HSTCTLL) & 0x00ff);
	}

	/* error case */
	//logerror("tms34010_host_control_r called on invalid register %d\n", reg);
	return 0;
}
