

#ifndef __ARMNEC_H__
#define __ARMNEC_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int ArmNecVer; // Version number of library

struct ArmNec
{
	unsigned char reg[16];		// [r7,#0x00] AL,AH,CL,CH,DL,DH,BL,BH,SPL,SPH,BPL,BPH,IXL,IXH,IYL,IYH
	unsigned short sreg[4];		// [r7,#0x10] DS1, PS, SS, DS0
	unsigned short ip;			// [r7,#0x18] IP
	unsigned short flags;		// [r7,#0x1A] Flags				M___ODIT SZ_A_P_C
	unsigned int prefix;		// [r7,#0x1C] F_______ ________ xxxxxxxx xxxxxxxx // runtime use in r8 and now reserved
	unsigned int flags_arm;		// [r7,#0x20] pppppppp ________ ________ ___ANZCV
								//            fixed arm flags  
	
	int cycles;					// [r7,#0x24] Cycles remain
	unsigned int pc;			// [r7,#0x28] Memory Base (.membase) + PC
	unsigned int mem_base;		// [r7,#0x2C] Memory Base
	unsigned int (*checkpc)(unsigned int ps, unsigned int ip);	// [r7,#0x30] called to recalc Memory Base+pc

	int reserve;										// [r7,#0x34]

	unsigned char **ReadMemMap;							// [r7,#0x38]
	unsigned char **WriteMemMap;						// [r7,#0x3C]

	unsigned int (*read8  )(unsigned int a);			// [r7,#0x40]
	unsigned int (*read16 )(unsigned int a);			// [r7,#0x44]
	void (*write8 )(unsigned int a,unsigned int d);		// [r7,#0x48]
	void (*write16)(unsigned int a,unsigned int d);		// [r7,#0x4C]
	unsigned int (*in8 )(unsigned int a);				// [r7,#0x50]
	void (*out8)(unsigned int a, unsigned int d);		// [r7,#0x54]
	int (*IrqCallback)(int int_level);					// [r7,#0x58]
	int (*UnrecognizedCallback)(unsigned int a);		// [r7,#0x5C]
};

void ArmV33Irq(struct ArmNec *, int);
int ArmV33Run(struct ArmNec *);
extern unsigned int ArmV33CryptTable[];

void ArmV30Irq(struct ArmNec *, int);
int ArmV30Run(struct ArmNec *);
extern unsigned int ArmV30CryptTable[];


#ifdef __cplusplus
}
#endif

#endif	// __ARMNEC_H__
