/**********************************************************************************************************
*       Copyright (C) 2007-2011
*       Copyright ? 2007 Marvell International Ltd.
*
*       This program is free software; you can redistribute it and/or
*       modify it under the terms of the GNU General Public License
*       as published by the Free Software Foundation; either version 2
*       of the License, or (at your option) any later version.
*
*       This program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY; without even the implied warranty of
*       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*       GNU General Public License for more details.
*
*       You should have received a copy of the GNU General Public License
*       along with this program; if not, write to the Free Software
*       Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
**********************************************************************************************************/

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
