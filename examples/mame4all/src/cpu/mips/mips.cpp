/*
 * MIPS emulator for the MAME project written by smf
 *
 * Work in progress, there is still much to do.
 *
 */

#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "mips.h"

#define EXC_INT 	0
#define EXC_TLBL	2
#define EXC_TLBS	3
#define EXC_ADEL	4
#define EXC_ADES	5
#define EXC_SYS 	8
#define EXC_BP		9
#define EXC_RI		10
#define EXC_OVF 	12

#define CP0_RANDOM		1
#define CP0_BADVADDR	8
#define CP0_SR			12
#define CP0_CAUSE		13
#define CP0_EPC 		14

#define SR_IEC ( 1L << 0 )
#define SR_KUC ( 1L << 1 )
#define SR_ISC ( 1L << 16 )
#define SR_SWC ( 1L << 17 )
#define SR_TS  ( 1L << 21 )
#define SR_BEV ( 1L << 22 )
#define SR_RE ( 1L << 25 )

#define CAUSE_BD ( 0x80000000 )

typedef struct
{
	UINT32 op;
	UINT32 pc;
	UINT32 nextop;
	UINT32 nextpc;
	UINT32 irq;
	UINT32 hi;
	UINT32 lo;
	UINT32 r[ 32 ];
	UINT32 cp0r[ 32 ];
} mips_cpu_context;

/*
 * 'mips' is a reserved word on mips machines.
 * renamed context and added some macros to access the contents
 */
mips_cpu_context mcc;
UINT32 context_pc = 0;

#define THISOP	mcc.op
#define THISPC	mcc.pc
#define NEXTOP	mcc.nextop
#define NEXTPC	mcc.nextpc
#define HI		mcc.hi
#define LO		mcc.lo
#define R(n)	mcc.r[n]
#define CP0R(n) mcc.cp0r[n]

#define RS		R(INS_RS(mcc.op))
#define RT		R(INS_RT(mcc.op))
#define RD		R(INS_RD(mcc.op))
#define SHAMT	INS_SHAMT(mcc.op)

int mips_ICount = 0;

static void mips_exception( int exception )
{
	CP0R( CP0_CAUSE ) = ( CP0R( CP0_CAUSE ) & 0xffffff83 ) | ( exception << 2 );
	if( THISPC != NEXTPC - 4 )
	{
		CP0R( CP0_EPC ) = THISPC - 4;
		CP0R( CP0_CAUSE ) |= CAUSE_BD;
	}
	else
	{
		CP0R( CP0_EPC ) = THISPC;
		CP0R( CP0_CAUSE ) &= ~CAUSE_BD;
	}
	CP0R( CP0_SR ) = ( CP0R( CP0_SR ) & 0xffffffc0 ) | ( ( CP0R( CP0_SR ) << 2 ) & 0x3f );
	if( CP0R( CP0_SR ) & SR_BEV )
	{
		mips_set_pc( 0xbfc00180 );
	}
	else
	{
		mips_set_pc( 0x80000080 );
	}
}

void mips_stop( void )
{
}

INLINE void mips_set_nextpc( UINT32 adr )
{
	THISPC = NEXTPC;
	THISOP = mcc.nextop;
	if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 3 ) ) != 0 )
	{
		CP0R( CP0_BADVADDR ) = adr;
		mips_exception( EXC_ADEL );
	}
	else
	{
		change_pc32( adr );
		NEXTPC = adr;
		mcc.nextop = cpu_readop32( NEXTPC );
	}
}

INLINE void mips_advance_pc( void )
{
	THISPC = NEXTPC;
	THISOP = mcc.nextop;
	NEXTPC += 4;
	mcc.nextop = cpu_readop32( NEXTPC );
}

void mips_reset( void *param )
{
	CP0R( CP0_RANDOM ) = 63;
	CP0R( CP0_SR ) = ( CP0R( CP0_SR ) & ~( SR_TS | SR_SWC | SR_KUC | SR_IEC ) ) | SR_BEV;
	mips_set_pc( 0xbfc00000 );
}

void mips_exit( void )
{
}

