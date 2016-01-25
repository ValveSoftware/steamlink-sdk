/****************************************************************************************************
 *
 *
 *		arm.c
 *		Portable ARM emulator.
 *
 *
 ***************************************************************************************************/

#include "arm.h"

#undef INLINE
#define INLINE static

struct ARM {
	UINT32 queue[3];	/* instruction queue */
	UINT32 psw; 		/* status word */
	UINT32 reg[16]; 	/* user register set */
	UINT32 reg_firq[7]; /* swapped R8-R14 during FIRQ */
	UINT32 reg_irq[2];	/* swapped R13-R14 during IRQ */
	UINT32 reg_svc[2];	/* swapped R13-R14 during SVC */
	UINT32 ppc; 		/* previous PC */
};

static struct ARM arm;
int arm_ICount;

#define AMASK	0x03fffffcL

/****************************************************************************
 * Read a byte from given memory location
 ****************************************************************************/
INLINE UINT32 ARM_RDMEM(UINT32 addr)
{
	return cpu_readmem26lew(addr & AMASK);
}

INLINE UINT32 ARM_RDMEM_32(UINT32 addr)
{
	UINT32 data = cpu_readmem26lew_dword(addr & AMASK);
	/*logerror("ARM_RDMEM_32 (%08x) -> %08x\n", addr, data);*/
	return data;
}

/****************************************************************************
 * Write a byte to given memory location
 ****************************************************************************/
INLINE void ARM_WRMEM(UINT32 addr, UINT32 val)
{
	cpu_writemem26lew(addr & AMASK, val & 0xff);
}

INLINE void ARM_WRMEM_32(UINT32 addr, UINT32 val)
{
	cpu_writemem26lew_dword(addr & AMASK, val);
}

#define OP		arm.queue[0]
#define PSW 	arm.psw
#define PC		arm.reg[15]

#define N		0x80000000L
#define Z		0x40000000L
#define C		0x20000000L
#define V		0x10000000L
#define I		0x08000000L
#define F		0x04000000L
#define S01 	0x00000003L

/* mode flags */
#define USER	0x00000000L
#define FIRQ	0x00000001L
#define IRQ 	0x00000002L
#define SVC 	0x00000003L

/* coniditional execution: bits 31-28 */
#define C_EQ	 0
#define C_NE	 1
#define C_CS	 2	/* aka HS (higher or same) */
#define C_CC	 3	/* aka UL (unsigned lower) or LO (lower) */
#define C_MI	 4
#define C_PL	 5
#define C_VS	 6
#define C_VC	 7
#define C_HI	 8
#define C_LS	 9
#define C_GE	10
#define C_LT	11
#define C_GT	12
#define C_LE	13
#define C_AL	14
#define C_NV	15

/* registers */
#define RD	arm.reg[(OP>>12)&15]
#define RN	arm.reg[(OP>>16)&15]

/* store multiple registers */
#define STM(wm,pre,post)					\
{											\
	int i;									\
	for( i = 0; i < 16; i++ )				\
	{										\
		if( OP & (1<<i) )					\
		{									\
			ea=(ea + pre) & AMASK;			\
			wm(ea, arm.reg[i]); 			\
			ea=(ea + post) & AMASK; 		\
		}									\
	}										\
}

/* load multiple registers */
#define LDM(rm,pre,post)					\
{											\
	int i;									\
	for( i = 0; i < 15; i++ )				\
	{										\
		if( OP & (1<<i) )					\
		{									\
			ea=(ea + pre) & AMASK;			\
			arm.reg[i] = rm(ea);			\
			ea=(ea + post) & AMASK; 		\
		}									\
	}										\
	if( OP & 0x8000 )						\
	{										\
		ea = (ea + pre) & AMASK;			\
		PUT_PC(rm(ea),0);					\
		ea = (ea + post) & AMASK;			\
	}										\
}

/* register operand #2
 * constant shift/rotate count:
 *	sssss mm0 xxxx
 *	shift mod Rs
 * or
 * register shift/rotate count:
 *  rrrr0 mm1 xxxx
 *	reg   mod Rs
 */
INLINE UINT32 RS(void)
{
	UINT32 m = OP & 0x70;
	UINT32 s = (OP>>7) & 31;
	UINT32 r = OP & 15;
	UINT32 rs = arm.reg[r];

	if( r == 15 )
		rs |= PSW;

	/*logerror("RS(%08x) m: %d, s: %d, Rs(%d): %08x\n", OP, m, s, r, rs);*/
	switch( m )
	{
	case 0x00:
		/* LSL (aka ASL) #0 .. 31# */
		return rs << s;

	case 0x10:
		/* LSL (aka ASL) R0 .. R15 */
		s = arm.reg[s >> 1];
		return rs << s;

	case 0x20:
		/* LSR #1 .. #32 */
		if (s)
			return rs >> s;
		/* LSR #32 effectively returns 0 */
		return 0;

	case 0x30:
		/* LSR R0 .. R15 */
		s = arm.reg[s >> 1];
		return rs >> s;

	case 0x40:
		/* ASR #1 .. #32 */
		if (s)
			return (UINT32)((INT32)rs >> s);
		/* ASR #32 effectively returns 0 or 0xffffffff depending on bit 31 */
		return (rs & N) ? 0xffffffffL : 0;

	case 0x50:
		/* ASR R0 .. R15 */
		s = arm.reg[s >> 1];
		return (UINT32)((INT32)rs >> s);

	case 0x60:
		/* shift count == 0 is RRX */
		if (s == 0)
		{
			UINT32 c = rs & 1;
			UINT32 rc = ((PSW & C) << 3) | (rs >> 1);
			PSW = (PSW & ~C) | (c << 29);
			return rc;
		}
		/* ROR #1 .. #31 */
		return (rs << (31 - s)) | (rs >> s);

	default:
		/* ROR R0 .. R15 */
		s = arm.reg[s >> 1];
		return (rs << (31 - s)) | (rs >> s);
	}
}

/* constant shifting:
 * ARM uses a 12 bit value where the upper 4 bits define a
 * 'rotate right' amount for the lower 8 bits.
 * The 8 bit pattern is rotated right 0 two 30 times inside
 * the 32 bit data word.
 */
#define CS(bits,value)	((UINT32)value>>bits)|((UINT32)value<<(31-bits))

