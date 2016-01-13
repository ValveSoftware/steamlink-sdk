/*******************************************************************************************
*	Copyright (C) 2007-2011
*	Copyright © 2007 Marvell International Ltd.
*
*	This program is free software; you can redistribute it and/or
*	modify it under the terms of the GNU General Public License
*	as published by the Free Software Foundation; either version 2
*	of the License, or (at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of:
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*
*	$Log: vsys#comm#cmodel#hal#drv#hal_dhub.c,v $
*	Revision 1.7  2007-11-26 09:12:33-08  kbhatt
*	merge changes with version of ctsai
*
*	Revision 1.6  2007-11-18 00:11:43-08  ctsai
*	disabled redundant io32wr & io32rd
*
*	Revision 1.5  2007-11-12 14:15:57-08  bofeng
*	removed #define    __CUSTOMIZED_IO__ as default
*	restore the modification for io32rd and io32wr
*
*	Revision 1.4  2007-10-30 11:08:35-07  ctsai
*	changed printf to xdbg
*
*	Revision 1.3  2007-10-24 21:41:16-07  kbhatt
*	add structures to api_dhub.h
*
*	Revision 1.2  2007-10-12 21:36:55-07  oussama
*	adapted the env to use Alpha's Cmodel driver
*
*	Revision 1.1  2007-08-29 20:27:00-07  lsha
*	Initial revision.
*
*
*	DESCRIPTION:
*	OS independent layer for dHub/HBO/SemaHub APIs.
*
********************************************************************************************/
#include	"api_dhub.h"
#include	"dHub.h"
#include	"avio.h"

#define MEMMAP_AVIO_BCM_REG_BASE	0xF7B50000
#define	__MEMIO_DIRECT
#ifndef __LITTLE_ENDIAN
#define	__LITTLE_ENDIAN
#endif


#ifdef	__INLINE_ASM_ARM__
#define	IO32RD(d, a)	__asm__ __volatile__ ("ldr %0, [%1]" : "=r" (d) : "r" (a))
#define	IO32WR(d, a)	__asm__ __volatile__ ("str %0, [%1]" : : "r" (d), "r" (a) : "memory")
			"ldrd r4, [%2] \n\t" "mov %0, r4    \n\t" "mov %1, r5    \n\t"	\
					: "=r" (d0), "=r" (d1) : "r" (a) : "r4", "r5")
#else
#define	IO32RD(d, a)		do{	(d) = *(UNSG32*)(a); }while(0)
#define	IO32WR(d, a)		do{	*(UNSG32*)(a) = (d); }while(0)
#endif

#define IO32CFG(cfgQ, i, a, d)  do{ if(cfgQ) { (cfgQ)[i][0] = (d); (cfgQ)[i][1] = (a); } \
                                    else IO32WR(d, a); \
                                    (i) ++; \
                                    }while(0)

typedef	unsigned int		MV_U32;

#define	MV_16BIT_LE(X)		(X)
#define	MV_32BIT_LE(X)		(X)
#define	MV_64BIT_LE(X)		(X)
#define	MV_16BIT_BE(X)		MV_BYTE_SWAP_16BIT(X)
#define	MV_32BIT_BE(X)		MV_BYTE_SWAP_32BIT(X)
#define	MV_64BIT_BE(X)		MV_BYTE_SWAP_64BIT(X)
#define	MV_32BIT_LE_FAST(val)		MV_32BIT_LE(val)
#define	MV_32BIT_BE_FAST(val)		MV_32BIT_BE(val)
#define	MV_MEMIO32_WRITE(addr, data)	((*((volatile unsigned int*)(addr))) = ((unsigned int)(data)))
#define	MV_MEMIO32_READ(addr)		((*((volatile unsigned int*)(addr))))


#define	MV_MEMIO_LE32_WRITE(addr, data)\
		MV_MEMIO32_WRITE(addr, MV_32BIT_LE_FAST(data))
#define	MV_MEMIO_LE32_READ(addr)\
		MV_32BIT_LE_FAST((MV_U32)(MV_MEMIO32_READ((MV_U32)(addr))))

#define	INTER_REGS_BASE		0
#define	REG_ADDR(offset)		((MV_U32)(INTER_REGS_BASE | (offset)))

#define	GA_REG_WORD32_READ(offset, holder)	\
		(*(holder) = MV_MEMIO_LE32_READ(REG_ADDR(offset)))
#define	GA_REG_WORD32_WRITE(offset, val)	\
		(MV_MEMIO_LE32_WRITE(REG_ADDR(offset), (MV_U32)(val)))

#define	bTST(x, b)		(((x) >> (b)) & 1)
#define	ModInc(x, i, mod)	do { (x) += (i); while((x) >= (mod)) (x) -= (mod); } while(0)

#ifdef DHUB_DBG
#define xdbg(fmt, args...)		printk(fmt, args...)
#else
#define xdbg(fmt, args...)		do { } while (0)
#endif

#pragma	pack(4)
UNSG32 sizeof_hdl_semaphore	= sizeof( HDL_semaphore	);
UNSG32 sizeof_hdl_hbo		= sizeof( HDL_hbo		);
UNSG32 sizeof_hdl_dhub		= sizeof( HDL_dhub	);
UNSG32 sizeof_hdl_dhub2d	= sizeof( HDL_dhub2d	);
#pragma	pack()


/**	SECTION - API definitions for $SemaHub
 */
/******************************************************************************
*	Function: semaphore_hdl
*	Description: Initialize HDL_semaphore with a $SemaHub BIU instance.
*******************************************************************************/
void	semaphore_hdl(
		UNSG32	ra,	/*	Base address of a BIU instance of $SemaHub */
		void	*hdl	/*	Handle to HDL_semaphore */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	sem->ra = ra;
	/**	ENDOFFUNCTION: semaphore_hdl **/
}

/******************************************************************************
*	Function: semaphore_cfg
*	Description: Configurate a semaphore's depth & reset pointers.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	semaphore_cfg(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id,		/*Semaphore ID in $SemaHub */
		SIGN32	depth,		/*Semaphore (virtual FIFO) depth */
		T64b	cfgQ[]		/*Pass NULL to directly init SemaHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	T32Semaphore_CFG	cfg;
	UNSG32 i = 0, a;
	a = sem->ra + RA_SemaHub_ARR + id*sizeof(SIE_Semaphore);

	cfg.u32 = 0; cfg.uCFG_DEPTH = sem->depth[id] = depth;
	IO32CFG(cfgQ, i, a + RA_Semaphore_CFG, cfg.u32);

	return i;
	/**	ENDOFFUNCTION: semaphore_cfg **/
}

/******************************************************************************
*	Function: semaphore_intr_enable
*	Description: Configurate interrupt enable bits of a semaphore.
*******************************************************************************/
void	semaphore_intr_enable(
		void		*hdl,		/*Handle to HDL_semaphore */
		SIGN32		id,		/*Semaphore ID in $SemaHub */
		SIGN32		empty,		/*Interrupt enable for CPU at condition 'empty' */
		SIGN32		full,		/*Interrupt enable for CPU at condition 'full' */
		SIGN32		almostEmpty,	/*Interrupt enable for CPU at condition 'almostEmpty' */
		SIGN32		almostFull,	/*Interrupt enable for CPU at condition 'almostFull' */
		SIGN32		cpu		/*CPU ID (0/1/2) */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaINTR_mask	mask;
	UNSG32 a;
	a = sem->ra + RA_SemaHub_ARR + id*sizeof(SIE_Semaphore);

	mask.u32 = 0;
	mask.umask_empty = empty;
	mask.umask_full = full;
	mask.umask_almostEmpty = almostEmpty;
	mask.umask_almostFull = almostFull;

	IO32WR(mask.u32, a + RA_Semaphore_INTR + cpu*sizeof(SIE_SemaINTR));
	/**	ENDOFFUNCTION: semaphore_intr_enable **/
}

