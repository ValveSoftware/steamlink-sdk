#ifndef __ASM_MACH_SYSTEM_H
#define __ASM_MACH_SYSTEM_H
extern void (*arm_pm_restart)(char str, const char *cmd);
extern void galois_arch_reset(char mode, const char *cmd);
#endif /*__ASM_MACH_SYSTEM_H*/