int mips_execute( int cycles )
{
	mips_ICount = cycles;
	do
	{
		THISOP = cpu_readop32(THISPC);
		THISPC += 4;

		switch( INS_OP( THISOP ) )
		{
		case OP_SPECIAL:
			switch( INS_FUNCT( THISOP ) )
			{
			case FUNCT_SLL:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RT << SHAMT;
				}
				mips_advance_pc();
				break;
			case FUNCT_SRL:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RT >> SHAMT;
				}
				mips_advance_pc();
				break;
			case FUNCT_SRA:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = (INT32) RT >> SHAMT;
				}
				mips_advance_pc();
				break;
			case FUNCT_SLLV:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RT << ( RS & 31 );
				}
				mips_advance_pc();
				break;
			case FUNCT_SRLV:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RT >> ( RS & 31 );
				}
				mips_advance_pc();
				break;
			case FUNCT_SRAV:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = (INT32) RT >> ( RS & 31 );
				}
				mips_advance_pc();
				break;
			case FUNCT_JR:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					mips_set_nextpc( RS );
				}
				break;
			case FUNCT_JALR:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = NEXTPC + 4;
				}
				mips_set_nextpc( RS );
				break;
			case FUNCT_SYSCALL:
				mips_exception( EXC_SYS );
				break;
			case FUNCT_BREAK:
				mips_exception( EXC_BP );
				break;
			case FUNCT_MFHI:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = HI;
				}
				mips_advance_pc();
				break;
			case FUNCT_MTHI:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					HI = RS;
				}
				mips_advance_pc();
				break;
			case FUNCT_MFLO:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = LO;
				}
				mips_advance_pc();
				break;
			case FUNCT_MTLO:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					LO = RS;
				}
				mips_advance_pc();
				break;
			case FUNCT_MULT:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					INT64 res;
					res = MUL_64_32_32( (INT32)RS, (INT32)RT );
					LO = LO32_32_64( res );
					HI = HI32_32_64( res );
					mips_advance_pc();
				}
				break;
			case FUNCT_MULTU:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					UINT64 res;
					res = MUL_U64_U32_U32( (INT32)RS, (INT32)RT );
					LO = LO32_U32_U64( res );
					HI = HI32_U32_U64( res );
					mips_advance_pc();
				}
				break;
			case FUNCT_DIV:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					LO = (INT32)RS / (INT32)RT;
					HI = (INT32)RS % (INT32)RT;
					mips_advance_pc();
				}
				break;
			case FUNCT_DIVU:
				if( INS_RD( THISOP ) != 0 )
				{
					mips_exception( EXC_RI );
				}
				else
				{
					LO = RS / RT;
					HI = RS % RT;
					mips_advance_pc();
				}
				break;
			case FUNCT_ADD:
				{
					UINT32 res;
					res = RS + RT;
					if( (INT32)( ( RS & RT & ~res ) | ( ~RS & ~RT & res ) ) < 0 )
					{
						mips_exception( EXC_OVF );
					}
					else
					{
						if( INS_RD( THISOP ) != 0 )
						{
							RD = res;
						}
						mips_advance_pc();
					}
				}
				break;
			case FUNCT_ADDU:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS + RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_SUB:
				{
					UINT32 res;
					res = RS - RT;
					if( (INT32)( ( RS & ~RT & ~res ) | ( ~RS & RT & res ) ) < 0 )
					{
						mips_exception( EXC_OVF );
					}
					else
					{
						if( INS_RD( THISOP ) != 0 )
						{
							RD = res;
						}
						mips_advance_pc();
					}
				}
				break;
			case FUNCT_SUBU:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS - RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_AND:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS & RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_OR:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS | RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_XOR:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS ^ RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_NOR:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = ~( RS | RT );
				}
				mips_advance_pc();
				break;
			case FUNCT_SLT:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = (INT32)RS < (INT32)RT;
				}
				mips_advance_pc();
				break;
			case FUNCT_SLTU:
				if( INS_RD( THISOP ) != 0 )
				{
					RD = RS < RT;
				}
				mips_advance_pc();
				break;
			default:
				mips_exception( EXC_RI );
				break;
			}
			break;
		case OP_REGIMM:
			switch( INS_RT( THISOP ) )
			{
			case RT_BLTZ:
				if( (INT32)RS < 0 )
				{
					mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
				}
				else
				{
					mips_advance_pc();
				}
				break;
			case RT_BGEZ:
				if( (INT32)RS >= 0 )
				{
					mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
				}
				else
				{
					mips_advance_pc();
				}
				break;
			case RT_BLTZAL:
				R( 31 ) = NEXTPC + 4;
				if( (INT32)RS < 0 )
				{
					mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
				}
				else
				{
					mips_advance_pc();
				}
				break;
			case RT_BGEZAL:
				R( 31 ) = NEXTPC + 4;
				if( (INT32)RS >= 0 )
				{
					mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
				}
				else
				{
					mips_advance_pc();
				}
				break;
			}
			break;
		case OP_J:
			mips_set_nextpc( ( NEXTPC & 0xF0000000 ) + ( INS_TARGET( THISOP ) << 2 ) );
			break;
		case OP_JAL:
			R( 31 ) = NEXTPC + 4;
			mips_set_nextpc( ( NEXTPC & 0xF0000000 ) + ( INS_TARGET( THISOP ) << 2 ) );
			break;
		case OP_BEQ:
			if( RS == RT )
			{
				mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case OP_BNE:
			if( RS != RT )
			{
				mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case OP_BLEZ:
			if( INS_RT( THISOP ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else if( (INT32)RS <= 0 )
			{
				mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case OP_BGTZ:
			if( INS_RT( THISOP ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else if( (INT32)RS > 0 )
			{
				mips_set_nextpc( NEXTPC + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case OP_ADDI:
			{
				UINT32 res,imm;
				imm = MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				res = RS + imm;
				if( (INT32)( ( RS & imm & ~res ) | ( ~RS & ~imm & res ) ) < 0 )
				{
					mips_exception( EXC_OVF );
				}
				else
				{
					if( INS_RT( THISOP ) != 0 )
					{
						RT = res;
					}
					mips_advance_pc();
				}
			}
			break;
		case OP_ADDIU:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
			}
			mips_advance_pc();
			break;
		case OP_SLTI:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = (INT32)RS < MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
			}
			mips_advance_pc();
			break;
		case OP_SLTIU:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = RS < (unsigned)MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
			}
			mips_advance_pc();
			break;
		case OP_ANDI:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = RS & INS_IMMEDIATE( THISOP );
			}
			mips_advance_pc();
			break;
		case OP_ORI:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = RS | INS_IMMEDIATE( THISOP );
			}
			mips_advance_pc();
			break;
		case OP_XORI:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = RS ^ INS_IMMEDIATE( THISOP );
			}
			mips_advance_pc();
			break;
		case OP_LUI:
			if( INS_RT( THISOP ) != 0 )
			{
				RT = INS_IMMEDIATE( THISOP ) << 16;
			}
			mips_advance_pc();
			break;
		case OP_COP0:
			switch( INS_RS( THISOP ) )
			{
			case RS_MFC:
				RT = CP0R( INS_RD( THISOP ) );
				mips_advance_pc();
				break;
			case RS_CFC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_MTC:
				CP0R( INS_RD( THISOP ) ) = RT;
				mips_advance_pc();
				break;
			case RS_CTC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_BC:
				mips_stop();
				switch( INS_RT( THISOP ) )
				{
				case RT_BCF:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				switch( INS_CO( THISOP ) )
				{
				case 1:
					switch( INS_CF( THISOP ) )
					{
					case CF_RFE:
						CP0R( CP0_SR ) = ( CP0R( CP0_SR ) & 0xffffffe0 ) | ( ( CP0R( CP0_SR ) >> 2 ) & 0x1f );
						mips_advance_pc();
						break;
					default:
						/* not implemented */
						mips_stop();
						mips_advance_pc();
						break;
					}
					break;
				default:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
			break;
		case OP_COP1:
			switch( INS_RS( THISOP ) )
			{
			case RS_MFC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CFC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_MTC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CTC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_BC:
				mips_stop();
				switch( INS_RT( THISOP ) )
				{
				case RT_BCF:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				mips_stop();
				switch( INS_CO( THISOP ) )
				{
				case 1:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
			break;
		case OP_COP2:
			switch( INS_RS( THISOP ) )
			{
			case RS_MFC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CFC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_MTC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CTC:
				/* not implemented */
				mips_stop();
				mips_advance_pc();
				break;
			case RS_BC:
				mips_stop();
				switch( INS_RT( THISOP ) )
				{
				case RT_BCF:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				mips_stop();
				switch( INS_CO( THISOP ) )
				{
				case 1:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* not implemented */
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
			break;
		case OP_LB:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADEL );
				}
				else
				{
					RT = MIPS_BYTE_EXTEND( cpu_readmem32lew( adr ) );
					mips_advance_pc();
				}
			}
			break;
		case OP_LH:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 1 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADEL );
				}
				else
				{
					RT = MIPS_WORD_EXTEND( cpu_readmem32lew_word( adr ) );
					mips_advance_pc();
				}
			}
			break;
		case OP_LWL:
			mips_stop();
			/* not implemented */
			mips_advance_pc();
			break;
		case OP_LW:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 3 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADEL );
				}
				else
				{
					RT = cpu_readmem32lew_dword( adr );
					mips_advance_pc();
				}
			}
			break;
		case OP_LBU:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADEL );
				}
				else
				{
					RT = cpu_readmem32lew( adr );
					mips_advance_pc();
				}
			}
			break;
		case OP_LHU:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 1 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADEL );
				}
				else
				{
					RT = cpu_readmem32lew_word( adr );
					mips_advance_pc();
				}
			}
			break;
		case OP_LWR:
			mips_stop();
			/* not implemented */
			mips_advance_pc();
			break;
		case OP_SB:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADES );
				}
				else
				{
					cpu_writemem32lew( adr, RT );
					mips_advance_pc();
				}
			}
			break;
		case OP_SH:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 1 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADES );
				}
				else
				{
					cpu_writemem32lew_word( adr, RT );
					mips_advance_pc();
				}
			}
			break;
		case OP_SWL:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		case OP_SW:
			if( CP0R( CP0_SR ) & SR_RE || CP0R( CP0_SR ) & SR_ISC )
			{
				/* not implemented */
				mips_advance_pc();
			}
			else
			{
				UINT32 adr;
				adr = RS + MIPS_WORD_EXTEND( INS_IMMEDIATE( THISOP ) );
				if( ( adr & ( ( ( CP0R( CP0_SR ) & 0x02 ) << 31 ) | 3 ) ) != 0 )
				{
					CP0R( CP0_BADVADDR ) = adr;
					mips_exception( EXC_ADES );
				}
				else
				{
					cpu_writemem32lew_dword( adr, RT );
					mips_advance_pc();
				}
			}
			break;
		case OP_SWR:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		case OP_LWC1:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		case OP_LWC2:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		case OP_SWC1:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		case OP_SWC2:
			/* not implemented */
			mips_stop();
			mips_advance_pc();
			break;
		default:
			mips_exception( EXC_RI );
			break;
		}
		mips_ICount--;
	} while( mips_ICount > 0 );

	return cycles - mips_ICount;
}