/******************************************************************************
*	Function: semaphore_query
*	Description: Query current status (counter & pointer) of a semaphore.
*	Return:	UNSG32		Current available unit level
*******************************************************************************/
UNSG32	semaphore_query(
		void	*hdl,	/*	Handle to HDL_semaphore */
		SIGN32	id,	/*	Semaphore ID in $SemaHub */
		SIGN32	master,	/*	0/1 as procuder/consumer query */
		UNSG32	*ptr	/*	Non-zero to receive semaphore r/w pointer */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaQuery_RESP	resp;
	T32SemaQueryMap_ADDR map;

	map.u32 = 0; map.uADDR_ID = id; map.uADDR_master = master;
	IO32RD(resp.u32, sem->ra + RA_SemaHub_Query + map.u32);

	if (ptr) *ptr = resp.uRESP_PTR;
	return resp.uRESP_CNT;
	/**	ENDOFFUNCTION: semaphore_query **/
}

/******************************************************************************
*	Function: semaphore_push
*	Description: Producer semaphore push.
*******************************************************************************/
void	semaphore_push(
			void	*hdl,	/*	Handle to HDL_semaphore */
			SIGN32	id,	/*	Semaphore ID in $SemaHub */
			SIGN32	delta	/*	Delta to push as a producer */
			)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaHub_PUSH		push;

	push.u32 = 0; push.uPUSH_ID = id; push.uPUSH_delta = delta;
	IO32WR(push.u32, sem->ra + RA_SemaHub_PUSH);
	/**	ENDOFFUNCTION: semaphore_push **/
}

/******************************************************************************
*	Function: semaphore_push
*	Description: Consumer semaphore pop.
*******************************************************************************/
void	semaphore_pop(
		void	*hdl,		/*	Handle to HDL_semaphore */
		SIGN32	id,		/*	Semaphore ID in $SemaHub */
		SIGN32	delta		/*	Delta to pop as a consumer */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	T32SemaHub_POP		pop;

	pop.u32 = 0; pop.uPOP_ID = id; pop.uPOP_delta = delta;
	IO32WR(pop.u32, sem->ra + RA_SemaHub_POP);
	/**	ENDOFFUNCTION: semaphore_pop **/
}

/******************************************************************************
*	Function: semaphore_chk_empty
*	Description: Check 'empty' status of a semaphore (or all semaphores).
*	Return:	UNSG32	status bit of given semaphore, or
*			status bits of all semaphores if id==-1
*******************************************************************************/
UNSG32	semaphore_chk_empty(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id		/*Semaphore ID in $SemaHub
					-1 to return all 32b of the interrupt status
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	UNSG32 d;

	IO32RD(d, sem->ra + RA_SemaHub_empty);
	return (id < 0) ? d : bTST(d, id);
	/**	ENDOFFUNCTION: semaphore_chk_empty **/
}

/******************************************************************************
*	Function: semaphore_chk_full
*	Description: Check 'full' status of a semaphore (or all semaphores).
*	Return:	UNSG32	status bit of given semaphore, or
*			status bits of all semaphores if id==-1
*******************************************************************************/
UNSG32	semaphore_chk_full(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id		/*Semaphore ID in $SemaHub
					-1 to return all 32b of the interrupt status
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	UNSG32 d;

	IO32RD(d, sem->ra + RA_SemaHub_full);
	return (id < 0) ? d : bTST(d, id);
	/**	ENDOFFUNCTION: semaphore_chk_full **/
}

/******************************************************************************
*	Function: semaphore_chk_almostEmpty
*	Description: Check 'almostEmpty' status of a semaphore (or all semaphores).
*	Return:	UNSG32	status bit of given semaphore, or
*			status bits of all semaphores if id==-1
*******************************************************************************/
UNSG32	semaphore_chk_almostEmpty(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id		/*Semaphore ID in $SemaHub
					-1 to return all 32b of the interrupt status
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	UNSG32 d;

	IO32RD(d, sem->ra + RA_SemaHub_almostEmpty);
	return (id < 0) ? d : bTST(d, id);
	/**	ENDOFFUNCTION: semaphore_chk_almostEmpty **/
}

/******************************************************************************
*	Function: semaphore_chk_almostFull
*	Description: Check 'almostFull' status of a semaphore (or all semaphores).
*	Return:	UNSG32	status bit of given semaphore, or
*			status bits of all semaphores if id==-1
*******************************************************************************/
UNSG32	semaphore_chk_almostFull(
		void	*hdl,		/*Handle to HDL_semaphore */
		SIGN32	id		/*Semaphore ID in $SemaHub
					-1 to return all 32b of the interrupt status
					*/
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;
	UNSG32 d;

	IO32RD(d, sem->ra + RA_SemaHub_almostFull);
	return (id < 0) ? d : bTST(d, id);
	/**	ENDOFFUNCTION: semaphore_chk_almostFull **/
}

/******************************************************************************
*	Function: semaphore_clr_empty
*	Description: Clear 'empty' status of a semaphore.
*******************************************************************************/
void	semaphore_clr_empty(
		void	*hdl,		/*	Handle to HDL_semaphore */
		SIGN32	id		/*	Semaphore ID in $SemaHub */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	IO32WR(1<<id, sem->ra + RA_SemaHub_empty);
	/**	ENDOFFUNCTION: semaphore_clr_empty **/
}

/******************************************************************************
*	Function: semaphore_clr_full
*	Description: Clear 'full' status of a semaphore.
*******************************************************************************/
void	semaphore_clr_full(
		void	*hdl,		/*	Handle to HDL_semaphore */
		SIGN32	id		/*	Semaphore ID in $SemaHub */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	IO32WR(1<<id, sem->ra + RA_SemaHub_full);
	/**	ENDOFFUNCTION: semaphore_clr_full **/
}

/******************************************************************************
*	Function: semaphore_clr_almostEmpty
*	Description: Clear 'almostEmpty' status of a semaphore.
*******************************************************************************/
void	semaphore_clr_almostEmpty(
		void	*hdl,		/*	Handle to HDL_semaphore */
		SIGN32	id		/*	Semaphore ID in $SemaHub */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	IO32WR(1<<id, sem->ra + RA_SemaHub_almostEmpty);
	/**	ENDOFFUNCTION: semaphore_clr_almostEmpty **/
}

/******************************************************************************
*	Function: semaphore_clr_almostFull
*	Description: Clear 'almostFull' status of a semaphore.
*******************************************************************************/
void	semaphore_clr_almostFull(
		void	*hdl,		/*	Handle to HDL_semaphore */
		SIGN32	id		/*	Semaphore ID in $SemaHub */
		)
{
	HDL_semaphore		*sem = (HDL_semaphore*)hdl;

	IO32WR(1<<id, sem->ra + RA_SemaHub_almostFull);
	/**	ENDOFFUNCTION: semaphore_clr_almostFull **/
}

/**	ENDOFSECTION
*/