static UINT32 imm12[4096] =
{
	CS( 0,0x00),CS( 0,0x01),CS( 0,0x02),CS( 0,0x03),CS( 0,0x04),CS( 0,0x05),CS( 0,0x06),CS( 0,0x07),CS( 0,0x08),CS( 0,0x09),CS( 0,0x0a),CS( 0,0x0b),CS( 0,0x0c),CS( 0,0x0d),CS( 0,0x0e),CS( 0,0x0f),
	CS( 0,0x10),CS( 0,0x11),CS( 0,0x12),CS( 0,0x13),CS( 0,0x14),CS( 0,0x15),CS( 0,0x16),CS( 0,0x17),CS( 0,0x18),CS( 0,0x19),CS( 0,0x1a),CS( 0,0x1b),CS( 0,0x1c),CS( 0,0x1d),CS( 0,0x1e),CS( 0,0x1f),
	CS( 0,0x20),CS( 0,0x21),CS( 0,0x22),CS( 0,0x23),CS( 0,0x24),CS( 0,0x25),CS( 0,0x26),CS( 0,0x27),CS( 0,0x28),CS( 0,0x29),CS( 0,0x2a),CS( 0,0x2b),CS( 0,0x2c),CS( 0,0x2d),CS( 0,0x2e),CS( 0,0x2f),
	CS( 0,0x30),CS( 0,0x31),CS( 0,0x32),CS( 0,0x33),CS( 0,0x34),CS( 0,0x35),CS( 0,0x36),CS( 0,0x37),CS( 0,0x38),CS( 0,0x39),CS( 0,0x3a),CS( 0,0x3b),CS( 0,0x3c),CS( 0,0x3d),CS( 0,0x3e),CS( 0,0x3f),
	CS( 0,0x40),CS( 0,0x41),CS( 0,0x42),CS( 0,0x43),CS( 0,0x44),CS( 0,0x45),CS( 0,0x46),CS( 0,0x47),CS( 0,0x48),CS( 0,0x49),CS( 0,0x4a),CS( 0,0x4b),CS( 0,0x4c),CS( 0,0x4d),CS( 0,0x4e),CS( 0,0x4f),
	CS( 0,0x50),CS( 0,0x51),CS( 0,0x52),CS( 0,0x53),CS( 0,0x54),CS( 0,0x55),CS( 0,0x56),CS( 0,0x57),CS( 0,0x58),CS( 0,0x59),CS( 0,0x5a),CS( 0,0x5b),CS( 0,0x5c),CS( 0,0x5d),CS( 0,0x5e),CS( 0,0x5f),
	CS( 0,0x60),CS( 0,0x61),CS( 0,0x62),CS( 0,0x63),CS( 0,0x64),CS( 0,0x65),CS( 0,0x66),CS( 0,0x67),CS( 0,0x68),CS( 0,0x69),CS( 0,0x6a),CS( 0,0x6b),CS( 0,0x6c),CS( 0,0x6d),CS( 0,0x6e),CS( 0,0x6f),
	CS( 0,0x70),CS( 0,0x71),CS( 0,0x72),CS( 0,0x73),CS( 0,0x74),CS( 0,0x75),CS( 0,0x76),CS( 0,0x77),CS( 0,0x78),CS( 0,0x79),CS( 0,0x7a),CS( 0,0x7b),CS( 0,0x7c),CS( 0,0x7d),CS( 0,0x7e),CS( 0,0x7f),
	CS( 0,0x80),CS( 0,0x81),CS( 0,0x82),CS( 0,0x83),CS( 0,0x84),CS( 0,0x85),CS( 0,0x86),CS( 0,0x87),CS( 0,0x88),CS( 0,0x89),CS( 0,0x8a),CS( 0,0x8b),CS( 0,0x8c),CS( 0,0x8d),CS( 0,0x8e),CS( 0,0x8f),
	CS( 0,0x90),CS( 0,0x91),CS( 0,0x92),CS( 0,0x93),CS( 0,0x94),CS( 0,0x95),CS( 0,0x96),CS( 0,0x97),CS( 0,0x98),CS( 0,0x99),CS( 0,0x9a),CS( 0,0x9b),CS( 0,0x9c),CS( 0,0x9d),CS( 0,0x9e),CS( 0,0x9f),
	CS( 0,0xa0),CS( 0,0xa1),CS( 0,0xa2),CS( 0,0xa3),CS( 0,0xa4),CS( 0,0xa5),CS( 0,0xa6),CS( 0,0xa7),CS( 0,0xa8),CS( 0,0xa9),CS( 0,0xaa),CS( 0,0xab),CS( 0,0xac),CS( 0,0xad),CS( 0,0xae),CS( 0,0xaf),
	CS( 0,0xb0),CS( 0,0xb1),CS( 0,0xb2),CS( 0,0xb3),CS( 0,0xb4),CS( 0,0xb5),CS( 0,0xb6),CS( 0,0xb7),CS( 0,0xb8),CS( 0,0xb9),CS( 0,0xba),CS( 0,0xbb),CS( 0,0xbc),CS( 0,0xbd),CS( 0,0xbe),CS( 0,0xbf),
	CS( 0,0xc0),CS( 0,0xc1),CS( 0,0xc2),CS( 0,0xc3),CS( 0,0xc4),CS( 0,0xc5),CS( 0,0xc6),CS( 0,0xc7),CS( 0,0xc8),CS( 0,0xc9),CS( 0,0xca),CS( 0,0xcb),CS( 0,0xcc),CS( 0,0xcd),CS( 0,0xce),CS( 0,0xcf),
	CS( 0,0xd0),CS( 0,0xd1),CS( 0,0xd2),CS( 0,0xd3),CS( 0,0xd4),CS( 0,0xd5),CS( 0,0xd6),CS( 0,0xd7),CS( 0,0xd8),CS( 0,0xd9),CS( 0,0xda),CS( 0,0xdb),CS( 0,0xdc),CS( 0,0xdd),CS( 0,0xde),CS( 0,0xdf),
	CS( 0,0xe0),CS( 0,0xe1),CS( 0,0xe2),CS( 0,0xe3),CS( 0,0xe4),CS( 0,0xe5),CS( 0,0xe6),CS( 0,0xe7),CS( 0,0xe8),CS( 0,0xe9),CS( 0,0xea),CS( 0,0xeb),CS( 0,0xec),CS( 0,0xed),CS( 0,0xee),CS( 0,0xef),
	CS( 0,0xf0),CS( 0,0xf1),CS( 0,0xf2),CS( 0,0xf3),CS( 0,0xf4),CS( 0,0xf5),CS( 0,0xf6),CS( 0,0xf7),CS( 0,0xf8),CS( 0,0xf9),CS( 0,0xfa),CS( 0,0xfb),CS( 0,0xfc),CS( 0,0xfd),CS( 0,0xfe),CS( 0,0xff),

	CS( 2,0x00),CS( 2,0x01),CS( 2,0x02),CS( 2,0x03),CS( 2,0x04),CS( 2,0x05),CS( 2,0x06),CS( 2,0x07),CS( 2,0x08),CS( 2,0x09),CS( 2,0x0a),CS( 2,0x0b),CS( 2,0x0c),CS( 2,0x0d),CS( 2,0x0e),CS( 2,0x0f),
	CS( 2,0x10),CS( 2,0x11),CS( 2,0x12),CS( 2,0x13),CS( 2,0x14),CS( 2,0x15),CS( 2,0x16),CS( 2,0x17),CS( 2,0x18),CS( 2,0x19),CS( 2,0x1a),CS( 2,0x1b),CS( 2,0x1c),CS( 2,0x1d),CS( 2,0x1e),CS( 2,0x1f),
	CS( 2,0x20),CS( 2,0x21),CS( 2,0x22),CS( 2,0x23),CS( 2,0x24),CS( 2,0x25),CS( 2,0x26),CS( 2,0x27),CS( 2,0x28),CS( 2,0x29),CS( 2,0x2a),CS( 2,0x2b),CS( 2,0x2c),CS( 2,0x2d),CS( 2,0x2e),CS( 2,0x2f),
	CS( 2,0x30),CS( 2,0x31),CS( 2,0x32),CS( 2,0x33),CS( 2,0x34),CS( 2,0x35),CS( 2,0x36),CS( 2,0x37),CS( 2,0x38),CS( 2,0x39),CS( 2,0x3a),CS( 2,0x3b),CS( 2,0x3c),CS( 2,0x3d),CS( 2,0x3e),CS( 2,0x3f),
	CS( 2,0x40),CS( 2,0x41),CS( 2,0x42),CS( 2,0x43),CS( 2,0x44),CS( 2,0x45),CS( 2,0x46),CS( 2,0x47),CS( 2,0x48),CS( 2,0x49),CS( 2,0x4a),CS( 2,0x4b),CS( 2,0x4c),CS( 2,0x4d),CS( 2,0x4e),CS( 2,0x4f),
	CS( 2,0x50),CS( 2,0x51),CS( 2,0x52),CS( 2,0x53),CS( 2,0x54),CS( 2,0x55),CS( 2,0x56),CS( 2,0x57),CS( 2,0x58),CS( 2,0x59),CS( 2,0x5a),CS( 2,0x5b),CS( 2,0x5c),CS( 2,0x5d),CS( 2,0x5e),CS( 2,0x5f),
	CS( 2,0x60),CS( 2,0x61),CS( 2,0x62),CS( 2,0x63),CS( 2,0x64),CS( 2,0x65),CS( 2,0x66),CS( 2,0x67),CS( 2,0x68),CS( 2,0x69),CS( 2,0x6a),CS( 2,0x6b),CS( 2,0x6c),CS( 2,0x6d),CS( 2,0x6e),CS( 2,0x6f),
	CS( 2,0x70),CS( 2,0x71),CS( 2,0x72),CS( 2,0x73),CS( 2,0x74),CS( 2,0x75),CS( 2,0x76),CS( 2,0x77),CS( 2,0x78),CS( 2,0x79),CS( 2,0x7a),CS( 2,0x7b),CS( 2,0x7c),CS( 2,0x7d),CS( 2,0x7e),CS( 2,0x7f),
	CS( 2,0x80),CS( 2,0x81),CS( 2,0x82),CS( 2,0x83),CS( 2,0x84),CS( 2,0x85),CS( 2,0x86),CS( 2,0x87),CS( 2,0x88),CS( 2,0x89),CS( 2,0x8a),CS( 2,0x8b),CS( 2,0x8c),CS( 2,0x8d),CS( 2,0x8e),CS( 2,0x8f),
	CS( 2,0x90),CS( 2,0x91),CS( 2,0x92),CS( 2,0x93),CS( 2,0x94),CS( 2,0x95),CS( 2,0x96),CS( 2,0x97),CS( 2,0x98),CS( 2,0x99),CS( 2,0x9a),CS( 2,0x9b),CS( 2,0x9c),CS( 2,0x9d),CS( 2,0x9e),CS( 2,0x9f),
	CS( 2,0xa0),CS( 2,0xa1),CS( 2,0xa2),CS( 2,0xa3),CS( 2,0xa4),CS( 2,0xa5),CS( 2,0xa6),CS( 2,0xa7),CS( 2,0xa8),CS( 2,0xa9),CS( 2,0xaa),CS( 2,0xab),CS( 2,0xac),CS( 2,0xad),CS( 2,0xae),CS( 2,0xaf),
	CS( 2,0xb0),CS( 2,0xb1),CS( 2,0xb2),CS( 2,0xb3),CS( 2,0xb4),CS( 2,0xb5),CS( 2,0xb6),CS( 2,0xb7),CS( 2,0xb8),CS( 2,0xb9),CS( 2,0xba),CS( 2,0xbb),CS( 2,0xbc),CS( 2,0xbd),CS( 2,0xbe),CS( 2,0xbf),
	CS( 2,0xc0),CS( 2,0xc1),CS( 2,0xc2),CS( 2,0xc3),CS( 2,0xc4),CS( 2,0xc5),CS( 2,0xc6),CS( 2,0xc7),CS( 2,0xc8),CS( 2,0xc9),CS( 2,0xca),CS( 2,0xcb),CS( 2,0xcc),CS( 2,0xcd),CS( 2,0xce),CS( 2,0xcf),
	CS( 2,0xd0),CS( 2,0xd1),CS( 2,0xd2),CS( 2,0xd3),CS( 2,0xd4),CS( 2,0xd5),CS( 2,0xd6),CS( 2,0xd7),CS( 2,0xd8),CS( 2,0xd9),CS( 2,0xda),CS( 2,0xdb),CS( 2,0xdc),CS( 2,0xdd),CS( 2,0xde),CS( 2,0xdf),
	CS( 2,0xe0),CS( 2,0xe1),CS( 2,0xe2),CS( 2,0xe3),CS( 2,0xe4),CS( 2,0xe5),CS( 2,0xe6),CS( 2,0xe7),CS( 2,0xe8),CS( 2,0xe9),CS( 2,0xea),CS( 2,0xeb),CS( 2,0xec),CS( 2,0xed),CS( 2,0xee),CS( 2,0xef),
	CS( 2,0xf0),CS( 2,0xf1),CS( 2,0xf2),CS( 2,0xf3),CS( 2,0xf4),CS( 2,0xf5),CS( 2,0xf6),CS( 2,0xf7),CS( 2,0xf8),CS( 2,0xf9),CS( 2,0xfa),CS( 2,0xfb),CS( 2,0xfc),CS( 2,0xfd),CS( 2,0xfe),CS( 2,0xff),

	CS( 4,0x00),CS( 4,0x01),CS( 4,0x02),CS( 4,0x03),CS( 4,0x04),CS( 4,0x05),CS( 4,0x06),CS( 4,0x07),CS( 4,0x08),CS( 4,0x09),CS( 4,0x0a),CS( 4,0x0b),CS( 4,0x0c),CS( 4,0x0d),CS( 4,0x0e),CS( 4,0x0f),
	CS( 4,0x10),CS( 4,0x11),CS( 4,0x12),CS( 4,0x13),CS( 4,0x14),CS( 4,0x15),CS( 4,0x16),CS( 4,0x17),CS( 4,0x18),CS( 4,0x19),CS( 4,0x1a),CS( 4,0x1b),CS( 4,0x1c),CS( 4,0x1d),CS( 4,0x1e),CS( 4,0x1f),
	CS( 4,0x20),CS( 4,0x21),CS( 4,0x22),CS( 4,0x23),CS( 4,0x24),CS( 4,0x25),CS( 4,0x26),CS( 4,0x27),CS( 4,0x28),CS( 4,0x29),CS( 4,0x2a),CS( 4,0x2b),CS( 4,0x2c),CS( 4,0x2d),CS( 4,0x2e),CS( 4,0x2f),
	CS( 4,0x30),CS( 4,0x31),CS( 4,0x32),CS( 4,0x33),CS( 4,0x34),CS( 4,0x35),CS( 4,0x36),CS( 4,0x37),CS( 4,0x38),CS( 4,0x39),CS( 4,0x3a),CS( 4,0x3b),CS( 4,0x3c),CS( 4,0x3d),CS( 4,0x3e),CS( 4,0x3f),
	CS( 4,0x40),CS( 4,0x41),CS( 4,0x42),CS( 4,0x43),CS( 4,0x44),CS( 4,0x45),CS( 4,0x46),CS( 4,0x47),CS( 4,0x48),CS( 4,0x49),CS( 4,0x4a),CS( 4,0x4b),CS( 4,0x4c),CS( 4,0x4d),CS( 4,0x4e),CS( 4,0x4f),
	CS( 4,0x50),CS( 4,0x51),CS( 4,0x52),CS( 4,0x53),CS( 4,0x54),CS( 4,0x55),CS( 4,0x56),CS( 4,0x57),CS( 4,0x58),CS( 4,0x59),CS( 4,0x5a),CS( 4,0x5b),CS( 4,0x5c),CS( 4,0x5d),CS( 4,0x5e),CS( 4,0x5f),
	CS( 4,0x60),CS( 4,0x61),CS( 4,0x62),CS( 4,0x63),CS( 4,0x64),CS( 4,0x65),CS( 4,0x66),CS( 4,0x67),CS( 4,0x68),CS( 4,0x69),CS( 4,0x6a),CS( 4,0x6b),CS( 4,0x6c),CS( 4,0x6d),CS( 4,0x6e),CS( 4,0x6f),
	CS( 4,0x70),CS( 4,0x71),CS( 4,0x72),CS( 4,0x73),CS( 4,0x74),CS( 4,0x75),CS( 4,0x76),CS( 4,0x77),CS( 4,0x78),CS( 4,0x79),CS( 4,0x7a),CS( 4,0x7b),CS( 4,0x7c),CS( 4,0x7d),CS( 4,0x7e),CS( 4,0x7f),
	CS( 4,0x80),CS( 4,0x81),CS( 4,0x82),CS( 4,0x83),CS( 4,0x84),CS( 4,0x85),CS( 4,0x86),CS( 4,0x87),CS( 4,0x88),CS( 4,0x89),CS( 4,0x8a),CS( 4,0x8b),CS( 4,0x8c),CS( 4,0x8d),CS( 4,0x8e),CS( 4,0x8f),
	CS( 4,0x90),CS( 4,0x91),CS( 4,0x92),CS( 4,0x93),CS( 4,0x94),CS( 4,0x95),CS( 4,0x96),CS( 4,0x97),CS( 4,0x98),CS( 4,0x99),CS( 4,0x9a),CS( 4,0x9b),CS( 4,0x9c),CS( 4,0x9d),CS( 4,0x9e),CS( 4,0x9f),
	CS( 4,0xa0),CS( 4,0xa1),CS( 4,0xa2),CS( 4,0xa3),CS( 4,0xa4),CS( 4,0xa5),CS( 4,0xa6),CS( 4,0xa7),CS( 4,0xa8),CS( 4,0xa9),CS( 4,0xaa),CS( 4,0xab),CS( 4,0xac),CS( 4,0xad),CS( 4,0xae),CS( 4,0xaf),
	CS( 4,0xb0),CS( 4,0xb1),CS( 4,0xb2),CS( 4,0xb3),CS( 4,0xb4),CS( 4,0xb5),CS( 4,0xb6),CS( 4,0xb7),CS( 4,0xb8),CS( 4,0xb9),CS( 4,0xba),CS( 4,0xbb),CS( 4,0xbc),CS( 4,0xbd),CS( 4,0xbe),CS( 4,0xbf),
	CS( 4,0xc0),CS( 4,0xc1),CS( 4,0xc2),CS( 4,0xc3),CS( 4,0xc4),CS( 4,0xc5),CS( 4,0xc6),CS( 4,0xc7),CS( 4,0xc8),CS( 4,0xc9),CS( 4,0xca),CS( 4,0xcb),CS( 4,0xcc),CS( 4,0xcd),CS( 4,0xce),CS( 4,0xcf),
	CS( 4,0xd0),CS( 4,0xd1),CS( 4,0xd2),CS( 4,0xd3),CS( 4,0xd4),CS( 4,0xd5),CS( 4,0xd6),CS( 4,0xd7),CS( 4,0xd8),CS( 4,0xd9),CS( 4,0xda),CS( 4,0xdb),CS( 4,0xdc),CS( 4,0xdd),CS( 4,0xde),CS( 4,0xdf),
	CS( 4,0xe0),CS( 4,0xe1),CS( 4,0xe2),CS( 4,0xe3),CS( 4,0xe4),CS( 4,0xe5),CS( 4,0xe6),CS( 4,0xe7),CS( 4,0xe8),CS( 4,0xe9),CS( 4,0xea),CS( 4,0xeb),CS( 4,0xec),CS( 4,0xed),CS( 4,0xee),CS( 4,0xef),
	CS( 4,0xf0),CS( 4,0xf1),CS( 4,0xf2),CS( 4,0xf3),CS( 4,0xf4),CS( 4,0xf5),CS( 4,0xf6),CS( 4,0xf7),CS( 4,0xf8),CS( 4,0xf9),CS( 4,0xfa),CS( 4,0xfb),CS( 4,0xfc),CS( 4,0xfd),CS( 4,0xfe),CS( 4,0xff),

	CS( 6,0x00),CS( 6,0x01),CS( 6,0x02),CS( 6,0x03),CS( 6,0x04),CS( 6,0x05),CS( 6,0x06),CS( 6,0x07),CS( 6,0x08),CS( 6,0x09),CS( 6,0x0a),CS( 6,0x0b),CS( 6,0x0c),CS( 6,0x0d),CS( 6,0x0e),CS( 6,0x0f),
	CS( 6,0x10),CS( 6,0x11),CS( 6,0x12),CS( 6,0x13),CS( 6,0x14),CS( 6,0x15),CS( 6,0x16),CS( 6,0x17),CS( 6,0x18),CS( 6,0x19),CS( 6,0x1a),CS( 6,0x1b),CS( 6,0x1c),CS( 6,0x1d),CS( 6,0x1e),CS( 6,0x1f),
	CS( 6,0x20),CS( 6,0x21),CS( 6,0x22),CS( 6,0x23),CS( 6,0x24),CS( 6,0x25),CS( 6,0x26),CS( 6,0x27),CS( 6,0x28),CS( 6,0x29),CS( 6,0x2a),CS( 6,0x2b),CS( 6,0x2c),CS( 6,0x2d),CS( 6,0x2e),CS( 6,0x2f),
	CS( 6,0x30),CS( 6,0x31),CS( 6,0x32),CS( 6,0x33),CS( 6,0x34),CS( 6,0x35),CS( 6,0x36),CS( 6,0x37),CS( 6,0x38),CS( 6,0x39),CS( 6,0x3a),CS( 6,0x3b),CS( 6,0x3c),CS( 6,0x3d),CS( 6,0x3e),CS( 6,0x3f),
	CS( 6,0x40),CS( 6,0x41),CS( 6,0x42),CS( 6,0x43),CS( 6,0x44),CS( 6,0x45),CS( 6,0x46),CS( 6,0x47),CS( 6,0x48),CS( 6,0x49),CS( 6,0x4a),CS( 6,0x4b),CS( 6,0x4c),CS( 6,0x4d),CS( 6,0x4e),CS( 6,0x4f),
	CS( 6,0x50),CS( 6,0x51),CS( 6,0x52),CS( 6,0x53),CS( 6,0x54),CS( 6,0x55),CS( 6,0x56),CS( 6,0x57),CS( 6,0x58),CS( 6,0x59),CS( 6,0x5a),CS( 6,0x5b),CS( 6,0x5c),CS( 6,0x5d),CS( 6,0x5e),CS( 6,0x5f),
	CS( 6,0x60),CS( 6,0x61),CS( 6,0x62),CS( 6,0x63),CS( 6,0x64),CS( 6,0x65),CS( 6,0x66),CS( 6,0x67),CS( 6,0x68),CS( 6,0x69),CS( 6,0x6a),CS( 6,0x6b),CS( 6,0x6c),CS( 6,0x6d),CS( 6,0x6e),CS( 6,0x6f),
	CS( 6,0x70),CS( 6,0x71),CS( 6,0x72),CS( 6,0x73),CS( 6,0x74),CS( 6,0x75),CS( 6,0x76),CS( 6,0x77),CS( 6,0x78),CS( 6,0x79),CS( 6,0x7a),CS( 6,0x7b),CS( 6,0x7c),CS( 6,0x7d),CS( 6,0x7e),CS( 6,0x7f),
	CS( 6,0x80),CS( 6,0x81),CS( 6,0x82),CS( 6,0x83),CS( 6,0x84),CS( 6,0x85),CS( 6,0x86),CS( 6,0x87),CS( 6,0x88),CS( 6,0x89),CS( 6,0x8a),CS( 6,0x8b),CS( 6,0x8c),CS( 6,0x8d),CS( 6,0x8e),CS( 6,0x8f),
	CS( 6,0x90),CS( 6,0x91),CS( 6,0x92),CS( 6,0x93),CS( 6,0x94),CS( 6,0x95),CS( 6,0x96),CS( 6,0x97),CS( 6,0x98),CS( 6,0x99),CS( 6,0x9a),CS( 6,0x9b),CS( 6,0x9c),CS( 6,0x9d),CS( 6,0x9e),CS( 6,0x9f),
	CS( 6,0xa0),CS( 6,0xa1),CS( 6,0xa2),CS( 6,0xa3),CS( 6,0xa4),CS( 6,0xa5),CS( 6,0xa6),CS( 6,0xa7),CS( 6,0xa8),CS( 6,0xa9),CS( 6,0xaa),CS( 6,0xab),CS( 6,0xac),CS( 6,0xad),CS( 6,0xae),CS( 6,0xaf),
	CS( 6,0xb0),CS( 6,0xb1),CS( 6,0xb2),CS( 6,0xb3),CS( 6,0xb4),CS( 6,0xb5),CS( 6,0xb6),CS( 6,0xb7),CS( 6,0xb8),CS( 6,0xb9),CS( 6,0xba),CS( 6,0xbb),CS( 6,0xbc),CS( 6,0xbd),CS( 6,0xbe),CS( 6,0xbf),
	CS( 6,0xc0),CS( 6,0xc1),CS( 6,0xc2),CS( 6,0xc3),CS( 6,0xc4),CS( 6,0xc5),CS( 6,0xc6),CS( 6,0xc7),CS( 6,0xc8),CS( 6,0xc9),CS( 6,0xca),CS( 6,0xcb),CS( 6,0xcc),CS( 6,0xcd),CS( 6,0xce),CS( 6,0xcf),
	CS( 6,0xd0),CS( 6,0xd1),CS( 6,0xd2),CS( 6,0xd3),CS( 6,0xd4),CS( 6,0xd5),CS( 6,0xd6),CS( 6,0xd7),CS( 6,0xd8),CS( 6,0xd9),CS( 6,0xda),CS( 6,0xdb),CS( 6,0xdc),CS( 6,0xdd),CS( 6,0xde),CS( 6,0xdf),
	CS( 6,0xe0),CS( 6,0xe1),CS( 6,0xe2),CS( 6,0xe3),CS( 6,0xe4),CS( 6,0xe5),CS( 6,0xe6),CS( 6,0xe7),CS( 6,0xe8),CS( 6,0xe9),CS( 6,0xea),CS( 6,0xeb),CS( 6,0xec),CS( 6,0xed),CS( 6,0xee),CS( 6,0xef),
	CS( 6,0xf0),CS( 6,0xf1),CS( 6,0xf2),CS( 6,0xf3),CS( 6,0xf4),CS( 6,0xf5),CS( 6,0xf6),CS( 6,0xf7),CS( 6,0xf8),CS( 6,0xf9),CS( 6,0xfa),CS( 6,0xfb),CS( 6,0xfc),CS( 6,0xfd),CS( 6,0xfe),CS( 6,0xff),

	CS( 8,0x00),CS( 8,0x01),CS( 8,0x02),CS( 8,0x03),CS( 8,0x04),CS( 8,0x05),CS( 8,0x06),CS( 8,0x07),CS( 8,0x08),CS( 8,0x09),CS( 8,0x0a),CS( 8,0x0b),CS( 8,0x0c),CS( 8,0x0d),CS( 8,0x0e),CS( 8,0x0f),
	CS( 8,0x10),CS( 8,0x11),CS( 8,0x12),CS( 8,0x13),CS( 8,0x14),CS( 8,0x15),CS( 8,0x16),CS( 8,0x17),CS( 8,0x18),CS( 8,0x19),CS( 8,0x1a),CS( 8,0x1b),CS( 8,0x1c),CS( 8,0x1d),CS( 8,0x1e),CS( 8,0x1f),
	CS( 8,0x20),CS( 8,0x21),CS( 8,0x22),CS( 8,0x23),CS( 8,0x24),CS( 8,0x25),CS( 8,0x26),CS( 8,0x27),CS( 8,0x28),CS( 8,0x29),CS( 8,0x2a),CS( 8,0x2b),CS( 8,0x2c),CS( 8,0x2d),CS( 8,0x2e),CS( 8,0x2f),
	CS( 8,0x30),CS( 8,0x31),CS( 8,0x32),CS( 8,0x33),CS( 8,0x34),CS( 8,0x35),CS( 8,0x36),CS( 8,0x37),CS( 8,0x38),CS( 8,0x39),CS( 8,0x3a),CS( 8,0x3b),CS( 8,0x3c),CS( 8,0x3d),CS( 8,0x3e),CS( 8,0x3f),
	CS( 8,0x40),CS( 8,0x41),CS( 8,0x42),CS( 8,0x43),CS( 8,0x44),CS( 8,0x45),CS( 8,0x46),CS( 8,0x47),CS( 8,0x48),CS( 8,0x49),CS( 8,0x4a),CS( 8,0x4b),CS( 8,0x4c),CS( 8,0x4d),CS( 8,0x4e),CS( 8,0x4f),
	CS( 8,0x50),CS( 8,0x51),CS( 8,0x52),CS( 8,0x53),CS( 8,0x54),CS( 8,0x55),CS( 8,0x56),CS( 8,0x57),CS( 8,0x58),CS( 8,0x59),CS( 8,0x5a),CS( 8,0x5b),CS( 8,0x5c),CS( 8,0x5d),CS( 8,0x5e),CS( 8,0x5f),
	CS( 8,0x60),CS( 8,0x61),CS( 8,0x62),CS( 8,0x63),CS( 8,0x64),CS( 8,0x65),CS( 8,0x66),CS( 8,0x67),CS( 8,0x68),CS( 8,0x69),CS( 8,0x6a),CS( 8,0x6b),CS( 8,0x6c),CS( 8,0x6d),CS( 8,0x6e),CS( 8,0x6f),
	CS( 8,0x70),CS( 8,0x71),CS( 8,0x72),CS( 8,0x73),CS( 8,0x74),CS( 8,0x75),CS( 8,0x76),CS( 8,0x77),CS( 8,0x78),CS( 8,0x79),CS( 8,0x7a),CS( 8,0x7b),CS( 8,0x7c),CS( 8,0x7d),CS( 8,0x7e),CS( 8,0x7f),
	CS( 8,0x80),CS( 8,0x81),CS( 8,0x82),CS( 8,0x83),CS( 8,0x84),CS( 8,0x85),CS( 8,0x86),CS( 8,0x87),CS( 8,0x88),CS( 8,0x89),CS( 8,0x8a),CS( 8,0x8b),CS( 8,0x8c),CS( 8,0x8d),CS( 8,0x8e),CS( 8,0x8f),
	CS( 8,0x90),CS( 8,0x91),CS( 8,0x92),CS( 8,0x93),CS( 8,0x94),CS( 8,0x95),CS( 8,0x96),CS( 8,0x97),CS( 8,0x98),CS( 8,0x99),CS( 8,0x9a),CS( 8,0x9b),CS( 8,0x9c),CS( 8,0x9d),CS( 8,0x9e),CS( 8,0x9f),
	CS( 8,0xa0),CS( 8,0xa1),CS( 8,0xa2),CS( 8,0xa3),CS( 8,0xa4),CS( 8,0xa5),CS( 8,0xa6),CS( 8,0xa7),CS( 8,0xa8),CS( 8,0xa9),CS( 8,0xaa),CS( 8,0xab),CS( 8,0xac),CS( 8,0xad),CS( 8,0xae),CS( 8,0xaf),
	CS( 8,0xb0),CS( 8,0xb1),CS( 8,0xb2),CS( 8,0xb3),CS( 8,0xb4),CS( 8,0xb5),CS( 8,0xb6),CS( 8,0xb7),CS( 8,0xb8),CS( 8,0xb9),CS( 8,0xba),CS( 8,0xbb),CS( 8,0xbc),CS( 8,0xbd),CS( 8,0xbe),CS( 8,0xbf),
	CS( 8,0xc0),CS( 8,0xc1),CS( 8,0xc2),CS( 8,0xc3),CS( 8,0xc4),CS( 8,0xc5),CS( 8,0xc6),CS( 8,0xc7),CS( 8,0xc8),CS( 8,0xc9),CS( 8,0xca),CS( 8,0xcb),CS( 8,0xcc),CS( 8,0xcd),CS( 8,0xce),CS( 8,0xcf),
	CS( 8,0xd0),CS( 8,0xd1),CS( 8,0xd2),CS( 8,0xd3),CS( 8,0xd4),CS( 8,0xd5),CS( 8,0xd6),CS( 8,0xd7),CS( 8,0xd8),CS( 8,0xd9),CS( 8,0xda),CS( 8,0xdb),CS( 8,0xdc),CS( 8,0xdd),CS( 8,0xde),CS( 8,0xdf),
	CS( 8,0xe0),CS( 8,0xe1),CS( 8,0xe2),CS( 8,0xe3),CS( 8,0xe4),CS( 8,0xe5),CS( 8,0xe6),CS( 8,0xe7),CS( 8,0xe8),CS( 8,0xe9),CS( 8,0xea),CS( 8,0xeb),CS( 8,0xec),CS( 8,0xed),CS( 8,0xee),CS( 8,0xef),
	CS( 8,0xf0),CS( 8,0xf1),CS( 8,0xf2),CS( 8,0xf3),CS( 8,0xf4),CS( 8,0xf5),CS( 8,0xf6),CS( 8,0xf7),CS( 8,0xf8),CS( 8,0xf9),CS( 8,0xfa),CS( 8,0xfb),CS( 8,0xfc),CS( 8,0xfd),CS( 8,0xfe),CS( 8,0xff),

	CS(10,0x00),CS(10,0x01),CS(10,0x02),CS(10,0x03),CS(10,0x04),CS(10,0x05),CS(10,0x06),CS(10,0x07),CS(10,0x08),CS(10,0x09),CS(10,0x0a),CS(10,0x0b),CS(10,0x0c),CS(10,0x0d),CS(10,0x0e),CS(10,0x0f),
	CS(10,0x10),CS(10,0x11),CS(10,0x12),CS(10,0x13),CS(10,0x14),CS(10,0x15),CS(10,0x16),CS(10,0x17),CS(10,0x18),CS(10,0x19),CS(10,0x1a),CS(10,0x1b),CS(10,0x1c),CS(10,0x1d),CS(10,0x1e),CS(10,0x1f),
	CS(10,0x20),CS(10,0x21),CS(10,0x22),CS(10,0x23),CS(10,0x24),CS(10,0x25),CS(10,0x26),CS(10,0x27),CS(10,0x28),CS(10,0x29),CS(10,0x2a),CS(10,0x2b),CS(10,0x2c),CS(10,0x2d),CS(10,0x2e),CS(10,0x2f),
	CS(10,0x30),CS(10,0x31),CS(10,0x32),CS(10,0x33),CS(10,0x34),CS(10,0x35),CS(10,0x36),CS(10,0x37),CS(10,0x38),CS(10,0x39),CS(10,0x3a),CS(10,0x3b),CS(10,0x3c),CS(10,0x3d),CS(10,0x3e),CS(10,0x3f),
	CS(10,0x40),CS(10,0x41),CS(10,0x42),CS(10,0x43),CS(10,0x44),CS(10,0x45),CS(10,0x46),CS(10,0x47),CS(10,0x48),CS(10,0x49),CS(10,0x4a),CS(10,0x4b),CS(10,0x4c),CS(10,0x4d),CS(10,0x4e),CS(10,0x4f),
	CS(10,0x50),CS(10,0x51),CS(10,0x52),CS(10,0x53),CS(10,0x54),CS(10,0x55),CS(10,0x56),CS(10,0x57),CS(10,0x58),CS(10,0x59),CS(10,0x5a),CS(10,0x5b),CS(10,0x5c),CS(10,0x5d),CS(10,0x5e),CS(10,0x5f),
	CS(10,0x60),CS(10,0x61),CS(10,0x62),CS(10,0x63),CS(10,0x64),CS(10,0x65),CS(10,0x66),CS(10,0x67),CS(10,0x68),CS(10,0x69),CS(10,0x6a),CS(10,0x6b),CS(10,0x6c),CS(10,0x6d),CS(10,0x6e),CS(10,0x6f),
	CS(10,0x70),CS(10,0x71),CS(10,0x72),CS(10,0x73),CS(10,0x74),CS(10,0x75),CS(10,0x76),CS(10,0x77),CS(10,0x78),CS(10,0x79),CS(10,0x7a),CS(10,0x7b),CS(10,0x7c),CS(10,0x7d),CS(10,0x7e),CS(10,0x7f),
	CS(10,0x80),CS(10,0x81),CS(10,0x82),CS(10,0x83),CS(10,0x84),CS(10,0x85),CS(10,0x86),CS(10,0x87),CS(10,0x88),CS(10,0x89),CS(10,0x8a),CS(10,0x8b),CS(10,0x8c),CS(10,0x8d),CS(10,0x8e),CS(10,0x8f),
	CS(10,0x90),CS(10,0x91),CS(10,0x92),CS(10,0x93),CS(10,0x94),CS(10,0x95),CS(10,0x96),CS(10,0x97),CS(10,0x98),CS(10,0x99),CS(10,0x9a),CS(10,0x9b),CS(10,0x9c),CS(10,0x9d),CS(10,0x9e),CS(10,0x9f),
	CS(10,0xa0),CS(10,0xa1),CS(10,0xa2),CS(10,0xa3),CS(10,0xa4),CS(10,0xa5),CS(10,0xa6),CS(10,0xa7),CS(10,0xa8),CS(10,0xa9),CS(10,0xaa),CS(10,0xab),CS(10,0xac),CS(10,0xad),CS(10,0xae),CS(10,0xaf),
	CS(10,0xb0),CS(10,0xb1),CS(10,0xb2),CS(10,0xb3),CS(10,0xb4),CS(10,0xb5),CS(10,0xb6),CS(10,0xb7),CS(10,0xb8),CS(10,0xb9),CS(10,0xba),CS(10,0xbb),CS(10,0xbc),CS(10,0xbd),CS(10,0xbe),CS(10,0xbf),
	CS(10,0xc0),CS(10,0xc1),CS(10,0xc2),CS(10,0xc3),CS(10,0xc4),CS(10,0xc5),CS(10,0xc6),CS(10,0xc7),CS(10,0xc8),CS(10,0xc9),CS(10,0xca),CS(10,0xcb),CS(10,0xcc),CS(10,0xcd),CS(10,0xce),CS(10,0xcf),
	CS(10,0xd0),CS(10,0xd1),CS(10,0xd2),CS(10,0xd3),CS(10,0xd4),CS(10,0xd5),CS(10,0xd6),CS(10,0xd7),CS(10,0xd8),CS(10,0xd9),CS(10,0xda),CS(10,0xdb),CS(10,0xdc),CS(10,0xdd),CS(10,0xde),CS(10,0xdf),
	CS(10,0xe0),CS(10,0xe1),CS(10,0xe2),CS(10,0xe3),CS(10,0xe4),CS(10,0xe5),CS(10,0xe6),CS(10,0xe7),CS(10,0xe8),CS(10,0xe9),CS(10,0xea),CS(10,0xeb),CS(10,0xec),CS(10,0xed),CS(10,0xee),CS(10,0xef),
	CS(10,0xf0),CS(10,0xf1),CS(10,0xf2),CS(10,0xf3),CS(10,0xf4),CS(10,0xf5),CS(10,0xf6),CS(10,0xf7),CS(10,0xf8),CS(10,0xf9),CS(10,0xfa),CS(10,0xfb),CS(10,0xfc),CS(10,0xfd),CS(10,0xfe),CS(10,0xff),

	CS(12,0x00),CS(12,0x01),CS(12,0x02),CS(12,0x03),CS(12,0x04),CS(12,0x05),CS(12,0x06),CS(12,0x07),CS(12,0x08),CS(12,0x09),CS(12,0x0a),CS(12,0x0b),CS(12,0x0c),CS(12,0x0d),CS(12,0x0e),CS(12,0x0f),
	CS(12,0x10),CS(12,0x11),CS(12,0x12),CS(12,0x13),CS(12,0x14),CS(12,0x15),CS(12,0x16),CS(12,0x17),CS(12,0x18),CS(12,0x19),CS(12,0x1a),CS(12,0x1b),CS(12,0x1c),CS(12,0x1d),CS(12,0x1e),CS(12,0x1f),
	CS(12,0x20),CS(12,0x21),CS(12,0x22),CS(12,0x23),CS(12,0x24),CS(12,0x25),CS(12,0x26),CS(12,0x27),CS(12,0x28),CS(12,0x29),CS(12,0x2a),CS(12,0x2b),CS(12,0x2c),CS(12,0x2d),CS(12,0x2e),CS(12,0x2f),
	CS(12,0x30),CS(12,0x31),CS(12,0x32),CS(12,0x33),CS(12,0x34),CS(12,0x35),CS(12,0x36),CS(12,0x37),CS(12,0x38),CS(12,0x39),CS(12,0x3a),CS(12,0x3b),CS(12,0x3c),CS(12,0x3d),CS(12,0x3e),CS(12,0x3f),
	CS(12,0x40),CS(12,0x41),CS(12,0x42),CS(12,0x43),CS(12,0x44),CS(12,0x45),CS(12,0x46),CS(12,0x47),CS(12,0x48),CS(12,0x49),CS(12,0x4a),CS(12,0x4b),CS(12,0x4c),CS(12,0x4d),CS(12,0x4e),CS(12,0x4f),
	CS(12,0x50),CS(12,0x51),CS(12,0x52),CS(12,0x53),CS(12,0x54),CS(12,0x55),CS(12,0x56),CS(12,0x57),CS(12,0x58),CS(12,0x59),CS(12,0x5a),CS(12,0x5b),CS(12,0x5c),CS(12,0x5d),CS(12,0x5e),CS(12,0x5f),
	CS(12,0x60),CS(12,0x61),CS(12,0x62),CS(12,0x63),CS(12,0x64),CS(12,0x65),CS(12,0x66),CS(12,0x67),CS(12,0x68),CS(12,0x69),CS(12,0x6a),CS(12,0x6b),CS(12,0x6c),CS(12,0x6d),CS(12,0x6e),CS(12,0x6f),
	CS(12,0x70),CS(12,0x71),CS(12,0x72),CS(12,0x73),CS(12,0x74),CS(12,0x75),CS(12,0x76),CS(12,0x77),CS(12,0x78),CS(12,0x79),CS(12,0x7a),CS(12,0x7b),CS(12,0x7c),CS(12,0x7d),CS(12,0x7e),CS(12,0x7f),
	CS(12,0x80),CS(12,0x81),CS(12,0x82),CS(12,0x83),CS(12,0x84),CS(12,0x85),CS(12,0x86),CS(12,0x87),CS(12,0x88),CS(12,0x89),CS(12,0x8a),CS(12,0x8b),CS(12,0x8c),CS(12,0x8d),CS(12,0x8e),CS(12,0x8f),
	CS(12,0x90),CS(12,0x91),CS(12,0x92),CS(12,0x93),CS(12,0x94),CS(12,0x95),CS(12,0x96),CS(12,0x97),CS(12,0x98),CS(12,0x99),CS(12,0x9a),CS(12,0x9b),CS(12,0x9c),CS(12,0x9d),CS(12,0x9e),CS(12,0x9f),
	CS(12,0xa0),CS(12,0xa1),CS(12,0xa2),CS(12,0xa3),CS(12,0xa4),CS(12,0xa5),CS(12,0xa6),CS(12,0xa7),CS(12,0xa8),CS(12,0xa9),CS(12,0xaa),CS(12,0xab),CS(12,0xac),CS(12,0xad),CS(12,0xae),CS(12,0xaf),
	CS(12,0xb0),CS(12,0xb1),CS(12,0xb2),CS(12,0xb3),CS(12,0xb4),CS(12,0xb5),CS(12,0xb6),CS(12,0xb7),CS(12,0xb8),CS(12,0xb9),CS(12,0xba),CS(12,0xbb),CS(12,0xbc),CS(12,0xbd),CS(12,0xbe),CS(12,0xbf),
	CS(12,0xc0),CS(12,0xc1),CS(12,0xc2),CS(12,0xc3),CS(12,0xc4),CS(12,0xc5),CS(12,0xc6),CS(12,0xc7),CS(12,0xc8),CS(12,0xc9),CS(12,0xca),CS(12,0xcb),CS(12,0xcc),CS(12,0xcd),CS(12,0xce),CS(12,0xcf),
	CS(12,0xd0),CS(12,0xd1),CS(12,0xd2),CS(12,0xd3),CS(12,0xd4),CS(12,0xd5),CS(12,0xd6),CS(12,0xd7),CS(12,0xd8),CS(12,0xd9),CS(12,0xda),CS(12,0xdb),CS(12,0xdc),CS(12,0xdd),CS(12,0xde),CS(12,0xdf),
	CS(12,0xe0),CS(12,0xe1),CS(12,0xe2),CS(12,0xe3),CS(12,0xe4),CS(12,0xe5),CS(12,0xe6),CS(12,0xe7),CS(12,0xe8),CS(12,0xe9),CS(12,0xea),CS(12,0xeb),CS(12,0xec),CS(12,0xed),CS(12,0xee),CS(12,0xef),
	CS(12,0xf0),CS(12,0xf1),CS(12,0xf2),CS(12,0xf3),CS(12,0xf4),CS(12,0xf5),CS(12,0xf6),CS(12,0xf7),CS(12,0xf8),CS(12,0xf9),CS(12,0xfa),CS(12,0xfb),CS(12,0xfc),CS(12,0xfd),CS(12,0xfe),CS(12,0xff),

	CS(14,0x00),CS(14,0x01),CS(14,0x02),CS(14,0x03),CS(14,0x04),CS(14,0x05),CS(14,0x06),CS(14,0x07),CS(14,0x08),CS(14,0x09),CS(14,0x0a),CS(14,0x0b),CS(14,0x0c),CS(14,0x0d),CS(14,0x0e),CS(14,0x0f),
	CS(14,0x10),CS(14,0x11),CS(14,0x12),CS(14,0x13),CS(14,0x14),CS(14,0x15),CS(14,0x16),CS(14,0x17),CS(14,0x18),CS(14,0x19),CS(14,0x1a),CS(14,0x1b),CS(14,0x1c),CS(14,0x1d),CS(14,0x1e),CS(14,0x1f),
	CS(14,0x20),CS(14,0x21),CS(14,0x22),CS(14,0x23),CS(14,0x24),CS(14,0x25),CS(14,0x26),CS(14,0x27),CS(14,0x28),CS(14,0x29),CS(14,0x2a),CS(14,0x2b),CS(14,0x2c),CS(14,0x2d),CS(14,0x2e),CS(14,0x2f),
	CS(14,0x30),CS(14,0x31),CS(14,0x32),CS(14,0x33),CS(14,0x34),CS(14,0x35),CS(14,0x36),CS(14,0x37),CS(14,0x38),CS(14,0x39),CS(14,0x3a),CS(14,0x3b),CS(14,0x3c),CS(14,0x3d),CS(14,0x3e),CS(14,0x3f),
	CS(14,0x40),CS(14,0x41),CS(14,0x42),CS(14,0x43),CS(14,0x44),CS(14,0x45),CS(14,0x46),CS(14,0x47),CS(14,0x48),CS(14,0x49),CS(14,0x4a),CS(14,0x4b),CS(14,0x4c),CS(14,0x4d),CS(14,0x4e),CS(14,0x4f),
	CS(14,0x50),CS(14,0x51),CS(14,0x52),CS(14,0x53),CS(14,0x54),CS(14,0x55),CS(14,0x56),CS(14,0x57),CS(14,0x58),CS(14,0x59),CS(14,0x5a),CS(14,0x5b),CS(14,0x5c),CS(14,0x5d),CS(14,0x5e),CS(14,0x5f),
	CS(14,0x60),CS(14,0x61),CS(14,0x62),CS(14,0x63),CS(14,0x64),CS(14,0x65),CS(14,0x66),CS(14,0x67),CS(14,0x68),CS(14,0x69),CS(14,0x6a),CS(14,0x6b),CS(14,0x6c),CS(14,0x6d),CS(14,0x6e),CS(14,0x6f),
	CS(14,0x70),CS(14,0x71),CS(14,0x72),CS(14,0x73),CS(14,0x74),CS(14,0x75),CS(14,0x76),CS(14,0x77),CS(14,0x78),CS(14,0x79),CS(14,0x7a),CS(14,0x7b),CS(14,0x7c),CS(14,0x7d),CS(14,0x7e),CS(14,0x7f),
	CS(14,0x80),CS(14,0x81),CS(14,0x82),CS(14,0x83),CS(14,0x84),CS(14,0x85),CS(14,0x86),CS(14,0x87),CS(14,0x88),CS(14,0x89),CS(14,0x8a),CS(14,0x8b),CS(14,0x8c),CS(14,0x8d),CS(14,0x8e),CS(14,0x8f),
	CS(14,0x90),CS(14,0x91),CS(14,0x92),CS(14,0x93),CS(14,0x94),CS(14,0x95),CS(14,0x96),CS(14,0x97),CS(14,0x98),CS(14,0x99),CS(14,0x9a),CS(14,0x9b),CS(14,0x9c),CS(14,0x9d),CS(14,0x9e),CS(14,0x9f),
	CS(14,0xa0),CS(14,0xa1),CS(14,0xa2),CS(14,0xa3),CS(14,0xa4),CS(14,0xa5),CS(14,0xa6),CS(14,0xa7),CS(14,0xa8),CS(14,0xa9),CS(14,0xaa),CS(14,0xab),CS(14,0xac),CS(14,0xad),CS(14,0xae),CS(14,0xaf),
	CS(14,0xb0),CS(14,0xb1),CS(14,0xb2),CS(14,0xb3),CS(14,0xb4),CS(14,0xb5),CS(14,0xb6),CS(14,0xb7),CS(14,0xb8),CS(14,0xb9),CS(14,0xba),CS(14,0xbb),CS(14,0xbc),CS(14,0xbd),CS(14,0xbe),CS(14,0xbf),
	CS(14,0xc0),CS(14,0xc1),CS(14,0xc2),CS(14,0xc3),CS(14,0xc4),CS(14,0xc5),CS(14,0xc6),CS(14,0xc7),CS(14,0xc8),CS(14,0xc9),CS(14,0xca),CS(14,0xcb),CS(14,0xcc),CS(14,0xcd),CS(14,0xce),CS(14,0xcf),
	CS(14,0xd0),CS(14,0xd1),CS(14,0xd2),CS(14,0xd3),CS(14,0xd4),CS(14,0xd5),CS(14,0xd6),CS(14,0xd7),CS(14,0xd8),CS(14,0xd9),CS(14,0xda),CS(14,0xdb),CS(14,0xdc),CS(14,0xdd),CS(14,0xde),CS(14,0xdf),
	CS(14,0xe0),CS(14,0xe1),CS(14,0xe2),CS(14,0xe3),CS(14,0xe4),CS(14,0xe5),CS(14,0xe6),CS(14,0xe7),CS(14,0xe8),CS(14,0xe9),CS(14,0xea),CS(14,0xeb),CS(14,0xec),CS(14,0xed),CS(14,0xee),CS(14,0xef),
	CS(14,0xf0),CS(14,0xf1),CS(14,0xf2),CS(14,0xf3),CS(14,0xf4),CS(14,0xf5),CS(14,0xf6),CS(14,0xf7),CS(14,0xf8),CS(14,0xf9),CS(14,0xfa),CS(14,0xfb),CS(14,0xfc),CS(14,0xfd),CS(14,0xfe),CS(14,0xff),

	CS(16,0x00),CS(16,0x01),CS(16,0x02),CS(16,0x03),CS(16,0x04),CS(16,0x05),CS(16,0x06),CS(16,0x07),CS(16,0x08),CS(16,0x09),CS(16,0x0a),CS(16,0x0b),CS(16,0x0c),CS(16,0x0d),CS(16,0x0e),CS(16,0x0f),
	CS(16,0x10),CS(16,0x11),CS(16,0x12),CS(16,0x13),CS(16,0x14),CS(16,0x15),CS(16,0x16),CS(16,0x17),CS(16,0x18),CS(16,0x19),CS(16,0x1a),CS(16,0x1b),CS(16,0x1c),CS(16,0x1d),CS(16,0x1e),CS(16,0x1f),
	CS(16,0x20),CS(16,0x21),CS(16,0x22),CS(16,0x23),CS(16,0x24),CS(16,0x25),CS(16,0x26),CS(16,0x27),CS(16,0x28),CS(16,0x29),CS(16,0x2a),CS(16,0x2b),CS(16,0x2c),CS(16,0x2d),CS(16,0x2e),CS(16,0x2f),
	CS(16,0x30),CS(16,0x31),CS(16,0x32),CS(16,0x33),CS(16,0x34),CS(16,0x35),CS(16,0x36),CS(16,0x37),CS(16,0x38),CS(16,0x39),CS(16,0x3a),CS(16,0x3b),CS(16,0x3c),CS(16,0x3d),CS(16,0x3e),CS(16,0x3f),
	CS(16,0x40),CS(16,0x41),CS(16,0x42),CS(16,0x43),CS(16,0x44),CS(16,0x45),CS(16,0x46),CS(16,0x47),CS(16,0x48),CS(16,0x49),CS(16,0x4a),CS(16,0x4b),CS(16,0x4c),CS(16,0x4d),CS(16,0x4e),CS(16,0x4f),
	CS(16,0x50),CS(16,0x51),CS(16,0x52),CS(16,0x53),CS(16,0x54),CS(16,0x55),CS(16,0x56),CS(16,0x57),CS(16,0x58),CS(16,0x59),CS(16,0x5a),CS(16,0x5b),CS(16,0x5c),CS(16,0x5d),CS(16,0x5e),CS(16,0x5f),
	CS(16,0x60),CS(16,0x61),CS(16,0x62),CS(16,0x63),CS(16,0x64),CS(16,0x65),CS(16,0x66),CS(16,0x67),CS(16,0x68),CS(16,0x69),CS(16,0x6a),CS(16,0x6b),CS(16,0x6c),CS(16,0x6d),CS(16,0x6e),CS(16,0x6f),
	CS(16,0x70),CS(16,0x71),CS(16,0x72),CS(16,0x73),CS(16,0x74),CS(16,0x75),CS(16,0x76),CS(16,0x77),CS(16,0x78),CS(16,0x79),CS(16,0x7a),CS(16,0x7b),CS(16,0x7c),CS(16,0x7d),CS(16,0x7e),CS(16,0x7f),
	CS(16,0x80),CS(16,0x81),CS(16,0x82),CS(16,0x83),CS(16,0x84),CS(16,0x85),CS(16,0x86),CS(16,0x87),CS(16,0x88),CS(16,0x89),CS(16,0x8a),CS(16,0x8b),CS(16,0x8c),CS(16,0x8d),CS(16,0x8e),CS(16,0x8f),
	CS(16,0x90),CS(16,0x91),CS(16,0x92),CS(16,0x93),CS(16,0x94),CS(16,0x95),CS(16,0x96),CS(16,0x97),CS(16,0x98),CS(16,0x99),CS(16,0x9a),CS(16,0x9b),CS(16,0x9c),CS(16,0x9d),CS(16,0x9e),CS(16,0x9f),
	CS(16,0xa0),CS(16,0xa1),CS(16,0xa2),CS(16,0xa3),CS(16,0xa4),CS(16,0xa5),CS(16,0xa6),CS(16,0xa7),CS(16,0xa8),CS(16,0xa9),CS(16,0xaa),CS(16,0xab),CS(16,0xac),CS(16,0xad),CS(16,0xae),CS(16,0xaf),
	CS(16,0xb0),CS(16,0xb1),CS(16,0xb2),CS(16,0xb3),CS(16,0xb4),CS(16,0xb5),CS(16,0xb6),CS(16,0xb7),CS(16,0xb8),CS(16,0xb9),CS(16,0xba),CS(16,0xbb),CS(16,0xbc),CS(16,0xbd),CS(16,0xbe),CS(16,0xbf),
	CS(16,0xc0),CS(16,0xc1),CS(16,0xc2),CS(16,0xc3),CS(16,0xc4),CS(16,0xc5),CS(16,0xc6),CS(16,0xc7),CS(16,0xc8),CS(16,0xc9),CS(16,0xca),CS(16,0xcb),CS(16,0xcc),CS(16,0xcd),CS(16,0xce),CS(16,0xcf),
	CS(16,0xd0),CS(16,0xd1),CS(16,0xd2),CS(16,0xd3),CS(16,0xd4),CS(16,0xd5),CS(16,0xd6),CS(16,0xd7),CS(16,0xd8),CS(16,0xd9),CS(16,0xda),CS(16,0xdb),CS(16,0xdc),CS(16,0xdd),CS(16,0xde),CS(16,0xdf),
	CS(16,0xe0),CS(16,0xe1),CS(16,0xe2),CS(16,0xe3),CS(16,0xe4),CS(16,0xe5),CS(16,0xe6),CS(16,0xe7),CS(16,0xe8),CS(16,0xe9),CS(16,0xea),CS(16,0xeb),CS(16,0xec),CS(16,0xed),CS(16,0xee),CS(16,0xef),
	CS(16,0xf0),CS(16,0xf1),CS(16,0xf2),CS(16,0xf3),CS(16,0xf4),CS(16,0xf5),CS(16,0xf6),CS(16,0xf7),CS(16,0xf8),CS(16,0xf9),CS(16,0xfa),CS(16,0xfb),CS(16,0xfc),CS(16,0xfd),CS(16,0xfe),CS(16,0xff),

	CS(18,0x00),CS(18,0x01),CS(18,0x02),CS(18,0x03),CS(18,0x04),CS(18,0x05),CS(18,0x06),CS(18,0x07),CS(18,0x08),CS(18,0x09),CS(18,0x0a),CS(18,0x0b),CS(18,0x0c),CS(18,0x0d),CS(18,0x0e),CS(18,0x0f),
	CS(18,0x10),CS(18,0x11),CS(18,0x12),CS(18,0x13),CS(18,0x14),CS(18,0x15),CS(18,0x16),CS(18,0x17),CS(18,0x18),CS(18,0x19),CS(18,0x1a),CS(18,0x1b),CS(18,0x1c),CS(18,0x1d),CS(18,0x1e),CS(18,0x1f),
	CS(18,0x20),CS(18,0x21),CS(18,0x22),CS(18,0x23),CS(18,0x24),CS(18,0x25),CS(18,0x26),CS(18,0x27),CS(18,0x28),CS(18,0x29),CS(18,0x2a),CS(18,0x2b),CS(18,0x2c),CS(18,0x2d),CS(18,0x2e),CS(18,0x2f),
	CS(18,0x30),CS(18,0x31),CS(18,0x32),CS(18,0x33),CS(18,0x34),CS(18,0x35),CS(18,0x36),CS(18,0x37),CS(18,0x38),CS(18,0x39),CS(18,0x3a),CS(18,0x3b),CS(18,0x3c),CS(18,0x3d),CS(18,0x3e),CS(18,0x3f),
	CS(18,0x40),CS(18,0x41),CS(18,0x42),CS(18,0x43),CS(18,0x44),CS(18,0x45),CS(18,0x46),CS(18,0x47),CS(18,0x48),CS(18,0x49),CS(18,0x4a),CS(18,0x4b),CS(18,0x4c),CS(18,0x4d),CS(18,0x4e),CS(18,0x4f),
	CS(18,0x50),CS(18,0x51),CS(18,0x52),CS(18,0x53),CS(18,0x54),CS(18,0x55),CS(18,0x56),CS(18,0x57),CS(18,0x58),CS(18,0x59),CS(18,0x5a),CS(18,0x5b),CS(18,0x5c),CS(18,0x5d),CS(18,0x5e),CS(18,0x5f),
	CS(18,0x60),CS(18,0x61),CS(18,0x62),CS(18,0x63),CS(18,0x64),CS(18,0x65),CS(18,0x66),CS(18,0x67),CS(18,0x68),CS(18,0x69),CS(18,0x6a),CS(18,0x6b),CS(18,0x6c),CS(18,0x6d),CS(18,0x6e),CS(18,0x6f),
	CS(18,0x70),CS(18,0x71),CS(18,0x72),CS(18,0x73),CS(18,0x74),CS(18,0x75),CS(18,0x76),CS(18,0x77),CS(18,0x78),CS(18,0x79),CS(18,0x7a),CS(18,0x7b),CS(18,0x7c),CS(18,0x7d),CS(18,0x7e),CS(18,0x7f),
	CS(18,0x80),CS(18,0x81),CS(18,0x82),CS(18,0x83),CS(18,0x84),CS(18,0x85),CS(18,0x86),CS(18,0x87),CS(18,0x88),CS(18,0x89),CS(18,0x8a),CS(18,0x8b),CS(18,0x8c),CS(18,0x8d),CS(18,0x8e),CS(18,0x8f),
	CS(18,0x90),CS(18,0x91),CS(18,0x92),CS(18,0x93),CS(18,0x94),CS(18,0x95),CS(18,0x96),CS(18,0x97),CS(18,0x98),CS(18,0x99),CS(18,0x9a),CS(18,0x9b),CS(18,0x9c),CS(18,0x9d),CS(18,0x9e),CS(18,0x9f),
	CS(18,0xa0),CS(18,0xa1),CS(18,0xa2),CS(18,0xa3),CS(18,0xa4),CS(18,0xa5),CS(18,0xa6),CS(18,0xa7),CS(18,0xa8),CS(18,0xa9),CS(18,0xaa),CS(18,0xab),CS(18,0xac),CS(18,0xad),CS(18,0xae),CS(18,0xaf),
	CS(18,0xb0),CS(18,0xb1),CS(18,0xb2),CS(18,0xb3),CS(18,0xb4),CS(18,0xb5),CS(18,0xb6),CS(18,0xb7),CS(18,0xb8),CS(18,0xb9),CS(18,0xba),CS(18,0xbb),CS(18,0xbc),CS(18,0xbd),CS(18,0xbe),CS(18,0xbf),
	CS(18,0xc0),CS(18,0xc1),CS(18,0xc2),CS(18,0xc3),CS(18,0xc4),CS(18,0xc5),CS(18,0xc6),CS(18,0xc7),CS(18,0xc8),CS(18,0xc9),CS(18,0xca),CS(18,0xcb),CS(18,0xcc),CS(18,0xcd),CS(18,0xce),CS(18,0xcf),
	CS(18,0xd0),CS(18,0xd1),CS(18,0xd2),CS(18,0xd3),CS(18,0xd4),CS(18,0xd5),CS(18,0xd6),CS(18,0xd7),CS(18,0xd8),CS(18,0xd9),CS(18,0xda),CS(18,0xdb),CS(18,0xdc),CS(18,0xdd),CS(18,0xde),CS(18,0xdf),
	CS(18,0xe0),CS(18,0xe1),CS(18,0xe2),CS(18,0xe3),CS(18,0xe4),CS(18,0xe5),CS(18,0xe6),CS(18,0xe7),CS(18,0xe8),CS(18,0xe9),CS(18,0xea),CS(18,0xeb),CS(18,0xec),CS(18,0xed),CS(18,0xee),CS(18,0xef),
	CS(18,0xf0),CS(18,0xf1),CS(18,0xf2),CS(18,0xf3),CS(18,0xf4),CS(18,0xf5),CS(18,0xf6),CS(18,0xf7),CS(18,0xf8),CS(18,0xf9),CS(18,0xfa),CS(18,0xfb),CS(18,0xfc),CS(18,0xfd),CS(18,0xfe),CS(18,0xff),

	CS(20,0x20),CS(20,0x21),CS(20,0x22),CS(20,0x23),CS(20,0x24),CS(20,0x25),CS(20,0x26),CS(20,0x27),CS(20,0x28),CS(20,0x29),CS(20,0x2a),CS(20,0x2b),CS(20,0x2c),CS(20,0x2d),CS(20,0x2e),CS(20,0x2f),
	CS(20,0x30),CS(20,0x31),CS(20,0x32),CS(20,0x33),CS(20,0x34),CS(20,0x35),CS(20,0x36),CS(20,0x37),CS(20,0x38),CS(20,0x39),CS(20,0x3a),CS(20,0x3b),CS(20,0x3c),CS(20,0x3d),CS(20,0x3e),CS(20,0x3f),
	CS(20,0x40),CS(20,0x41),CS(20,0x42),CS(20,0x43),CS(20,0x44),CS(20,0x45),CS(20,0x46),CS(20,0x47),CS(20,0x48),CS(20,0x49),CS(20,0x4a),CS(20,0x4b),CS(20,0x4c),CS(20,0x4d),CS(20,0x4e),CS(20,0x4f),
	CS(20,0x50),CS(20,0x51),CS(20,0x52),CS(20,0x53),CS(20,0x54),CS(20,0x55),CS(20,0x56),CS(20,0x57),CS(20,0x58),CS(20,0x59),CS(20,0x5a),CS(20,0x5b),CS(20,0x5c),CS(20,0x5d),CS(20,0x5e),CS(20,0x5f),
	CS(20,0x60),CS(20,0x61),CS(20,0x62),CS(20,0x63),CS(20,0x64),CS(20,0x65),CS(20,0x66),CS(20,0x67),CS(20,0x68),CS(20,0x69),CS(20,0x6a),CS(20,0x6b),CS(20,0x6c),CS(20,0x6d),CS(20,0x6e),CS(20,0x6f),
	CS(20,0x70),CS(20,0x71),CS(20,0x72),CS(20,0x73),CS(20,0x74),CS(20,0x75),CS(20,0x76),CS(20,0x77),CS(20,0x78),CS(20,0x79),CS(20,0x7a),CS(20,0x7b),CS(20,0x7c),CS(20,0x7d),CS(20,0x7e),CS(20,0x7f),
	CS(20,0x80),CS(20,0x81),CS(20,0x82),CS(20,0x83),CS(20,0x84),CS(20,0x85),CS(20,0x86),CS(20,0x87),CS(20,0x88),CS(20,0x89),CS(20,0x8a),CS(20,0x8b),CS(20,0x8c),CS(20,0x8d),CS(20,0x8e),CS(20,0x8f),
	CS(20,0x90),CS(20,0x91),CS(20,0x92),CS(20,0x93),CS(20,0x94),CS(20,0x95),CS(20,0x96),CS(20,0x97),CS(20,0x98),CS(20,0x99),CS(20,0x9a),CS(20,0x9b),CS(20,0x9c),CS(20,0x9d),CS(20,0x9e),CS(20,0x9f),
	CS(20,0xa0),CS(20,0xa1),CS(20,0xa2),CS(20,0xa3),CS(20,0xa4),CS(20,0xa5),CS(20,0xa6),CS(20,0xa7),CS(20,0xa8),CS(20,0xa9),CS(20,0xaa),CS(20,0xab),CS(20,0xac),CS(20,0xad),CS(20,0xae),CS(20,0xaf),
	CS(20,0xb0),CS(20,0xb1),CS(20,0xb2),CS(20,0xb3),CS(20,0xb4),CS(20,0xb5),CS(20,0xb6),CS(20,0xb7),CS(20,0xb8),CS(20,0xb9),CS(20,0xba),CS(20,0xbb),CS(20,0xbc),CS(20,0xbd),CS(20,0xbe),CS(20,0xbf),
	CS(20,0xc0),CS(20,0xc1),CS(20,0xc2),CS(20,0xc3),CS(20,0xc4),CS(20,0xc5),CS(20,0xc6),CS(20,0xc7),CS(20,0xc8),CS(20,0xc9),CS(20,0xca),CS(20,0xcb),CS(20,0xcc),CS(20,0xcd),CS(20,0xce),CS(20,0xcf),
	CS(20,0xd0),CS(20,0xd1),CS(20,0xd2),CS(20,0xd3),CS(20,0xd4),CS(20,0xd5),CS(20,0xd6),CS(20,0xd7),CS(20,0xd8),CS(20,0xd9),CS(20,0xda),CS(20,0xdb),CS(20,0xdc),CS(20,0xdd),CS(20,0xde),CS(20,0xdf),
	CS(20,0xe0),CS(20,0xe1),CS(20,0xe2),CS(20,0xe3),CS(20,0xe4),CS(20,0xe5),CS(20,0xe6),CS(20,0xe7),CS(20,0xe8),CS(20,0xe9),CS(20,0xea),CS(20,0xeb),CS(20,0xec),CS(20,0xed),CS(20,0xee),CS(20,0xef),
	CS(20,0xf0),CS(20,0xf1),CS(20,0xf2),CS(20,0xf3),CS(20,0xf4),CS(20,0xf5),CS(20,0xf6),CS(20,0xf7),CS(20,0xf8),CS(20,0xf9),CS(20,0xfa),CS(20,0xfb),CS(20,0xfc),CS(20,0xfd),CS(20,0xfe),CS(20,0xff),

	CS(22,0x00),CS(22,0x01),CS(22,0x02),CS(22,0x03),CS(22,0x04),CS(22,0x05),CS(22,0x06),CS(22,0x07),CS(22,0x08),CS(22,0x09),CS(22,0x0a),CS(22,0x0b),CS(22,0x0c),CS(22,0x0d),CS(22,0x0e),CS(22,0x0f),
	CS(22,0x10),CS(22,0x11),CS(22,0x12),CS(22,0x13),CS(22,0x14),CS(22,0x15),CS(22,0x16),CS(22,0x17),CS(22,0x18),CS(22,0x19),CS(22,0x1a),CS(22,0x1b),CS(22,0x1c),CS(22,0x1d),CS(22,0x1e),CS(22,0x1f),
	CS(22,0x20),CS(22,0x21),CS(22,0x22),CS(22,0x23),CS(22,0x24),CS(22,0x25),CS(22,0x26),CS(22,0x27),CS(22,0x28),CS(22,0x29),CS(22,0x2a),CS(22,0x2b),CS(22,0x2c),CS(22,0x2d),CS(22,0x2e),CS(22,0x2f),
	CS(22,0x30),CS(22,0x31),CS(22,0x32),CS(22,0x33),CS(22,0x34),CS(22,0x35),CS(22,0x36),CS(22,0x37),CS(22,0x38),CS(22,0x39),CS(22,0x3a),CS(22,0x3b),CS(22,0x3c),CS(22,0x3d),CS(22,0x3e),CS(22,0x3f),
	CS(22,0x40),CS(22,0x41),CS(22,0x42),CS(22,0x43),CS(22,0x44),CS(22,0x45),CS(22,0x46),CS(22,0x47),CS(22,0x48),CS(22,0x49),CS(22,0x4a),CS(22,0x4b),CS(22,0x4c),CS(22,0x4d),CS(22,0x4e),CS(22,0x4f),
	CS(22,0x50),CS(22,0x51),CS(22,0x52),CS(22,0x53),CS(22,0x54),CS(22,0x55),CS(22,0x56),CS(22,0x57),CS(22,0x58),CS(22,0x59),CS(22,0x5a),CS(22,0x5b),CS(22,0x5c),CS(22,0x5d),CS(22,0x5e),CS(22,0x5f),
	CS(22,0x60),CS(22,0x61),CS(22,0x62),CS(22,0x63),CS(22,0x64),CS(22,0x65),CS(22,0x66),CS(22,0x67),CS(22,0x68),CS(22,0x69),CS(22,0x6a),CS(22,0x6b),CS(22,0x6c),CS(22,0x6d),CS(22,0x6e),CS(22,0x6f),
	CS(22,0x70),CS(22,0x71),CS(22,0x72),CS(22,0x73),CS(22,0x74),CS(22,0x75),CS(22,0x76),CS(22,0x77),CS(22,0x78),CS(22,0x79),CS(22,0x7a),CS(22,0x7b),CS(22,0x7c),CS(22,0x7d),CS(22,0x7e),CS(22,0x7f),
	CS(22,0x80),CS(22,0x81),CS(22,0x82),CS(22,0x83),CS(22,0x84),CS(22,0x85),CS(22,0x86),CS(22,0x87),CS(22,0x88),CS(22,0x89),CS(22,0x8a),CS(22,0x8b),CS(22,0x8c),CS(22,0x8d),CS(22,0x8e),CS(22,0x8f),
	CS(22,0x90),CS(22,0x91),CS(22,0x92),CS(22,0x93),CS(22,0x94),CS(22,0x95),CS(22,0x96),CS(22,0x97),CS(22,0x98),CS(22,0x99),CS(22,0x9a),CS(22,0x9b),CS(22,0x9c),CS(22,0x9d),CS(22,0x9e),CS(22,0x9f),
	CS(22,0xa0),CS(22,0xa1),CS(22,0xa2),CS(22,0xa3),CS(22,0xa4),CS(22,0xa5),CS(22,0xa6),CS(22,0xa7),CS(22,0xa8),CS(22,0xa9),CS(22,0xaa),CS(22,0xab),CS(22,0xac),CS(22,0xad),CS(22,0xae),CS(22,0xaf),
	CS(22,0xb0),CS(22,0xb1),CS(22,0xb2),CS(22,0xb3),CS(22,0xb4),CS(22,0xb5),CS(22,0xb6),CS(22,0xb7),CS(22,0xb8),CS(22,0xb9),CS(22,0xba),CS(22,0xbb),CS(22,0xbc),CS(22,0xbd),CS(22,0xbe),CS(22,0xbf),
	CS(22,0xc0),CS(22,0xc1),CS(22,0xc2),CS(22,0xc3),CS(22,0xc4),CS(22,0xc5),CS(22,0xc6),CS(22,0xc7),CS(22,0xc8),CS(22,0xc9),CS(22,0xca),CS(22,0xcb),CS(22,0xcc),CS(22,0xcd),CS(22,0xce),CS(22,0xcf),
	CS(22,0xd0),CS(22,0xd1),CS(22,0xd2),CS(22,0xd3),CS(22,0xd4),CS(22,0xd5),CS(22,0xd6),CS(22,0xd7),CS(22,0xd8),CS(22,0xd9),CS(22,0xda),CS(22,0xdb),CS(22,0xdc),CS(22,0xdd),CS(22,0xde),CS(22,0xdf),
	CS(22,0xe0),CS(22,0xe1),CS(22,0xe2),CS(22,0xe3),CS(22,0xe4),CS(22,0xe5),CS(22,0xe6),CS(22,0xe7),CS(22,0xe8),CS(22,0xe9),CS(22,0xea),CS(22,0xeb),CS(22,0xec),CS(22,0xed),CS(22,0xee),CS(22,0xef),
	CS(22,0xf0),CS(22,0xf1),CS(22,0xf2),CS(22,0xf3),CS(22,0xf4),CS(22,0xf5),CS(22,0xf6),CS(22,0xf7),CS(22,0xf8),CS(22,0xf9),CS(22,0xfa),CS(22,0xfb),CS(22,0xfc),CS(22,0xfd),CS(22,0xfe),CS(22,0xff),

	CS(24,0x00),CS(24,0x01),CS(24,0x02),CS(24,0x03),CS(24,0x04),CS(24,0x05),CS(24,0x06),CS(24,0x07),CS(24,0x08),CS(24,0x09),CS(24,0x0a),CS(24,0x0b),CS(24,0x0c),CS(24,0x0d),CS(24,0x0e),CS(24,0x0f),
	CS(24,0x10),CS(24,0x11),CS(24,0x12),CS(24,0x13),CS(24,0x14),CS(24,0x15),CS(24,0x16),CS(24,0x17),CS(24,0x18),CS(24,0x19),CS(24,0x1a),CS(24,0x1b),CS(24,0x1c),CS(24,0x1d),CS(24,0x1e),CS(24,0x1f),
	CS(24,0x20),CS(24,0x21),CS(24,0x22),CS(24,0x23),CS(24,0x24),CS(24,0x25),CS(24,0x26),CS(24,0x27),CS(24,0x28),CS(24,0x29),CS(24,0x2a),CS(24,0x2b),CS(24,0x2c),CS(24,0x2d),CS(24,0x2e),CS(24,0x2f),
	CS(24,0x30),CS(24,0x31),CS(24,0x32),CS(24,0x33),CS(24,0x34),CS(24,0x35),CS(24,0x36),CS(24,0x37),CS(24,0x38),CS(24,0x39),CS(24,0x3a),CS(24,0x3b),CS(24,0x3c),CS(24,0x3d),CS(24,0x3e),CS(24,0x3f),
	CS(24,0x40),CS(24,0x41),CS(24,0x42),CS(24,0x43),CS(24,0x44),CS(24,0x45),CS(24,0x46),CS(24,0x47),CS(24,0x48),CS(24,0x49),CS(24,0x4a),CS(24,0x4b),CS(24,0x4c),CS(24,0x4d),CS(24,0x4e),CS(24,0x4f),
	CS(24,0x50),CS(24,0x51),CS(24,0x52),CS(24,0x53),CS(24,0x54),CS(24,0x55),CS(24,0x56),CS(24,0x57),CS(24,0x58),CS(24,0x59),CS(24,0x5a),CS(24,0x5b),CS(24,0x5c),CS(24,0x5d),CS(24,0x5e),CS(24,0x5f),
	CS(24,0x60),CS(24,0x61),CS(24,0x62),CS(24,0x63),CS(24,0x64),CS(24,0x65),CS(24,0x66),CS(24,0x67),CS(24,0x68),CS(24,0x69),CS(24,0x6a),CS(24,0x6b),CS(24,0x6c),CS(24,0x6d),CS(24,0x6e),CS(24,0x6f),
	CS(24,0x70),CS(24,0x71),CS(24,0x72),CS(24,0x73),CS(24,0x74),CS(24,0x75),CS(24,0x76),CS(24,0x77),CS(24,0x78),CS(24,0x79),CS(24,0x7a),CS(24,0x7b),CS(24,0x7c),CS(24,0x7d),CS(24,0x7e),CS(24,0x7f),
	CS(24,0x80),CS(24,0x81),CS(24,0x82),CS(24,0x83),CS(24,0x84),CS(24,0x85),CS(24,0x86),CS(24,0x87),CS(24,0x88),CS(24,0x89),CS(24,0x8a),CS(24,0x8b),CS(24,0x8c),CS(24,0x8d),CS(24,0x8e),CS(24,0x8f),
	CS(24,0x90),CS(24,0x91),CS(24,0x92),CS(24,0x93),CS(24,0x94),CS(24,0x95),CS(24,0x96),CS(24,0x97),CS(24,0x98),CS(24,0x99),CS(24,0x9a),CS(24,0x9b),CS(24,0x9c),CS(24,0x9d),CS(24,0x9e),CS(24,0x9f),
	CS(24,0xa0),CS(24,0xa1),CS(24,0xa2),CS(24,0xa3),CS(24,0xa4),CS(24,0xa5),CS(24,0xa6),CS(24,0xa7),CS(24,0xa8),CS(24,0xa9),CS(24,0xaa),CS(24,0xab),CS(24,0xac),CS(24,0xad),CS(24,0xae),CS(24,0xaf),
	CS(24,0xb0),CS(24,0xb1),CS(24,0xb2),CS(24,0xb3),CS(24,0xb4),CS(24,0xb5),CS(24,0xb6),CS(24,0xb7),CS(24,0xb8),CS(24,0xb9),CS(24,0xba),CS(24,0xbb),CS(24,0xbc),CS(24,0xbd),CS(24,0xbe),CS(24,0xbf),
	CS(24,0xc0),CS(24,0xc1),CS(24,0xc2),CS(24,0xc3),CS(24,0xc4),CS(24,0xc5),CS(24,0xc6),CS(24,0xc7),CS(24,0xc8),CS(24,0xc9),CS(24,0xca),CS(24,0xcb),CS(24,0xcc),CS(24,0xcd),CS(24,0xce),CS(24,0xcf),
	CS(24,0xd0),CS(24,0xd1),CS(24,0xd2),CS(24,0xd3),CS(24,0xd4),CS(24,0xd5),CS(24,0xd6),CS(24,0xd7),CS(24,0xd8),CS(24,0xd9),CS(24,0xda),CS(24,0xdb),CS(24,0xdc),CS(24,0xdd),CS(24,0xde),CS(24,0xdf),
	CS(24,0xe0),CS(24,0xe1),CS(24,0xe2),CS(24,0xe3),CS(24,0xe4),CS(24,0xe5),CS(24,0xe6),CS(24,0xe7),CS(24,0xe8),CS(24,0xe9),CS(24,0xea),CS(24,0xeb),CS(24,0xec),CS(24,0xed),CS(24,0xee),CS(24,0xef),
	CS(24,0xf0),CS(24,0xf1),CS(24,0xf2),CS(24,0xf3),CS(24,0xf4),CS(24,0xf5),CS(24,0xf6),CS(24,0xf7),CS(24,0xf8),CS(24,0xf9),CS(24,0xfa),CS(24,0xfb),CS(24,0xfc),CS(24,0xfd),CS(24,0xfe),CS(24,0xff),

	CS(26,0x00),CS(26,0x01),CS(26,0x02),CS(26,0x03),CS(26,0x04),CS(26,0x05),CS(26,0x06),CS(26,0x07),CS(26,0x08),CS(26,0x09),CS(26,0x0a),CS(26,0x0b),CS(26,0x0c),CS(26,0x0d),CS(26,0x0e),CS(26,0x0f),
	CS(26,0x10),CS(26,0x11),CS(26,0x12),CS(26,0x13),CS(26,0x14),CS(26,0x15),CS(26,0x16),CS(26,0x17),CS(26,0x18),CS(26,0x19),CS(26,0x1a),CS(26,0x1b),CS(26,0x1c),CS(26,0x1d),CS(26,0x1e),CS(26,0x1f),
	CS(26,0x20),CS(26,0x21),CS(26,0x22),CS(26,0x23),CS(26,0x24),CS(26,0x25),CS(26,0x26),CS(26,0x27),CS(26,0x28),CS(26,0x29),CS(26,0x2a),CS(26,0x2b),CS(26,0x2c),CS(26,0x2d),CS(26,0x2e),CS(26,0x2f),
	CS(26,0x30),CS(26,0x31),CS(26,0x32),CS(26,0x33),CS(26,0x34),CS(26,0x35),CS(26,0x36),CS(26,0x37),CS(26,0x38),CS(26,0x39),CS(26,0x3a),CS(26,0x3b),CS(26,0x3c),CS(26,0x3d),CS(26,0x3e),CS(26,0x3f),
	CS(26,0x40),CS(26,0x41),CS(26,0x42),CS(26,0x43),CS(26,0x44),CS(26,0x45),CS(26,0x46),CS(26,0x47),CS(26,0x48),CS(26,0x49),CS(26,0x4a),CS(26,0x4b),CS(26,0x4c),CS(26,0x4d),CS(26,0x4e),CS(26,0x4f),
	CS(26,0x50),CS(26,0x51),CS(26,0x52),CS(26,0x53),CS(26,0x54),CS(26,0x55),CS(26,0x56),CS(26,0x57),CS(26,0x58),CS(26,0x59),CS(26,0x5a),CS(26,0x5b),CS(26,0x5c),CS(26,0x5d),CS(26,0x5e),CS(26,0x5f),
	CS(26,0x60),CS(26,0x61),CS(26,0x62),CS(26,0x63),CS(26,0x64),CS(26,0x65),CS(26,0x66),CS(26,0x67),CS(26,0x68),CS(26,0x69),CS(26,0x6a),CS(26,0x6b),CS(26,0x6c),CS(26,0x6d),CS(26,0x6e),CS(26,0x6f),
	CS(26,0x70),CS(26,0x71),CS(26,0x72),CS(26,0x73),CS(26,0x74),CS(26,0x75),CS(26,0x76),CS(26,0x77),CS(26,0x78),CS(26,0x79),CS(26,0x7a),CS(26,0x7b),CS(26,0x7c),CS(26,0x7d),CS(26,0x7e),CS(26,0x7f),
	CS(26,0x80),CS(26,0x81),CS(26,0x82),CS(26,0x83),CS(26,0x84),CS(26,0x85),CS(26,0x86),CS(26,0x87),CS(26,0x88),CS(26,0x89),CS(26,0x8a),CS(26,0x8b),CS(26,0x8c),CS(26,0x8d),CS(26,0x8e),CS(26,0x8f),
	CS(26,0x90),CS(26,0x91),CS(26,0x92),CS(26,0x93),CS(26,0x94),CS(26,0x95),CS(26,0x96),CS(26,0x97),CS(26,0x98),CS(26,0x99),CS(26,0x9a),CS(26,0x9b),CS(26,0x9c),CS(26,0x9d),CS(26,0x9e),CS(26,0x9f),
	CS(26,0xa0),CS(26,0xa1),CS(26,0xa2),CS(26,0xa3),CS(26,0xa4),CS(26,0xa5),CS(26,0xa6),CS(26,0xa7),CS(26,0xa8),CS(26,0xa9),CS(26,0xaa),CS(26,0xab),CS(26,0xac),CS(26,0xad),CS(26,0xae),CS(26,0xaf),
	CS(26,0xb0),CS(26,0xb1),CS(26,0xb2),CS(26,0xb3),CS(26,0xb4),CS(26,0xb5),CS(26,0xb6),CS(26,0xb7),CS(26,0xb8),CS(26,0xb9),CS(26,0xba),CS(26,0xbb),CS(26,0xbc),CS(26,0xbd),CS(26,0xbe),CS(26,0xbf),
	CS(26,0xc0),CS(26,0xc1),CS(26,0xc2),CS(26,0xc3),CS(26,0xc4),CS(26,0xc5),CS(26,0xc6),CS(26,0xc7),CS(26,0xc8),CS(26,0xc9),CS(26,0xca),CS(26,0xcb),CS(26,0xcc),CS(26,0xcd),CS(26,0xce),CS(26,0xcf),
	CS(26,0xd0),CS(26,0xd1),CS(26,0xd2),CS(26,0xd3),CS(26,0xd4),CS(26,0xd5),CS(26,0xd6),CS(26,0xd7),CS(26,0xd8),CS(26,0xd9),CS(26,0xda),CS(26,0xdb),CS(26,0xdc),CS(26,0xdd),CS(26,0xde),CS(26,0xdf),
	CS(26,0xe0),CS(26,0xe1),CS(26,0xe2),CS(26,0xe3),CS(26,0xe4),CS(26,0xe5),CS(26,0xe6),CS(26,0xe7),CS(26,0xe8),CS(26,0xe9),CS(26,0xea),CS(26,0xeb),CS(26,0xec),CS(26,0xed),CS(26,0xee),CS(26,0xef),
	CS(26,0xf0),CS(26,0xf1),CS(26,0xf2),CS(26,0xf3),CS(26,0xf4),CS(26,0xf5),CS(26,0xf6),CS(26,0xf7),CS(26,0xf8),CS(26,0xf9),CS(26,0xfa),CS(26,0xfb),CS(26,0xfc),CS(26,0xfd),CS(26,0xfe),CS(26,0xff),

	CS(28,0x00),CS(28,0x01),CS(28,0x02),CS(28,0x03),CS(28,0x04),CS(28,0x05),CS(28,0x06),CS(28,0x07),CS(28,0x08),CS(28,0x09),CS(28,0x0a),CS(28,0x0b),CS(28,0x0c),CS(28,0x0d),CS(28,0x0e),CS(28,0x0f),
	CS(28,0x10),CS(28,0x11),CS(28,0x12),CS(28,0x13),CS(28,0x14),CS(28,0x15),CS(28,0x16),CS(28,0x17),CS(28,0x18),CS(28,0x19),CS(28,0x1a),CS(28,0x1b),CS(28,0x1c),CS(28,0x1d),CS(28,0x1e),CS(28,0x1f),
	CS(28,0x20),CS(28,0x21),CS(28,0x22),CS(28,0x23),CS(28,0x24),CS(28,0x25),CS(28,0x26),CS(28,0x27),CS(28,0x28),CS(28,0x29),CS(28,0x2a),CS(28,0x2b),CS(28,0x2c),CS(28,0x2d),CS(28,0x2e),CS(28,0x2f),
	CS(28,0x30),CS(28,0x31),CS(28,0x32),CS(28,0x33),CS(28,0x34),CS(28,0x35),CS(28,0x36),CS(28,0x37),CS(28,0x38),CS(28,0x39),CS(28,0x3a),CS(28,0x3b),CS(28,0x3c),CS(28,0x3d),CS(28,0x3e),CS(28,0x3f),
	CS(28,0x40),CS(28,0x41),CS(28,0x42),CS(28,0x43),CS(28,0x44),CS(28,0x45),CS(28,0x46),CS(28,0x47),CS(28,0x48),CS(28,0x49),CS(28,0x4a),CS(28,0x4b),CS(28,0x4c),CS(28,0x4d),CS(28,0x4e),CS(28,0x4f),
	CS(28,0x50),CS(28,0x51),CS(28,0x52),CS(28,0x53),CS(28,0x54),CS(28,0x55),CS(28,0x56),CS(28,0x57),CS(28,0x58),CS(28,0x59),CS(28,0x5a),CS(28,0x5b),CS(28,0x5c),CS(28,0x5d),CS(28,0x5e),CS(28,0x5f),
	CS(28,0x60),CS(28,0x61),CS(28,0x62),CS(28,0x63),CS(28,0x64),CS(28,0x65),CS(28,0x66),CS(28,0x67),CS(28,0x68),CS(28,0x69),CS(28,0x6a),CS(28,0x6b),CS(28,0x6c),CS(28,0x6d),CS(28,0x6e),CS(28,0x6f),
	CS(28,0x70),CS(28,0x71),CS(28,0x72),CS(28,0x73),CS(28,0x74),CS(28,0x75),CS(28,0x76),CS(28,0x77),CS(28,0x78),CS(28,0x79),CS(28,0x7a),CS(28,0x7b),CS(28,0x7c),CS(28,0x7d),CS(28,0x7e),CS(28,0x7f),
	CS(28,0x80),CS(28,0x81),CS(28,0x82),CS(28,0x83),CS(28,0x84),CS(28,0x85),CS(28,0x86),CS(28,0x87),CS(28,0x88),CS(28,0x89),CS(28,0x8a),CS(28,0x8b),CS(28,0x8c),CS(28,0x8d),CS(28,0x8e),CS(28,0x8f),
	CS(28,0x90),CS(28,0x91),CS(28,0x92),CS(28,0x93),CS(28,0x94),CS(28,0x95),CS(28,0x96),CS(28,0x97),CS(28,0x98),CS(28,0x99),CS(28,0x9a),CS(28,0x9b),CS(28,0x9c),CS(28,0x9d),CS(28,0x9e),CS(28,0x9f),
	CS(28,0xa0),CS(28,0xa1),CS(28,0xa2),CS(28,0xa3),CS(28,0xa4),CS(28,0xa5),CS(28,0xa6),CS(28,0xa7),CS(28,0xa8),CS(28,0xa9),CS(28,0xaa),CS(28,0xab),CS(28,0xac),CS(28,0xad),CS(28,0xae),CS(28,0xaf),
	CS(28,0xb0),CS(28,0xb1),CS(28,0xb2),CS(28,0xb3),CS(28,0xb4),CS(28,0xb5),CS(28,0xb6),CS(28,0xb7),CS(28,0xb8),CS(28,0xb9),CS(28,0xba),CS(28,0xbb),CS(28,0xbc),CS(28,0xbd),CS(28,0xbe),CS(28,0xbf),
	CS(28,0xc0),CS(28,0xc1),CS(28,0xc2),CS(28,0xc3),CS(28,0xc4),CS(28,0xc5),CS(28,0xc6),CS(28,0xc7),CS(28,0xc8),CS(28,0xc9),CS(28,0xca),CS(28,0xcb),CS(28,0xcc),CS(28,0xcd),CS(28,0xce),CS(28,0xcf),
	CS(28,0xd0),CS(28,0xd1),CS(28,0xd2),CS(28,0xd3),CS(28,0xd4),CS(28,0xd5),CS(28,0xd6),CS(28,0xd7),CS(28,0xd8),CS(28,0xd9),CS(28,0xda),CS(28,0xdb),CS(28,0xdc),CS(28,0xdd),CS(28,0xde),CS(28,0xdf),
	CS(28,0xe0),CS(28,0xe1),CS(28,0xe2),CS(28,0xe3),CS(28,0xe4),CS(28,0xe5),CS(28,0xe6),CS(28,0xe7),CS(28,0xe8),CS(28,0xe9),CS(28,0xea),CS(28,0xeb),CS(28,0xec),CS(28,0xed),CS(28,0xee),CS(28,0xef),
	CS(28,0xf0),CS(28,0xf1),CS(28,0xf2),CS(28,0xf3),CS(28,0xf4),CS(28,0xf5),CS(28,0xf6),CS(28,0xf7),CS(28,0xf8),CS(28,0xf9),CS(28,0xfa),CS(28,0xfb),CS(28,0xfc),CS(28,0xfd),CS(28,0xfe),CS(28,0xff),

	CS(30,0x00),CS(30,0x01),CS(30,0x02),CS(30,0x03),CS(30,0x04),CS(30,0x05),CS(30,0x06),CS(30,0x07),CS(30,0x08),CS(30,0x09),CS(30,0x0a),CS(30,0x0b),CS(30,0x0c),CS(30,0x0d),CS(30,0x0e),CS(30,0x0f),
	CS(30,0x10),CS(30,0x11),CS(30,0x12),CS(30,0x13),CS(30,0x14),CS(30,0x15),CS(30,0x16),CS(30,0x17),CS(30,0x18),CS(30,0x19),CS(30,0x1a),CS(30,0x1b),CS(30,0x1c),CS(30,0x1d),CS(30,0x1e),CS(30,0x1f),
	CS(30,0x20),CS(30,0x21),CS(30,0x22),CS(30,0x23),CS(30,0x24),CS(30,0x25),CS(30,0x26),CS(30,0x27),CS(30,0x28),CS(30,0x29),CS(30,0x2a),CS(30,0x2b),CS(30,0x2c),CS(30,0x2d),CS(30,0x2e),CS(30,0x2f),
	CS(30,0x30),CS(30,0x31),CS(30,0x32),CS(30,0x33),CS(30,0x34),CS(30,0x35),CS(30,0x36),CS(30,0x37),CS(30,0x38),CS(30,0x39),CS(30,0x3a),CS(30,0x3b),CS(30,0x3c),CS(30,0x3d),CS(30,0x3e),CS(30,0x3f),
	CS(30,0x40),CS(30,0x41),CS(30,0x42),CS(30,0x43),CS(30,0x44),CS(30,0x45),CS(30,0x46),CS(30,0x47),CS(30,0x48),CS(30,0x49),CS(30,0x4a),CS(30,0x4b),CS(30,0x4c),CS(30,0x4d),CS(30,0x4e),CS(30,0x4f),
	CS(30,0x50),CS(30,0x51),CS(30,0x52),CS(30,0x53),CS(30,0x54),CS(30,0x55),CS(30,0x56),CS(30,0x57),CS(30,0x58),CS(30,0x59),CS(30,0x5a),CS(30,0x5b),CS(30,0x5c),CS(30,0x5d),CS(30,0x5e),CS(30,0x5f),
	CS(30,0x60),CS(30,0x61),CS(30,0x62),CS(30,0x63),CS(30,0x64),CS(30,0x65),CS(30,0x66),CS(30,0x67),CS(30,0x68),CS(30,0x69),CS(30,0x6a),CS(30,0x6b),CS(30,0x6c),CS(30,0x6d),CS(30,0x6e),CS(30,0x6f),
	CS(30,0x70),CS(30,0x71),CS(30,0x72),CS(30,0x73),CS(30,0x74),CS(30,0x75),CS(30,0x76),CS(30,0x77),CS(30,0x78),CS(30,0x79),CS(30,0x7a),CS(30,0x7b),CS(30,0x7c),CS(30,0x7d),CS(30,0x7e),CS(30,0x7f),
	CS(30,0x80),CS(30,0x81),CS(30,0x82),CS(30,0x83),CS(30,0x84),CS(30,0x85),CS(30,0x86),CS(30,0x87),CS(30,0x88),CS(30,0x89),CS(30,0x8a),CS(30,0x8b),CS(30,0x8c),CS(30,0x8d),CS(30,0x8e),CS(30,0x8f),
	CS(30,0x90),CS(30,0x91),CS(30,0x92),CS(30,0x93),CS(30,0x94),CS(30,0x95),CS(30,0x96),CS(30,0x97),CS(30,0x98),CS(30,0x99),CS(30,0x9a),CS(30,0x9b),CS(30,0x9c),CS(30,0x9d),CS(30,0x9e),CS(30,0x9f),
	CS(30,0xa0),CS(30,0xa1),CS(30,0xa2),CS(30,0xa3),CS(30,0xa4),CS(30,0xa5),CS(30,0xa6),CS(30,0xa7),CS(30,0xa8),CS(30,0xa9),CS(30,0xaa),CS(30,0xab),CS(30,0xac),CS(30,0xad),CS(30,0xae),CS(30,0xaf),
	CS(30,0xb0),CS(30,0xb1),CS(30,0xb2),CS(30,0xb3),CS(30,0xb4),CS(30,0xb5),CS(30,0xb6),CS(30,0xb7),CS(30,0xb8),CS(30,0xb9),CS(30,0xba),CS(30,0xbb),CS(30,0xbc),CS(30,0xbd),CS(30,0xbe),CS(30,0xbf),
	CS(30,0xc0),CS(30,0xc1),CS(30,0xc2),CS(30,0xc3),CS(30,0xc4),CS(30,0xc5),CS(30,0xc6),CS(30,0xc7),CS(30,0xc8),CS(30,0xc9),CS(30,0xca),CS(30,0xcb),CS(30,0xcc),CS(30,0xcd),CS(30,0xce),CS(30,0xcf),
	CS(30,0xd0),CS(30,0xd1),CS(30,0xd2),CS(30,0xd3),CS(30,0xd4),CS(30,0xd5),CS(30,0xd6),CS(30,0xd7),CS(30,0xd8),CS(30,0xd9),CS(30,0xda),CS(30,0xdb),CS(30,0xdc),CS(30,0xdd),CS(30,0xde),CS(30,0xdf),
	CS(30,0xe0),CS(30,0xe1),CS(30,0xe2),CS(30,0xe3),CS(30,0xe4),CS(30,0xe5),CS(30,0xe6),CS(30,0xe7),CS(30,0xe8),CS(30,0xe9),CS(30,0xea),CS(30,0xeb),CS(30,0xec),CS(30,0xed),CS(30,0xee),CS(30,0xef),
	CS(30,0xf0),CS(30,0xf1),CS(30,0xf2),CS(30,0xf3),CS(30,0xf4),CS(30,0xf5),CS(30,0xf6),CS(30,0xf7),CS(30,0xf8),CS(30,0xf9),CS(30,0xfa),CS(30,0xfb),CS(30,0xfc),CS(30,0xfd),CS(30,0xfe),CS(30,0xff)
};

