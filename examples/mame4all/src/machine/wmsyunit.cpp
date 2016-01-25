/*************************************************************************

	Driver for Williams/Midway games using the TMS34010 processor

	Hints for finding speedups:
	
		search disassembly for ": CAF9"

**************************************************************************/

#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "cpu/m6809/m6809.h"
#include "6821pia.h"
#include "sndhrdw/williams.h"



/* constant definitions */
#define SOUND_NARC					1
#define SOUND_CVSD_SMALL			2
#define SOUND_CVSD					3
#define SOUND_ADPCM					4


/* protection data types */
struct protection_data
{
	UINT16	reset_sequence[3];
	UINT16	data_sequence[100];
};
static const struct protection_data *prot_data;
static UINT16 prot_result;
static UINT16 prot_sequence[3];
static UINT8 prot_index;


/* speedup installation macros */
#define INSTALL_SPEEDUP_1_16BIT(addr, pc, spin1, offs1, offs2) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = offs1; \
	wms_speedup_spin[2] = offs2; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_1_16bit);
	
#define INSTALL_SPEEDUP_1_MIXEDBITS(addr, pc, spin1, offs1, offs2) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = offs1; \
	wms_speedup_spin[2] = offs2; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_1_mixedbits);
	
#define INSTALL_SPEEDUP_1_32BIT(addr, pc, spin1, offs1, offs2) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = offs1; \
	wms_speedup_spin[2] = offs2; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_1_32bit);
	
#define INSTALL_SPEEDUP_3(addr, pc, spin1, spin2, spin3) \
	wms_speedup_pc = (pc); \
	wms_speedup_offset = ((addr) & 0x10) >> 3; \
	wms_speedup_spin[0] = spin1; \
	wms_speedup_spin[1] = spin2; \
	wms_speedup_spin[2] = spin3; \
	wms_speedup_base = (UINT8*)install_mem_read_handler(0, TOBYTE((addr) & ~0x1f), TOBYTE((addr) | 0x1f), wms_generic_speedup_3);
	

/* code related variables */
       UINT8 *	wms_code_rom;
       UINT8 *	wms_scratch_ram;

/* input-related variables */
static UINT8	term2_analog_select;

/* CMOS-related variables */
       UINT8 *	wms_cmos_ram;
       UINT32 	wms_cmos_page;
static UINT8	cmos_w_enable;

/* graphics-related variables */
extern struct rectangle wms_visible_area;
extern UINT8 *	wms_gfx_rom;
extern size_t	wms_gfx_rom_size;

/* update-related variables */
extern UINT8	wms_partial_update_offset;

/* sound-related variables */
static UINT8	sound_type;

/* speedup-related variables */
       offs_t 	wms_speedup_pc;
       offs_t 	wms_speedup_offset;
       offs_t 	wms_speedup_spin[3];
       UINT8 *	wms_speedup_base;

/* hack-related variables */
static UINT8 *	t2_hack_mem;



/*************************************
 *
 *	CMOS reads/writes
 *
 *************************************/

WRITE_HANDLER( wms_yunit_cmos_w )
{
	//logerror("%08x:CMOS Write @ %05X\n", cpu_get_pc(), offset);
	COMBINE_WORD_MEM(&wms_cmos_ram[offset + wms_cmos_page], data);
}


READ_HANDLER( wms_yunit_cmos_r )
{
	return READ_WORD(&wms_cmos_ram[offset + wms_cmos_page]);
}



/*************************************
 *
 *	CMOS enable and protection
 *
 *************************************/

WRITE_HANDLER( wms_yunit_cmos_enable_w )
{
	cmos_w_enable = (~data >> 9) & 1;

	//logerror("%08x:Protection write = %04X\n", cpu_get_pc(), data);

	/* only go down this path if we have a data structure */
	if (prot_data)
	{
		/* mask off the data */
		data &= 0x0f00;
		
		/* update the FIFO */
		prot_sequence[0] = prot_sequence[1];
		prot_sequence[1] = prot_sequence[2];
		prot_sequence[2] = data;
		
		/* special case: sequence entry 1234 means Strike Force, which is different */
		if (prot_data->reset_sequence[0] == 0x1234)
		{
			if (data == 0x500)
			{
				prot_result = cpu_readmem29_word(TOBYTE(0x10a4390)) << 4;
				//logerror("  desired result = %04X\n", prot_result);
			}
		}
		
		/* all other games use the same pattern */
		else
		{
			/* look for a reset */
			if (prot_sequence[0] == prot_data->reset_sequence[0] &&
				prot_sequence[1] == prot_data->reset_sequence[1] &&
				prot_sequence[2] == prot_data->reset_sequence[2])
			{
				//logerror("Protection reset\n");
				prot_index = 0;
			}
			
			/* look for a clock */
			if ((prot_sequence[1] & 0x0800) != 0 && (prot_sequence[2] & 0x0800) == 0)
			{
				prot_result = prot_data->data_sequence[prot_index++];
				//logerror("Protection clock (new data = %04X)\n", prot_result);
			}
		}
	}
}