/**	SECTION - API definitions for $HBO
*/
/******************************************************************************
*	Function: hbo_hdl
*	Description: Initialize HDL_hbo with a $HBO BIU instance.
*******************************************************************************/
void	hbo_hdl(
		UNSG32	mem,		/*Base address of HBO SRAM */
		UNSG32	ra,		/*Base address of a BIU instance of $HBO */
		void	*hdl		/*Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);

	hbo->mem = mem; hbo->ra = ra;
	semaphore_hdl(ra + RA_HBO_FiFoCtl, fifoCtl);
	/**	ENDOFFUNCTION: hbo_hdl **/
}

/******************************************************************************
*	Function: hbo_fifoCtl
*	Description: Get HDL_semaphore pointer from a HBO instance.
*	Return:	void*		Handle for HBO.FiFoCtl
*******************************************************************************/
void*	hbo_fifoCtl(
		void	*hdl		/*Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);

	return fifoCtl;
	/**	ENDOFFUNCTION: hbo_fifoCtl **/
}

/******************************************************************************
*	Function: hbo_queue_cfg
*	Description: Configurate a FIFO's base, depth & reset pointers.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	hbo_queue_cfg(
		void	*hdl,		/*Handle to HDL_hbo */
		SIGN32	id,		/*Queue ID in $HBO */
		UNSG32	base,		/*Channel FIFO base address (byte address) */
		SIGN32	depth,		/*Channel FIFO depth, in 64b word */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init HBO, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	HDL_semaphore		*fifoCtl = &(hbo->fifoCtl);
	T32FiFo_CFG			cfg;
	UNSG32 i = 0, a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);
	IO32CFG(cfgQ, i, a + RA_FiFo_START, 0);

	cfg.u32 = 0; cfg.uCFG_BASE = hbo->base[id] = base;
	IO32CFG(cfgQ, i, a + RA_FiFo_CFG, cfg.u32);

	i += semaphore_cfg(fifoCtl, id, depth, cfgQ ? (cfgQ + i) : NULL);
	IO32CFG(cfgQ, i, a + RA_FiFo_START, enable);

	return i;
	/**	ENDOFFUNCTION: hbo_queue_cfg **/
}

/******************************************************************************
*	Function: hbo_queue_enable
*	Description: HBO FIFO enable/disable.
*	Return:	UNSG32	Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	hbo_queue_enable(
		void	*hdl,		/*Handle to HDL_hbo */
		SIGN32	id,		/*Queue ID in $HBO */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init HBO, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_hbo			*hbo = (HDL_hbo*)hdl;
	UNSG32 i = 0, a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

	IO32CFG(cfgQ, i, a + RA_FiFo_START, enable);
	return i;
	/**	ENDOFFUNCTION: hbo_queue_enable **/
}

/******************************************************************************
*	Function: hbo_queue_clear
*	Description: Issue HBO FIFO clear (will NOT wait for finish).
*******************************************************************************/
void	hbo_queue_clear(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id		/*	Queue ID in $HBO */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	UNSG32 a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

	IO32WR(1, a + RA_FiFo_CLEAR);
	/**	ENDOFFUNCTION: hbo_queue_enable **/
}

/******************************************************************************
*	Function: hbo_queue_busy
*	Description: Read HBO 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		'BUSY' status bits of all channels
*******************************************************************************/
UNSG32	hbo_queue_busy(
		void	*hdl		/*	Handle to HDL_hbo */
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	UNSG32 d;

	IO32RD(d, hbo->ra + RA_HBO_BUSY);
	return d;
	/**	ENDOFFUNCTION: hbo_queue_busy **/
}

/******************************************************************************
*	Function: hbo_queue_clear_done
*	Description: Wait for a given channel or all channels to be cleared.
*******************************************************************************/
void	hbo_queue_clear_done(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id		/*	Queue ID in $HBO
						-1 to wait for all channel clear done
						*/
		)
{
	UNSG32 d;
	do {
	d = hbo_queue_busy(hdl);
	if (id >= 0) d = bTST(d, id);
	} while(d);

	/**	ENDOFFUNCTION: hbo_queue_clear_done **/
}

/******************************************************************************
*	Function: hbo_queue_read
*	Description: Read a number of 64b data & pop FIFO from HBO SRAM.
*	Return:	UNSG32		Number of 64b data being read (=n), or (when cfgQ==NULL)
*				0 if there're not sufficient data in FIFO
*******************************************************************************/
UNSG32	hbo_queue_read(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id,		/*	Queue ID in $HBO */
		SIGN32	n,		/*	Number 64b entries to read */
		T64b	data[],		/*	To receive read data */
		UNSG32	*ptr		/*	Pass in current FIFO pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						*/
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
    SIGN32 i;
	UNSG32 p, base, depth;

	base = hbo->mem + hbo->base[id]; depth = hbo->fifoCtl.depth[id];
	i = hbo_queue_query(hdl, id, SemaQueryMap_ADDR_master_consumer, &p);
	if (i < n)
		return 0;

	if (ptr) p = *ptr;
	for (i = 0; i < n; i ++) {
		IO32RD(data[i][0], base + p*8);
		IO32RD(data[i][1], base + p*8 + 4);
		ModInc(p, 1, depth);
				}
	hbo_queue_pop(hdl, id, n);
	if (ptr) *ptr = p;
	return n;
	/**	ENDOFFUNCTION: hbo_queue_read **/
}

/******************************************************************************
*	Function: hbo_queue_write
*	Description: Write a number of 64b data & push FIFO to HBO SRAM.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*				0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	hbo_queue_write(
		void	*hdl,		/*	Handle to HDL_hbo */
		SIGN32	id,		/*	Queue ID in $HBO */
		SIGN32	n,		/*	Number 64b entries to write */
		T64b	data[],		/*	Write data */
		T64b	cfgQ[],		/*	Pass NULL to directly update HBO, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						*/
		UNSG32	*ptr		/*	Pass in current FIFO pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						*/
		)
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
    SIGN32 i, p, depth;
	UNSG32 base, j = 0;

	base = hbo->mem + hbo->base[id]; depth = hbo->fifoCtl.depth[id];
	if (!ptr) {
		i = hbo_queue_query(hdl, id, SemaQueryMap_ADDR_master_producer, &p);
		if(i > depth - n)
			return 0;
	} else  {
		p = *ptr;
	}

	for (i = 0; i < n; i ++) {
		IO32CFG(cfgQ, j, base + p*8, data[i][0]);
		IO32CFG(cfgQ, j, base + p*8 + 4, data[i][1]);
		ModInc(p, 1, depth);
				}
	if (!cfgQ)
		hbo_queue_push(hdl, id, n);
	else {
		T32SemaHub_PUSH	push;
		push.u32 = 0; push.uPUSH_ID = id; push.uPUSH_delta = n;
		IO32CFG(cfgQ, j, hbo->fifoCtl.ra + RA_SemaHub_PUSH, push.u32);
				}
	if (ptr) *ptr = p;
	return j;
	/**	ENDOFFUNCTION: hbo_queue_write **/
}

/**	ENDOFSECTION
*/



/**	SECTION - API definitions for $dHubReg
*/
/******************************************************************************
*	Function: dhub_hdl
*	Description: Initialize HDL_dhub with a $dHub BIU instance.
*******************************************************************************/
void	dhub_hdl(
		UNSG32	mem,		/*	Base address of dHub.HBO SRAM */
		UNSG32	ra,		/*	Base address of a BIU instance of $dHub */
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub		*dhub = (HDL_dhub*)hdl;
	HDL_hbo			*hbo = &(dhub->hbo);
	HDL_semaphore		*semaHub = &(dhub->semaHub);

	dhub->ra = ra;
	semaphore_hdl(ra + RA_dHubReg_SemaHub, semaHub);
	hbo_hdl(mem, ra + RA_dHubReg_HBO, hbo );
	/**	ENDOFFUNCTION: dhub_hdl **/
}