/* immediate operand #2 */
#define IM	imm12[OP & 0xfff]

INLINE void swapregs(UINT32 *dst, UINT32 *src, int num_regs)
{
	while (num_regs-- > 0)
	{
		UINT32 tmp = *src;
		*src++ = *dst;
		*dst++ = tmp;
	}
}

INLINE void fill_queue(void)
{
	change_pc26lew(PC);
	/* pre-fetch the instruction queue */
	arm.ppc = PC;
	arm.queue[1] = ARM_RDMEM_32(PC);
	PC = (PC + 4) & AMASK;
	arm.queue[2] = ARM_RDMEM_32(PC);
	PC = (PC + 4) & AMASK;
}

INLINE void shift_queue(void)
{
	/* move the instruction queue */
	arm.ppc = PC;
	arm.queue[0] = arm.queue[1];
	arm.queue[1] = arm.queue[2];
	arm.queue[2] = ARM_RDMEM_32(PC);
	/* PC is now 8 ahead of the current instruction in queue[0] */
	PC = (PC + 4) & AMASK;
}

INLINE void PUT_PC(UINT32 val, int link)
{
	switch( (PSW & S01) | ((val & S01) << 2) )
	{
	case  0: /* from USER mode to USER mode */
	case  5: /* from FIRQ mode to FIRQ mode */
	case 10: /* from IRQ  mode to IRQ  mode */
	case 15: /* from SVC  mode to SVC  mode */
		break;

	case  1: /* from FIRQ mode to USER mode */
	case  4: /* from USER mode to FIRQ mode */
		/* swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		break;

	case  2: /* from IRQ mode to USER mode */
	case  8: /* from USER mode to IRQ mode */
		/* swap USER R13/R14 with IRQ registers */
		swapregs(&arm.reg[13], &arm.reg_irq[0], 2);
		break;

	case  3: /* from SVC mode to USER mode */
	case 12: /* from USER mode to SVC mode */
		/* swap USER R13/R14 with SVC registers */
		swapregs(&arm.reg[13], &arm.reg_svc[0], 2);
		break;

	case  6: /* from IRQ mode to FIRQ mode */
		/* swap IRQ registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_irq[0], 2);
		/* now swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		break;

	case  7: /* from SVC mode to FIRQ mode */
		/* swap SVC registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_svc[0], 2);
		/* now swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		break;

	case  9: /* from FIRQ mode to IRQ mode */
		/* swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		/* now swap IRQ registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_irq[0], 2);
		break;

	case 11: /* from FIRQ mode to SVC mode */
		/* swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		/* now swap SVC registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_svc[0], 2);
		break;

	case 13: /* from SVC mode to FIRQ mode */
		/* swap SVC registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_svc[0], 2);
		/* now swap USER R8-R14 with FIRQ registers */
		swapregs(&arm.reg[ 8], &arm.reg_firq[0], 7);
		break;

	case 14: /* from IRQ mode to SVC mode */
		/* swap IRQ registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_irq[0], 2);
		/* swap SVC registers with USER R13/R14 registers */
		swapregs(&arm.reg[13], &arm.reg_svc[0], 2);
		break;
	}

	/* if this is a BL (branch with link) make a copy of R15 and the status now */
	if (link) arm.reg[14] = arm.reg[15] | PSW;

	/* store the status flags in an extra field */
	PSW = val & (N|Z|C|V|I|F|S01);

	/* only store the address part of val into r[15] */
	PC = val ^ PSW;
	fill_queue();

}