unsigned mips_get_context( void *dst )
{
	if( dst )
	{
		*(mips_cpu_context *)dst = mcc;
	}
	return sizeof( mips_cpu_context );
}

void mips_set_context( void *src )
{
	if( src )
	{
		mcc = *(mips_cpu_context *)src;
		change_pc32( NEXTPC );
	}
}

unsigned mips_get_pc( void )
{
	return THISPC;
}

unsigned mips_get_op_counter( void )
{
	return NEXTPC;
}

void mips_set_pc( unsigned val )
{
	mips_set_nextpc( val );
	mips_advance_pc();
}

unsigned mips_get_sp(void)
{
	/* because there is no hardware stack and the pipeline causes the cpu to execute the
	instruction after a subroutine call before the subroutine there is little chance of
	cmd_step_over() in mamedbg.c working. */
	return 0;
}

void mips_set_sp( unsigned val )
{
	/* no hardware stack */
}

unsigned mips_get_reg( int regnum )
{
    switch( regnum )
    {
	case MIPS_PC:		return THISPC;
	case MIPS_OC:		return NEXTPC;
	case MIPS_HI:		return HI;
	case MIPS_LO:		return LO;
	case MIPS_R0:		return R( 0);
	case MIPS_R1:		return R( 1);
	case MIPS_R2:		return R( 2);
	case MIPS_R3:		return R( 3);
	case MIPS_R4:		return R( 4);
	case MIPS_R5:		return R( 5);
	case MIPS_R6:		return R( 6);
	case MIPS_R7:		return R( 7);
	case MIPS_R8:		return R( 8);
	case MIPS_R9:		return R( 9);
	case MIPS_R10:		return R(10);
	case MIPS_R11:		return R(11);
	case MIPS_R12:		return R(12);
	case MIPS_R13:		return R(13);
	case MIPS_R14:		return R(14);
	case MIPS_R15:		return R(15);
	case MIPS_R16:		return R(16);
	case MIPS_R17:		return R(17);
	case MIPS_R18:		return R(18);
	case MIPS_R19:		return R(19);
	case MIPS_R20:		return R(20);
	case MIPS_R21:		return R(21);
	case MIPS_R22:		return R(22);
	case MIPS_R23:		return R(23);
	case MIPS_R24:		return R(24);
	case MIPS_R25:		return R(25);
	case MIPS_R26:		return R(26);
	case MIPS_R27:		return R(27);
	case MIPS_R28:		return R(28);
	case MIPS_R29:		return R(29);
	case MIPS_R30:		return R(30);
	case MIPS_R31:		return R(31);
	case MIPS_CP0R0:	return CP0R( 0);
	case MIPS_CP0R1:	return CP0R( 1);
	case MIPS_CP0R2:	return CP0R( 2);
	case MIPS_CP0R3:	return CP0R( 3);
	case MIPS_CP0R4:	return CP0R( 4);
	case MIPS_CP0R5:	return CP0R( 5);
	case MIPS_CP0R6:	return CP0R( 6);
	case MIPS_CP0R7:	return CP0R( 7);
	case MIPS_CP0R8:	return CP0R( 8);
	case MIPS_CP0R9:	return CP0R( 9);
	case MIPS_CP0R10:	return CP0R(10);
	case MIPS_CP0R11:	return CP0R(11);
	case MIPS_CP0R12:	return CP0R(12);
	case MIPS_CP0R13:	return CP0R(13);
	case MIPS_CP0R14:	return CP0R(14);
	case MIPS_CP0R15:	return CP0R(15);
	case MIPS_CP0R16:	return CP0R(16);
	case MIPS_CP0R17:	return CP0R(17);
	case MIPS_CP0R18:	return CP0R(18);
	case MIPS_CP0R19:	return CP0R(19);
	case MIPS_CP0R20:	return CP0R(20);
	case MIPS_CP0R21:	return CP0R(21);
	case MIPS_CP0R22:	return CP0R(22);
	case MIPS_CP0R23:	return CP0R(23);
	case MIPS_CP0R24:	return CP0R(24);
	case MIPS_CP0R25:	return CP0R(25);
	case MIPS_CP0R26:	return CP0R(26);
	case MIPS_CP0R27:	return CP0R(27);
	case MIPS_CP0R28:	return CP0R(28);
	case MIPS_CP0R29:	return CP0R(29);
	case MIPS_CP0R30:	return CP0R(30);
	case MIPS_CP0R31:	return CP0R(31);
    }
    return 0;
}

