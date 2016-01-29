/*******************************************************
 *
 *      Portable (hopefully ;-) 8085A emulator
 *
 *      Written by J. Buchmueller for use with MAME
 *
 *		Partially based on Z80Em by Marcel De Kogel
 *
 *      CPU related macros
 *
 *******************************************************/

#define SF              0x80
#define ZF              0x40
#define YF              0x20
#define HF              0x10
#define XF              0x08
#define VF              0x04
#define NF              0x02
#define CF              0x01

#define IM_SID          0x80
#define IM_SOD          0x40
#define IM_IEN          0x20
#define IM_TRAP         0x10
#define IM_INTR         0x08
#define IM_RST75        0x04
#define IM_RST65        0x02
#define IM_RST55        0x01

#define ADDR_TRAP       0x0024
#define ADDR_RST55      0x002c
#define ADDR_RST65      0x0034
#define ADDR_RST75      0x003c
#define ADDR_INTR       0x0038

#define M_INR(R) ++R; I.AF.b.l=(I.AF.b.l&CF)|ZS[R]|((R==0x80)?VF:0)|((R&0x0F)?0:HF)
#define M_DCR(R) I.AF.b.l=(I.AF.b.l&CF)|NF|((R==0x80)?VF:0)|((R&0x0F)?0:HF); I.AF.b.l|=ZS[--R]
#define M_MVI(R) R=ARG()

#define M_ANA(R) I.AF.b.h&=R; I.AF.b.l=ZSP[I.AF.b.h]|HF
#define M_ORA(R) I.AF.b.h|=R; I.AF.b.l=ZSP[I.AF.b.h]
#define M_XRA(R) I.AF.b.h^=R; I.AF.b.l=ZSP[I.AF.b.h]

#define M_RLC { 												\
	I.AF.b.h = (I.AF.b.h << 1) | (I.AF.b.h >> 7);				\
	I.AF.b.l = (I.AF.b.l & ~(HF+NF+CF)) | (I.AF.b.h & CF);		\
}

#define M_RRC { 												\
	I.AF.b.l = (I.AF.b.l & ~(HF+NF+CF)) | (I.AF.b.h & CF);		\
	I.AF.b.h = (I.AF.b.h >> 1) | (I.AF.b.h << 7);				\
}

#define M_RAL { 												\
	int c = I.AF.b.l&CF;										\
	I.AF.b.l = (I.AF.b.l & ~(HF+NF+CF)) | (I.AF.b.h >> 7);		\
	I.AF.b.h = (I.AF.b.h << 1) | c; 							\
}

#define M_RAR { 												\
	int c = (I.AF.b.l&CF) << 7; 								\
	I.AF.b.l = (I.AF.b.l & ~(HF+NF+CF)) | (I.AF.b.h & CF);		\
	I.AF.b.h = (I.AF.b.h >> 1) | c; 							\
}

#ifdef X86_ASM
#define M_ADD(R)												\
 asm (															\
 " addb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb %%ah,%%al        \n"                                     \
 :"=g" (I.AF.b.h), "=a" (I.AF.b.l)                              \
 :"r" (R), "0" (I.AF.b.h)                                       \
 )
#else
#define M_ADD(R) {												\
int q = I.AF.b.h+R; 											\
	I.AF.b.l=ZS[q&255]|((q>>8)&CF)| 							\
		((I.AF.b.h^q^R)&HF)|									\
		(((R^I.AF.b.h^SF)&(R^q)&SF)>>5);						\
	I.AF.b.h=q; 												\
}
#endif

#ifdef X86_ASM
#define M_ADC(R)												\
 asm (															\
 " shrb $1,%%al         \n"                                     \
 " adcb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (I.AF.b.h), "=a" (I.AF.b.l)                              \
 :"r" (R), "a" (I.AF.b.l), "0" (I.AF.b.h)                       \
 )
#else
#define M_ADC(R) {												\
	int q = I.AF.b.h+R+(I.AF.b.l&CF);							\
	I.AF.b.l=ZS[q&255]|((q>>8)&CF)| 							\
		((I.AF.b.h^q^R)&HF)|									\
		(((R^I.AF.b.h^SF)&(R^q)&SF)>>5);						\
	I.AF.b.h=q; 												\
}
#endif

#ifdef X86_ASM
#define M_SUB(R)												\
 asm (															\
 " subb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb $2,%%al          \n" /* set N flag */                    \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (I.AF.b.h), "=a" (I.AF.b.l)                              \
 :"r" (R), "0" (I.AF.b.h)                                       \
 )
