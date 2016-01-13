/******************************************************************************* 
* Copyright (C) Marvell International Ltd. and its affiliates 
* 
* Marvell GPL License Option 
* 
* If you received this File from Marvell, you may opt to use, redistribute and/or 
* modify this File in accordance with the terms and conditions of the General 
* Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
* available along with the File in the license.txt file or by writing to the Free 
* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
* on the worldwide web at http://www.gnu.org/licenses/gpl.txt.  
* 
* THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
* DISCLAIMED.  The GPL License provides additional details about this warranty 
* disclaimer.  
********************************************************************************/

#ifndef	CBASE
#define	CBASE						"           CBASE >>>    "
/**	CBASE
 */


#include "ctypes.h"


/**	SECTION - platform dependent includes, definitions, and data types
 */

#ifndef	__CODE_LINK__
#ifndef	_CRT_SECURE_NO_DEPRECATE
#define	_CRT_SECURE_NO_DEPRECATE	/* To avoid CRT warnings */
#endif

	#include	<stdio.h>
	#include	<stdlib.h>
	#include	<stdarg.h>
	#include	<string.h>
	#include	<math.h>
	#include	<time.h>
	#include	<assert.h>

#else
	#ifndef __KERNEL__
	#include	"string.h"			/* Customized: memcpy, memset, and str... */
	#endif

	#if	__CODE_LINK__ > 0
	#include	"malloc.h"			/* Customized: malloc, free */
	#endif
#endif

#if(defined(WIN32))
	#include	<io.h>
	#include	<direct.h>

	#if(!defined(__XPLATFORM__))
	#include	<windows.h>
	#endif
		#define	__reversed_bit_fields__

	#if(defined(_TGTOS) && (_TGTOS == CE))
		#define	__OS_MSVC_CE__

	#elif(defined(DRIVER))
		#define	__OS_MSVC_KERNEL__

	#elif(defined(_CONSOLE) || defined(__XPLATFORM__))
		#define	__OS_MSVC_CONSOLE__

	#else
		#define	__OS_MSVC_USER__
	#endif

#elif(defined(__ARMCC_VERSION))

#elif(defined(__GNUC__))
	#if(defined(__KERNEL__))
		#define	__OS_GNUC_KERNEL__
	#else
		#include	<unistd.h>
		#include	<sys/time.h>
		#include	<sys/types.h>
		#define	__OS_GNUC_USER__
	#endif
#else
		#define	__OS_UNKNOWN__
#endif
#if 0
		typedef	UNSG8				T8b;
		typedef	UNSG16				T16b;
		typedef	UNSG32				T32b;
		typedef	UNSG32				T64b [2];
		typedef	UNSG32				T96b [3];
		typedef	UNSG32				T128b[4];
		typedef	UNSG32				T160b[5];
		typedef	UNSG32				T192b[6];
		typedef	UNSG32				T224b[7];
		typedef	UNSG32				T256b[8];
#endif

/**	ENDOFSECTION
 */



/**	SECTION - symbols
 */
	#ifdef	__MAIN__
		#define	Global
		#define	GlobalAssign(g, x)	g = (x)
	#else
		#define	Global				extern
		#define	GlobalAssign(g, x)	g
	#endif

	#ifdef	CONST
	#undef	CONST
	#endif
		#define	CONST				static const

	#ifdef	INLINE
	#undef	INLINE
	#endif
	#if(defined(WIN32))
		#define	INLINE				static __forceinline
  #elif(defined(__ARMCC_VERSION))
    #define INLINE				static __inline

	#else
		#define INLINE				static inline
	#endif

	#ifdef	NULL
	#undef	NULL
	#endif
		#define	NULL				0

	#if(defined(WIN32))
		#ifndef	__OS_MSVC_CE__
		#define	$QPTR				qword ptr
		#define	$DPTR				dword ptr
		#define	$CNT				edx
		#define	MMX_WIN32
		#endif

		#define	ptr32u(ptr)			((UNSG32)PtrToUlong((char*)ptr))

	#elif(defined(__ARMCC_VERSION)) //xingguo wang RVDS
	    #define	ptr32u(ptr)			((UNSG32)(ptr))
	    #define	_unlink				remove
	    #define	_stricmp            strcmp
	    #define	_unlink				remove

	#else
		#define	ptr32u(ptr)			((UNSG32)(ptr))

		#define	_stricmp			strcasecmp
		#define	_unlink				unlink
		#define	_open				open
		#define	_close				close
		#define	_read				read
		#define	_write				write
		#define	_tell				tell
		#define	_lseek				lseek
		#define	_lseeki64			lseeki64
	#endif

	#ifndef	MemCpy
		#define	MemCpy				memcpy
	#endif

	#ifndef	MemSet
		#define	MemSet				memset
	#endif