/******************************************************************************
*	Function: dhub_semaphore
*	Description: Get HDL_semaphore pointer from a dHub instance.
*	Return:	void*	Handle for dHub.SemaHub
*******************************************************************************/
void*	dhub_semaphore(
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_semaphore		*semaHub = &(dhub->semaHub);

	return semaHub;
	/**	ENDOFFUNCTION: dhub_semaphore **/
}

/******************************************************************************
*	Function: dhub_hbo
*	Description: Get HDL_hbo pointer from a dHub instance.
*	Return:	void*		Handle for dHub.HBO
*******************************************************************************/
void*	dhub_hbo(
	void	*hdl		/*	Handle to HDL_dhub */
	)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);

	return hbo;
	/**	ENDOFFUNCTION: dhub_hbo **/
}

/******************************************************************************
*	Function: dhub_channel_cfg
*	Description: Configurate a dHub channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*				0 if either cmdQ or dataQ in HBO is still busy
*******************************************************************************/
UNSG32	dhub_channel_cfg(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		UNSG32	baseCmd,	/*Channel FIFO base address (byte address) for cmdQ */
		UNSG32	baseData,	/*Channel FIFO base address (byte address) for dataQ */
		SIGN32	depthCmd,	/*Channel FIFO depth for cmdQ, in 64b word */
		SIGN32	depthData,	/*Channel FIFO depth for dataQ, in 64b word */
		SIGN32	MTU,		/*See 'dHubChannel.CFG.MTU' */
		SIGN32	QoS,		/*See 'dHubChannel.CFG.QoS' */
		SIGN32	selfLoop,	/*See 'dHubChannel.CFG.selfLoop' */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init dHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);
	T32dHubChannel_CFG	cfg;
	UNSG32 i = 0, a, busyStatus, cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

	xdbg ("hal_dhub::  value of id is %0d \n" , id ) ;
	xdbg ("hal_dhub::  value of baseCmd   is %0d \n" , baseCmd ) ;
	xdbg ("hal_dhub::  value of baseData  is %0d \n" , baseData ) ;
	xdbg ("hal_dhub::  value of depthCmd  is %0d \n" , depthCmd ) ;
	xdbg ("hal_dhub::  value of depthData is %0d \n" , depthData ) ;
	xdbg ("hal_dhub::  value of MTU       is %0d \n" , MTU ) ;
	xdbg ("hal_dhub::  value of QOS       is %0d \n" , QoS ) ;
	xdbg ("hal_dhub::  value of SelfLoop  is %0d \n" , selfLoop ) ;
	xdbg ("hal_dhub::  value of Enable    is %0d \n" , enable ) ;

	if (!cfgQ) {
		hbo_queue_enable(hbo,  cmdID, 0, NULL);
		hbo_queue_clear(hbo,  cmdID);
		hbo_queue_enable(hbo, dataID, 0, NULL);
		hbo_queue_clear(hbo, dataID);
		busyStatus = hbo_queue_busy(hbo);
		//if(bTST(busyStatus, cmdID) || bTST(busyStatus, dataID))
		//	return 0;
						}
	a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);
	xdbg ("hal_dhub::  value of Channel Addr    is %0x \n" , a ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_START, 0);

	cfg.u32 = 0; cfg.uCFG_MTU = MTU; cfg.uCFG_QoS = QoS; cfg.uCFG_selfLoop = selfLoop;
	switch (MTU) {
		case dHubChannel_CFG_MTU_8byte	: dhub->MTUb[id] = 3; break;
		case dHubChannel_CFG_MTU_32byte	: dhub->MTUb[id] = 5; break;
		case dHubChannel_CFG_MTU_128byte: dhub->MTUb[id] = 7; break;
		case dHubChannel_CFG_MTU_1024byte : dhub->MTUb[id] = 10; break;
						}
	xdbg ("hal_dhub::  addr of ChannelCFG is %0x data is %0x \n",
				a + RA_dHubChannel_CFG , cfg.u32  ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_CFG, cfg.u32);

	i += hbo_queue_cfg(hbo,  cmdID, baseCmd , depthCmd,
				enable, cfgQ ? (cfgQ + i) : NULL);
	i += hbo_queue_cfg(hbo, dataID, baseData, depthData,
				enable, cfgQ ? (cfgQ + i) : NULL);
	xdbg ("hal_dhub::  addr of ChannelEN is %0x data is %0x \n" ,
				a + RA_dHubChannel_START , enable  ) ;
	IO32CFG(cfgQ, i, a + RA_dHubChannel_START, enable);

	return i;
	/**	ENDOFFUNCTION: dhub_channel_cfg **/
}

/******************************************************************************
*	Function: dhub_channel_enable
*	Description: dHub channel enable/disable.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	dhub_channel_enable(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		SIGN32	enable,		/*0 to disable, 1 to enable */
		T64b	cfgQ[]		/*Pass NULL to directly init dHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	UNSG32 i = 0, a;
	a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);

	IO32CFG(cfgQ, i, a + RA_dHubChannel_START, enable);
	return i;
	/**	ENDOFFUNCTION: dhub_channel_enable **/
}

/******************************************************************************
*	Function: dhub_channel_clear
*	Description: Issue dHub channel clear (will NOT wait for finish).
*******************************************************************************/
void	dhub_channel_clear(
		void	*hdl,	/*	Handle to HDL_dhub */
		SIGN32	id	/*	Channel ID in $dHubReg */
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	UNSG32 a;
	a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);

	IO32WR(1, a + RA_dHubChannel_CLEAR);
	/**	ENDOFFUNCTION: dhub_channel_clear **/
}

/******************************************************************************
*	Function: dhub_channel_flush
*	Description: Issue dHub channel (H2M only) flush (will NOT wait for finish).
*******************************************************************************/
void	dhub_channel_flush(
		void	*hdl,		/*	Handle to HDL_dhub */
		SIGN32	id		/*	Channel ID in $dHubReg */
		)
{
	UNSG32 a;
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);

	IO32WR(1, a + RA_dHubChannel_FLUSH);
	/**	ENDOFFUNCTION: dhub_channel_flush **/
}

/******************************************************************************
*	Function: dhub_channel_busy
*	Description: Read dHub 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		'BUSY' status bits of all channels
*******************************************************************************/
UNSG32	dhub_channel_busy(
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	UNSG32 d;

	IO32RD(d, dhub->ra + RA_dHubReg_BUSY);
	return d;
	/**	ENDOFFUNCTION: dhub_channel_busy **/
}

/******************************************************************************
*	Function: dhub_channel_pending
*	Description: Read dHub 'PENDING' status for all channel FIFOs.
*	Return:	UNSG32		'PENDING' status bits of all channels
*******************************************************************************/
UNSG32	dhub_channel_pending(
		void	*hdl		/*	Handle to HDL_dhub */
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	UNSG32 d;

	IO32RD(d, dhub->ra + RA_dHubReg_PENDING);
	return d;
	/**	ENDOFFUNCTION: dhub_channel_pending **/
}

/******************************************************************************
*	Function: dhub_channel_clear_done
*	Description: Wait for a given channel or all channels to be cleared.
*******************************************************************************/
void	dhub_channel_clear_done(
		void	*hdl,		/*	Handle to HDL_dhub */
		SIGN32	id		/*	Channel ID in $dHubReg
						-1 to wait for all channel clear done
						*/
		)
{
	UNSG32 d;
	do {
	d = dhub_channel_busy(hdl);
	d |= dhub_channel_pending(hdl);
	if (id >= 0) d = bTST(d, id);
	} while(d);

	/**	ENDOFFUNCTION: dhub_channel_clear_done **/
}