void mips_set_reg( int regnum, unsigned val )
{
	switch( regnum )
	{
	case MIPS_PC:		mips_set_pc( val ); 	break;
	case MIPS_OC:		mips_set_nextpc( val );	break;
	case MIPS_HI:		HI = val;				break;
	case MIPS_LO:		LO = val;				break;
	case MIPS_R0:		R( 0) = val;			break;
	case MIPS_R1:		R( 1) = val;			break;
	case MIPS_R2:		R( 2) = val;			break;
	case MIPS_R3:		R( 3) = val;			break;
	case MIPS_R4:		R( 4) = val;			break;
	case MIPS_R5:		R( 5) = val;			break;
	case MIPS_R6:		R( 6) = val;			break;
	case MIPS_R7:		R( 7) = val;			break;
	case MIPS_R8:		R( 8) = val;			break;
	case MIPS_R9:		R( 9) = val;			break;
	case MIPS_R10:		R(10) = val;			break;
	case MIPS_R11:		R(11) = val;			break;
	case MIPS_R12:		R(12) = val;			break;
	case MIPS_R13:		R(13) = val;			break;
	case MIPS_R14:		R(14) = val;			break;
	case MIPS_R15:		R(15) = val;			break;
	case MIPS_R16:		R(16) = val;			break;
	case MIPS_R17:		R(17) = val;			break;
	case MIPS_R18:		R(18) = val;			break;
	case MIPS_R19:		R(19) = val;			break;
	case MIPS_R20:		R(20) = val;			break;
	case MIPS_R21:		R(21) = val;			break;
	case MIPS_R22:		R(22) = val;			break;
	case MIPS_R23:		R(23) = val;			break;
	case MIPS_R24:		R(24) = val;			break;
	case MIPS_R25:		R(25) = val;			break;
	case MIPS_R26:		R(26) = val;			break;
	case MIPS_R27:		R(27) = val;			break;
	case MIPS_R28:		R(28) = val;			break;
	case MIPS_R29:		R(29) = val;			break;
	case MIPS_R30:		R(30) = val;			break;
	case MIPS_R31:		R(31) = val;			break;
	case MIPS_CP0R0:	CP0R( 0) = val; 		break;
	case MIPS_CP0R1:	CP0R( 1) = val; 		break;
	case MIPS_CP0R2:	CP0R( 2) = val; 		break;
	case MIPS_CP0R3:	CP0R( 3) = val; 		break;
	case MIPS_CP0R4:	CP0R( 4) = val; 		break;
	case MIPS_CP0R5:	CP0R( 5) = val; 		break;
	case MIPS_CP0R6:	CP0R( 6) = val; 		break;
	case MIPS_CP0R7:	CP0R( 7) = val; 		break;
	case MIPS_CP0R8:	CP0R( 8) = val; 		break;
	case MIPS_CP0R9:	CP0R( 9) = val; 		break;
	case MIPS_CP0R10:	CP0R(10) = val; 		break;
	case MIPS_CP0R11:	CP0R(11) = val; 		break;
	case MIPS_CP0R12:	CP0R(12) = val; 		break;
	case MIPS_CP0R13:	CP0R(13) = val; 		break;
	case MIPS_CP0R14:	CP0R(14) = val; 		break;
	case MIPS_CP0R15:	CP0R(15) = val; 		break;
	case MIPS_CP0R16:	CP0R(16) = val; 		break;
	case MIPS_CP0R17:	CP0R(17) = val; 		break;
	case MIPS_CP0R18:	CP0R(18) = val; 		break;
	case MIPS_CP0R19:	CP0R(19) = val; 		break;
	case MIPS_CP0R20:	CP0R(20) = val; 		break;
	case MIPS_CP0R21:	CP0R(21) = val; 		break;
	case MIPS_CP0R22:	CP0R(22) = val; 		break;
	case MIPS_CP0R23:	CP0R(23) = val; 		break;
	case MIPS_CP0R24:	CP0R(24) = val; 		break;
	case MIPS_CP0R25:	CP0R(25) = val; 		break;
	case MIPS_CP0R26:	CP0R(26) = val; 		break;
	case MIPS_CP0R27:	CP0R(27) = val; 		break;
	case MIPS_CP0R28:	CP0R(28) = val; 		break;
	case MIPS_CP0R29:	CP0R(29) = val; 		break;
	case MIPS_CP0R30:	CP0R(30) = val; 		break;
	case MIPS_CP0R31:	CP0R(31) = val; 		break;
	}
}

void mips_set_nmi_line( int state )
{
	switch( state )
	{
		case CLEAR_LINE:
			return;
		case ASSERT_LINE:
			return;
		default:
			return;
	}
}

void mips_set_irq_line( int irqline, int state )
{
	switch( state )
	{
		case CLEAR_LINE:
			return;
		case ASSERT_LINE:
			return;
		default:
			return;
	}
}

void mips_set_irq_callback(int (*callback)(int irqline))
{
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/

const char *mips_info( void *context, int regnum )
{
	switch( regnum )
	{
	case CPU_INFO_NAME:			return "PSX";
	case CPU_INFO_FAMILY:		return "MIPS";
	case CPU_INFO_VERSION:		return "1.0";
	case CPU_INFO_FILE:			return __FILE__;
	case CPU_INFO_CREDITS:		return "Copyright 2000 smf";
	}
	return "";
}

unsigned mips_dasm( char *buffer, UINT32 pc )
{
	unsigned ret;
	change_pc32( pc );
	sprintf( buffer, "$%08x", cpu_readop32( pc ) );
	ret = 4;
	change_pc32( NEXTPC );
	return ret;
}