READ_HANDLER( wms_yunit_protection_r )
{
	/* return the most recently clocked value */
	//logerror("%08X:Protection read = %04X\n", cpu_get_pc(), prot_result);
	return prot_result;
}



/*************************************
 *
 *	Generic input ports
 *
 *************************************/

READ_HANDLER( wms_yunit_input_r )
{
	return readinputport(offset / 2);
}



/*************************************
 *
 *	Special Terminator 2 input ports
 *
 *************************************/

static READ_HANDLER( term2_input_r )
{
	if (offset != 4)
		return readinputport(offset / 2);

	switch (term2_analog_select)
	{
		default:
		case 0:  return input_port_2_r(offset);  break;
		case 1:  return input_port_6_r(offset);  break;
		case 2:  return input_port_7_r(offset);  break;
		case 3:  return input_port_8_r(offset);  break;
	}
}

static WRITE_HANDLER( term2_sound_w )
{
	if (offset == 0)
		term2_analog_select = (data >> 0x0c) & 0x03;

	williams_adpcm_data_w(offset, data);
}



/*************************************
 *
 *	Special Terminator 2 hack
 *
 *************************************/

static WRITE_HANDLER( term2_hack_w )
{
    if (offset == 0 && cpu_get_pc() == 0xffce5230)
    {
        WRITE_WORD(&t2_hack_mem[offset], 0);
        return;
    }
	COMBINE_WORD_MEM(&t2_hack_mem[offset], data);
}



/*************************************
 *
 *	Macros for speedup loops
 *
 *************************************/

#ifdef LSB_FIRST
	#define BYTE_XOR_LE(a)  (a)
	#define BIG_DWORD_LE(x) (x)
#else
	#define BYTE_XOR_LE(a)  ((UINT8*)((UINT32)(a) ^ 1))
	#define BIG_DWORD_LE(x) (((UINT32)(x) >> 16) + ((x) << 16))
#endif


#ifndef ALIGN_SHORTS
	#define READ_U16(a)    (*(INT16 *)a)
	#define WRITE_U16(a,x) (*(INT16 *)a = (x))
#else
	#ifdef LSB_FIRST  /* unaligned read and write macros, cpg */
		#define READ_U16(a)    ((INT16)(*(UINT8 *)a | (*((UINT8 *)a+1) << 8)))
		#define WRITE_U16(a,x) do { *(UINT8 *)a = x; *((UINT8 *)a+1) = (x>>8); } while (0)
	#else
		#define READ_U16(a)    ((INT16)((*(UINT8 *)a << 8) | *((UINT8 *)a+1)))
		#define WRITE_U16(a,x) do { *(UINT8 *)a = (x>>8); *((UINT8 *)a+1) = x; } while (0)
	#endif
#endif

#ifndef ALIGN_INTS
	#define READ_U32(a)    (*(INT32 *)a)
	#define WRITE_U32(a,x) (*(INT32 *)a = (x))
#else
	#ifdef LSB_FIRST  /* unaligned read and write macros, cpg */
		#define READ_U32(a) ((INT32)(*(UINT8 *)a | (*((UINT8 *)a+1) << 8) \
							| (*((UINT8 *)a+2) << 16) | (*((UINT8 *)a+3) << 24)))
		#define WRITE_U32(a,x) do { *(UINT8 *)a = x; *((UINT8 *)a+1) = (x>>8); \
							*((UINT8 *)a+2) = (x>>16); *((UINT8 *)a+3) = (x>>24); } while (0)
	#else
		#define READ_U32(a) ((INT32)((*(UINT8 *)a << 24) | (*((UINT8 *)a+1) << 16) \
							| (*((UINT8 *)a+2) << 8) | *((UINT8 *)a+3)))
		#define WRITE_U32(a,x) do { *(UINT8 *)a = (x>>24); *((UINT8 *)a+1) = (x>>16); \
							*((UINT8 *)a+2) = (x>>8); *((UINT8 *)a+3) = x; } while (0)
	#endif
#endif

#define SCRATCH_RAM(offs)		&wms_scratch_ram[TOBYTE((offs) & 0x3fffff)]

#define READ_INT8(REG)			(*(INT8 *)BYTE_XOR_LE(SCRATCH_RAM(REG)))
#define READ_INT16(REG)			READ_U16(SCRATCH_RAM(REG))
#define READ_INT32(REG)			BIG_DWORD_LE(READ_U32(SCRATCH_RAM(REG)))

#define WRITE_INT8(REG,DATA)	(*(INT8 *)BYTE_XOR_LE(SCRATCH_RAM(REG)) = (DATA))
#define WRITE_INT16(REG,DATA)	WRITE_U16(SCRATCH_RAM(REG), DATA)
#define WRITE_INT32(REG,DATA)	WRITE_U32(SCRATCH_RAM(REG), BIG_DWORD_LE(DATA))