/******************************************************************************
*	Function: dhub_channel_write_cmd
*	Description: Write a 64b command for a dHub channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ if success, or
*				0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	dhub_channel_write_cmd(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		UNSG32	addr,		/*CMD: buffer address */
		SIGN32	size,		/*CMD: number of bytes to transfer */
		SIGN32	semOnMTU,	/*CMD: semaphore operation at CMD/MTU (0/1) */
		SIGN32	chkSemId,	/*CMD: non-zero to check semaphore */
		SIGN32	updSemId,	/*CMD: non-zero to update semaphore */
		SIGN32	interrupt,	/*CMD: raise interrupt at CMD finish */
		T64b	cfgQ[],		/*Pass NULL to directly update dHub, or
					Pass non-zero to receive programming sequence
					in (adr,data) pairs
					*/
		UNSG32	*ptr		/*Pass in current cmdQ pointer (in 64b word),
					& receive updated new pointer,
					Pass NULL to read from HW
					*/
		)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);
	SIE_dHubCmd			cmd;
	SIGN32 i;

	cmd.ie_HDR.u32dHubCmdHDR_DESC = 0;
	i = size >> dhub->MTUb[id];
	if((i << dhub->MTUb[id]) < size)
		cmd.ie_HDR.uDESC_size = size;
	else {
		cmd.ie_HDR.uDESC_sizeMTU = 1;
		cmd.ie_HDR.uDESC_size = i;
						}
	cmd.ie_HDR.uDESC_chkSemId = chkSemId; cmd.ie_HDR.uDESC_updSemId = updSemId;
	cmd.ie_HDR.uDESC_semOpMTU = semOnMTU; cmd.ie_HDR.uDESC_interrupt = interrupt;
	cmd.uMEM_addr = addr;

	return hbo_queue_write(hbo, dhub_id2hbo_cmdQ(id), 1, (T64b*)&cmd, cfgQ, ptr);
	/**	ENDOFFUNCTION: dhub_channel_write_cmd **/
}


void	dhub_channel_generate_cmd(
		void	*hdl,		/*Handle to HDL_dhub */
		SIGN32	id,		/*Channel ID in $dHubReg */
		UNSG32	addr,		/*CMD: buffer address */
		SIGN32	size,		/*CMD: number of bytes to transfer */
		SIGN32	semOnMTU,	/*CMD: semaphore operation at CMD/MTU (0/1) */
		SIGN32	chkSemId,	/*CMD: non-zero to check semaphore */
		SIGN32	updSemId,	/*CMD: non-zero to update semaphore */
		SIGN32	interrupt,	/*CMD: raise interrupt at CMD finish */
		SIGN32	*pData
					)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	SIE_dHubCmd			cmd;
	SIGN32 i, *pcmd;

	cmd.ie_HDR.u32dHubCmdHDR_DESC = 0;
	i = size >> dhub->MTUb[id];
	if ((i << dhub->MTUb[id]) < size)
		cmd.ie_HDR.uDESC_size = size;
	else {
		cmd.ie_HDR.uDESC_sizeMTU = 1;
		cmd.ie_HDR.uDESC_size = i;
						}
	cmd.ie_HDR.uDESC_chkSemId = chkSemId; cmd.ie_HDR.uDESC_updSemId = updSemId;
	cmd.ie_HDR.uDESC_semOpMTU = semOnMTU; cmd.ie_HDR.uDESC_interrupt = interrupt;
	cmd.uMEM_addr = addr;

        pcmd = (SIGN32 *)(&cmd);
        pData[0] = pcmd[0];
        pData[1] = pcmd[1];
	/**	ENDOFFUNCTION: dhub_channel_write_cmd **/
}

#ifdef VPP_DIAGS
/******************************************************************************
*	Function: dhub_channel_big_write_cmd
*	Description: Write a sequence of 64b command for a dHub channel.
*	Return:	UNSG32	Number of (adr,pair) added to cfgQ if success, or
*			0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	diag_dhub_channel_big_write_cmd(
		void		*hdl,		/*Handle to HDL_dhub */
		SIGN32		id,		/*Channel ID in $dHubReg */
		UNSG32		addr,		/*CMD: buffer address */
		SIGN32		size,		/*CMD: number of bytes to transfer */
		SIGN32		semOnMTU,	/*CMD: semaphore operation at CMD/MTU (0/1) */
		SIGN32		chkSemId,	/*CMD: non-zero to check semaphore */
		SIGN32		updSemId,	/*CMD: non-zero to update semaphore */
		SIGN32		interrupt,	/*CMD: raise interrupt at CMD finish */
		T64b		cfgQ[],		/*Pass NULL to directly update dHub, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						*/
		UNSG32		*ptr		/*Pass in current cmdQ pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						*/
					)
{
	HDL_dhub			*dhub = (HDL_dhub*)hdl;
	HDL_hbo				*hbo = &(dhub->hbo);
	SIE_dHubCmd			cmd;
	UNSG32				addr_MTU_multiple;
	UNSG32				size_MTU_multiple;
	UNSG32				addr_extra;
	UNSG32				size_extra;
	UNSG32				size_fragment;
	UNSG32				pairs = 0;

	addr_MTU_multiple = addr >> dhub->MTUb[id];
	size_MTU_multiple = size >> dhub->MTUb[id];
	addr_extra = addr - (addr_MTU_multiple << dhub->MTUb[id]);
	size_extra = size - (size_MTU_multiple << dhub->MTUb[id]);

	if (size < (1 << 16))
		pairs += dhub_channel_write_cmd(hdl, id, addr, size,
						semOnMTU, chkSemId,
						updSemId, interrupt, cfgQ, ptr);
	else if (addr_extra)
	{
		size_fragment = (1 << dhub->MTUb[id]) - addr_extra;
		pairs += dhub_channel_write_cmd(hdl, id, addr, size_fragment,
						semOnMTU, chkSemId, updSemId,
						0, cfgQ, ptr);
		pairs += diag_dhub_channel_big_write_cmd(hdl, id, addr + size_fragment,
						size - size_fragment, semOnMTU, chkSemId,
						updSemId, interrupt, cfgQ, ptr);
	}
	else
	{
		if (size < ((1 << 16) << dhub->MTUb[id]))
			if (size_extra)
			{
				pairs += dhub_channel_write_cmd(hdl, id, addr,
						size - size_extra, semOnMTU, chkSemId,
						updSemId, 0, cfgQ, ptr);
				pairs += dhub_channel_write_cmd(hdl, id,
						addr + size - size_extra, size_extra,
						semOnMTU, chkSemId,
						updSemId, interrupt, cfgQ, ptr);
			}
			else
				pairs += dhub_channel_write_cmd(hdl, id, addr, size,
						semOnMTU, chkSemId, updSemId,
						interrupt, cfgQ, ptr);
		else
		{
			size_fragment = 0xFFFF << dhub->MTUb[id];
			pairs += dhub_channel_write_cmd(hdl, id, addr, size_fragment,
						semOnMTU, chkSemId, updSemId, 0, cfgQ, ptr);
			pairs += diag_dhub_channel_big_write_cmd(hdl, id, addr + size_fragment,
						size - size_fragment, semOnMTU, chkSemId,
						updSemId, interrupt, cfgQ, ptr);
		}
	}
	return (pairs);
}
#endif
/******************************************************************************
*	Function: dhub_channel_big_write_cmd
*	Description: Write a sequence of 64b command for a dHub channel.
*	Return:	UNSG32	-Number of (adr,pair) added to cfgQ if success, or
*			0 if there're not sufficient space in FIFO
*******************************************************************************/
UNSG32	dhub_channel_big_write_cmd(
		void	*hdl,		/*	Handle to HDL_dhub */
		SIGN32	id,		/*	Channel ID in $dHubReg */
		UNSG32	addr,		/*	CMD: buffer address */
		SIGN32	size,		/*	CMD: number of bytes to transfer */
		SIGN32	semOnMTU,	/*	CMD: semaphore operation at CMD/MTU (0/1) */
		SIGN32	chkSemId,	/*	CMD: non-zero to check semaphore */
		SIGN32	updSemId,	/*	CMD: non-zero to update semaphore */
		SIGN32	interrupt,	/*	CMD: raise interrupt at CMD finish */
		T64b	cfgQ[],		/*	Pass NULL to directly update dHub, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						*/
		UNSG32	*ptr		/*	Pass in current cmdQ pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						*/
		)
{
	HDL_dhub	*dhub = (HDL_dhub*)hdl;
	SIGN32 i;
	SIGN32 j, jj;

	i = size >> dhub->MTUb[id];
	//size < 64K
	if ( size<(1<<16) )
	{
		j = dhub_channel_write_cmd(hdl, id, addr, size, semOnMTU, chkSemId,
			updSemId, interrupt, cfgQ, ptr);
	}
	else {
		SIGN32 size0, size1;
		size0 = 0xffff << dhub->MTUb[id];
		j = 0;

	//size > 128x64k
		while ( i > 0xffff )
		{
			jj = dhub_channel_write_cmd(hdl, id, addr, size0, semOnMTU,
				chkSemId, updSemId, 0, cfgQ, ptr);

			if (cfgQ) cfgQ += jj;
			j += jj;

			i -= 0xffff;
			size -= size0;
			addr += size0;
		}

		if ( (i << dhub->MTUb[id]) == size )
		{
			j += dhub_channel_write_cmd(hdl, id, addr, size, semOnMTU, chkSemId,
				updSemId, interrupt, cfgQ, ptr);
		}
		else
		{
			size0 = i << dhub->MTUb[id];
			j += dhub_channel_write_cmd(hdl, id, addr, size0, semOnMTU, chkSemId,
				updSemId, 0, cfgQ, ptr);
			if (cfgQ) cfgQ += j;
			addr += size0;
			size1 = size - size0;
			j += dhub_channel_write_cmd(hdl, id, addr, size1, semOnMTU, chkSemId,
				updSemId, interrupt, cfgQ, ptr);
		}
	}

	return (j);
}

