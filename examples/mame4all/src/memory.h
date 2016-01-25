#ifndef MEMORY_H
#define MEMORY_H

#include "osd_cpu.h"
#include <stddef.h>


#define MAX_BANKS		16


/***************************************************************************
	Core memory read/write/opbase handler types.
***************************************************************************/

/* ----- typedefs for data and offset types ----- */
typedef UINT8			data8_t;
typedef UINT16			data16_t;
typedef UINT32			data32_t;
typedef UINT32 offs_t;
typedef UINT32 data_t;

typedef data_t (*mem_read_handler)(offs_t offset);
typedef void (*mem_write_handler)(offs_t offset,data_t data);
typedef offs_t (*opbase_handler)(offs_t address);

#ifdef DJGPP
#define READ_HANDLER(name)		data_t name(offs_t __attribute__ ((unused)) offset)
#define WRITE_HANDLER(name) 	void name(offs_t __attribute__ ((unused)) offset,data_t __attribute__ ((unused)) data)
#define OPBASE_HANDLER(name)	offs_t name(offs_t __attribute__ ((unused)) address)
#else
#define READ_HANDLER(name)		data_t name(offs_t offset)
#define WRITE_HANDLER(name) 	void name(offs_t offset,data_t data)
#define OPBASE_HANDLER(name)	offs_t name(offs_t address)
#endif



/***************************************************************************

Note that the memory hooks are not passed the actual memory address where
the operation takes place, but the offset from the beginning of the block
they are assigned to. This makes handling of mirror addresses easier, and
makes the handlers a bit more "object oriented". If you handler needs to
read/write the main memory area, provide a "base" pointer: it will be
initialized by the main engine to point to the beginning of the memory block
assigned to the handler. You may also provided a pointer to "size": it
will be set to the length of the memory area processed by the handler.

***************************************************************************/

struct MemoryReadAddress
{
	offs_t start, end;
	mem_read_handler handler;				/* see special values below */
};

#define MRA_NOP   0 						/* don't care, return 0 */
#define MRA_RAM   ((mem_read_handler)-1)	/* plain RAM location (return its contents) */
#define MRA_ROM   ((mem_read_handler)-2)	/* plain ROM location (return its contents) */
#define MRA_BANK1 ((mem_read_handler)-10)	/* bank memory */
#define MRA_BANK2 ((mem_read_handler)-11)	/* bank memory */
#define MRA_BANK3 ((mem_read_handler)-12)	/* bank memory */
#define MRA_BANK4 ((mem_read_handler)-13)	/* bank memory */
#define MRA_BANK5 ((mem_read_handler)-14)	/* bank memory */
#define MRA_BANK6 ((mem_read_handler)-15)	/* bank memory */
#define MRA_BANK7 ((mem_read_handler)-16)	/* bank memory */
#define MRA_BANK8 ((mem_read_handler)-17)	/* bank memory */
#define MRA_BANK9 ((mem_read_handler)-18)	/* bank memory */
#define MRA_BANK10 ((mem_read_handler)-19)	/* bank memory */
#define MRA_BANK11 ((mem_read_handler)-20)	/* bank memory */
#define MRA_BANK12 ((mem_read_handler)-21)	/* bank memory */
#define MRA_BANK13 ((mem_read_handler)-22)	/* bank memory */
#define MRA_BANK14 ((mem_read_handler)-23)	/* bank memory */
#define MRA_BANK15 ((mem_read_handler)-24)	/* bank memory */
#define MRA_BANK16 ((mem_read_handler)-25)	/* bank memory */

struct MemoryWriteAddress
{
	offs_t start, end;
	mem_write_handler handler;				/* see special values below */
	unsigned char **base;					/* optional (see explanation above) */
	size_t *size;							/* optional (see explanation above) */
};

#define MWA_NOP 0							/* do nothing */
#define MWA_RAM ((mem_write_handler)-1) 	/* plain RAM location (store the value) */
#define MWA_ROM ((mem_write_handler)-2) 	/* plain ROM location (do nothing) */
/*
   If the CPU opcodes are encrypted, they are fetched from a different memory space.
   In such a case, if the program dynamically creates code in RAM and executes it,
   it won't work unless you use MWA_RAMROM to affect both memory spaces.
 */
