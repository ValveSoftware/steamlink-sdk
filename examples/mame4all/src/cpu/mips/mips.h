#ifndef _MIPS_H
#define _MIPS_H

#include "osd_cpu.h"

enum
{
	MIPS_PC = 1, MIPS_OC,
	MIPS_HI, MIPS_LO,
	MIPS_R0, MIPS_R1,
	MIPS_R2, MIPS_R3,
	MIPS_R4, MIPS_R5,
	MIPS_R6, MIPS_R7,
	MIPS_R8, MIPS_R9,
	MIPS_R10, MIPS_R11,
	MIPS_R12, MIPS_R13,
	MIPS_R14, MIPS_R15,
	MIPS_R16, MIPS_R17,
	MIPS_R18, MIPS_R19,
	MIPS_R20, MIPS_R21,
	MIPS_R22, MIPS_R23,
	MIPS_R24, MIPS_R25,
	MIPS_R26, MIPS_R27,
	MIPS_R28, MIPS_R29,
	MIPS_R30, MIPS_R31,
	MIPS_CP0R0, MIPS_CP0R1,
	MIPS_CP0R2, MIPS_CP0R3,
	MIPS_CP0R4, MIPS_CP0R5,
	MIPS_CP0R6, MIPS_CP0R7,
	MIPS_CP0R8, MIPS_CP0R9,
	MIPS_CP0R10, MIPS_CP0R11,
	MIPS_CP0R12, MIPS_CP0R13,
	MIPS_CP0R14, MIPS_CP0R15,
	MIPS_CP0R16, MIPS_CP0R17,
	MIPS_CP0R18, MIPS_CP0R19,
	MIPS_CP0R20, MIPS_CP0R21,
	MIPS_CP0R22, MIPS_CP0R23,
	MIPS_CP0R24, MIPS_CP0R25,
	MIPS_CP0R26, MIPS_CP0R27,
	MIPS_CP0R28, MIPS_CP0R29,
	MIPS_CP0R30, MIPS_CP0R31
};

extern int mips_ICount;

#define MIPS_INT_NONE	( 0 )
#define MIPS_INT1	( 1 )
#define MIPS_INT2	( 2 )
#define MIPS_INT3	( 4 )
#define MIPS_INT4	( 8 )
#define MIPS_INT5	( 16 )
#define MIPS_INT6	( 32 )
#define MIPS_INT7	( 64 )
#define MIPS_INT8	( 128 )

#define MIPS_BYTE_EXTEND( a ) ( (INT32)(INT8)a )
#define MIPS_WORD_EXTEND( a ) ( (INT32)(INT16)a )

#define INS_OP( op ) ( ( op >> 26 ) & 63 )
#define INS_RS( op ) ( ( op >> 21 ) & 31 )
#define INS_RT( op ) ( ( op >> 16 ) & 31 )
#define INS_IMMEDIATE( op ) ( op & 0xffff )
#define INS_TARGET( op ) ( op & 0x3ffffff )
#define INS_RD( op ) ( ( op >> 11 ) & 31 )
#define INS_SHAMT( op ) ( ( op >> 6 ) & 31 )
#define INS_FUNCT( op ) ( op & 63 )
#define INS_CODE( op ) ( ( op >> 6 ) & 0xfffff )
#define INS_CO( op ) ( ( op >> 25 ) & 1 )
#define INS_COFUN( op ) ( op & 0x1ffffff )
#define INS_CF( op ) ( op & 63 )

#define GTE_OP( op ) ( op & 0x1f01bff ) 
#define GTE_SF( op ) ( ( op >> 19 ) & 1 )
#define GTE_MX( op ) ( ( op >> 17 ) & 3 )
#define GTE_V( op ) ( ( op >> 15 ) & 3 )
#define GTE_CV( op ) ( ( op >> 13 ) & 3 )
#define GTE_LM( op ) ( ( op >> 10 ) & 1 )

