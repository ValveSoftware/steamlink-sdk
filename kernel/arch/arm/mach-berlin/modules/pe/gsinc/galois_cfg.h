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

#ifndef	__GALOIS_CFG_H__
#define	__GALOIS_CFG_H__

/*!
 * IO read/write through software/hardware socket
 * #define	__MEMIO_SOCKET
 */

/*!
 * IO read/write through PCI-Express bus
 * #define	__MEMIO_PCIE
 */

/*!
 * IO read/write directly using MEMI/O
 */
#define	__MEMIO_DIRECT

/*!
 * Program is running under little endian system.
 */
#ifndef __LITTLE_ENDIAN
#define	__LITTLE_ENDIAN
#endif

/*!
 * Program is running under big endian system.
 * #define	__BIG_ENDIAN
 */

/*!
 * Program is running under Linux on ARM processor
 */
#define	__LINUX_ARM


/*!
 * Program is running under VxWorks on ARM processor
 * #define	__VxWORKS_ARM
 */

#endif	/* __GALOIS_CFG_H__	*/
