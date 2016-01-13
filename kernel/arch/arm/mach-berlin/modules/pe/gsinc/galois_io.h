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

#ifndef	__GALOIS_IO_H__
#define	__GALOIS_IO_H__

#ifdef	__MEMIO_SOCKET
#include <assert.h>
#include "trans_h.h"
#endif

#ifdef	__MEMIO_PCIE
#include <asm/io.h>
#include <asm/byteorder.h>
#endif

#include "galois_cfg.h"
#include "galois_type.h"
#include "galois_common.h"


#ifdef	__MEMIO_PCIE
extern	MV_U32						gu32_galois_pcie_ioremap_base;
#endif

#define	CPU_PHY_MEM(x)					((MV_U32)(x))
#define	CPU_MEMIO_CACHED_ADDR(x)			((void*)(x))
#define	CPU_MEMIO_UNCACHED_ADDR(x)			((void*)(x))

/*!
 * CPU endian check
 */
#if defined( CPU_BIG_ENDIAN ) && defined( __LITTLE_ENDIAN )
#error CPU endian conflict!!!
#elif defined(__BYTE_ORDER)
#if __BYTE_ORDER == __BIG_ENDIAN
#error CPU endian conflict!!!
#endif
#elif defined( __BIG_ENDIAN )
#error CPU endian conflict!!!
#endif

/*!
 * CPU architecture dependent 32, 16, 8 bit read/write IO addresses
 */
#if defined(BERLIN_SINGLE_CPU) && !defined(__LINUX_KERNEL__)
#define	MV_MEMIO32_WRITE(addr, data)		((*((volatile unsigned int *)devmem_phy_to_virt(addr))) = ((unsigned int)(data)))
#define	MV_MEMIO32_READ(addr)				((*((volatile unsigned int *)devmem_phy_to_virt(addr))))
#define	MV_MEMIO16_WRITE(addr, data)		((*((volatile unsigned short *)devmem_phy_to_virt(addr))) = ((unsigned short)(data)))
#define	MV_MEMIO16_READ(addr)				((*((volatile unsigned short *)devmem_phy_to_virt(addr))))
#define	MV_MEMIO08_WRITE(addr, data)		((*((volatile unsigned char *)devmem_phy_to_virt(addr))) = ((unsigned char)(data)))
#define	MV_MEMIO08_READ(addr)				((*((volatile unsigned char *)devmem_phy_to_virt(addr))))
#else
#define	MV_MEMIO32_WRITE(addr, data)		((*((volatile unsigned int*)(addr))) = ((unsigned int)(data)))
#define	MV_MEMIO32_READ(addr)				((*((volatile unsigned int*)(addr))))
#define	MV_MEMIO16_WRITE(addr, data)		((*((volatile unsigned short*)(addr))) = ((unsigned short)(data)))
#define	MV_MEMIO16_READ(addr)				((*((volatile unsigned short*)(addr))))
#define	MV_MEMIO08_WRITE(addr, data)		((*((volatile unsigned char*)(addr))) = ((unsigned char)(data)))
#define	MV_MEMIO08_READ(addr)				((*((volatile unsigned char*)(addr))))
#endif

/*!
 * No Fast Swap implementation (in assembler) for ARM
 */
#define	MV_32BIT_LE_FAST(val)				MV_32BIT_LE(val)
#define	MV_16BIT_LE_FAST(val)				MV_16BIT_LE(val)
#define	MV_32BIT_BE_FAST(val)				MV_32BIT_BE(val)
#define	MV_16BIT_BE_FAST(val)				MV_16BIT_BE(val)

/*!
 * 32 and 16 bit read/write in big/little endian mode
 */

/*!
 * 16bit write in little endian mode
 */
#define	MV_MEMIO_LE16_WRITE(addr, data)		MV_MEMIO16_WRITE(addr, MV_16BIT_LE_FAST(data))

/*!
 * 16bit read in little endian mode
 */
#define	MV_MEMIO_LE16_READ(addr)			MV_16BIT_LE_FAST((MV_U16)(MV_MEMIO16_READ((MV_U32)(addr))))

/*!
 * 32bit write in little endian mode
 */
#define	MV_MEMIO_LE32_WRITE(addr, data)		MV_MEMIO32_WRITE(addr, MV_32BIT_LE_FAST(data))

/*!
 * 32bit read in little endian mode
 */
#define	MV_MEMIO_LE32_READ(addr)			MV_32BIT_LE_FAST((MV_U32)(MV_MEMIO32_READ((MV_U32)(addr))))

/*!
 * Generate 32bit mask
 */
#define	GA_REG_MASK(bits, l_shift)			((bits) ? (((bits) < 32) ? (((1uL << (bits)) - 1) << (l_shift)) : (0xFFFFFFFFuL << (l_shift))) : 0)