#define BURN_TIME(INST_CNT)		tms34010_ICount -= INST_CNT

/* General speed up loop body */
#define DO_SPEEDUP_LOOP(X, LOC, OFFS1, OFFS2, A8SIZE, A7SIZE)	\
{																\
	UINT32 a0 = LOC;											\
	UINT32 a2;													\
	UINT32 a4 = 0;                               				\
	 INT32 a1 = 0x80000000;										\
	 INT32 a5 = 0x80000000;										\
	while ((a2 = READ_INT32(a0)) != 0 && tms34010_ICount > 0)	\
	{															\
		INT32 a8 = READ_##A8SIZE(a2 + OFFS1);					\
		INT32 a7 = READ_##A7SIZE(a2 + OFFS2);					\
																\
		if (a8 > a1)											\
		{														\
			BURN_TIME(22);										\
			a4 = a0;											\
			a0 = a2;											\
			a1 = a8;											\
			a5 = a7;											\
			continue;											\
		}														\
		if (a8 < a1)											\
		{														\
			BURN_TIME(45);										\
			WRITE_INT32(a4, a2);								\
			WRITE_INT32(a0, READ_INT32(a2));					\
			WRITE_INT32(a2, a0);								\
			a4 = a2;											\
			continue;											\
		}														\
		if (a7 >= a5)											\
		{														\
			BURN_TIME(25);										\
			a4 = a0;											\
			a0 = a2;											\
			a1 = a8;											\
			a5 = a7;											\
			continue;											\
		}														\
		BURN_TIME(46);											\
		WRITE_INT32(a4, a2);									\
		WRITE_INT32(a0, READ_INT32(a2));						\
		WRITE_INT32(a2, a0);									\
		a4 = a2;												\
	}															\
}																\



/*************************************
 *
 *	Terminator 2 speedup loop (icky!)
 *
 *************************************/

#define T2_FFC08C40																\
	a5x = (INT32)(READ_INT8(a1+0x2d0));				/* MOVB   *A1(2D0h),A5  */  \
	WRITE_INT8(a1+0x2d0, a2 & 0xff);				/* MOVB   A2,*A1(2D0h)  */  \
	a3x = 0xf0;										/* MOVI   F0h,A3		*/	\
	a5x = (UINT32)a5x * (UINT32)a3x;				/* MPYU   A3,A5			*/	\
	a5x += 0x1008000;								/* ADDI   1008000h,A5	*/  \
	a3x = (UINT32)a2  * (UINT32)a3x;				/* MPYU   A2,A3			*/	\
	a3x += 0x1008000;								/* ADDI   1008000h,A3	*/  \
	a7x = (INT32)(READ_INT16(a1+0x190));			/* MOVE   *A1(190h),A7,0*/	\
	a6x = (INT32)(READ_INT16(a5x+0x50));			/* MOVE   *A5(50h),A6,0 */	\
	a7x -= a6x;										/* SUB    A6,A7			*/  \
	a6x = (INT32)(READ_INT16(a3x+0x50));			/* MOVE   *A3(50h),A6,0 */	\
	a7x += a6x;										/* ADD    A6,A7			*/  \
	WRITE_INT16(a1+0x190, a7x & 0xffff);			/* MOVE   A7,*A1(190h),0*/	\
	a5x = READ_INT32(a5x+0xa0);						/* MOVE   *A5(A0h),A5,1 */	\
	a3x = READ_INT32(a3x+0xa0);						/* MOVE   *A3(A0h),A3,1 */	\
	a6x = READ_INT32(a1+0x140);						/* MOVE   *A1(140h),A6,1*/	\
	a6xa7x = (INT64)a6x * a3x / a5x;				/* MPYS   A3,A6			*/  \
													/* DIVS   A5,A6			*/  \
	WRITE_INT32(a1+0x140, a6xa7x & 0xffffffff);		/* MOVE   A6,*A1(140h),1*/