#define MWA_RAMROM ((mem_write_handler)-3)
#define MWA_BANK1 ((mem_write_handler)-10)	/* bank memory */
#define MWA_BANK2 ((mem_write_handler)-11)	/* bank memory */
#define MWA_BANK3 ((mem_write_handler)-12)	/* bank memory */
#define MWA_BANK4 ((mem_write_handler)-13)	/* bank memory */
#define MWA_BANK5 ((mem_write_handler)-14)	/* bank memory */
#define MWA_BANK6 ((mem_write_handler)-15)	/* bank memory */
#define MWA_BANK7 ((mem_write_handler)-16)	/* bank memory */
#define MWA_BANK8 ((mem_write_handler)-17)	/* bank memory */
#define MWA_BANK9 ((mem_write_handler)-18)	/* bank memory */
#define MWA_BANK10 ((mem_write_handler)-19) /* bank memory */
#define MWA_BANK11 ((mem_write_handler)-20) /* bank memory */
#define MWA_BANK12 ((mem_write_handler)-21) /* bank memory */
#define MWA_BANK13 ((mem_write_handler)-22) /* bank memory */
#define MWA_BANK14 ((mem_write_handler)-23) /* bank memory */
#define MWA_BANK15 ((mem_write_handler)-24) /* bank memory */
#define MWA_BANK16 ((mem_write_handler)-25) /* bank memory */



/***************************************************************************

IN and OUT ports are handled like memory accesses, the hook template is the
same so you can interchange them. Of course there is no 'base' pointer for
IO ports.

***************************************************************************/

struct IOReadPort
{
	int start,end;
	mem_read_handler handler;				/* see special values below */
};

#define IORP_NOP 0	/* don't care, return 0 */


struct IOWritePort
{
	int start,end;
	mem_write_handler handler;				/* see special values below */
};

#define IOWP_NOP 0	/* do nothing */


/***************************************************************************

If a memory region contains areas that are outside of the ROM region for
an address space, the memory system will allocate an array of structures
to track the external areas.

***************************************************************************/

#define MAX_EXT_MEMORY 64

struct ExtMemory
{
	int start,end,region;
	unsigned char *data;
};

extern struct ExtMemory ext_memory[MAX_EXT_MEMORY];



/***************************************************************************

For a given number of address bits, we need to determine how many elements
there are in the first and second-order lookup tables. We also need to know
how many low-order bits to ignore. The ABITS* values represent these
constants for each address space type we support.

***************************************************************************/

/* memory element block size */
#define MH_SBITS		8			/* sub element bank size */
#define MH_PBITS		8			/* port current element size */
#define MH_ELEMAX		64			/* sub elements limit */
#define MH_HARDMAX		64			/* hardware functions limit */