/**	ENDOFSECTION
 */



#ifdef	__cplusplus
	extern	"C"
	{
#endif

/**	SECTION - pre-defined function arguments and returns
 */
		typedef enum {

		ARG_KEEP					= - 32768,			/* No update, keep old value */
		ARG_AUTO					= - 32767,			/* Function chooses the value for it */

		} EFuncArgument;

		typedef enum {

		ERR_UNKNOWN					= - 1,				/* Error type not defined */
		ERR_POINTER					= - 2,				/* Invalid pointer */
		ERR_FILE					= - 3,				/* File access failure */
		ERR_FIFO					= - 4,				/* FIFO overflow or underflow */
		ERR_MEMORY					= - 5,				/* Not enough memory or allocation failure */
		ERR_MISMATCH				= - 6,				/* Mis-matches found in hand-shaking */
		ERR_PARAMETER 				= - 7,				/* False function parameter */
		ERR_HARDWARE				= - 8,				/* Hardware error */
		ERR_TIMING					= - 9,				/* Sychronize-related violation */

		SUCCESS						= 0

		} EFuncReturn;
#ifndef	__CODE_LINK__
#if 0
		static	char	*eperr[]	= {	"SUCCESS",
										"ERR_UNKNOWN",
										"ERR_POINTER",
										"ERR_FILE",
										"ERR_FIFO",
										"ERR_MEMORY",
										"ERR_MISMATCH",
										"ERR_PARAMETER",
										"ERR_HARDWARE",
										"ERR_TIMING",
											};
#endif
#else
		#define	eperr				garrps8
#endif

/**	ENDOFSECTION
 */



/**	SECTION - unit constants for storage, frequency and bit-rate
 */
		#define	KILO				(1024)
		#define	kilo				(1000)

		#define	MEGA				(1024  * 1024)
		#define	mega				(1000  * 1000)

		#define	GIGA				(1024. * 1024. * 1024.)
		#define	giga				(1000. * 1000. * 1000.)

		#define	TERA				(1024. * 1024. * 1024. * 1024.)
		#define	tera				(1000. * 1000. * 1000. * 1000.)

		#define	KILOth				(1. / KILO)
		#define	kiloth				(1. / kilo)

		#define	MEGAth				(1. / MEGA)
		#define	megath				(1. / mega)

		#define	GIGAth				(1. / GIGA)
		#define	gigath				(1. / giga)

		#define	TERAth				(1. / TERA)
		#define	terath				(1. / tera)

		#define	INFINITY32			(1 << 28)			/* Help to avoid 32b operation over-flow */

/**	ENDOFSECTION
 */



/**	SECTION - unified 'xdbg(fmt, ...)' and '_LOG'/'_dftLOG'/'_fLOG' (masking enabled) functions
 */
		Global	UNSG32	GlobalAssign(dbghfp, 0), GlobalAssign(dbgmask, ~0);

#ifdef	__CODE_LINK__
		#define	xdbg						1 ? 0 :
#else
	#if(defined(__OS_GNUC_KERNEL__))
		#define	xdbg						printk

	#elif(defined(__OS_MSVC_KERNEL__))
		#define	xdbg						DbgPrint

	#elif(defined(__OS_MSVC_CE__))
		#define	xdbg						KITLOutputDebugString

	#elif(defined(__OS_MSVC_USER__))
		INLINE void _dbg(const char *fmt, va_list args)
											{	char str[KILO]; vsprintf(str, fmt, args); OutputDebugString(str);
														}
	#else
		#define	_dbg						vprintf
	#endif

	#ifdef	xdbg
		#define	fdbg						1 ? 0 :
		#define	_dflush(fp)					do{}while(0)
	#else
		INLINE void xdbg(const char *fmt, ...)
											{	va_list args; va_start(args, fmt);
												if(!dbghfp) _dbg(fmt, args); else vfprintf((FILE*)dbghfp, fmt, args);
												va_end(args);
														}
		INLINE void fdbg(UNSG32 hfp, const char *fmt, ...)
											{	va_list args; if(!hfp) return; va_start(args, fmt);
												vfprintf((FILE*)hfp, fmt, args); va_end(args);
														}
		#define	_dflush(fp)					do{	if(fp) fflush((FILE*)fp); }while(0)
	#endif
#endif

	/* Usage: _LOG(log_file, log_mask, item_bit, ("item = %d\n", item_value)) */
		#define	_LOG(fp, mask, b, fmt_args)	do{ if(((b) < 0) || bTST(mask, b)) { UNSG32 dft = dbghfp;				\
													dbghfp = ptr32u(fp); xdbg fmt_args; dbghfp = dft; }				\
														}while(0)
	/* Usage: _dftLOG(bLogFlush, item_bit, ("item = %d\n", item_value)) */
		#define	_dftLOG(flush, b, fmt_args)	do{	_LOG(dbghfp, dbgmask, b, fmt_args); if(flush) _dflush(dbghfp);		\
														}while(0)
	/* Usage: _fLOG(log_file, ("item = %d\n", item_value)) */
		#define	_fLOG(fp, fmt_args)			do{	_LOG(fp, 1, 0, fmt_args); _dflush(fp); }while(0)

