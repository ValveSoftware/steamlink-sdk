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

#ifndef __GALOIS_TYPE_H__
#define __GALOIS_TYPE_H__

/*!
 * Defines
 * The following is a list of Marvell status
 */
#define MV_ERROR		(int)(-1)
#define MV_INVALID		(int)(-1)
#define MV_FALSE		0
#define MV_TRUE			(!(MV_FALSE))

#define MV_OK			(0x00)  /* Operation succeeded                   */
#define MV_FAIL			(0x01)	/* Operation failed                      */
#define MV_BAD_VALUE		(0x02)  /* Illegal value (general)               */
#define MV_OUT_OF_RANGE		(0x03)  /* The value is out of range             */
#define MV_BAD_PARAM		(0x04)  /* Illegal parameter in function called  */
#define MV_BAD_PTR		(0x05)  /* Illegal pointer value                 */
#define MV_BAD_SIZE		(0x06)  /* Illegal size                          */
#define MV_BAD_STATE		(0x07)  /* Illegal state of state machine        */
#define MV_SET_ERROR		(0x08)  /* Set operation failed                  */
#define MV_GET_ERROR		(0x09)  /* Get operation failed                  */
#define MV_CREATE_ERROR		(0x0A)  /* Fail while creating an item           */
#define MV_NOT_FOUND		(0x0B)  /* Item not found                        */
#define MV_NO_MORE		(0x0C)  /* No more items found                   */
#define MV_NO_SUCH		(0x0D)  /* No such item                          */
#define MV_TIMEOUT		(0x0E)  /* Time Out                              */
#define MV_NO_CHANGE		(0x0F)  /* Parameter(s) is already in this value */
#define MV_NOT_SUPPORTED	(0x10)  /* This request is not support           */
#define MV_NOT_IMPLEMENTED	(0x11)  /* Request supported but not implemented */
#define MV_NOT_INITIALIZED	(0x12)  /* The item is not initialized           */
#define MV_NO_RESOURCE		(0x13)  /* Resource not available (memory ...)   */
#define MV_FULL			(0x14)  /* Item is full (Queue or table etc...)  */
#define MV_EMPTY		(0x15)  /* Item is empty (Queue or table etc...) */
#define MV_INIT_ERROR		(0x16)  /* Error occured while INIT process      */
#define MV_HW_ERROR		(0x17)  /* Hardware error                        */
#define MV_TX_ERROR		(0x18)  /* Transmit operation not succeeded      */
#define MV_RX_ERROR		(0x19)  /* Recieve operation not succeeded       */
#define MV_NOT_READY		(0x1A)	/* The other side is not ready yet       */
#define MV_ALREADY_EXIST	(0x1B)  /* Tried to create existing item         */
#define MV_OUT_OF_CPU_MEM	(0x1C)  /* Cpu memory allocation failed.         */
#define MV_NOT_STARTED		(0x1D)  /* Not started yet         */
#define MV_BUSY			(0x1E)  /* Item is busy.                         */
#define MV_TERMINATE		(0x1F)  /* Item terminates it's work.            */
#define MV_NOT_ALIGNED		(0x20)  /* Wrong alignment                       */
#define MV_NOT_ALLOWED		(0x21)  /* Operation NOT allowed                 */
#define MV_WRITE_PROTECT	(0x22)  /* Write protected                       */


#ifndef NULL
#define NULL			((void*)0)
#endif


/*!
 * Defines
 * The following is a list of Marvell type definition
 */
typedef	float				MV_FLOAT;
typedef	double				MV_BOUBLE;

typedef char				MV_8;
typedef unsigned char			MV_U8;
typedef short				MV_16;
typedef unsigned short			MV_U16;
typedef void					MV_VOID;


#if	(defined(WIN32))
#	include	<io.h>
#	include	<direct.h>

	typedef	unsigned int		MV_U32;
	typedef	signed int		MV_32;
	typedef	unsigned __int64	MV_U64;
	typedef	signed __int64		MV_64;
/*---------------------------------------------------------------------------
    ARMCC (RVCT)
  ---------------------------------------------------------------------------*/
#elif defined (__ARMCC_VERSION)

    typedef	unsigned int        MV_U32;
    typedef	signed int          MV_32;
    typedef	unsigned long long  MV_U64;
    typedef	signed long long    MV_64;

#elif	(defined(__GNUC__))
#if defined(__UBOOT__) || defined(__LINUX_KERNEL__) || defined(NON_OS)
#else
#	include	<unistd.h>
#	include	<sys/time.h>
#	include	<sys/types.h>
#endif
	typedef	unsigned long		MV_U32;
	typedef	signed long		MV_32;
	typedef	unsigned long long	MV_U64;
	typedef	signed long long	MV_64;

#else
	typedef	unsigned long		MV_U32;
	typedef	signed long		MV_32;
	typedef	unsigned long long	MV_U64;
	typedef	signed long long	MV_64;

#endif
#endif /* __GALOIS_TYPE_H__ */