/* 16 bits address */
#define ABITS1_16		12
#define ABITS2_16		4
#define ABITS_MIN_16	0			/* minimum memory block is 1 byte */
/* 16 bits address (little endian word access) */
#define ABITS1_16LEW	12
#define ABITS2_16LEW	3
#define ABITS_MIN_16LEW 1			/* minimum memory block is 2 bytes */
/* 16 bits address (big endian word access) */
#define ABITS1_16BEW	12
#define ABITS2_16BEW	3
#define ABITS_MIN_16BEW 1			/* minimum memory block is 2 bytes */
/* 20 bits address */
#define ABITS1_20		12
#define ABITS2_20		8
#define ABITS_MIN_20	0			/* minimum memory block is 1 byte */
/* 21 bits address */
#define ABITS1_21		13
#define ABITS2_21		8
#define ABITS_MIN_21	0			/* minimum memory block is 1 byte */
/* 24 bits address (word access - byte granularity) */
#define ABITS1_24		16
#define ABITS2_24		8
#define ABITS_MIN_24	0			/* minimum memory block is 1 byte */
/* 24 bits address (big endian - word access) */
#define ABITS1_24BEW	15
#define ABITS2_24BEW	8
#define ABITS_MIN_24BEW 1			/* minimum memory block is 2 bytes */
/* 26 bits address (little endian - dword access) */
#define ABITS1_26LEW	16
#define ABITS2_26LEW	8
#define ABITS_MIN_26LEW 2			/* minimum memory block is 4 bytes */
/* 29 bits address (dword access) */
#define ABITS1_29		19
#define ABITS2_29		8
#define ABITS_MIN_29	2			/* minimum memory block is 4 bytes */
/* 32 bits address (dword access) */
#define ABITS1_32		23
#define ABITS2_32		8
#define ABITS_MIN_32	1			/* minimum memory block is 2 bytes */
/* 32 bits address (little endian dword access) */
#define ABITS1_32LEW	23
#define ABITS2_32LEW	8
#define ABITS_MIN_32LEW 1			/* minimum memory block is 2 bytes */
/* mask bits */
#define MHMASK(abits)	 (0xffffffff >> (32 - abits))


/***************************************************************************

	Global variables

***************************************************************************/

typedef unsigned char MHELE;

extern MHELE ophw;						/* opcode handler */
extern MHELE *cur_mrhard;				/* current set of read handlers */
extern MHELE *cur_mwhard;				/* current set of write handlers */

extern unsigned char *OP_RAM;			/* opcode RAM base */
extern unsigned char *OP_ROM;			/* opcode ROM base */
extern unsigned char *cpu_bankbase[];	/* array of bank bases */



/***************************************************************************

	Macros

***************************************************************************/

/* ----- 16-bit memory accessing ----- */
#define READ_WORD(a)		  (*(UINT16 *)(a))
#define WRITE_WORD(a,d) 	  (*(UINT16 *)(a) = (d))
#define COMBINE_WORD(w,d)	  (((w) & ((d) >> 16)) | ((d) & 0xffff))
#define COMBINE_WORD_MEM(a,d) (WRITE_WORD((a), (READ_WORD(a) & ((d) >> 16)) | (d)))

/* ----- opcode reading ----- */
#define cpu_readop(A)		(OP_ROM[A])
#define cpu_readop16(A) 	READ_WORD(&OP_ROM[A])
#define cpu_readop_arg(A)   (OP_RAM[A])
#define cpu_readop_arg16(A) READ_WORD(&OP_RAM[A])

/* ----- bank switching for CPU cores ----- */
#define change_pc_generic(pc,abits2,abitsmin,shift,setop)	\
{															\
	if (cur_mrhard[(pc)>>(abits2+abitsmin+shift)] != ophw)	\
		setop(pc);											\
}
#define change_pc(pc)		change_pc_generic(pc, ABITS2_16, ABITS_MIN_16, 0, cpu_setOPbase16)
#define change_pc16(pc) 	change_pc_generic(pc, ABITS2_16, ABITS_MIN_16, 0, cpu_setOPbase16)
#define change_pc16bew(pc)	change_pc_generic(pc, ABITS2_16BEW, ABITS_MIN_16BEW, 0, cpu_setOPbase16bew)
#define change_pc16lew(pc)	change_pc_generic(pc, ABITS2_16LEW, ABITS_MIN_16LEW, 0, cpu_setOPbase16lew)
#define change_pc20(pc) 	change_pc_generic(pc, ABITS2_20, ABITS_MIN_20, 0, cpu_setOPbase20)
#define change_pc21(pc) 	change_pc_generic(pc, ABITS2_21, ABITS_MIN_21, 0, cpu_setOPbase21)
#define change_pc24(pc) 	change_pc_generic(pc, ABITS2_24, ABITS_MIN_24, 0, cpu_setOPbase24)
#define change_pc24bew(pc)	change_pc_generic(pc, ABITS2_24BEW, ABITS_MIN_24BEW, 0, cpu_setOPbase24bew)
#define change_pc26lew(pc)	change_pc_generic(pc, ABITS2_26LEW, ABITS_MIN_26LEW, 0, cpu_setOPbase26lew)
#define change_pc29(pc)     change_pc_generic(pc, ABITS2_29, ABITS_MIN_29, 3, cpu_setOPbase29)
#define change_pc32(pc) 	change_pc_generic(pc, ABITS2_32, ABITS_MIN_32, 0, cpu_setOPbase32)
#define change_pc32lew(pc)	change_pc_generic(pc, ABITS2_32LEW, ABITS_MIN_32LEW, 0, cpu_setOPbase32lew)