/**	ENDOFSECTION
*/



/**	SECTION - API definitions for $dHubReg2D
*/
/******************************************************************************
*	Function: dhub2d_hdl
*	Description: Initialize HDL_dhub2d with a $dHub2D BIU instance.
*******************************************************************************/
void	dhub2d_hdl(	UNSG32	mem,	/*	Base address of dHub2D.dHub.HBO SRAM */
			UNSG32	ra,	/*	Base address of a BIU instance of $dHub2D */
			void	*hdl	/*	Handle to HDL_dhub2d */
			)
{
	HDL_dhub2d	*dhub2d = (HDL_dhub2d*)hdl;
	HDL_dhub	*dhub = &(dhub2d->dhub);

	dhub2d->ra = ra;
	dhub_hdl(mem, ra + RA_dHubReg2D_dHub, dhub);
	/**	ENDOFFUNCTION: dhub2d_hdl **/
}

/******************************************************************************
*	Function: dhub2d_channel_cfg
*	Description: Configurate a dHub2D channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	dhub2d_channel_cfg(
	void		*hdl,		/*	Handle to HDL_dhub2d */
	SIGN32		id,		/*	Channel ID in $dHubReg2D */
	UNSG32		addr,		/*	CMD: 2D-buffer address */
	SIGN32		stride,		/*	CMD: line stride size in bytes */
	SIGN32		width,		/*	CMD: buffer width in bytes */
	SIGN32		height,		/*	CMD: buffer height in lines */
	SIGN32		semLoop,	/*	CMD: loop size (1~4) of semaphore operations */
	SIGN32		semOnMTU,	/*	CMD: semaphore operation at CMD/MTU (0/1) */
	SIGN32		chkSemId[],	/*	CMD: semaphore loop pattern - non-zero to check */
	SIGN32		updSemId[],	/*	CMD: semaphore loop pattern - non-zero to update */
	SIGN32		interrupt,	/*	CMD: raise interrupt at CMD finish */
	SIGN32		enable,		/*	0 to disable, 1 to enable */
	T64b		cfgQ[]		/*	Pass NULL to directly init dHub2D, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
					*/
	)
{
	HDL_dhub2d			*dhub2d = (HDL_dhub2d*)hdl;
	HDL_dhub			*dhub = &(dhub2d->dhub);
	SIE_dHubCmd2D		cmd;
	SIE_dHubCmdHDR		hdr;
	SIGN32 i, size = width;
	UNSG32 a, j = 0;
	a = dhub2d->ra + RA_dHubReg2D_ARR + id*sizeof(SIE_dHubCmd2D);
	IO32CFG(cfgQ, j, a + RA_dHubCmd2D_START, 0);

	cmd.uMEM_addr = addr;
	cmd.uDESC_stride = stride; cmd.uDESC_numLine = height;
	cmd.uDESC_hdrLoop = semLoop; cmd.uDESC_interrupt = interrupt;
	IO32CFG(cfgQ, j, a + RA_dHubCmd2D_MEM, cmd.u32dHubCmd2D_MEM);
	IO32CFG(cfgQ, j, a + RA_dHubCmd2D_DESC, cmd.u32dHubCmd2D_DESC);

	hdr.u32dHubCmdHDR_DESC = 0;
	i = size >> dhub->MTUb[id];
	if ((i << dhub->MTUb[id]) < size)
		hdr.uDESC_size = size;
	else {
		hdr.uDESC_sizeMTU = 1;
		hdr.uDESC_size = i;
						}
	hdr.uDESC_semOpMTU = semOnMTU;
	for (i = 0; i < semLoop; i ++) {
		if (chkSemId) {
			hdr.uDESC_chkSemId = chkSemId[i];
		}
		if (updSemId) {
			hdr.uDESC_updSemId = updSemId[i];
		}
		IO32CFG(cfgQ, j, a + RA_dHubCmd2D_HDR + i*sizeof(SIE_dHubCmdHDR),
					hdr.u32dHubCmdHDR_DESC);
	}
	IO32CFG(cfgQ, j, a + RA_dHubCmd2D_START, enable);

	return j;
	/**	ENDOFFUNCTION: dhub2d_channel_cfg **/
}

/******************************************************************************
*	Function: dhub2d_channel_enable
*	Description: dHub2D channel enable/disable.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*******************************************************************************/
UNSG32	dhub2d_channel_enable(
		void	*hdl,		/*	Handle to HDL_dhub2d */
		SIGN32	id,		/*	Channel ID in $dHubReg2D */
		SIGN32	enable,		/*	0 to disable, 1 to enable */
		T64b	cfgQ[]		/*	Pass NULL to directly init dHub2D, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						*/
		)
{
	HDL_dhub2d			*dhub2d = (HDL_dhub2d*)hdl;
	UNSG32 i = 0, a;
	a = dhub2d->ra + RA_dHubReg2D_ARR + id*sizeof(SIE_dHubCmd2D);

	IO32CFG(cfgQ, i, a + RA_dHubCmd2D_START, enable);
	return i;
	/**	ENDOFFUNCTION: dhub2d_channel_enable **/
}