/**	ENDOFSECTION
 */



/**	SECTION - unified 'IO32RD', 'IO32WR', and 'IO64RD' functions
 */
#ifdef	__INLINE_ASM_ARM__
	/**	Read 32-bit data from certain address
	 *	a: input, address
	 *	d: output 32-bit data
	 */
		#define	IO32RD(d, a)				__asm__ __volatile__ ("ldr %0, [%1]" : "=r" (d) : "r" (a))

	/**	Write 32-bit data from certain address
	 *	a: input, address
	 *	d: input 32-bit data
	 */
		#define	IO32WR(d, a)				__asm__ __volatile__ ("str %0, [%1]" : : "r" (d), "r" (a) : "memory")

	/**	Read 64-bit data from certain address (MUST be 8-byte aligned)
	 *	a: input, address
	 *	d0: output first 32-bit data
	 *	d1: output second 32-bit data
	 */
		#define	IO64RD(d0, d1, a)			__asm__ __volatile__ (													\
												"ldrd r4, [%2] \n\t" "mov %0, r4    \n\t" "mov %1, r5    \n\t"		\
												: "=r" (d0), "=r" (d1) : "r" (a) : "r4", "r5"						\
														)
#else

#if defined(BERLIN_SINGLE_CPU) && !defined(__LINUX_KERNEL__)
		#define	IO32RD(d, a)					do{	(d) = (*((volatile UNSG32 *)devmem_phy_to_virt(a))); }while(0)
		#define	IO32WR(d, a)				do{	(*((volatile UNSG32 *)devmem_phy_to_virt(a)) = (UNSG32)(d)); }while(0)
#else
		#define	IO32RD(d, a)				do{	(d) = *(UNSG32*)(a); }while(0)
		#define	IO32WR(d, a)				do{	*(UNSG32*)(a) = (d); }while(0)
#endif
		#define	IO64RD(d0, d1, a)			do{	IO32RD(d0, a); IO32RD(d1, (a) + 4); }while(0)
#endif

		/* Directly write a 32b register or append to 'T64b cfgQ[]', in (adr,data) pairs */
		#define	IO32CFG(cfgQ, i, a, d)		do{	if(cfgQ) { (cfgQ)[i][0] = (a); (cfgQ)[i][1] = (d); }				\
												else IO32WR(d, a);													\
												(i) ++;																\
														}while(0)

/**	ENDOFSECTION
 */

#ifdef	__cplusplus
	}
#endif



/**	CBASE
 */
#endif

/**	ENDOFFILE: cbase.h ************************************************************************************************
 */