static READ_HANDLER( term2_speedup_r )
{
	if (offset)
	{
		return READ_WORD(&wms_scratch_ram[TOBYTE(0xaa050)]);
	}
	else
	{
		UINT32 value1 = READ_WORD(&wms_scratch_ram[TOBYTE(0xaa040)]);

		/* Suspend cpu if it's waiting for an interrupt */
		if (cpu_get_pc() == 0xffcdc270 && !value1)
		{
			INT32 a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a14,b0,b1,b2;
			INT32 a3x,a5x,a6x,a7x;
			INT64 a6xa7x;

			b1 = 0;			 									/* CLR    B1 */
			b2 = (INT32)(READ_INT16(0x100F640));				/* MOVE   @100F640h,B2,0 */
			if (!b2)											/* JREQ   FFC029F0h */
			{
				cpu_spinuntil_int();
				return value1;
			}
			b2--;												/* DEC    B2 */
			b0  = 0x01008000;									/* MOVI   1008000h,B0 */
			a4  = 0x7fffffff;									/* MOVI   7FFFFFFFh,A4 */

			while (1)
			{
				/* FFC07770 */
				a10 = b0;										/* MOVE   B0,A10 */
				a0  = a10;										/* MOVE   A10,A0 */
				a3  = a4;										/* MOVE   A4,A3 */
																/* CMP    B2,B1 */
				if (b1 < b2)									/* JRLT   FFC07800h */
				{
					/* FFC07800 */
					a4 = (INT32)(READ_INT16(a10+0xc0));			/* MOVE   *A10(C0h),A4,0 */
					a4 <<= 16;									/* SLL    10h,A4 */
				}
				else
				{
					/* FFC077C0 */
					a4 = 0x80000000;							/* MOVI   80000000h,A4 */
				}												/* JR     FFC07830h */

				/* FFC07830 */
				b0 += 0xf0;										/* ADDI   F0h,B0 */
				a6 = 0x80000000;								/* MOVI   80000000h,A6 */
				a5 = 0x80000000;								/* MOVI   80000000h,A5 */
				goto t2_FFC07DD0;								/* JR     FFC07DD0h */

			t2_FFC078C0:
				a8  = READ_INT32(a1+0x1c0);						/* MOVE   *A1(1C0h),A8,1 */
				a7  = READ_INT32(a1+0x1a0);						/* MOVE   *A1(1A0h),A7,1 */
				a14 = (INT32)(READ_INT16(a1+0x220));			/* MOVE   *A1(220h),A14,0 */
				if (a14 & 0x6000)								/* BTST   Eh,A14 */
				{												/* JRNE   FFC07C50h */
					goto t2_FFC07C50;							/* BTST   Dh,A14 */
				}												/* JRNE   FFC07C50h */

				if (a8 <= a3)									/* CMP    A3,A8 */
				{
					goto t2_FFC07AE0;							/* JRLE   FFC07AE0h */
				}

				a2 = b1 - 1;									/* MOVE   B1,A2;  DEC    A2 */
				T2_FFC08C40										/* CALLR  FFC08C40h */
				a14 = READ_INT32(a1);							/* MOVE   *A1,A14,1 */
				WRITE_INT32(a0, a14);							/* MOVE   A14,*A0,1 */
				WRITE_INT32(a14+0x20, a0);						/* MOVE   A0,*A14(20h),1 */
				a14 = b0 - 0x1e0;								/* MOVE   B0,A14; SUBI   1E0h,A14 */
				WRITE_INT32(a1+0x20, a14);						/* MOVE   A14,*A1(20h),1 */
				a9 = READ_INT32(a14);							/* MOVE   *A14,A9,1 */
				WRITE_INT32(a14, a1);							/* MOVE   A1,*A14,1 */
				WRITE_INT32(a9+0x20, a1);						/* MOVE   A1,*A9(20h),1 */
				WRITE_INT32(a1, a9);							/* MOVE   A9,*A1,1 */
				goto t2_FFC07DD0;								/* JR     FFC07DD0h */

			t2_FFC07AE0:
				if (a8 >= a4)									/* CMP    A4,A8 */
				{
					goto t2_FFC07C50;							/* JRGE   FFC07C50h */
				}

				a2 = b1 + 1;									/* MOVE   B1,A2; INC    A2 */
				T2_FFC08C40										/* CALLR  FFC08C40h */
				a14 = READ_INT32(a1);							/* MOVE   *A1,A14,1 */
				WRITE_INT32(a0, a14);							/* MOVE   A14,*A0,1 */
				WRITE_INT32(a14+0x20, a0);						/* MOVE   A0,*A14(20h),1 */
				a14 = b0;										/* MOVE   B0,A14 */
				a9 = READ_INT32(a14+0x20);						/* MOVE   *A14(20h),A9,1 */
				WRITE_INT32(a1, a14);							/* MOVE   A14,*A1,1 */
				WRITE_INT32(a14+0x20, a1);						/* MOVE   A1,*A14(20h),1 */
				WRITE_INT32(a9, a1);							/* MOVE   A1,*A9,1 */
				WRITE_INT32(a1+0x20, a9);						/* MOVE   A9,*A1(20h),1 */
				goto t2_FFC07DD0;

			t2_FFC07C50:
				if (a8 > a6) 									/* CMP    A6,A8 */
				{
					a1 = a0; 									/* MOVE   A1,A0 */
					a6 = a8; 									/* MOVE   A8,A6 */
					a5 = a7; 									/* MOVE   A7,A5 */
					goto t2_FFC07DD0;
				}

				if ((a8 == a6) && (a7 >= a5)) 					/* CMP    A5,A7 */
				{
					a1 = a0; 									/* MOVE   A1,A0 */
					a6 = a8; 									/* MOVE   A8,A6 */
					a5 = a7; 									/* MOVE   A7,A5 */
					goto t2_FFC07DD0;
				}

				/* FFC07CC0 */
				a14 = READ_INT32(a0+0x20);						/* MOVE   *A0(20h),A14,1 */
				WRITE_INT32(a14, a1);							/* MOVE   A1,*A14,1 */
				WRITE_INT32(a1+0x20, a14);						/* MOVE   A14,*A1(20h),1 */
				a14 = READ_INT32(a1);							/* MOVE   *A1,A14,1 */
				WRITE_INT32(a0, a14);							/* MOVE   A14,*A0,1 */
				WRITE_INT32(a1, a0);							/* MOVE   A0,*A1,1 */
				WRITE_INT32(a0 +0x20, a1);						/* MOVE   A1,*A0(20h),1 */
				WRITE_INT32(a14+0x20, a0);						/* MOVE   A0,*A14(20h),1 */

			t2_FFC07DD0:
				BURN_TIME(50);
				if (tms34010_ICount <= 0)
				{
					break;
				}

				a1 = READ_INT32(a0);							/* MOVE   *A0,A1,1 */
				if (a10 != a1)									/* CMP    A1,A10 */
				{
					goto t2_FFC078C0;							/* JRNE   FFC078C0h */
				}

				b1++;											/* INC    B1 */
				if (b1 > b2)									/* CMP    B2,B1 */
				{
					cpu_spinuntil_int();
					return value1;								/* JRLE   FFC07770h; RTS */
				}
			}
		}

		return value1;
	}
}