/*!
 * Galois's register address translate
 */
#if	(defined(__MEMIO_DIRECT))
#	ifndef	INTER_REGS_BASE
#	define	INTER_REGS_BASE				0
#	endif

#	define	REG_ADDR(offset)			((MV_U32)(INTER_REGS_BASE | (offset)))

#elif	(defined(__MEMIO_PCIE))
	/*!
	 * After ioremap the value in BAR1, got IO base address. value in BAR0 is for PCI-e core, BAR1 is for galois
	 */
#	define	REG_ADDR(offset)			((MV_U32)(gu32_galois_pcie_ioremap_base + (offset)))

#else
#	define	REG_ADDR(offset)			((MV_U32)(offset))

#endif

/*!
 * Galois controller register read/write macros
 *
 * offset -- address offset (32bits)
 * holder -- pointer to the variable that will be used to store the data being read in.
 * val -- variable contains the data that will be written out.
 * bitMask -- variable contains the data (32bits) that will be written out.
 * clearMask -- variable contains the mask (32bits) that will be used to clear the corresponding bits.
 *
 * GA_REG_WORD32_READ(offset, holder) -- Read a Double-Word (32bits) from 'offset' to 'holder'
 * GA_REG_WORD16_READ(offset, holder) -- Read a Word (16bits) from 'offset' to 'holder'
 * GA_REG_BYTE_READ(offset, holder) -- Read a Byte (8bits) from 'offset' to 'holder'
 *
 * GA_REG_WORD32_WRITE(offset, val) -- Write a Double-Word (32bits) to 'offset'
 * GA_REG_WORD16_WRITE(offset, val) -- Write a Word (16bits) to 'offset'
 * GA_REG_BYTE_WIRTE(offset, val) -- Write a Byte (8bits) to 'offset'
 *
 * GA_REG_WORD32_BIT_SET(offset, bitMask) -- Set bits to '1b' at 'offset', 'bitMask' should only be used to set '1b' for corresponding bits.
 * GA_REG_WORD32_BITS_SET(offset, clearMask, val) -- Clear the bits to zero for the bits in clearMask are '1b' and write 'val' to 'offset'.
 * GA_REG_WORD32_BIT_CLEAR(offset, clearMask) -- Clear the bits to zero for the bits in bitMask are '1b'
 *
 */
#if	(defined(__MEMIO_DIRECT))

#if defined(BERLIN_SINGLE_CPU) && !defined(__LINUX_KERNEL__)
//temp use this, 'cause the cpu endian definition has confliction
#	define	GA_REG_WORD32_READ(offset, holder)	(*(holder) = (*((volatile unsigned int *)devmem_phy_to_virt(offset))))
#else
#	define	GA_REG_WORD32_READ(offset, holder)	(*(holder) = MV_MEMIO_LE32_READ(REG_ADDR(offset)))
#endif
#	define	GA_REG_WORD16_READ(offset, holder)	(*(holder) = MV_MEMIO_LE16_READ(REG_ADDR(offset)))
#	define	GA_REG_BYTE_READ(offset, holder)	(*(holder) = MV_MEMIO08_READ(REG_ADDR(offset)))

#if defined(BERLIN_SINGLE_CPU) && !defined(__LINUX_KERNEL__)
#	define	GA_REG_WORD32_WRITE(addr, data)		((*((volatile unsigned int *)devmem_phy_to_virt(addr))) = ((unsigned int)(data)))
#else
#	define	GA_REG_WORD32_WRITE(offset, val)	(MV_MEMIO_LE32_WRITE(REG_ADDR(offset), (MV_U32)(val)))
#endif
#	define	GA_REG_WORD16_WRITE(offset, val)	(MV_MEMIO_LE16_WRITE(REG_ADDR(offset), (MV_U16)(val)))
#	define	GA_REG_BYTE_WRITE(offset, val)		(MV_MEMIO08_WRITE(REG_ADDR(offset), (MV_U8)(val)))

#	define	GA_REG_WORD32_BIT_SET(offset, bitMask)	(MV_MEMIO32_WRITE(REG_ADDR(offset),					\
							(MV_MEMIO_LE32_READ(REG_ADDR(offset)) | MV_32BIT_LE_FAST(bitMask))))
#	define	GA_REG_WORD32_BITS_SET(offset, clearMask, val)									\
							(MV_MEMIO32_WRITE(REG_ADDR(offset),					\
							((MV_MEMIO_LE32_READ(REG_ADDR(offset)) & MV_32BIT_LE_FAST(~(clearMask)))\
							| MV_32BIT_LE_FAST(val))))
