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

#ifndef CTYPES_H_
#define CTYPES_H_

typedef	unsigned char       UNSG8;
typedef	signed char         SIGN8;
typedef	unsigned short      UNSG16;
typedef	signed short        SIGN16;
typedef	unsigned int        UNSG32;
typedef	signed int          SIGN32;
typedef	unsigned long long  UNSG64;
typedef	signed long long    SIGN64;
typedef	float               REAL32;
typedef	double              REAL64;

#ifndef INLINE
#define INLINE          static inline
#endif

/*---------------------------------------------------------------------------
    NULL
  ---------------------------------------------------------------------------*/

#ifndef NULL
    #ifdef __cplusplus
        #define NULL            0
    #else
        #define NULL            ((void *)0)
    #endif
#endif


/*---------------------------------------------------------------------------
    Multiple-word types
  ---------------------------------------------------------------------------*/
#ifndef	Txxb
	#define	Txxb
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

#endif