/* ----- for use OPbaseOverride driver, request override callback to next cpu_setOPbase ----- */
#define catch_nextBranch()	(ophw = 0xff)

/* -----  bank switching macro ----- */
#define cpu_setbank(bank, base) 						\
{														\
	if (bank >= 1 && bank <= MAX_BANKS) 				\
	{													\
		cpu_bankbase[bank] = (UINT8 *)(base);			\
		if (ophw == bank)								\
		{												\
			ophw = 0xff;								\
			cpu_setOPbase16(cpu_get_pc());				\
		}												\
	}													\
}


/***************************************************************************

	Function prototypes

***************************************************************************/

/* ----- memory setup function ----- */
int memory_init(void);
void memory_shutdown(void);
void memorycontextswap(int activecpu);

/* memory hardware element map */
/* value:					   */
#define HT_RAM	  0 	/* RAM direct		 */
#define HT_BANK1  1 	/* bank memory #1	 */
#define HT_BANK2  2 	/* bank memory #2	 */
#define HT_BANK3  3 	/* bank memory #3	 */
#define HT_BANK4  4 	/* bank memory #4	 */
#define HT_BANK5  5 	/* bank memory #5	 */
#define HT_BANK6  6 	/* bank memory #6	 */
#define HT_BANK7  7 	/* bank memory #7	 */
#define HT_BANK8  8 	/* bank memory #8	 */
#define HT_BANK9  9 	/* bank memory #9	 */
#define HT_BANK10 10	/* bank memory #10	 */
#define HT_BANK11 11	/* bank memory #11	 */
#define HT_BANK12 12	/* bank memory #12	 */
#define HT_BANK13 13	/* bank memory #13	 */
#define HT_BANK14 14	/* bank memory #14	 */
#define HT_BANK15 15	/* bank memory #15	 */
#define HT_BANK16 16	/* bank memory #16	 */
#define HT_NON	  17	/* non mapped memory */
#define HT_NOP	  18	/* NOP memory		 */
#define HT_RAMROM 19	/* RAM ROM memory	 */
#define HT_ROM	  20	/* ROM memory		 */

#define HT_USER   21	/* user functions	 */
/* [MH_HARDMAX]-0xff	  link to sub memory element  */
/*						  (value-MH_HARDMAX)<<MH_SBITS -> element bank */

#define HT_BANKMAX (HT_BANK1 + MAX_BANKS - 1)

#if LSB_FIRST
	#define BYTE_XOR_BE(a) ((a) ^ 1)
	#define BYTE_XOR_LE(a) (a)
#else
	#define BYTE_XOR_BE(a) (a)
	#define BYTE_XOR_LE(a) ((a) ^ 1)
#endif

/* stupid workarounds so that we can generate an address mask that works even for 32 bits */
#define ADDRESS_TOPBIT(abits)		(1UL << (ABITS1_##abits + ABITS2_##abits + ABITS_MIN_##abits - 1))
#define ADDRESS_MASK(abits) 		(ADDRESS_TOPBIT(abits) | (ADDRESS_TOPBIT(abits) - 1))

extern unsigned char *cpu_bankbase[HT_BANKMAX + 1];

/* ----- memory read functions ----- */

extern MHELE *cur_mrhard;
extern MHELE readhardware[MH_ELEMAX << MH_SBITS];	/* mem/port read  */
extern mem_read_handler memoryreadhandler[MH_HARDMAX];
extern int memoryreadoffset[MH_HARDMAX];