/*
 * Write to the destination register.
 * This needs special treatment for Rd = 15,
 * which is a combination of the program counter
 * and the processor status word
 */
INLINE void PUT_RD(UINT32 val)
{
	UINT32 rd = (OP>>12)&15;
	if ( rd == 15 ) /* destination is R15 (PC) ? */
		PUT_PC(val, 0);
	else
		arm.reg[rd] = val;
}

#define GET_C	((PSW & C) >> 29)

#define SET_NZ(d)											\
	PSW = (PSW & ~(N | Z)) |								\
		(d & N) |											\
		(d ? Z : 0)

#define SET_NZCV_ADD(d,s,v) 								\
	PSW = (PSW & ~(N | Z | C | V)) |						\
		(d & N) |											\
		(d ? Z : 0) |										\
		(v != 0 && d < s ? C : 0) | 						\
		( ((INT32)v > 0 && (INT32)d < (INT32)s) ||			\
		  ((INT32)v < 0 && (INT32)d > (INT32)d) ? V : 0)

#define SET_NZCV_SUB(d,s,v) 								\
	PSW = (PSW & ~(N | Z | C | V)) |						\
		(d & N) |											\
		(d ? Z : 0) |										\
		(v != 0 && d > s ? C : 0) | 						\
		( ((INT32)v < 0 && (INT32)d < (INT32)s) ||			\
		  ((INT32)v > 0 && (INT32)d > (INT32)d) ? V : 0)