#else
#define M_SUB(R) {												\
	int q = I.AF.b.h-R; 										\
	I.AF.b.l=ZS[q&255]|((q>>8)&CF)|NF|							\
		((I.AF.b.h^q^R)&HF)|									\
		(((R^I.AF.b.h)&(I.AF.b.h^q)&SF)>>5);					\
	I.AF.b.h=q; 												\
}
#endif

#ifdef X86_ASM
#define M_SBB(R)												\
 asm (															\
 " shrb $1,%%al         \n"                                     \
 " sbbb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb $2,%%al          \n" /* set N flag */                    \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (I.AF.b.h), "=a" (I.AF.b.l)                              \
 :"r" (R), "a" (I.AF.b.l), "0" (I.AF.b.h)                       \
 )
#else
#define M_SBB(R) {                                              \
	int q = I.AF.b.h-R-(I.AF.b.l&CF);							\
	I.AF.b.l=ZS[q&255]|((q>>8)&CF)|NF|							\
		((I.AF.b.h^q^R)&HF)|									\
		(((R^I.AF.b.h)&(I.AF.b.h^q)&SF)>>5);					\
	I.AF.b.h=q; 												\
}
#endif

#ifdef X86_ASM
#define M_CMP(R)												\
 asm (															\
 " cmpb %2,%0          \n"                                      \
 " lahf                \n"                                      \
 " setob %%al          \n" /* al = 1 if overflow */             \
 " shlb $2,%%al        \n" /* shift to P/V bit position */      \
 " andb $0xd1,%%ah     \n" /* sign, zero, half carry, carry */  \
 " orb $2,%%al         \n" /* set N flag */                     \
 " orb %%ah,%%al       \n" /* combine with P/V */               \
 :"=g" (I.AF.b.h), "=a" (I.AF.b.l)                              \
 :"r" (R), "0" (I.AF.b.h)                                       \
 )
#else
#define M_CMP(R) {                                              \
	int q = I.AF.b.h-R; 										\
	I.AF.b.l=ZS[q&255]|((q>>8)&CF)|NF|							\
		((I.AF.b.h^q^R)&HF)|									\
		(((R^I.AF.b.h)&(I.AF.b.h^q)&SF)>>5);					\
}
#endif

#define M_IN													\
	I.XX.d=ARG();												\
	I.AF.b.h=cpu_readport(I.XX.d);

#define M_OUT													\
	I.XX.d=ARG();												\
	cpu_writeport(I.XX.d,I.AF.b.h)

#ifdef	X86_ASM
#define M_DAD(R)												\
 asm (															\
 " andb $0xc4,%1        \n"                                     \
 " addb %%al,%%cl       \n"                                     \
 " adcb %%ah,%%ch       \n"                                     \
 " lahf                 \n"                                     \
 " andb $0x11,%%ah      \n"                                     \
 " orb %%ah,%1          \n"                                     \
 :"=c" (I.HL.d), "=g" (I.AF.b.l)                                \
 :"0" (I.HL.d), "1" (I.AF.b.l), "a" (I.R.d)                     \
 )
#else
#define M_DAD(R) {                                              \
	int q = I.HL.d + I.R.d; 									\
	I.AF.b.l = ( I.AF.b.l & ~(HF+CF) ) |						\
		( ((I.HL.d^q^I.R.d) >> 8) & HF ) |						\
		( (q>>16) & CF );										\
	I.HL.w.l = q;												\
}
#endif

#define M_PUSH(R) {                                             \
	WM(--I.SP.w.l, I.R.b.h);									\
	WM(--I.SP.w.l, I.R.b.l);									\
}

#define M_POP(R) {												\
	I.R.b.l = RM(I.SP.w.l++);									\
	I.R.b.h = RM(I.SP.w.l++);									\
}

#define M_RET(cc)												\
{																\
	if (cc) 													\
	{															\
		i8085_ICount -= 6;										\
		M_POP(PC);												\
		change_pc16(I.PC.d);									\
	}															\
}

#define M_JMP(cc) { 											\
	if (cc) {													\
		I.PC.w.l = ARG16(); 									\
		change_pc16(I.PC.d);									\
	} else I.PC.w.l += 2;										\
}

#define M_CALL(cc)												\
{																\
	if (cc) 													\
	{															\
		UINT16 a = ARG16(); 									\
		i8085_ICount -= 6;										\
		M_PUSH(PC); 											\
		I.PC.d = a; 											\
		change_pc16(I.PC.d);									\
	} else I.PC.w.l += 2;										\
}

#define M_RST(nn) { 											\
	M_PUSH(PC); 												\
	I.PC.d = 8 * nn;											\
	change_pc16(I.PC.d);										\
}