#define OP_SPECIAL ( 0 )
#define OP_REGIMM ( 1 )
#define OP_J ( 2 )
#define OP_JAL ( 3 )
#define OP_BEQ ( 4 )
#define OP_BNE ( 5 )
#define OP_BLEZ ( 6 )
#define OP_BGTZ ( 7 )
#define OP_ADDI ( 8 )
#define OP_ADDIU ( 9 )
#define OP_SLTI ( 10 )
#define OP_SLTIU ( 11 )
#define OP_ANDI ( 12 )
#define OP_ORI ( 13 )
#define OP_XORI ( 14 )
#define OP_LUI ( 15 )
#define OP_COP0 ( 16 )
#define OP_COP1 ( 17 )
#define OP_COP2 ( 18 )
#define OP_LB ( 32 )
#define OP_LH ( 33 )
#define OP_LWL ( 34 )
#define OP_LW ( 35 )
#define OP_LBU ( 36 )
#define OP_LHU ( 37 )
#define OP_LWR ( 38 )
#define OP_SB ( 40 )
#define OP_SH ( 41 )
#define OP_SWL ( 42 )
#define OP_SW ( 43 )
#define OP_SWR ( 46 )
#define OP_LWC1 ( 49 )
#define OP_LWC2 ( 50 )
#define OP_SWC1 ( 57 )
#define OP_SWC2 ( 58 )

/* OP_SPECIAL */
#define FUNCT_SLL ( 0 )
#define FUNCT_SRL ( 2 )
#define FUNCT_SRA ( 3 )
#define FUNCT_SLLV ( 4 )
#define FUNCT_SRLV ( 6 )
#define FUNCT_SRAV ( 7 )
#define FUNCT_JR ( 8 )
#define FUNCT_JALR ( 9 )
#define FUNCT_SYSCALL ( 12 )
#define FUNCT_BREAK ( 13 )
#define FUNCT_MFHI ( 16 )
#define FUNCT_MTHI ( 17 )
#define FUNCT_MFLO ( 18 )
#define FUNCT_MTLO ( 19 )
#define FUNCT_MULT ( 24 )
#define FUNCT_MULTU ( 25 )
#define FUNCT_DIV ( 26 )
#define FUNCT_DIVU ( 27 )
#define FUNCT_ADD ( 32 )
#define FUNCT_ADDU ( 33 )
#define FUNCT_SUB ( 34 )
#define FUNCT_SUBU ( 35 )
#define FUNCT_AND ( 36 )
#define FUNCT_OR ( 37 )
#define FUNCT_XOR ( 38 )
#define FUNCT_NOR ( 39 )
#define FUNCT_SLT ( 42 )
#define FUNCT_SLTU ( 43 )

/* OP_REGIMM */
#define RT_BLTZ ( 0 )
#define RT_BGEZ ( 1 )
#define RT_BLTZAL ( 16 )
#define RT_BGEZAL ( 17 )

/* OP_COP0/OP_COP1/OP_COP2 */
#define RS_MFC ( 0 )
#define RS_CFC ( 2 )
#define RS_MTC ( 4 )
#define RS_CTC ( 6 )
#define RS_BC ( 8 )

/* RS_BC */
#define RT_BCF ( 0 )
#define RT_BCT ( 1 )

/* OP_COP0 */
#define CF_RFE ( 16 )

#ifdef LSB_FIRST
#define READ_LONG(a)          (*(UINT32 *)(a))
#else
#define READ_LONG(a)		  (READ_WORD(a+2)<<16)|READ_WORD(a)
#endif

#define cpu_readop32(A)     READ_LONG(&OP_ROM[A])
extern void mips_stop( void );

extern void mips_reset(void *param);
extern void mips_exit(void);
extern int mips_execute(int cycles);
extern unsigned mips_get_context(void *dst);
extern void mips_set_context(void *src);
extern unsigned mips_get_pc(void);
extern unsigned mips_get_op_counter( void );
extern void mips_set_pc(unsigned val);
extern unsigned mips_get_sp(void);
extern void mips_set_sp(unsigned val);
extern unsigned mips_get_reg(int regnum);
extern void mips_set_reg(int regnum, unsigned val);
extern void mips_set_nmi_line(int linestate);
extern void mips_set_irq_line(int irqline, int linestate);
extern void mips_set_irq_callback(int (*callback)(int irqline));
extern const char *mips_info(void *context, int regnum);
extern unsigned mips_dasm(char *buffer, unsigned pc);

#endif