/*****************************************************************************
 *
 *	ARITHMETIC OPCODES
 *	AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC
 *	TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN
 *
 *****************************************************************************/

/*
 *	----	xxxx 0000 0000 nnnn dddd ssss ssss ssss
 *	AND 	logical and with register
 */
static void and_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN & rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 0001 nnnn dddd ssss ssss ssss
 *	ANDS	logical and with register - set status
 */
static void ands_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN & rs;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 0010 nnnn dddd ssss ssss ssss
 *	EOR 	logical exclusive or with register
 */
static void eor_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN ^ rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 0011 nnnn dddd ssss ssss ssss
 *	EORS	logical exclusive or with register - set status
 */
static void eors_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN ^ rs;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 0100 nnnn dddd ssss ssss ssss
 *	SUB 	subtract register
 */
static void sub_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 0101 nnnn dddd ssss ssss ssss
 *	SUBS	subtract register - set status
 */
static void subs_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - rs;
	SET_NZCV_SUB(rd,RN,rs);
	PUT_RD(rd);
}

/*
 *	NZCV	31	 27   23   19	15	 11   7    3
 *	----	cccc 0000 0110 nnnn dddd ssss ssss ssss
 *	RSB 	reverse subtract register (swap Rn and Rs)
 */
static void rsb_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - rs;
	PUT_RD(rd);
}