#ifdef MAME_MEMINLINE
INLINE READ_HANDLER(cpu_readmem16);
INLINE READ_HANDLER(cpu_readmem16bew);
INLINE READ_HANDLER(cpu_readmem16bew_word);
INLINE READ_HANDLER(cpu_readmem16lew);
INLINE READ_HANDLER(cpu_readmem16lew_word);
INLINE READ_HANDLER(cpu_readmem20);
INLINE READ_HANDLER(cpu_readmem21);
INLINE READ_HANDLER(cpu_readmem24);
INLINE READ_HANDLER(cpu_readmem24bew);
INLINE READ_HANDLER(cpu_readmem24bew_word);
INLINE READ_HANDLER(cpu_readmem24bew_dword);
INLINE READ_HANDLER(cpu_readmem26lew);
INLINE READ_HANDLER(cpu_readmem26lew_word);
INLINE READ_HANDLER(cpu_readmem26lew_dword);
INLINE READ_HANDLER(cpu_readmem29);
INLINE READ_HANDLER(cpu_readmem29_word);
INLINE READ_HANDLER(cpu_readmem29_dword);
INLINE READ_HANDLER(cpu_readmem32);
INLINE READ_HANDLER(cpu_readmem32_word);
INLINE READ_HANDLER(cpu_readmem32_dword);
INLINE READ_HANDLER(cpu_readmem32lew);
INLINE READ_HANDLER(cpu_readmem32lew_word);
INLINE READ_HANDLER(cpu_readmem32lew_dword);
#include "memory_read.h"
#else
READ_HANDLER(cpu_readmem16);
READ_HANDLER(cpu_readmem16bew);
READ_HANDLER(cpu_readmem16bew_word);
READ_HANDLER(cpu_readmem16lew);
READ_HANDLER(cpu_readmem16lew_word);
READ_HANDLER(cpu_readmem20);
READ_HANDLER(cpu_readmem21);
READ_HANDLER(cpu_readmem24);
READ_HANDLER(cpu_readmem24bew);
READ_HANDLER(cpu_readmem24bew_word);
READ_HANDLER(cpu_readmem24bew_dword);
READ_HANDLER(cpu_readmem26lew);
READ_HANDLER(cpu_readmem26lew_word);
READ_HANDLER(cpu_readmem26lew_dword);
READ_HANDLER(cpu_readmem29);
READ_HANDLER(cpu_readmem29_word);
READ_HANDLER(cpu_readmem29_dword);
READ_HANDLER(cpu_readmem32);
READ_HANDLER(cpu_readmem32_word);
READ_HANDLER(cpu_readmem32_dword);
READ_HANDLER(cpu_readmem32lew);
READ_HANDLER(cpu_readmem32lew_word);
READ_HANDLER(cpu_readmem32lew_dword);
#endif

/* ----- memory write functions ----- */

extern MHELE *cur_mwhard;
extern MHELE writehardware[MH_ELEMAX << MH_SBITS]; /* mem/port write */
extern mem_write_handler memorywritehandler[MH_HARDMAX];
extern int memorywriteoffset[MH_HARDMAX];