/******************************************************************************
*	Function: dhub2d_channel_clear
*	Description: Issue dHub2D channel clear (will NOT wait for finish).
*******************************************************************************/
void	dhub2d_channel_clear(
		void	*hdl,	/*	Handle to HDL_dhub2d */
		SIGN32	id	/*	Channel ID in $dHubReg2D */
		)
{
	HDL_dhub2d			*dhub2d = (HDL_dhub2d*)hdl;
	UNSG32 a;
	a = dhub2d->ra + RA_dHubReg2D_ARR + id*sizeof(SIE_dHubCmd2D);

	IO32WR( 1, a + RA_dHubCmd2D_CLEAR);
	return ;
	/**	ENDOFFUNCTION: dhub2d_channel_clear **/
}

/******************************************************************************
*	Function: dhub2d_channel_busy
*	Description: Read dHub2D 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		BUSY' status bits of all channels
*******************************************************************************/
UNSG32	dhub2d_channel_busy(
		void	*hdl	/*	Handle to HDL_dhub2d */
		)
{
	HDL_dhub2d			*dhub2d = (HDL_dhub2d*)hdl;
	UNSG32 d;

	IO32RD(d, dhub2d->ra + RA_dHubReg2D_BUSY);
	return d;
	/**	ENDOFFUNCTION: dhub2d_channel_busy **/
}

/******************************************************************************
*	Function: dhub2d_channel_clear_done
*	Description: Wait for a given channel or all channels to be cleared.
*******************************************************************************/
void	dhub2d_channel_clear_done(
		void	*hdl,	/*	Handle to HDL_dhub2d */
		SIGN32	id	/*	Channel ID in $dHubReg2D
					*/
		)
{
	UNSG32 d;
	do{
	d = dhub2d_channel_busy(hdl);
	if(id >= 0) d = bTST(d, id);
	} while(d);

	/**	ENDOFFUNCTION: dhub2d_channel_clear_done **/
}

void BCM_SCHED_Open(void)
{

}

void BCM_SCHED_Close(void)
{

}

void BCM_SCHED_SetMux(UNSG32 QID, UNSG32 TrigEvent)
{
        UNSG32 addr;


        if (TrigEvent > BCM_SCHED_TRIG_NONE)
            return;

        if (QID <= BCM_SCHED_Q11) {
            addr = RA_AVIO_BCM_Q0 + QID * 4;

            GA_REG_WORD32_WRITE(MEMMAP_AVIO_BCM_REG_BASE + addr, TrigEvent);
        } else if ((QID >= BCM_SCHED_Q14) && (QID <= BCM_SCHED_Q18)) {
            addr = RA_AVIO_BCM_Q14 + (QID-BCM_SCHED_Q14) * 4;

            GA_REG_WORD32_WRITE(MEMMAP_AVIO_BCM_REG_BASE + addr, TrigEvent);
        }

	return;
}

int BCM_SCHED_PushCmd(UNSG32 QID, UNSG32 *pCmd, UNSG32 *cfgQ)
{
	UNSG32 value, addr, j;

	if ((QID > BCM_SCHED_Q18) || !pCmd)
		return -1; /* parameter error */

	if (!cfgQ) {
	    GA_REG_WORD32_READ(MEMMAP_AVIO_BCM_REG_BASE + RA_AVIO_BCM_FULL_STS, &value);
	    if (value & (1 << QID)) {
		return 0; /* Q FIFO is full */
	    }
	}

	if (pCmd) {
                j = 0;
		addr = QID * 4 * 2;
                if (cfgQ) {
                    cfgQ[0] = pCmd[0];
                    cfgQ[1] = MEMMAP_AVIO_BCM_REG_BASE + addr;
                    cfgQ[2] = pCmd[1];
                    cfgQ[3] = MEMMAP_AVIO_BCM_REG_BASE + addr + 4;
                } else {

	            GA_REG_WORD32_WRITE(MEMMAP_AVIO_BCM_REG_BASE + addr, pCmd[0]);
	            GA_REG_WORD32_WRITE(MEMMAP_AVIO_BCM_REG_BASE + addr + 4, pCmd[1]);

                }
                j = 2;
	}

        return j;
}

void BCM_SCHED_GetEmptySts(UNSG32 QID, UNSG32 *EmptySts)
{
    UNSG32 bcmQ_sts;

    GA_REG_WORD32_READ(MEMMAP_AVIO_BCM_REG_BASE + RA_AVIO_BCM_EMP_STS, &bcmQ_sts);
    if (bcmQ_sts & (1 << QID)) {
        *EmptySts = 1;
    } else {
        *EmptySts = 0;
    }
}

/****************************************************
 *  dhub2d_channel_clear_seq()
 *
 *  dHub2D channel clear sequence
 ****************************************************/
void dhub2d_channel_clear_seq(void *hdl, SIGN32 id)
{
    UNSG32 cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

    dhub2d_channel_clear((HDL_dhub2d *)hdl, id);

    hbo_queue_enable(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID, 0, NULL);

    dhub_channel_enable(&((HDL_dhub2d *)hdl)->dhub, id, 0, NULL);
    dhub_channel_clear(&((HDL_dhub2d *)hdl)->dhub, id);

    dhub_channel_clear_done(&((HDL_dhub2d *)hdl)->dhub, id);

    hbo_queue_clear(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID);

    hbo_queue_enable(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID, 0, NULL);
    hbo_queue_clear(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID);
}

/****************************************************
 *  dhub2d_channel_start_seq()
 *
 *  dHub2D channel start sequence
 ****************************************************/
void dhub2d_channel_start_seq(void *hdl, SIGN32 id)
{
    UNSG32 cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

    hbo_queue_enable(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID, 1, NULL);
    hbo_queue_enable(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID, 1, NULL);

    dhub_channel_enable(&((HDL_dhub2d *)hdl)->dhub, id, 1, NULL);
}

/******************************************************************************
 *  DHub/HBO clear/disable/enable functions
 *  Same as original ones, but use BCM buffer for VIP VBI clear sequence.
 *******************************************************************************/
typedef struct VIP_BCMBUF_T {
    unsigned int *head;          // head of total BCM buffer
    unsigned int *tail;          // tail of the buffer, used for checking wrap around
    unsigned int *writer;        // write pointer of queue
    int size;                    // size of total BCM buffer
} VIP_BCMBUF;
void	dhub_channel_clear_bcmbuf(
        void		*hdl,			/*Handle to HDL_dhub */
        SIGN32		id,			/*Channel ID in $dHubReg */
        VIP_BCMBUF *pbcmbuf
        )
{
    HDL_dhub			*dhub = (HDL_dhub*)hdl;
    UNSG32 a;
    a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);

    if (pbcmbuf == NULL)
        return;

    if(pbcmbuf->writer == pbcmbuf->tail){
        /*the buffer is full, no space for wrap around*/
        return;
    }

    /*save the data to the buffer*/
   *pbcmbuf->writer = 1;
    pbcmbuf->writer ++;
   *pbcmbuf->writer = a + RA_dHubChannel_CLEAR;
    pbcmbuf->writer ++;

    //IO32WR(1, a + RA_dHubChannel_CLEAR);
}