/*
 *	NZCV	31	 27   23   19	15	 11   7    3
 *	NZ--	cccc 0000 0111 nnnn dddd ssss ssss ssss
 *	RSBS	reverse subtract register (swap Rn and Rs) - set status
 */
static void rsbs_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = rs - RN;
	SET_NZCV_SUB(rd,rs,RN);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 1000 nnnn dddd ssss ssss ssss
 *	ADD 	add register
 */
static void add_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN + rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 1001 nnnn dddd ssss ssss ssss
 *	ADDS	add register - set status
 */
static void adds_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN + rs;
	SET_NZCV_ADD(rd,RN,rs);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 1010 nnnn dddd ssss ssss ssss
 *	ADC 	add register with carry
 */
static void adc_r (void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = RN + rs + c;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 1011 nnnn dddd ssss ssss ssss
 *	ADCS	add register with carry - set status
 */
static void adcs_r(void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = RN + rs + c;
	SET_NZCV_ADD(rd,RN,rs + c);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 1100 nnnn dddd ssss ssss ssss
 *	SBC 	subtract register with carry
 */
static void sbc_r (void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = RN - rs - c;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 1101 nnnn dddd ssss ssss ssss
 *	SBCS	subtract register with carry - set status
 */
static void sbcs_r(void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = RN - rs - c;
	SET_NZCV_SUB(rd,RN,rs - c);
	PUT_RD(rd);
}

/*
 *	----	cccc 0000 1110 nnnn dddd ssss ssss ssss
 *	RSC 	reverse subtract register with carry (swap Rn and Rs)
 */
static void rsc_r (void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = rs - RN - c;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0000 1111 nnnn dddd ssss ssss ssss
 *	RSCS	reverse subtract register with carry (swap Rn and Rs) - set status
 */
static void rscs_r(void)
{
	UINT32 c = GET_C;
	UINT32 rs = RS();
	UINT32 rd = rs - RN - c;
	SET_NZCV_SUB(rd,rs,RN - c);
	PUT_RD(rd);
}

#if 0
/*
 *	NZ--	cccc 0001 0000 nnnn dddd ssss ssss ssss
 *	TST 	test register - no distinct opcode because TST always sets the status
 */
static void tst_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN & rs;
}
#endif

/*
 *	NZ--	cccc 0001 0001 nnnn dddd ssss ssss ssss
 *	TSTS	test register - set status
 */
static void tsts_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN & rs;
	SET_NZ(rd);
}

#if 0
/*
 *	NZ--	cccc 0001 0010 nnnn dddd ssss ssss ssss
 *	TEQ 	test equal bits register - no disctinct opcode because TEQ always sets the status
 */
static void teq_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN ^ rs;
}
#endif

/*
 *	NZ--	cccc 0001 0011 nnnn dddd ssss ssss ssss
 *	TEQS	test equal bits register - set status
 */
static void teqs_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN ^ rs;
	SET_NZ(rd);
}

#if 0
/*
 *	NZ--	cccc 0001 0100 nnnn dddd ssss ssss ssss
 *	CMP 	compare register - no distinct opcode because CMP always sets the status
 */
static void cmp_r (void)
{
	UINT32 rs = RS();
	UINT32 dst = RN - rs;
}
#endif

/*
 *	NZ--	cccc 0001 0101 nnnn dddd ssss ssss ssss
 *	CMPS	compare register - set status
 */
static void cmps_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - rs;
	SET_NZCV_SUB(rd,RN,rs);
}

#if 0
/*
 *	NZ--	cccc 0001 0110 nnnn dddd ssss ssss ssss
 *	CMN 	compare with negative register - no distinct opcode because CMN always sets the status
 */
static void cmn_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - ~rs;
}
#endif

/*
 *	NZ--	cccc 0001 0111 nnnn dddd ssss ssss ssss
 *	CMNS	compare with negative register - set status
 */
static void cmns_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN - ~rs;
	SET_NZCV_SUB(rd,RN,~rs);
}

/*
 *	----	cccc 0001 1000 nnnn dddd ssss ssss ssss
 *	ORR 	logical or with register
 */
static void orr_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RN | rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0001 1001 nnnn dddd ssss ssss ssss
 *	ORRS	logical or with register - set status
 */
static void orrs_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RN | rs;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0001 1010 nnnn dddd ssss ssss ssss
 *	MOV 	register
 */
static void mov_r (void)
{
	UINT32 rs = RS();
	PUT_RD(rs);
}

/*
 *	NZ--	cccc 0001 1011 nnnn dddd ssss ssss ssss
 *	MOVS	register - set status
 */