#ifdef MAME_MEMINLINE
INLINE WRITE_HANDLER(cpu_writemem16);
INLINE WRITE_HANDLER(cpu_writemem16bew);
INLINE WRITE_HANDLER(cpu_writemem16bew_word);
INLINE WRITE_HANDLER(cpu_writemem16lew);
INLINE WRITE_HANDLER(cpu_writemem16lew_word);
INLINE WRITE_HANDLER(cpu_writemem20);
INLINE WRITE_HANDLER(cpu_writemem21);
INLINE WRITE_HANDLER(cpu_writemem24);
INLINE WRITE_HANDLER(cpu_writemem24bew);
INLINE WRITE_HANDLER(cpu_writemem24bew_word);
INLINE WRITE_HANDLER(cpu_writemem24bew_dword);
INLINE WRITE_HANDLER(cpu_writemem26lew);
INLINE WRITE_HANDLER(cpu_writemem26lew_word);
INLINE WRITE_HANDLER(cpu_writemem26lew_dword);
INLINE WRITE_HANDLER(cpu_writemem29);
INLINE WRITE_HANDLER(cpu_writemem29_word);
INLINE WRITE_HANDLER(cpu_writemem29_dword);
INLINE WRITE_HANDLER(cpu_writemem32);
INLINE WRITE_HANDLER(cpu_writemem32_word);
INLINE WRITE_HANDLER(cpu_writemem32_dword);
INLINE WRITE_HANDLER(cpu_writemem32lew);
INLINE WRITE_HANDLER(cpu_writemem32lew_word);
INLINE WRITE_HANDLER(cpu_writemem32lew_dword);
#include "memory_write.h"
#else
WRITE_HANDLER(cpu_writemem16);
WRITE_HANDLER(cpu_writemem16bew);
WRITE_HANDLER(cpu_writemem16bew_word);
WRITE_HANDLER(cpu_writemem16lew);
WRITE_HANDLER(cpu_writemem16lew_word);
WRITE_HANDLER(cpu_writemem20);
WRITE_HANDLER(cpu_writemem21);
WRITE_HANDLER(cpu_writemem24);
WRITE_HANDLER(cpu_writemem24bew);
WRITE_HANDLER(cpu_writemem24bew_word);
WRITE_HANDLER(cpu_writemem24bew_dword);
WRITE_HANDLER(cpu_writemem26lew);
WRITE_HANDLER(cpu_writemem26lew_word);
WRITE_HANDLER(cpu_writemem26lew_dword);
WRITE_HANDLER(cpu_writemem29);
WRITE_HANDLER(cpu_writemem29_word);
WRITE_HANDLER(cpu_writemem29_dword);
WRITE_HANDLER(cpu_writemem32);
WRITE_HANDLER(cpu_writemem32_word);
WRITE_HANDLER(cpu_writemem32_dword);
WRITE_HANDLER(cpu_writemem32lew);
WRITE_HANDLER(cpu_writemem32lew_word);
WRITE_HANDLER(cpu_writemem32lew_dword);
#endif

/* ----- port I/O functions ----- */
int cpu_readport(int port);
void cpu_writeport(int port, int value);

/* ----- dynamic memory/port mapping ----- */
void *install_mem_read_handler(int cpu, int start, int end, mem_read_handler handler);
void *install_mem_write_handler(int cpu, int start, int end, mem_write_handler handler);
void *install_port_read_handler(int cpu, int start, int end, mem_read_handler handler);
void *install_port_write_handler(int cpu, int start, int end, mem_write_handler handler);

/* ----- dynamic bank handlers ----- */
void cpu_setbankhandler_r(int bank, mem_read_handler handler);
void cpu_setbankhandler_w(int bank, mem_write_handler handler);

/* ----- opcode base control ---- */
void cpu_setOPbase16(int pc);
void cpu_setOPbase16bew(int pc);
void cpu_setOPbase16lew(int pc);
void cpu_setOPbase20(int pc);
void cpu_setOPbase21(int pc);
void cpu_setOPbase24(int pc);
void cpu_setOPbase24bew(int pc);
void cpu_setOPbase26lew(int pc);
void cpu_setOPbase29(int pc);
void cpu_setOPbase32(int pc);
void cpu_setOPbase32lew(int pc);
void cpu_setOPbaseoverride(int cpu, opbase_handler function);

/* ----- harder-to-explain functions ---- */

/* use this to set the a different opcode base address when using a CPU with
   opcodes and data encrypted separately */
void memory_set_opcode_base(int cpu, unsigned char *base);

/* look up a chunk of memory and get its start/end addresses, and its base.
Pass in the cpu number and the offset. It will find the chunk containing
that offset and return the start and end addresses, along with a pointer to
the base of the memory.
This can be used (carefully!) by drivers that wish to access memory directly
without going through the readmem/writemem accessors (e.g., blitters). */
unsigned char *findmemorychunk(int cpu, int offset, int *chunkstart, int *chunkend);

#endif