UNSG32	dhub_channel_enable_bcmbuf(
        void		*hdl,			/*Handle to HDL_dhub */
        SIGN32		id,			/*Channel ID in $dHubReg */
        SIGN32		enable,			/*0 to disable, 1 to enable */
        VIP_BCMBUF *pbcmbuf
        )
{
    HDL_dhub			*dhub = (HDL_dhub*)hdl;
    UNSG32 i = 0, a;
    a = dhub->ra + RA_dHubReg_ARR + id*sizeof(SIE_dHubChannel);

    *pbcmbuf->writer = enable;
    pbcmbuf->writer ++;
    *pbcmbuf->writer = a + RA_dHubChannel_START;
    pbcmbuf->writer ++;

    //IO32CFG(cfgQ, i, a + RA_dHubChannel_START, enable);
    return i;
}

void	hbo_queue_clear_bcmbuf(
        void	*hdl,		/*	Handle to HDL_hbo */
        SIGN32	id,		/*	Queue ID in $HBO */
        VIP_BCMBUF *pbcmbuf
        )
{
    HDL_hbo				*hbo = (HDL_hbo*)hdl;
    UNSG32 a;
    a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

    if (pbcmbuf == NULL)
        return;

    if(pbcmbuf->writer == pbcmbuf->tail){
        /*the buffer is full, no space for wrap around*/
        return;
    }

    /*save the data to the buffer*/
    *pbcmbuf->writer = 1;
    pbcmbuf->writer ++;
    *pbcmbuf->writer = a + RA_FiFo_CLEAR;
    pbcmbuf->writer ++;

    //IO32WR(1, a + RA_FiFo_CLEAR);
    /**	ENDOFFUNCTION: hbo_queue_enable **/
}

UNSG32	hbo_queue_enable_bcmbuf(
        void	*hdl,		/*	Handle to HDL_hbo */
        SIGN32	id,		/*	Queue ID in $HBO */
        SIGN32	enable,		/*	0 to disable, 1 to enable */
        VIP_BCMBUF *pbcmbuf
        )
{
	HDL_hbo				*hbo = (HDL_hbo*)hdl;
	UNSG32 i = 0, a;
	a = hbo->ra + RA_HBO_ARR + id*sizeof(SIE_FiFo);

	*pbcmbuf->writer = enable;
	pbcmbuf->writer ++;
	*pbcmbuf->writer = a + RA_FiFo_START;
	pbcmbuf->writer ++;

	//IO32CFG(cfgQ, i, a + RA_FiFo_START, enable);
	return i;
}

void	dhub2d_channel_clear_bcmbuf(
	void		*hdl,		/*	Handle to HDL_dhub2d */
	SIGN32		id,		/*	Channel ID in $dHubReg2D */
        VIP_BCMBUF *pbcmbuf
	)
{
	HDL_dhub2d			*dhub2d = (HDL_dhub2d*)hdl;
	UNSG32 a;
	a = dhub2d->ra + RA_dHubReg2D_ARR + id*sizeof(SIE_dHubCmd2D);

	/*save the data to the buffer*/
	*pbcmbuf->writer = 1;
	pbcmbuf->writer ++;
	*pbcmbuf->writer = a + RA_dHubCmd2D_CLEAR;
	pbcmbuf->writer ++;

	//IO32WR( 1, a + RA_dHubCmd2D_CLEAR);
	return ;
	/**	ENDOFFUNCTION: dhub2d_channel_clear **/
}


/****************************************************
 *  dhub2d_channel_clear_seq()
 *
 *  dHub 2D channel clear sequence
 *  No wait, return immediately. Use BCM buffer.
 ****************************************************/
void dhub2d_channel_clear_seq_bcm(void *hdl, SIGN32 id, VIP_BCMBUF *pbcmbuf)
{
	UNSG32 cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

	dhub2d_channel_clear_bcmbuf((HDL_dhub2d *)hdl, id, pbcmbuf);

	hbo_queue_enable_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID, 0, pbcmbuf);

	dhub_channel_enable_bcmbuf(&((HDL_dhub2d *)hdl)->dhub, id, 0, pbcmbuf);
	dhub_channel_clear_bcmbuf(&((HDL_dhub2d *)hdl)->dhub, id, pbcmbuf);

	hbo_queue_clear_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID, pbcmbuf);

	hbo_queue_enable_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID, 0, pbcmbuf);
	hbo_queue_clear_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID, pbcmbuf);

	//restart!
	hbo_queue_enable_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), cmdID, 1, pbcmbuf);
	hbo_queue_enable_bcmbuf(dhub_hbo(&((HDL_dhub2d *)hdl)->dhub), dataID, 1, pbcmbuf);
	dhub_channel_enable_bcmbuf(&((HDL_dhub2d *)hdl)->dhub, id, 1, pbcmbuf);
}




/****************************************************
 *  dhub_channel_clear_seq()
 *
 *  dHub channel clear sequence
 *  No wait, return immediately. Use BCM buffer.
 ****************************************************/
void dhub_channel_clear_seq(void *hdl, SIGN32 id, VIP_BCMBUF *pbcmbuf)
{
	UNSG32 cmdID = dhub_id2hbo_cmdQ(id), dataID = dhub_id2hbo_data(id);

	hbo_queue_enable_bcmbuf(dhub_hbo(hdl), cmdID, 0, pbcmbuf);

	dhub_channel_enable_bcmbuf(hdl, id, 0, pbcmbuf);

	dhub_channel_clear_bcmbuf(hdl, id, pbcmbuf);

	hbo_queue_clear_bcmbuf(dhub_hbo(hdl), cmdID, pbcmbuf);

	hbo_queue_enable_bcmbuf(dhub_hbo(hdl), dataID, 0, pbcmbuf);

	hbo_queue_clear_bcmbuf(dhub_hbo(hdl), dataID, pbcmbuf);

	//restart!
	hbo_queue_enable_bcmbuf(dhub_hbo(hdl), cmdID, 1, pbcmbuf);
	hbo_queue_enable_bcmbuf(dhub_hbo(hdl), dataID, 1, pbcmbuf);
	dhub_channel_enable_bcmbuf(hdl, id, 1, pbcmbuf);
}


/**	ENDOFSECTION
 */

/**	ENDOFFILE: hal_dhub.c
 */
#include <linux/module.h>
EXPORT_SYMBOL(dhub2d_channel_cfg);
EXPORT_SYMBOL(dhub_channel_big_write_cmd);
EXPORT_SYMBOL(dhub_channel_write_cmd);
EXPORT_SYMBOL(semaphore_intr_enable);
EXPORT_SYMBOL(semaphore_cfg);
EXPORT_SYMBOL(dhub_semaphore);
EXPORT_SYMBOL(dhub_channel_cfg);
EXPORT_SYMBOL(dhub_channel_generate_cmd);
EXPORT_SYMBOL(dhub_channel_clear_done);
EXPORT_SYMBOL(dhub_channel_enable);
EXPORT_SYMBOL(dhub_channel_clear);
EXPORT_SYMBOL(dhub_hdl);
EXPORT_SYMBOL(semaphore_chk_full);
EXPORT_SYMBOL(semaphore_clr_full);
EXPORT_SYMBOL(semaphore_pop);
EXPORT_SYMBOL(BCM_SCHED_PushCmd);
EXPORT_SYMBOL(BCM_SCHED_GetEmptySts);
EXPORT_SYMBOL(hbo_queue_clear);
EXPORT_SYMBOL(hbo_queue_enable);
EXPORT_SYMBOL(hbo_queue_clear_done);
EXPORT_SYMBOL(dhub2d_hdl);