static void movs_r(void)
{
	UINT32 rd = RS();
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0001 1100 nnnn dddd ssss ssss ssss
 *	BIC 	bit clear register
 */
static void bic_r (void)
{
	UINT32 rs = RS();
	UINT32 rd = RD & ~rs;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0001 1101 nnnn dddd ssss ssss ssss
 *	BICS	bit clear register - set status
 */
static void bics_r(void)
{
	UINT32 rs = RS();
	UINT32 rd = RD & ~rs;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0001 1110 nnnn dddd ssss ssss ssss
 *	MVN 	move negative register
 */
static void mvn_r (void)
{
	UINT32 rs = RS();
	PUT_RD(~rs);
}

/*
 *	NZ--	cccc 0001 1111 nnnn dddd ssss ssss ssss
 *	MVNS	move negative register - set status
 */
static void mvns_r(void)
{
	UINT32 rd = ~RS();
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	xxxx 0010 0000 nnnn dddd iiii iiii iiii
 *	AND 	logical and with immediate
 */
static void and_i (void)
{
	PUT_RD(RN & IM);
}

/*
 *	NZ--	cccc 0010 0001 nnnn dddd iiii iiii iiii
 *	ANDS	logical and with immediate - set status
 */
static void ands_i(void)
{
	UINT32 rd = RN & IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 0010 nnnn dddd iiii iiii iiii
 *	EOR 	logical exclusive or with immediate
 */
static void eor_i (void)
{
	PUT_RD(RN ^ IM);
}

/*
 *	NZ--	cccc 0010 0011 nnnn dddd iiii iiii iiii
 *	EORS	logical exclusive or with immediate - set status
 */
static void eors_i(void)
{
	UINT32 rd = RN ^ IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 0100 nnnn dddd iiii iiii iiii
 *	SUB 	subtract immediate
 */
static void sub_i (void)
{
	PUT_RD(RN - IM);
}

/*
 *	NZCV	cccc 0010 0101 nnnn dddd iiii iiii iiii
 *	SUBS	subtract immediate - set status
 */
static void subs_i(void)
{
	UINT32 im = IM;
	UINT32 rd = RN - im;
	SET_NZCV_SUB(rd,RN,im);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 0110 nnnn dddd iiii iiii iiii
 *	RSB 	reverse subtract from immediate
 */
static void rsb_i (void)
{
	PUT_RD(IM - RN);
}

/*
 *	NZCV	cccc 0010 0111 nnnn dddd iiii iiii iiii
 *	RSBS	reverse subtract from immediate - set status
 */
static void rsbs_i(void)
{
	UINT32 im = IM;
	UINT32 rd = im - RN;
	SET_NZCV_SUB(rd,im,RN);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 1000 nnnn dddd iiii iiii iiii
 *	ADD 	add immediate
 */
static void add_i (void)
{
	PUT_RD(RN + IM);
}

/*
 *	NZCV	cccc 0010 1001 nnnn dddd iiii iiii iiii
 *	ADDS	add immediate - set status
 */
static void adds_i(void)
{
	UINT32 im = IM;
	UINT32 rd = RN + im;
	SET_NZCV_ADD(rd,RN,im);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 1010 nnnn dddd iiii iiii iiii
 *	ADC 	add immediate with carry
 */
static void adc_i (void)
{
	UINT32 c = GET_C;
	PUT_RD(RN + IM + c);
}

/*
 *	NZCV	cccc 0010 1011 nnnn dddd iiii iiii iiii
 *	ADCS	add immediate with carry - set status
 */
static void adcs_i(void)
{
	UINT32 c = GET_C;
	UINT32 im = IM;
	UINT32 rd = RN + (im + c);
	SET_NZCV_ADD(rd,RN,im + c);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 1100 nnnn dddd iiii iiii iiii
 *	SBC 	subtract immediate with carry
 */
static void sbc_i (void)
{
	UINT32 c = GET_C;
	PUT_RD(RN - IM - c);
}

/*
 *	NZCV	cccc 0010 1101 nnnn dddd iiii iiii iiii
 *	SBCS	subtract immediate with carry - set status
 */
static void sbcs_i(void)
{
	UINT32 c = GET_C;
	UINT32 im = IM;
	UINT32 rd = RN - (im + c);
	SET_NZCV_SUB(rd,RN,im + c);
	PUT_RD(rd);
}

/*
 *	----	cccc 0010 1110 nnnn dddd iiii iiii iiii
 *	RSC 	reverse subtract from immediate with carry
 */
static void rsc_i (void)
{
	UINT32 c = GET_C;
	PUT_RD(RN - (IM+c));
}

/*
 *	NZCV	cccc 0010 1111 nnnn dddd iiii iiii iiii
 *	RSCS	reverse subtract from immediate with carry - set status
 */
static void rscs_i(void)
{
	UINT32 c = GET_C;
	UINT32 im = IM;
	UINT32 rd = RN - (im + c);
	SET_NZCV_SUB(rd,RN,im + c);
	PUT_RD(rd);
}

#if 0
/*
 *	NZ--	cccc 0011 0000 nnnn dddd iiii iiii iiii
 *	TST 	test immediate - no distinct opcode because TST always sets the status
 */
static void tst_i (void)
{
	UINT32 rd = RN & IM;
}
#endif

/*
 *	NZ--	cccc 0011 0001 nnnn dddd iiii iiii iiii
 *	TSTS	test immediate - set status
 */
static void tsts_i(void)
{
	UINT32 rd = RN & IM;
	SET_NZ(rd);
}

#if 0
/*
 *	NZ--	cccc 0011 0010 nnnn dddd iiii iiii iiii
 *	TEQ 	test equal bits immediate - no disctinct opcode because TEQ always sets the status
 */
static void teq_i (void)
{
	UINT32 rd = RN ^ IM;
}
#endif

/*
 *	NZ--	cccc 0011 0011 nnnn dddd iiii iiii iiii
 *	TEQS	test equal bits immediate - set status
 */
static void teqs_i(void)
{
	UINT32 rd = RN ^ IM;
	SET_NZ(rd);
}

#if 0
/*
 *	NZ--	cccc 0011 0100 nnnn dddd iiii iiii iiii
 *	CMP 	compare immediate - no distinct opcode because CMP always sets the status
 */
static void cmp_i (void)
{
	UINT32 dst = RN - IM;
}
#endif

/*
 *	NZCV	cccc 0011 0101 nnnn dddd iiii iiii iiii
 *	CMPS	compare immediate - set status
 */
static void cmps_i(void)
{
	UINT32 im = IM;
	UINT32 rd = RN - im;
	SET_NZCV_SUB(rd,RN,im);
}

#if 0
/*
 *	NZ--	cccc 0011 0110 nnnn dddd iiii iiii iiii
 *	CMN 	compare with negative immediate - no distinct opcode because CMN always sets the status
 */
static void cmn_i (void)
{
	UINT32 im = im;
	UINT32 rd = RN - ~im;
}
#endif

/*
 *	NZCV	cccc 0011 0111 nnnn dddd iiii iiii iiii
 *	CMNS	compare with negative immediate - set status
 */
static void cmns_i(void)
{
	UINT32 im = IM;
	UINT32 rd = RN - ~im;
	SET_NZCV_SUB(rd,RN,~im);
}

/*
 *	----	cccc 0011 1000 nnnn dddd iiii iiii iiii
 *	ORR 	logical or with immediate
 */
static void orr_i (void)
{
	PUT_RD(RN | IM);
}

/*
 *	NZ--	cccc 0011 1001 nnnn dddd iiii iiii iiii
 *	ORRS	logical or with immediate - set status
 */
static void orrs_i(void)
{
	UINT32 rd = RN | IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0011 1010 nnnn dddd iiii iiii iiii
 *	MOV 	move immediate
 */
static void mov_i (void)
{
	PUT_RD(IM);
}

/*
 *	NZ--	cccc 0011 1011 nnnn dddd iiii iiii iiii
 *	MOVS	move immediate - set status
 */
static void movs_i(void)
{
	UINT32 rd = IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0011 1100 nnnn dddd iiii iiii iiii
 *	BIC 	bit clear immediate
 */
static void bic_i (void)
{
	UINT32 rd = RN & ~IM;
	PUT_RD(rd);
}

/*
 *	NZ--	cccc 0011 1101 nnnn dddd iiii iiii iiii
 *	BICS	bit clear immediate - set status
 */
static void bics_i(void)
{
	UINT32 rd = RN & ~IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*
 *	----	cccc 0011 1110 nnnn dddd iiii iiii iiii
 *	MVN 	move negative immediate
 */
static void mvn_i (void)
{
	PUT_RD(~IM);
}

/*
 *	NZ--	cccc 0011 1111 nnnn dddd iiii iiii iiii
 *	MVNS	move negative immediate - set status
 */
static void mvns_i(void)
{
	UINT32 rd = ~IM;
	SET_NZ(rd);
	PUT_RD(rd);
}

/*****************************************************************************
 *
 *	SINGLE REGISTER TRANSFER OPCODES
 *	LDR, STR
 *
 *****************************************************************************/

/*
 *	----	cccc 0100 0000 nnnn dddd ssss ssss ssss
 *	STR 	store register - preindexed, subtract index register
 */
static void str_1rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	ARM_WRMEM_32(ea,RD);
}

/*
 *	----	cccc 0100 0001 nnnn dddd ssss ssss ssss
 *	LDR 	load register - preindexed, subtract index register
 */
static void ldr_1rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
}

/*
 *	----	cccc 0100 0010 nnnn dddd ssss ssss ssss
 *	STR!	store register - preindexed, subtract index register, write back ea
 */
static void str_1rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN = ea;
}

/*
 *	----	cccc 0100 0011 nnnn dddd ssss ssss ssss
 *	LDR!	load register - preindexed, subtract index register, write back ea
 */
static void ldr_1rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN = ea;
}

/*
 *	----	cccc 0100 0100 nnnn dddd ssss ssss ssss
 *	STRB	store register byte - preindexed, subtract index register
 */
static void strb_1rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
}

/*
 *	----	cccc 0100 0101 nnnn dddd ssss ssss ssss
 *	LDRB	load register byte - preindexed, subtract index register
 */
static void ldrb_1rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
}

/*
 *	----	cccc 0100 0110 nnnn dddd ssss ssss ssss
 *	STRB!	store register byte - preindexed, subtract index register, write back ea
 */
static void strb_1rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN = ea;
}

/*
 *	----	cccc 0100 0111 nnnn dddd ssss ssss ssss
 *	LDRB!	load register byte - preindexed, subtract index register, write back ea
 */
static void ldrb_1rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN - rs) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN = ea;
}

/*
 *	----	cccc 0100 1000 nnnn dddd ssss ssss ssss
 *	STR 	store register - preindexed, add index register
 */
static void str_1ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	ARM_WRMEM_32(ea,RD);
}

/*
 *	----	cccc 0100 1001 nnnn dddd ssss ssss ssss
 *	LDR 	load register - preindexed, add index register
 */
static void ldr_1ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
}

/*
 *	----	cccc 0100 1010 nnnn dddd ssss ssss ssss
 *	STR!	store register - preindexed, add index register, write back ea
 */
static void str_1raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN = ea;
}

/*
 *	----	cccc 0100 1011 nnnn dddd ssss ssss ssss
 *	LDR!	load register - preindexed, add index register, write back ea
 */
static void ldr_1raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN = ea;
}

/*
 *	----	cccc 0100 1100 nnnn dddd ssss ssss ssss
 *	STRB	store register byte - preindexed, add index register
 */
static void strb_1ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
}

/*
 *	----	cccc 0100 1101 nnnn dddd ssss ssss ssss
 *	LDRB	load register byte - preindexed, add index register
 */
static void ldrb_1ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
}

/*
 *	----	cccc 0100 1110 nnnn dddd ssss ssss ssss
 *	STR!	store register byte - preindexed, add index register, write back ea
 */
static void strb_1raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN = ea;
}

/*
 *	----	cccc 0100 1111 nnnn dddd ssss ssss ssss
 *	LDR!	load register byte - preindexed, add index register, write back ea
 */
static void ldrb_1raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = (RN + rs) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN = ea;
}


/*
 *	----	cccc 0101 0000 nnnn dddd ssss ssss ssss
 *	STR 	store register - postindexed, subtract index register
 */
static void str_2rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN -= rs;
}

/*
 *	----	cccc 0101 0001 nnnn dddd ssss ssss ssss
 *	LDR 	load register - postindexed, subtract index register
 */
static void ldr_2rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN -= rs;
}

/*
 *	----	cccc 0101 0010 nnnn dddd ssss ssss ssss
 *	STR!	store register - postindexed, subtract index register, write back ea
 */
static void str_2rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN -= rs;
}

/*
 *	----	cccc 0101 0011 nnnn dddd ssss ssss ssss
 *	LDR!	load register - postindexed, subtract index register, write back ea
 */
static void ldr_2rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN -= rs;
}

/*
 *	----	cccc 0101 0100 nnnn dddd ssss ssss ssss
 *	STRB	store register byte - postindexed, subtract index register
 */
static void strb_2rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN -= rs;
}

/*
 *	----	cccc 0101 0101 nnnn dddd ssss ssss ssss
 *	LDRB	load register byte - postindexed, subtract index register
 */
static void ldrb_2rs(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN -= rs;
}

/*
 *	----	cccc 0101 0110 nnnn dddd ssss ssss ssss
 *	STRB!	store register byte - postindexed, subtract index register, write back ea
 */
static void strb_2rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN -= rs;
}

/*
 *	----	cccc 0101 0111 nnnn dddd ssss ssss ssss
 *	LDRB!	load register byte - postindexed, subtract index register, write back ea
 */
static void ldrb_2rsw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN -= rs;
}

/*
 *	----	cccc 0101 1000 nnnn dddd ssss ssss ssss
 *	STR 	store register - postindexed, add index register
 */
static void str_2ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN += rs;
}

/*
 *	----	cccc 0101 1001 nnnn dddd ssss ssss ssss
 *	LDR 	load register - postindexed, add index register
 */
static void ldr_2ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN += rs;
}

/*
 *	----	cccc 0101 1010 nnnn dddd ssss ssss ssss
 *	STR!	store register - postindexed, add index register, write back ea
 */
static void str_2raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN += rs;
}

/*
 *	----	cccc 0101 1011 nnnn dddd ssss ssss ssss
 *	LDR!	load register - postindexed, add index register, write back ea
 */
static void ldr_2raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN += rs;
}

/*
 *	----	cccc 0101 1100 nnnn dddd ssss ssss ssss
 *	STRB	store register byte - postindexed, add index register
 */
static void strb_2ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN += rs;
}

/*
 *	----	cccc 0101 1101 nnnn dddd ssss ssss ssss
 *	LDRB	load register byte - postindexed, add index register
 */
static void ldrb_2ra(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN += rs;
}

/*
 *	----	cccc 0101 1110 nnnn dddd ssss ssss ssss
 *	STR!	store register byte - postindexed, add index register, write back ea
 */
static void strb_2raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN += rs;
}

/*
 *	----	cccc 0101 1111 nnnn dddd ssss ssss ssss
 *	LDR!	load register byte - postindexed, add index register, write back ea
 */
static void ldrb_2raw(void)
{
	UINT32 rs = RS();
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN += rs;
}



/*
 *	----	cccc 0110 0000 nnnn dddd iiii iiii iiii
 *	STR 	store register - preindexed, subtract index register
 */
static void str_1is(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	ARM_WRMEM_32(ea,RD);
}

/*
 *	----	cccc 0110 0001 nnnn dddd iiii iiii iiii
 *	LDR 	load register - preindexed, subtract index register
 */
static void ldr_1is(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
}

/*
 *	----	cccc 0110 0010 nnnn dddd iiii iiii iiii
 *	STR!	store register - preindexed, subtract index register, write back ea
 */
static void str_1isw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN = ea;
}

/*
 *	----	cccc 0110 0011 nnnn dddd iiii iiii iiii
 *	LDR!	load register - preindexed, subtract index register, write back ea
 */
static void ldr_1isw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN = ea;
}

/*
 *	----	cccc 0110 0100 nnnn dddd iiii iiii iiii
 *	STRB	store register byte - preindexed, subtract index register
 */
static void strb_1is(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
}

/*
 *	----	cccc 0110 0101 nnnn dddd iiii iiii iiii
 *	LDRB	load register byte - preindexed, subtract index register
 */
static void ldrb_1is(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
}

/*
 *	----	cccc 0110 0110 nnnn dddd iiii iiii iiii
 *	STRB!	store register byte - preindexed, subtract index register, write back ea
 */
static void strb_1isw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN = ea;
}

/*
 *	----	cccc 0110 0111 nnnn dddd iiii iiii iiii
 *	LDRB!	load register byte - preindexed, subtract index register, write back ea
 */
static void ldrb_1isw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN - im) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN = ea;
}

/*
 *	----	cccc 0110 1000 nnnn dddd iiii iiii iiii
 *	STR 	store register - preindexed, add index register
 */
static void str_1ia(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	ARM_WRMEM_32(ea,RD);
}

/*
 *	----	cccc 0110 1001 nnnn dddd iiii iiii iiii
 *	LDR 	load register - preindexed, add index register
 */
static void ldr_1ia(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
}

/*
 *	----	cccc 0110 1010 nnnn dddd iiii iiii iiii
 *	STR!	store register - preindexed, add index register, write back ea
 */
static void str_1iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN = ea;
}

/*
 *	----	cccc 0110 1011 nnnn dddd iiii iiii iiii
 *	LDR!	load register - preindexed, add index register, write back ea
 */
static void ldr_1iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN = ea;
}

/*
 *	----	cccc 0110 1100 nnnn dddd iiii iiii iiii
 *	STRB	store register byte - preindexed, add index register
 */
static void strb_1ia(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
}

/*
 *	----	cccc 0110 1101 nnnn dddd iiii iiii iiii
 *	LDRB	load register byte - preindexed, add index register
 */
static void ldrb_1ia(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
}

/*
 *	----	cccc 0110 1110 nnnn dddd iiii iiii iiii
 *	STR!	store register byte - preindexed, add index register, write back ea
 */
static void strb_1iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN = ea;
}

/*
 *	----	cccc 0110 1111 nnnn dddd iiii iiii iiii
 *	LDR!	load register byte - preindexed, add index register, write back ea
 */
static void ldrb_1iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = (RN + im) & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN = ea;
}


/*
 *	----	cccc 0111 0000 nnnn dddd iiii iiii iiii
 *	STR 	store register - postindexed, subtract index register
 */
static void str_2is(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN -= im;
}

/*
 *	----	cccc 0111 0001 nnnn dddd iiii iiii iiii
 *	LDR 	load register - postindexed, subtract index register
 */
static void ldr_2is(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN -= im;
}

/*
 *	----	cccc 0111 0010 nnnn dddd iiii iiii iiii
 *	STR!	store register - postindexed, subtract index register, write back ea
 */
static void str_2isw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN -= im;
}

/*
 *	----	cccc 0111 0011 nnnn dddd iiii iiii iiii
 *	LDR!	load register - postindexed, subtract index register, write back ea
 */
static void ldr_2isw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN -= im;
}

/*
 *	----	cccc 0111 0100 nnnn dddd iiii iiii iiii
 *	STRB	store register byte - postindexed, subtract index register
 */
static void strb_2is(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN -= im;
}

/*
 *	----	cccc 0111 0101 nnnn dddd iiii iiii iiii
 *	LDRB	load register byte - postindexed, subtract index register
 */
static void ldrb_2is(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN -= im;
}

/*
 *	----	cccc 0111 0110 nnnn dddd iiii iiii iiii
 *	STRB!	store register byte - postindexed, subtract index register, write back ea
 */
static void strb_2isw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN -= im;
}

/*
 *	----	cccc 0111 0111 nnnn dddd iiii iiii iiii
 *	LDRB!	load register byte - postindexed, subtract index register, write back ea
 */
static void ldrb_2isw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN -= im;
}

/*
 *	----	cccc 0111 1000 nnnn dddd iiii iiii iiii
 *	STR 	store register - postindexed, add index register
 */
static void str_2ia(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN += im;
}

/*
 *	----	cccc 0111 1001 nnnn dddd iiii iiii iiii
 *	LDR 	load register - postindexed, add index register
 */
static void ldr_2ia(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN += im;
}

/*
 *	----	cccc 0111 1010 nnnn dddd iiii iiii iiii
 *	STR!	store register - postindexed, add index register, write back ea
 */
static void str_2iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM_32(ea,RD);
	RN += im;
}

/*
 *	----	cccc 0111 1011 nnnn dddd iiii iiii iiii
 *	LDR!	load register - postindexed, add index register, write back ea
 */
static void ldr_2iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM_32(ea));
	RN += im;
}

/*
 *	----	cccc 0111 1100 nnnn dddd iiii iiii iiii
 *	STRB	store register byte - postindexed, add index register
 */
static void strb_2ia(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN += im;
}

/*
 *	----	cccc 0111 1101 nnnn dddd iiii iiii iiii
 *	LDRB	load register byte - postindexed, add index register
 */
static void ldrb_2ia(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN += im;
}

/*
 *	----	cccc 0111 1110 nnnn dddd iiii iiii iiii
 *	STR!	store register byte - postindexed, add index register, write back ea
 */
static void strb_2iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	ARM_WRMEM(ea,RD & 0xff);
	RN += im;
}

/*
 *	----	cccc 0111 1111 nnnn dddd iiii iiii iiii
 *	LDR!	load register byte - postindexed, add index register, write back ea
 */
static void ldrb_2iaw(void)
{
	UINT32 im = IM;
	UINT32 ea = RN & AMASK;
	PUT_RD(ARM_RDMEM(ea));
	RN += im;
}


/*****************************************************************************
 *
 *	MULTIPLE REGISTER TRANSFER OPCODES
 *	LDM, STM
 *
 *****************************************************************************/

/*
 *	----	cccc 1000 0000 nnnn rrrr rrrr rrrr rrrr
 *	STMDA	store multiple registers, increment index after
 */
static void stmda(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,0,-4);
}

/*
 *	----	cccc 1000 0001 nnnn rrrr rrrr rrrr rrrr
 *	LDMDA	load multiple registers, increment index after
 */
static void ldmda(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,0,-4);
}

/*
 *	----	cccc 1000 0010 nnnn rrrr rrrr rrrr rrrr
 *	STMDA!	store multiple registers, increment index after - write back
 */
static void stmda_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,0,-4);
	RN = ea;
}

/*
 *	----	cccc 1000 0011 nnnn rrrr rrrr rrrr rrrr
 *	LDMDA!	load multiple registers, increment index after - write back
 */