#	define	GA_REG_WORD32_BIT_CLEAR(offset, clearMask)									\
							(MV_MEMIO32_WRITE(REG_ADDR(offset),					\
							(MV_MEMIO_LE32_READ(REG_ADDR(offset)) & MV_32BIT_LE_FAST(~(clearMask)))))

#elif	(defined(__MEMIO_PCIE))
/*!
 * System dependent little endian from / to CPU conversions
 */
#	define	MV_CPU_TO_LE16(x)			cpu_to_le16(x)
#	define	MV_CPU_TO_LE32(x)			cpu_to_le32(x)

#	define	MV_LE16_TO_CPU(x)			le16_to_cpu(x)
#	define	MV_LE32_TO_CPU(x)			le32_to_cpu(x)

/*!
 * System dependent register read / write in byte/word16/word32 variants
 */
#	define	GA_REG_WORD32_READ(offset, holder)	(*(holder) = MV_LE32_TO_CPU(readl(REG_ADDR(offset))))
#	define	GA_REG_WORD16_READ(offset, holder)	(*(holder) = MV_LE16_TO_CPU(readw(REG_ADDR(offset))))
#	define	GA_REG_BYTE_READ(offset, holder)	(*(holder) = readb(REG_ADDR(offset)))

#	define	GA_REG_WORD32_WRITE(offset, val)	writel(MV_CPU_TO_LE32((MV_U32)(val)), REG_ADDR(offset))
#	define	GA_REG_WORD16_WRITE(offset, val)	writew(MV_CPU_TO_LE16((MV_U16)(val)), REG_ADDR(offset))
#	define	GA_REG_BYTE_WRITE(offset, val)		writeb((MV_U8)(val), REG_ADDR(offset))

#	define	GA_REG_WORD32_BIT_SET(offset, bitMask)	writel((readl(REG_ADDR(offset)) | MV_CPU_TO_LE32(bitMask)), REG_ADDR(offset))
#	define	GA_REG_WORD32_BITS_SET(offset, clearMask, val)									\
							writel(((readl(REG_ADDR(offset)) & MV_CPU_TO_LE32(~(clearMask)))	\
							| MV_CPU_TO_LE32(val)), REG_ADDR(offset))
#	define	GA_REG_WORD32_BIT_CLEAR(offset, clearMask)									\
							writel((readl(REG_ADDR(offset)) & MV_CPU_TO_LE32(~(clearMask))), REG_ADDR(offset))

#elif	(defined(__MMIO_SOCKET))
#	define	GA_REG_WORD32_READ(offset, holder)	(assert(BFM_HOST_Bus_Read32(REG_ADDR(offset), (MV_U32 *)(holder)) >= 0))
#	define	GA_REG_WORD16_READ(offset, holder)	(assert(BFM_HOST_Bus_Read16(REG_ADDR(offset), (MV_U16 *)(holder)) >= 0))
#	define	GA_REG_BYTE_READ(offset, holder)	(assert(BFM_HOST_Bus_Read8(REG_ADDR(offset), (MV_U8 *)(holder)) >= 0))

#	define	GA_REG_WORD32_WRITE(offset, val)	(assert(BFM_Host_Bus_Write32(REG_ADDR(offset), (MV_U32)(val)) >= 0))
#	define	GA_REG_WORD16_WRITE(offset, val)	(assert(BFM_Host_Bus_Write16(REG_ADDR(offset), (MV_U16)(val)) >= 0))
#	define	GA_REG_BYTE_WRITE(offset, val)		(assert(BFM_Host_Bus_Write8(REG_ADDR(offset), (MV_U8)(val)) >= 0))

#	define	GA_REG_WORD32_BIT_SET(offset, bitMask)										\
		do {														\
			MV_U32		temp;											\
																\
			GA_REG_WORD32_READ(offset, &temp);									\
			temp |= (bitMask);											\
			GA_REG_WORD32_WRITE(offset, temp);									\
		} while (0)

#	define	GA_REG_WORD32_BITS_SET(offset, clearMask, val)									\
		do {														\
			MV_U32		temp;											\
																\
			GA_REG_WORD32_READ(offset, &temp);									\
			temp &= ~(clearMask);											\
			temp |= val;												\
			GA_REG_WORD32_WRITE(offset, temp);									\
		} while (0)

#	define	GA_REG_WORD32_BIT_CLEAR(offset, clearMask)									\
		do {														\
			MV_U32		temp;											\
																\
			GA_REG_WORD32_READ(offset, &temp);									\
			temp &= ~(clearMask);											\
			GA_REG_WORD32_WRITE(offset, temp);									\
		} while (0)

#else
#	error "MEMI/O way not selected"

#endif	/* __MEMIO_SOCKET || __MEMIO_PCIE || __MEMIO_DIRECT	*/

#endif	/* __GALOIS_IO_H__ */