/*************************************
 *
 *	Speedups with 1 location
 *	(16-bit version)
 *
 *************************************/

#define DO_SPEEDUP_LOOP_1(LOC, OFFS1, OFFS2, A8SIZE, A7SIZE)\
															\
	DO_SPEEDUP_LOOP(A, LOC, OFFS1, OFFS2, A8SIZE, A7SIZE);	\
	if (tms34010_ICount > 0)								\
		cpu_spinuntil_int();								\

READ_HANDLER( wms_generic_speedup_1_16bit )
{
	UINT16 value = READ_WORD(&wms_speedup_base[offset]);
	
	/* just return if this isn't the offset we're looking for */
	if (offset != wms_speedup_offset)
		return value;

	/* suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == wms_speedup_pc && !value)
	{
		DO_SPEEDUP_LOOP_1(wms_speedup_spin[0], wms_speedup_spin[1], wms_speedup_spin[2], INT16, INT16);
	}
	return value;
}



/*************************************
 *
 *	Speedups with 1 location
 *	(mixed 16/32-bit version)
 *
 *************************************/

READ_HANDLER( wms_generic_speedup_1_mixedbits )
{
	UINT16 value = READ_WORD(&wms_speedup_base[offset]);
	
	/* just return if this isn't the offset we're looking for */
	if (offset != wms_speedup_offset)
		return value;

	/* suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == wms_speedup_pc && !value)
	{
		DO_SPEEDUP_LOOP_1(wms_speedup_spin[0], wms_speedup_spin[1], wms_speedup_spin[2], INT16, INT32);
	}
	return value;
}



/*************************************
 *
 *	Speedups with 1 location
 *	(32-bit version)
 *
 *************************************/

READ_HANDLER( wms_generic_speedup_1_32bit )
{
	UINT16 value = READ_WORD(&wms_speedup_base[offset]);
	
	/* just return if this isn't the offset we're looking for */
	if (offset != wms_speedup_offset)
		return value;

	/* suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == wms_speedup_pc && !value)
	{
		DO_SPEEDUP_LOOP_1(wms_speedup_spin[0], wms_speedup_spin[1], wms_speedup_spin[2], INT32, INT32);
	}
	return value;
}



/*************************************
 *
 *	Speedups with 3 locations
 *
 *************************************/

#define DO_SPEEDUP_LOOP_3(LOC1, LOC2, LOC3)					\
															\
	UINT32 temp1,temp2,temp3;								\
															\
	while (tms34010_ICount > 0) 							\
	{														\
		temp1 = READ_INT32(LOC1);							\
		temp2 = READ_INT32(LOC2);							\
		temp3 = READ_INT32(LOC3);							\
		if (!temp1 && !temp2 && !temp3)						\
		{													\
			cpu_spinuntil_int();							\
			break;											\
		}													\
		DO_SPEEDUP_LOOP(A, LOC1, 0xc0, 0xa0, INT32, INT32);	\
		DO_SPEEDUP_LOOP(B, LOC2, 0xc0, 0xa0, INT32, INT32);	\
		DO_SPEEDUP_LOOP(C, LOC3, 0xc0, 0xa0, INT32, INT32);	\
	}

READ_HANDLER( wms_generic_speedup_3 )
{
	UINT16 value = READ_WORD(&wms_speedup_base[offset]);
	
	/* just return if this isn't the offset we're looking for */
	if (offset != wms_speedup_offset)
		return value;

	/* suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == wms_speedup_pc && !value)
	{
		DO_SPEEDUP_LOOP_3(wms_speedup_spin[0], wms_speedup_spin[1], wms_speedup_spin[2]);
	}
	return value;
}



/*************************************
 *
 *	Generic driver init
 *
 *************************************/

static UINT8 *cvsd_protection_base;
static WRITE_HANDLER( cvsd_protection_w )
{
	/* because the entire CVSD ROM is banked, we have to make sure that writes */
	/* go to the proper location (i.e., bank 0); currently bank 0 always lives */
	/* in the 0x10000-0x17fff space, so we just need to add 0x8000 to get the  */
	/* proper offset */
	cvsd_protection_base[0x8000 + offset] = data;
}


static void init_generic(int bpp, int sound, int prot_start, int prot_end, int max_x, int max_y)
{
	offs_t gfx_chunk = wms_gfx_rom_size / 4;
	UINT8 d1, d2, d3, d4, d5, d6;
	UINT8 *base;
	int i;
	
	/* set up code ROMs */
	memcpy(wms_code_rom, memory_region(REGION_USER1), memory_region_length(REGION_USER1));
	
	/* load graphics ROMs */
	base = memory_region(REGION_GFX1);
	switch (bpp)
	{
		case 4:
			for (i = 0; i < wms_gfx_rom_size; i += 2)
			{
				d1 = ((base[0 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d2 = ((base[1 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d3 = ((base[0 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d4 = ((base[1 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
		
				wms_gfx_rom[i + 0] = d1 | (d2 << 2);
				wms_gfx_rom[i + 1] = d3 | (d4 << 2);
			}
			break;

		case 6:
			for (i = 0; i < wms_gfx_rom_size; i += 2)
			{
				d1 = ((base[0 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d2 = ((base[1 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d3 = ((base[2 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d4 = ((base[0 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d5 = ((base[1 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d6 = ((base[2 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
		
				wms_gfx_rom[i + 0] = d1 | (d2 << 2) | (d3 << 4);
				wms_gfx_rom[i + 1] = d4 | (d5 << 2) | (d6 << 4);
			}
			break;
			
		case 8:
			for (i = 0; i < wms_gfx_rom_size; i += 4)
			{
				wms_gfx_rom[i + 0] = base[0 * gfx_chunk + i / 4];
				wms_gfx_rom[i + 1] = base[1 * gfx_chunk + i / 4];
				wms_gfx_rom[i + 2] = base[2 * gfx_chunk + i / 4];
				wms_gfx_rom[i + 3] = base[3 * gfx_chunk + i / 4];
			}
			break;
	}
	
	/* load sound ROMs and set up sound handlers */
	sound_type = sound;
	switch (sound)
	{
		case SOUND_CVSD_SMALL:
			base = memory_region(REGION_CPU2);
			memcpy(&base[0x20000], &base[0x10000], 0x10000);
			memcpy(&base[0x40000], &base[0x30000], 0x10000);
			memcpy(&base[0x60000], &base[0x50000], 0x10000);
			cvsd_protection_base = (UINT8*)install_mem_write_handler(1, prot_start, prot_end, cvsd_protection_w);
			break;
			
		case SOUND_ADPCM:
			base = memory_region(REGION_SOUND1);
			memcpy(base + 0xa0000, base + 0x20000, 0x20000);
			memcpy(base + 0x80000, base + 0x60000, 0x20000);
			memcpy(base + 0x60000, base + 0x20000, 0x20000);
			install_mem_write_handler(1, prot_start, prot_end, MWA_RAM);
			break;
		
		case SOUND_CVSD:
		case SOUND_NARC:
			install_mem_write_handler(1, prot_start, prot_end, MWA_RAM);
			break;
	}
	
	/* default visible area */
	wms_visible_area = Machine->drv->default_visible_area;
	wms_visible_area.max_x = max_x;
	wms_visible_area.max_y = max_y;
	
	/* default update offset */
	wms_partial_update_offset = 0;
}



/*************************************
 *
 *	Z-unit init
 *
 * 	music: 6809 driving YM2151, DAC
 * 	effects: 6809 driving CVSD, DAC
 *
 *************************************/

void init_narc(void)
{
	/* common init */
	init_generic(8, SOUND_NARC, 0xcdff, 0xce29, 511, 426);

	/* speedups */
	INSTALL_SPEEDUP_1_32BIT(0x0101b310, 0xffde33e0, 0x1000040, 0xc0, 0xa0);
}

void init_narc3(void)
{
	UINT32 bank, offset;

	/* common init */
	init_generic(8, SOUND_NARC, 0xcdff, 0xce29, 511, 426);

	/* expand code ROMs */
	for (bank = 0; bank < 4; bank++)
		for (offset = 0; offset < 0x20000; offset++)
			wms_code_rom[bank * 0x40000 + 0x20000 + offset] = wms_code_rom[bank * 0x40000 + offset];

	/* speedups */
	INSTALL_SPEEDUP_1_32BIT(0x0101b310, 0xffae30c0, 0x1000040, 0xc0, 0xa0);
}



/*************************************
 *
 *	Y-unit init (CVSD)
 *
 * 	music: 6809 driving YM2151, DAC, and CVSD
 *
 *************************************/


/********************** Trog **************************/

static void init_trog_common(void)
{
	/* protection */
	static const struct protection_data trog_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x3000, 0x1000, 0x2000, 0x0000, 
		  0x2000, 0x3000,
		  0x3000, 0x1000,
		  0x0000, 0x0000, 0x2000, 0x3000, 0x1000, 0x1000, 0x2000 }
	};
	prot_data = &trog_protection_data;

	/* common init */
	init_generic(4, SOUND_CVSD_SMALL, 0x9eaf, 0x9ed9, 394, 274);
}

void init_trog(void)
{
	init_trog_common();
	INSTALL_SPEEDUP_1_32BIT(0x010a20a0, 0xffe20630, 0x1000040, 0xc0, 0xa0);
}

void init_trog3(void)
{
	init_trog_common();
	INSTALL_SPEEDUP_1_32BIT(0x010a2090, 0xffe20660, 0x1000040, 0xc0, 0xa0);
}

void init_trogp(void)
{
	init_trog_common();
	INSTALL_SPEEDUP_1_32BIT(0x010a1ee0, 0xffe210d0, 0x1000040, 0xc0, 0xa0);
}


/********************** Smash TV **********************/

static void init_smashtv_common(void)
{
	/* common init */
	init_generic(6, SOUND_CVSD_SMALL, 0x9cf6, 0x9d21, 394, 274);
}

void init_smashtv(void)
{
	init_smashtv_common();
	INSTALL_SPEEDUP_1_MIXEDBITS(0x01086760, 0xffe0a340, 0x1000040, 0xa0, 0x80);
}

void init_smashtv4(void)
{
	init_smashtv_common();
	INSTALL_SPEEDUP_1_MIXEDBITS(0x01086790, 0xffe0a320, 0x1000040, 0xa0, 0x80);
}


/********************** High Impact Football **********************/

static void init_hiimpact_common(void)
{
	/* protection */
	static const struct protection_data hiimpact_protection_data =
	{
		{ 0x0b00, 0x0b00, 0x0b00 },
		{ 0x2000, 0x4000, 0x4000, 0x0000, 0x6000, 0x6000, 0x2000, 0x4000, 
		  0x2000, 0x4000, 0x2000, 0x0000, 0x4000, 0x6000, 0x2000 }
	};
	prot_data = &hiimpact_protection_data;

	/* common init */
	init_generic(6, SOUND_CVSD, 0x9b79, 0x9ba3, 394, 274);
}

void init_hiimpact(void)
{
	init_hiimpact_common();
	INSTALL_SPEEDUP_3(0x01053150, 0xffe28bb0, 0x1000080, 0x10000a0, 0x10000c0);
}


/********************** Super High Impact Football **********************/

static void init_shimpact_common(void)
{
	/* protection */
	static const struct protection_data shimpact_protection_data =
	{
		{ 0x0f00, 0x0e00, 0x0d00 },
		{ 0x0000, 0x4000, 0x2000, 0x5000, 0x2000, 0x1000, 0x4000, 0x6000,
		  0x3000, 0x0000, 0x2000, 0x5000, 0x5000, 0x5000, 0x2000 }
	};
	prot_data = &shimpact_protection_data;

	/* common init */
	init_generic(6, SOUND_CVSD, 0x9c06, 0x9c15, 394, 274);
}

void init_shimpact(void)
{
	init_shimpact_common();
	INSTALL_SPEEDUP_3(0x01052070, 0xffe27f00, 0x1000000, 0x1000020, 0x1000040);
}


/********************** Strike Force **********************/

static void init_strkforc_common(void)
{
	/* protection */
	static const struct protection_data strkforc_protection_data =
	{
		{ 0x1234 }
	};
	prot_data = &strkforc_protection_data;

	/* common init */
	init_generic(4, SOUND_CVSD_SMALL, 0x9f7d, 0x9fa7, 399, 274);
}

void init_strkforc(void)
{
	init_strkforc_common();
	INSTALL_SPEEDUP_1_32BIT(0x01071dd0, 0xffe0a290, 0x1000060, 0xc0, 0xa0);
}



/*************************************
 *
 *	Y-unit init (ADPCM)
 *
 * 	music: 6809 driving YM2151, DAC, and OKIM6295
 *
 *************************************/


/********************** Mortal Kombat **********************/

static void init_mk_common(void)
{
	/* protection */
	static const struct protection_data mk_protection_data =
	{
		{ 0x0d00, 0x0c00, 0x0900 },
		{ 0x4600, 0xf600, 0xa600, 0x0600, 0x2600, 0x9600, 0xc600, 0xe600,
		  0x8600, 0x7600, 0x8600, 0x8600, 0x9600, 0xd600, 0x6600, 0xb600,
		  0xd600, 0xe600, 0xf600, 0x7600, 0xb600, 0xa600, 0x3600 }
	};
	prot_data = &mk_protection_data;

	/* common init */
	init_generic(6, SOUND_ADPCM, 0xfb9c, 0xfbc6, 398, 281);
}

void init_mkla1(void)
{
	init_mk_common();
	INSTALL_SPEEDUP_3(0x0104f000, 0xffcddc00, 0x104b6b0, 0x104b6f0, 0x104b710);
}

void init_mkla2(void)
{
	init_mk_common();
	INSTALL_SPEEDUP_3(0x0104f020, 0xffcde000, 0x104b6b0, 0x104b6f0, 0x104b710);
}

void init_mkla3(void)
{
	init_mk_common();
	INSTALL_SPEEDUP_3(0x0104f040, 0xffce1ec0, 0x104b6b0, 0x104b6f0, 0x104b710);
}

void init_mkla4(void)
{
	init_mk_common();
	INSTALL_SPEEDUP_3(0x0104f050, 0xffce21d0, 0x104b6b0, 0x104b6f0, 0x104b710);
}


/********************** Terminator 2 **********************/

static void init_term2_common(void)
{
	/* protection */
	static const struct protection_data term2_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4000, 0xf000, 0xa000 }
	};
	prot_data = &term2_protection_data;

	/* common init */
	init_generic(6, SOUND_ADPCM, 0xfa8d, 0xfa9c, 398, 281);

	/* special inputs */
	install_mem_read_handler(0, TOBYTE(0x01c00000), TOBYTE(0x01c0005f), term2_input_r);
	install_mem_write_handler(0, TOBYTE(0x01e00000), TOBYTE(0x01e0001f), term2_sound_w);

	/* HACK: this prevents the freeze on the movies */
	/* until we figure whats causing it, this is better than nothing */
	t2_hack_mem = (UINT8*)install_mem_write_handler(0, TOBYTE(0x010aa0e0),	TOBYTE(0x010aa0ff), term2_hack_w);
}

void init_term2(void)
{
	init_term2_common();
	install_mem_read_handler(0, TOBYTE(0x010aa040), TOBYTE(0x010aa05f), term2_speedup_r);
}


/********************** Total Carnage **********************/

static void init_totcarn_common(void)
{
	/* protection */
	static const struct protection_data totcarn_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4a00, 0x6a00, 0xda00, 0x6a00, 0x9a00, 0x4a00, 0x2a00, 0x9a00, 0x1a00,
		  0x8a00, 0xaa00 }
	};
	prot_data = &totcarn_protection_data;

	/* common init */
	init_generic(6, SOUND_ADPCM, 0xfc04, 0xfc2e, 394, 281);
	wms_partial_update_offset = 8;
}

void init_totcarn(void)
{
	init_totcarn_common();
	INSTALL_SPEEDUP_1_16BIT(0x0107dde0, 0xffc0c970, 0x1000040, 0xa0, 0x90);
}

void init_totcarnp(void)
{
	init_totcarn_common();
	INSTALL_SPEEDUP_1_16BIT(0x0107dde0, 0xffc0c970, 0x1000040, 0xa0, 0x90);
}



/*************************************
 *
 *	Machine init
 *
 *************************************/

void wms_yunit_init_machine(void)
{
	/* reset sound */
	switch (sound_type)
	{
		case SOUND_NARC:
			williams_narc_init(1);
			break;
		
		case SOUND_CVSD:
		case SOUND_CVSD_SMALL:
			pia_unconfig();
			williams_cvsd_init(1, 0);
			pia_reset();
			break;
		
		case SOUND_ADPCM:
			williams_adpcm_init(1);
			break;
	}
}



/*************************************
 *
 *	Sound write handlers
 *
 *************************************/

WRITE_HANDLER( wms_yunit_sound_w )
{
	/* check for out-of-bounds accesses */
	if (offset)
	{
		//logerror("%08X:Unexpected write to sound (hi) = %04X\n", cpu_get_pc(), data);
		return;
	}
	
	/* call through based on the sound type */
	switch (sound_type)
	{
		case SOUND_NARC:
			williams_narc_data_w(0, data);
			break;
	
		case SOUND_CVSD_SMALL:
		case SOUND_CVSD:
			williams_cvsd_data_w(0, (data & 0xff) | ((data & 0x200) >> 1));
			break;
		
		case SOUND_ADPCM:
			williams_adpcm_data_w(0, data);
			break;
	}
}