static void ldmda_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,0,-4);
	RN = ea;
}

/*
 *	----	cccc 1000 0100 nnnn rrrr rrrr rrrr rrrr
 *	STMDAB	store multiple register bytes, increment index after
 */
static void stmdab(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,0,-1);
}

/*
 *	----	cccc 1000 0101 nnnn rrrr rrrr rrrr rrrr
 *	LDMDAB	load multiple register bytes, increment index after
 */
static void ldmdab(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,0,-1);
}

/*
 *	----	cccc 1000 0110 nnnn rrrr rrrr rrrr rrrr
 *	STMDAB! store multiple register bytes, increment index after - write back
 */
static void stmdab_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,0,-1);
	RN = ea;
}

/*
 *	----	cccc 1000 0111 nnnn rrrr rrrr rrrr rrrr
 *	LDMDAB! load multiple register bytes, increment index after - write back
 */
static void ldmdab_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,0,-1);
	RN = ea;
}

/*
 *	----	cccc 1000 1000 nnnn rrrr rrrr rrrr rrrr
 *	STMIA	store multiple registers, increment index after
 */
static void stmia(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,0,+4);
}

/*
 *	----	cccc 1000 1001 nnnn rrrr rrrr rrrr rrrr
 *	LDMIA	load multiple registers, increment index after
 */
static void ldmia(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,0,+4);
}

/*
 *	----	cccc 1000 1010 nnnn rrrr rrrr rrrr rrrr
 *	STMIA!	store multiple registers, increment index after - write back
 */
static void stmia_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,0,+4);
	RN = ea;
}

/*
 *	----	cccc 1000 1011 nnnn rrrr rrrr rrrr rrrr
 *	LDMIA!	load multiple registers, increment index after - write back
 */
static void ldmia_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,0,+4);
	RN = ea;
}

/*
 *	----	cccc 1000 1100 nnnn rrrr rrrr rrrr rrrr
 *	STMIAB	store multiple register bytes, increment index after
 */
static void stmiab(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,0,+1);
}

/*
 *	----	cccc 1000 1101 nnnn rrrr rrrr rrrr rrrr
 *	LDMIAB	load multiple register bytes, increment index after
 */
static void ldmiab(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,0,+1);
}

/*
 *	----	cccc 1000 1110 nnnn rrrr rrrr rrrr rrrr
 *	STMIAB! store multiple register bytes, increment index after - write back
 */
static void stmiab_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,0,+1);
	RN = ea;
}

/*
 *	----	cccc 1000 1111 nnnn rrrr rrrr rrrr rrrr
 *	LDMIAB! load multiple register bytes, increment index after - write back
 */
static void ldmiab_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,0,+1);
	RN = ea;
}

/*
 *	----	cccc 1001 0000 nnnn rrrr rrrr rrrr rrrr
 *	STMDB	store multiple registers, increment index before
 */
static void stmdb(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,-4,0);
}

/*
 *	----	cccc 1001 0001 nnnn rrrr rrrr rrrr rrrr
 *	LDMDB	load multiple registers, increment index before
 */
static void ldmdb(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,-4,0);
}

/*
 *	----	cccc 1001 0010 nnnn rrrr rrrr rrrr rrrr
 *	STMDB!	store multiple registers, increment index before - write back
 */
static void stmdb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,-4,0);
	RN = ea;
}

/*
 *	----	cccc 1001 0011 nnnn rrrr rrrr rrrr rrrr
 *	LDMDB!	load multiple registers, increment index before - write back
 */
static void ldmdb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,-4,0);
	RN = ea;
}

/*
 *	----	cccc 1001 0100 nnnn rrrr rrrr rrrr rrrr
 *	STMDBB	store multiple register bytes, decrement index before
 */
static void stmdbb(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,-1,0);
}

/*
 *	----	cccc 1001 0101 nnnn rrrr rrrr rrrr rrrr
 *	LDMDBB	load multiple register bytes, decrement index before
 */
static void ldmdbb(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,-1,0);
}

/*
 *	----	cccc 1001 0110 nnnn rrrr rrrr rrrr rrrr
 *	STMDBB! store multiple register bytes, increment index before - write back
 */
static void stmdbb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,-1,0);
	RN = ea;
}

/*
 *	----	cccc 1001 0111 nnnn rrrr rrrr rrrr rrrr
 *	LDMDBB! load multiple register bytes, increment index before - write back
 */
static void ldmdbb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,-1,0);
	RN = ea;
}

/*
 *	----	cccc 1001 1000 nnnn rrrr rrrr rrrr rrrr
 *	STMIB	store multiple registers, increment index before
 */
static void stmib(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,+4,0);
}

/*
 *	----	cccc 1001 1001 nnnn rrrr rrrr rrrr rrrr
 *	LDMIB	load multiple registers, increment index before
 */
static void ldmib(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,+4,0);
}

/*
 *	----	cccc 1001 1010 nnnn rrrr rrrr rrrr rrrr
 *	STMIB!	store multiple registers, increment index before - write back
 */
static void stmib_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM_32,+4,0);
	RN = ea;
}

/*
 *	----	cccc 1001 1011 nnnn rrrr rrrr rrrr rrrr
 *	LDMIB!	load multiple registers, increment index before - write back
 */
static void ldmib_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM_32,+4,0);
	RN = ea;
}

/*
 *	----	cccc 1001 1100 nnnn rrrr rrrr rrrr rrrr
 *	STMIBB	store multiple register bytes, increment index before
 */
static void stmibb(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,+1,0);
}

/*
 *	----	cccc 1001 1101 nnnn rrrr rrrr rrrr rrrr
 *	LDMIBB	load multiple register bytes, increment index before
 */
static void ldmibb(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,+1,0);
}

/*
 *	----	cccc 1001 1110 nnnn rrrr rrrr rrrr rrrr
 *	STMIBB! store multiple register bytes, increment index before - write back
 */
static void stmibb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	STM(ARM_WRMEM,+1,0);
	RN = ea;
}

/*
 *	----	cccc 1001 1111 nnnn rrrr rrrr rrrr rrrr
 *	LDMIBB! load multiple register bytes, increment index before - write back
 */
static void ldmibb_w(void)
{
	UINT32 ea = (RN) & AMASK;
	LDM(ARM_RDMEM,+1,0);
	RN = ea;
}

/*****************************************************************************
 *
 *	BRANCH OPCODES
 *	B, BL
 *
 *****************************************************************************/

/*
 *	----	cccc 1010 oooo oooo oooo oooo oooo oooo
 *	B		branch relative
 */
static void b(void)
{
	UINT32 offs = OP & 0x00ffffff;

	PC = (arm.ppc + (offs << 2)) & AMASK;
	fill_queue();
}

/*
 *	----	cccc 1011 oooo oooo oooo oooo oooo oooo
 *	BL		branch relative with link
 */
static void bl(void)
{
	UINT32 offs = OP & 0x00ffffff;

	/* save PC */
	arm.reg[14] = PC | PSW;

	PC = (arm.ppc + (offs << 2)) & AMASK;
	fill_queue();
}

/*****************************************************************************
 *
 *	ILLEGAL OPCODES
 *
 *****************************************************************************/

/*
 *	----	cccc 1100 xxxx xxxx xxxx xxxx xxxx xxxx
 *	??? 	illegal opcodes
 */
static void ill_c(void)
{
	/*logerror("ARM #%d: %08x illegal opcode %08x\n", cpu_getactivecpu(), PC, OP);*/
}

/*
 *	----	cccc 1101 xxxx xxxx xxxx xxxx xxxx xxxx
 * ???		illegal opcodes
 */
static void ill_d(void)
{
	/*logerror("ARM #%d: %08x illegal opcode %08x\n", cpu_getactivecpu(), PC, OP);*/
}

/*
 *	----	cccc 1110 xxxx xxxx xxxx xxxx xxxx xxxx
 *	??? 	illegal opcodes
 */
static void ill_e(void)
{
	/*logerror("ARM #%d: %08x illegal opcode %08x\n", cpu_getactivecpu(), PC, OP);*/
}

/*
 *	----	cccc 1111 xxxx xxxx xxxx xxxx xxxx xxxx
 *	SWI 	software interrupt
 */
static void swi(void)
{
}

/* basic opcodes: bits 27-20 */
void (*func[256])(void) =
{
	/* x00xxxxx - x1fxxxxx */
	and_r,	  ands_r,	eor_r,	  eors_r,	sub_r,	  subs_r,	rsb_r,	  rsbs_r,
	add_r,	  adds_r,	adc_r,	  adcs_r,	sbc_r,	  sbcs_r,	rsc_r,	  rscs_r,
	tsts_r,   tsts_r,	teqs_r,   teqs_r,	cmps_r,   cmps_r,	cmns_r,   cmns_r,
	orr_r,	  orrs_r,	mov_r,	  movs_r,	bic_r,	  bics_r,	mvn_r,	  mvns_r,
	/* x20xxxxx - x3fxxxxx */
	and_i,	  ands_i,	eor_i,	  eors_i,	sub_i,	  subs_i,	rsb_i,	  rsbs_i,
	add_i,	  adds_i,	adc_i,	  adcs_i,	sbc_i,	  sbcs_i,	rsc_i,	  rscs_i,
	tsts_i,   tsts_i,	teqs_i,   teqs_i,	cmps_i,   cmps_i,	cmns_i,   cmns_i,
	orr_i,	  orrs_i,	mov_i,	  movs_i,	bic_i,	  bics_i,	mvn_i,	  mvns_i,
	/* x40xxxxx - x5fxxxxx */
	str_1rs,  ldr_1rs,	str_1rsw, ldr_1rsw, strb_1rs, ldrb_1rs, strb_1rsw,ldrb_1rsw,
	str_1ra,  ldr_1ra,	str_1raw, ldr_1raw, strb_1ra, ldrb_1ra, strb_1raw,ldrb_1raw,
	str_2rs,  ldr_2rs,	str_2rsw, ldr_2rsw, strb_2rs, ldrb_2rs, strb_2rsw,ldrb_2rsw,
	str_2ra,  ldr_2ra,	str_2raw, ldr_2raw, strb_2ra, ldrb_2ra, strb_2raw,ldrb_2raw,
	/* x60xxxxx - x7fxxxxx */
	str_1is,  ldr_1is,	str_1isw, ldr_1isw, strb_1is, ldrb_1is, strb_1isw,ldrb_1isw,
	str_1ia,  ldr_1ia,	str_1iaw, ldr_1iaw, strb_1ia, ldrb_1ia, strb_1iaw,ldrb_1iaw,
	str_2is,  ldr_2is,	str_2isw, ldr_2isw, strb_2is, ldrb_2is, strb_2isw,ldrb_2isw,
	str_2ia,  ldr_2ia,	str_2iaw, ldr_2iaw, strb_2ia, ldrb_2ia, strb_2iaw,ldrb_2iaw,
	/* x80xxxxx - x9fxxxxx */
	stmda,	  ldmda,	stmda_w,  ldmda_w,	stmdab,   ldmdab,	stmdab_w, ldmdab_w,
	stmia,	  ldmia,	stmia_w,  ldmia_w,	stmiab,   ldmiab,	stmiab_w, ldmiab_w,
	stmdb,	  ldmdb,	stmdb_w,  ldmdb_w,	stmdbb,   ldmdbb,	stmdbb_w, ldmdbb_w,
	stmib,	  ldmib,	stmib_w,  ldmib_w,	stmibb,   ldmibb,	stmibb_w, ldmibb_w,
	/* xa0xxxxx - xbfxxxxx */
	b,		  b,		b,		  b,		b,		  b,		b,		  b,
	b,		  b,		b,		  b,		b,		  b,		b,		  b,
	bl, 	  bl,		bl, 	  bl,		bl, 	  bl,		bl, 	  bl,
	bl, 	  bl,		bl, 	  bl,		bl, 	  bl,		bl, 	  bl,
	/* xc0xxxxx - xdfxxxxx */
	ill_c,	  ill_c,	ill_c,	  ill_c,	ill_c,	  ill_c,	ill_c,	  ill_c,
	ill_c,	  ill_c,	ill_c,	  ill_c,	ill_c,	  ill_c,	ill_c,	  ill_c,
	ill_d,	  ill_d,	ill_d,	  ill_d,	ill_d,	  ill_d,	ill_d,	  ill_d,
	ill_d,	  ill_d,	ill_d,	  ill_d,	ill_d,	  ill_d,	ill_d,	  ill_d,
	/* xe0xxxxx - xffxxxxx */
	ill_e,	  ill_e,	ill_e,	  ill_e,	ill_e,	  ill_e,	ill_e,	  ill_e,
	ill_e,	  ill_e,	ill_e,	  ill_e,	ill_e,	  ill_e,	ill_e,	  ill_e,
	swi,	  swi,		swi,	  swi,		swi,	  swi,		swi,	  swi,
	swi,	  swi,		swi,	  swi,		swi,	  swi,		swi,	  swi,
};

void arm_reset(void *param)
{
	memset(&arm, 0, sizeof(struct ARM));
	PUT_PC(0x00000000, 0);
}

void arm_exit(void)
{
	/* nothing to do here */
}

int arm_execute(int cycles)
{
	arm_ICount = cycles;
//	change_pc26lew(-1);

	do
	{
		int cond;

		shift_queue();

		/* conditionally execute _every_ opcode (yes, the ARM is like this) */
		switch (OP >> 28)
		{
		case C_EQ: cond =  PC & Z; break;
		case C_NE: cond = ~PC & Z; break;
		case C_CS: cond =  PC & C; break;
		case C_CC: cond = ~PC & C; break;
		case C_MI: cond =  PC & N; break;
		case C_PL: cond = ~PC & N; break;
		case C_VS: cond =  PC & V; break;
		case C_VC: cond = ~PC & V; break;
		case C_HI: cond = ((PC & C) >> 1) & (~PC & Z); break;
		case C_LS: cond = ((~PC & C) >> 1) | (PC & Z); break;
		case C_GE: cond = ((~PC & N) >> 3) ^ (PC & V); break;
		case C_LT: cond = ((PC & N) >> 3) ^ (PC & V); break;
		case C_GT: cond = (((~PC & N) >> 3) ^ (PC & V)) & ((~PC << 1) & Z); break;
		case C_LE: cond = (((PC & N) >> 3) ^ (PC & V)) | ((PC << 1) & Z); break;
		case C_AL: cond = 1; break;
		case C_NV:
		default:
			cond = 0; break;
		}

		/* conidition matched? */
		if (cond)
			(*func[(OP>>20) & 0xff])();

		arm_ICount -= 4;

	} while (arm_ICount > 0);

	return cycles - arm_ICount;
}

unsigned arm_get_context(void *dst)
{
	if (dst)
		*(struct ARM *)dst = arm;
	return sizeof(struct ARM);
}

void arm_set_context(void *src)
{
	if (src)
		arm = *(struct ARM *)src;
}

unsigned arm_get_pc(void)
{
	return PC - 8;
}

void arm_set_pc(unsigned val)
{
	PUT_PC(val, 0);
}

unsigned arm_get_sp(void)
{
	return arm.reg[14];
}

void arm_set_sp(unsigned val)
{
	arm.reg[14] = val;
}

unsigned arm_get_reg(int regnum)
{
	switch( regnum )
	{
	case ARM_OP:   return OP;
	case ARM_Q1:   return arm.queue[1];
	case ARM_Q2:   return arm.queue[2];
	case ARM_PSW:  return PSW;
	case ARM_R0:   return arm.reg[ 0];
	case ARM_R1:   return arm.reg[ 1];
	case ARM_R2:   return arm.reg[ 2];
	case ARM_R3:   return arm.reg[ 3];
	case ARM_R4:   return arm.reg[ 4];
	case ARM_R5:   return arm.reg[ 5];
	case ARM_R6:   return arm.reg[ 6];
	case ARM_R7:   return arm.reg[ 7];
	case ARM_R8:   return arm.reg[ 8];
	case ARM_R9:   return arm.reg[ 9];
	case ARM_R10:  return arm.reg[10];
	case ARM_R11:  return arm.reg[11];
	case ARM_R12:  return arm.reg[12];
	case ARM_R13:  return arm.reg[13];
	case ARM_R14:  return arm.reg[14];
	case ARM_R15:  return arm.reg[15];
	case ARM_FR8:  return arm.reg_firq[0];
	case ARM_FR9:  return arm.reg_firq[1];
	case ARM_FR10: return arm.reg_firq[2];
	case ARM_FR11: return arm.reg_firq[3];
	case ARM_FR12: return arm.reg_firq[4];
	case ARM_FR13: return arm.reg_firq[5];
	case ARM_FR14: return arm.reg_firq[6];
	case ARM_IR13: return arm.reg_irq[0];
	case ARM_IR14: return arm.reg_irq[1];
	case ARM_SR13: return arm.reg_svc[0];
	case ARM_SR14: return arm.reg_svc[1];
	}
	return 0;
}

void arm_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
	case ARM_OP:   OP = val;			   break;
	case ARM_Q1:   arm.queue[1] = val;	   break;
	case ARM_Q2:   arm.queue[2] = val;	   break;
	case ARM_PSW:  PSW = val & (N|Z|C|V|I|F|S01); break;
	case ARM_R0:   arm.reg[ 0] = val;	   break;
	case ARM_R1:   arm.reg[ 1] = val;	   break;
	case ARM_R2:   arm.reg[ 2] = val;	   break;
	case ARM_R3:   arm.reg[ 3] = val;	   break;
	case ARM_R4:   arm.reg[ 4] = val;	   break;
	case ARM_R5:   arm.reg[ 5] = val;	   break;
	case ARM_R6:   arm.reg[ 6] = val;	   break;
	case ARM_R7:   arm.reg[ 7] = val;	   break;
	case ARM_R8:   arm.reg[ 8] = val;	   break;
	case ARM_R9:   arm.reg[ 9] = val;	   break;
	case ARM_R10:  arm.reg[10] = val;	   break;
	case ARM_R11:  arm.reg[11] = val;	   break;
	case ARM_R12:  arm.reg[12] = val;	   break;
	case ARM_R13:  arm.reg[13] = val;	   break;
	case ARM_R14:  arm.reg[14] = val;	   break;
	case ARM_R15:  arm.reg[15] = val&AMASK;break;
	case ARM_FR8:  arm.reg_firq[0] = val;  break;
	case ARM_FR9:  arm.reg_firq[1] = val;  break;
	case ARM_FR10: arm.reg_firq[2] = val;  break;
	case ARM_FR11: arm.reg_firq[3] = val;  break;
	case ARM_FR12: arm.reg_firq[4] = val;  break;
	case ARM_FR13: arm.reg_firq[5] = val;  break;
	case ARM_FR14: arm.reg_firq[6] = val;  break;
	case ARM_IR13: arm.reg_irq[0] = val;   break;
	case ARM_IR14: arm.reg_irq[1] = val;   break;
	case ARM_SR13: arm.reg_svc[0] = val;   break;
	case ARM_SR14: arm.reg_svc[1] = val;   break;
	}
}

void arm_set_nmi_line(int state)
{
	/* not yet */
}

void arm_set_irq_line(int irqline, int state)
{
	/* not yet */
}

void arm_set_irq_callback(int (*callback)(int irqline))
{
	/* not yet */
}

const char *arm_info(void *context, int regnum)
{
	switch( regnum )
	{
	case CPU_INFO_NAME: 		return "ARM";
	case CPU_INFO_FAMILY:		return "Acorn Risc Machine";
	case CPU_INFO_VERSION:		return "1.0";
	case CPU_INFO_FILE: 		return __FILE__;
	case CPU_INFO_CREDITS:		return "Copyright 2000 hjb";
	}

	return "";
}

unsigned arm_dasm(char *buffer, unsigned pc)
{
	change_pc26lew(pc);
	sprintf(buffer, "$%08x", ARM_RDMEM_32(pc));
	return 4;
}

