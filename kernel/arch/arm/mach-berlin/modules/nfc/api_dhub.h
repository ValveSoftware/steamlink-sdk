/******************************************************************************************************
*       Copyright (C) 2007-2011
*       Copyright © 2007 Marvell International Ltd.
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
*
*	$Log: vsys#comm#cmodel#hal#drv#api_dhub.h,v $
*	Revision 1.4  2007-10-30 10:58:43-07  chengjun
*	Remove interrpt from HBO to semaphore. Map dHub channel 0 interrupt to semaphore channel 0.
*
*	Revision 1.3  2007-10-24 21:41:17-07  kbhatt
*	add structures to api_dhub.h
*
*	Revision 1.2  2007-10-12 21:36:54-07  oussama
*	adapted the env to use Alpha's Cmodel driver
*
*	Revision 1.1  2007-08-29 20:27:01-07  lsha
*	Initial revision.
*
*
*	DESCRIPTION:
*	OS independent layer for dHub/HBO/SemaHub APIs.
*
****************************************************************************************************/

#ifndef	DHUB_API
#define	DHUB_API			"        DHUB_API >>>    "
/**	DHUB_API
 */

#include	"ctypes.h"

typedef	struct	HDL_semaphore {
UNSG32		ra;			/*!	Base address of $SemaHub !*/
SIGN32		depth[32];		/*!	Array of semaphore (virtual FIFO) depth !*/
} HDL_semaphore;

typedef	struct	HDL_hbo {
UNSG32			mem;		/*!	Base address of HBO SRAM !*/
UNSG32			ra;		/*!	Base address of $HBO !*/
HDL_semaphore	fifoCtl;			/*!	Handle of HBO.FiFoCtl !*/
UNSG32			base[32];	/*!	Array of HBO queue base address !*/
} HDL_hbo;

typedef	struct	HDL_dhub {
UNSG32			ra;		/*!	Base address of $dHub !*/
HDL_semaphore	semaHub;		/*!	Handle of dHub.SemaHub !*/
HDL_hbo			hbo;		/*!	Handle of dHub.HBO !*/
SIGN32			MTUb[16];	/*!	Array of dHub channel MTU size bits !*/
} HDL_dhub;

typedef	struct	HDL_dhub2d {
UNSG32			ra;		/*!	Base address of $dHub2D !*/
HDL_dhub		dhub;		/*!	Handle of dHub2D.dHub !*/
} HDL_dhub2d;


#ifdef	__cplusplus
	extern	"C"
	{
#endif

/**	SECTION - handle of local contexts
 */
extern	UNSG32	sizeof_hdl_semaphore;	/*!	sizeof(HDL_semaphore) !*/
extern	UNSG32	sizeof_hdl_hbo;		/*!	sizeof(HDL_hbo) !*/
extern	UNSG32	sizeof_hdl_dhub;	/*!	sizeof(HDL_dhub) !*/
extern	UNSG32	sizeof_hdl_dhub2d;	/*!	sizeof(HDL_dhub2d) !*/

/**	ENDOFSECTION
 */



/**	SECTION - API definitions for $SemaHub
 */
/********************************************************************************************
*	Function: semaphore_hdl
*	Description: Initialize HDL_semaphore with a $SemaHub BIU instance.
*********************************************************************************************/
void	semaphore_hdl(
		UNSG32		ra,		/*!	Base address of a BIU instance of $SemaHub !*/
		void		*hdl		/*!	Handle to HDL_semaphore !*/
		);

/********************************************************************************************
*	Function: semaphore_cfg
*	Description: Configurate a semaphore's depth & reset pointers.
*	Return:	UNSG32			Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	semaphore_cfg(
		void		*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32		id,		/*!	Semaphore ID in $SemaHub !*/
		SIGN32		depth,		/*!	Semaphore (virtual FIFO) depth !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init SemaHub, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: semaphore_intr_enable
*	Description: Configurate interrupt enable bits of a semaphore.
*********************************************************************************************/
void	semaphore_intr_enable(
		void		*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32		id,		/*!	Semaphore ID in $SemaHub !*/
		SIGN32		empty,		/*!	Interrupt enable for CPU at condition 'empty' !*/
		SIGN32		full,		/*!	Interrupt enable for CPU at condition 'full' !*/
		SIGN32		almostEmpty,	/*!	Interrupt enable for CPU at condition 'almostEmpty' !*/
		SIGN32		almostFull,	/*!	Interrupt enable for CPU at condition 'almostFull' !*/
		SIGN32		cpu		/*!	CPU ID (0/1/2) !*/
		);

/********************************************************************************************
*	Function: semaphore_query
*	Description: Query current status (counter & pointer) of a semaphore.
*	Return:	UNSG32		Current available unit level
*********************************************************************************************/
UNSG32	semaphore_query(
		void		*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32		id,		/*!	Semaphore ID in $SemaHub !*/
		SIGN32		master,		/*!	0/1 as procuder/consumer query !*/
		UNSG32		*ptr		/*!	Non-zero to receive semaphore r/w pointer !*/
		);

/********************************************************************************************
*	Function: semaphore_push
*	Description: Producer semaphore push.
*********************************************************************************************/
void	semaphore_push(
		void		*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32		id,		/*!	Semaphore ID in $SemaHub !*/
		SIGN32		delta		/*!	Delta to push as a producer !*/
		);

/********************************************************************************************
*	Function: semaphore_push
*	Description: Consumer semaphore pop.
*********************************************************************************************/
void	semaphore_pop(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id,			/*!	Semaphore ID in $SemaHub !*/
		SIGN32		delta			/*!	Delta to pop as a consumer !*/
		);

/********************************************************************************************
*	Function: semaphore_chk_empty
*	Description: Check 'empty' status of a semaphore (or all semaphores).
*	Return:	UNSG32		status bit of given semaphore, or
*				status bits of all semaphores if id==-1
*********************************************************************************************/
UNSG32	semaphore_chk_empty(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id			/*!	Semaphore ID in $SemaHub
								-1 to return all 32b of the interrupt status
								!*/
		);

/********************************************************************************************
*	Function: semaphore_chk_full
*	Description: Check 'full' status of a semaphore (or all semaphores).
*	Return:	UNSG32		status bit of given semaphore, or
*				status bits of all semaphores if id==-1
*********************************************************************************************/
UNSG32	semaphore_chk_full(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id			/*!	Semaphore ID in $SemaHub
								-1 to return all 32b of the interrupt status
								!*/
		);

/********************************************************************************************
*	Function: semaphore_chk_almostEmpty
*	Description: Check 'almostEmpty' status of a semaphore (or all semaphores).
*	Return:	UNSG32		status bit of given semaphore, or
*				status bits of all semaphores if id==-1
*********************************************************************************************/
UNSG32	semaphore_chk_almostEmpty(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id			/*!	Semaphore ID in $SemaHub
								-1 to return all 32b of the interrupt status
								!*/
		);

/********************************************************************************************
*	Function: semaphore_chk_almostFull
*	Description: Check 'almostFull' status of a semaphore (or all semaphores).
*	Return:	UNSG32		status bit of given semaphore, or
*				status bits of all semaphores if id==-1
*********************************************************************************************/
UNSG32	semaphore_chk_almostFull(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id			/*!	Semaphore ID in $SemaHub
								-1 to return all 32b of the interrupt status
								!*/
		);

/********************************************************************************************
*	Function: semaphore_clr_empty
*	Description: Clear 'empty' status of a semaphore.
*********************************************************************************************/
void	semaphore_clr_empty(
		void		*hdl,			/*!	Handle to HDL_semaphore !*/
		SIGN32		id			/*!	Semaphore ID in $SemaHub !*/
		);

/********************************************************************************************
*	Function: semaphore_clr_full
*	Description: Clear 'full' status of a semaphore.
*********************************************************************************************/
void	semaphore_clr_full(
		void		*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32		id		/*!	Semaphore ID in $SemaHub !*/
		);

/********************************************************************************************
*	Function: semaphore_clr_almostEmpty
*	Description: Clear 'almostEmpty' status of a semaphore.
*********************************************************************************************/
void	semaphore_clr_almostEmpty(
		void	*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32	id		/*!	Semaphore ID in $SemaHub !*/
		);

/********************************************************************************************
*	Function: semaphore_clr_almostFull
*	Description: Clear 'almostFull' status of a semaphore.
*********************************************************************************************/
void	semaphore_clr_almostFull(
		void	*hdl,		/*!	Handle to HDL_semaphore !*/
		SIGN32	id		/*!	Semaphore ID in $SemaHub !*/
			);
/**	ENDOFSECTION
 */



/**	SECTION - API definitions for $HBO
 */
/********************************************************************************************
*	Function: hbo_hdl
*	Description: Initialize HDL_hbo with a $HBO BIU instance.
*********************************************************************************************/
void	hbo_hdl(
		UNSG32		mem,		/*!	Base address of HBO SRAM !*/
		UNSG32		ra,		/*!	Base address of a BIU instance of $HBO !*/
		void		*hdl		/*!	Handle to HDL_hbo !*/
		);

/********************************************************************************************
*	Function: hbo_fifoCtl
*	Description: Get HDL_semaphore pointer from a HBO instance.
*	Return:		void*	Handle for HBO.FiFoCtl
*********************************************************************************************/
void*	hbo_fifoCtl(void	*hdl		/*!	Handle to HDL_hbo !*/
					);

/********************************************************************************************
*	DEFINITION - convert HBO FIFO control to semaphore control
*********************************************************************************************/
#define	hbo_queue_intr_enable		(hdl,id,empty,full,almostEmpty,almostFull,cpu)		\
		semaphore_intr_enable		(hbo_fifoCtl(hdl),id,empty,full,almostEmpty,almostFull,cpu)

#define	hbo_queue_query(hdl,id,master,ptr)						\
		semaphore_query(hbo_fifoCtl(hdl),id,master,ptr)

#define	hbo_queue_push(hdl,id,delta)							\
		semaphore_push(hbo_fifoCtl(hdl),id,delta)

#define	hbo_queue_pop(hdl,id,delta)							\
		semaphore_pop(hbo_fifoCtl(hdl),id,delta)

#define	hbo_queue_chk_empty(hdl,id)							\
		semaphore_chk_empty(hbo_fifoCtl(hdl),id)

#define	hbo_queue_chk_full(hdl,id)							\
		semaphore_chk_full(hbo_fifoCtl(hdl),id)

#define	hbo_queue_chk_almostEmpty(hdl,id)						\
		semaphore_chk_almostEmpty(hbo_fifoCtl(hdl),id)

#define	hbo_queue_chk_almostFull(hdl,id)						\
		semaphore_chk_almostFull(hbo_fifoCtl(hdl),id)

#define	hbo_queue_clr_empty(hdl,id)							\
		semaphore_clr_empty(hbo_fifoCtl(hdl),id)

#define	hbo_queue_clr_full(hdl,id)							\
		semaphore_clr_full(hbo_fifoCtl(hdl),id)

#define	hbo_queue_clr_almostEmpty	(hdl,id)						\
		semaphore_clr_almostEmpty(hbo_fifoCtl(hdl),id)

#define	hbo_queue_clr_almostFull(hdl,id)							\
		semaphore_clr_almostFull(hbo_fifoCtl(hdl),id)

/********************************************************************************************
*	Function: hbo_queue_cfg
*	Description: Configurate a FIFO's base, depth & reset pointers.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	hbo_queue_cfg(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id,		/*!	Queue ID in $HBO !*/
		UNSG32		base,		/*!	Channel FIFO base address (byte address) !*/
		SIGN32		depth,		/*!	Channel FIFO depth, in 64b word !*/
		SIGN32		enable,		/*!	0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init HBO, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: hbo_queue_enable
*	Description: HBO FIFO enable/disable.
*	Return:		UNSG32		Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	hbo_queue_enable(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id,		/*!	Queue ID in $HBO !*/
		SIGN32		enable,		/*!	0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init HBO, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: hbo_queue_clear
*	Description: Issue HBO FIFO clear (will NOT wait for finish).
*********************************************************************************************/
void	hbo_queue_clear(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id		/*!	Queue ID in $HBO !*/
		);

/********************************************************************************************
*	Function: hbo_queue_busy
*	Description: Read HBO 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		'BUSY' status bits of all channels
*********************************************************************************************/
UNSG32	hbo_queue_busy(
		void		*hdl		/*!	Handle to HDL_hbo !*/
		);

/********************************************************************************************
*	Function: hbo_queue_clear_done
*	Description: Wait for a given channel or all channels to be cleared.
*********************************************************************************************/
void	hbo_queue_clear_done(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id		/*!	Queue ID in $HBO
							-1 to wait for all channel clear done
							!*/
		);

/********************************************************************************************
*	Function: hbo_queue_read
*	Description: Read a number of 64b data & pop FIFO from HBO SRAM.
*	Return:	UNSG32		Number of 64b data being read (=n), or (when cfgQ==NULL)
*				0 if there're not sufficient data in FIFO
*********************************************************************************************/
UNSG32	hbo_queue_read(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id,		/*!	Queue ID in $HBO !*/
		SIGN32		n,		/*!	Number 64b entries to read !*/
		T64b		data[],		/*!	To receive read data !*/
		UNSG32		*ptr		/*!	Pass in current FIFO pointer (in 64b word),
							& receive updated new pointer,
							Pass NULL to read from HW
							!*/
		);

/********************************************************************************************
*	Function: hbo_queue_write
*	Description: Write a number of 64b data & push FIFO to HBO SRAM.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*				0 if there're not sufficient space in FIFO
*********************************************************************************************/
UNSG32	hbo_queue_write(
		void		*hdl,		/*!	Handle to HDL_hbo !*/
		SIGN32		id,		/*!	Queue ID in $HBO !*/
		SIGN32		n,		/*!	Number 64b entries to write !*/
		T64b		data[],		/*!	Write data !*/
		T64b		cfgQ[],		/*!	Pass NULL to directly update HBO, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		UNSG32		*ptr		/*!	Pass in current FIFO pointer (in 64b word),
							& receive updated new pointer,
							Pass NULL to read from HW
							!*/
		);
/**	ENDOFSECTION
*/



/**	SECTION - API definitions for $dHubReg
*/
/********************************************************************************************
*	Function: dhub_hdl
*	Description: Initialize HDL_dhub with a $dHub BIU instance.
*********************************************************************************************/
void	dhub_hdl(
		UNSG32		mem,		/*!	Base address of dHub.HBO SRAM !*/
		UNSG32		ra,		/*!	Base address of a BIU instance of $dHub !*/
		void		*hdl		/*!	Handle to HDL_dhub !*/
		);

/********************************************************************************************
*	Function: dhub_semaphore
*	Description: Get HDL_semaphore pointer from a dHub instance.
*	Return:	void*	Handle for dHub.SemaHub
*********************************************************************************************/
void*	dhub_semaphore(
		void		*hdl		/*!	Handle to HDL_dhub !*/
		);

/********************************************************************************************
*	Function: dhub_hbo
*	Description: Get HDL_hbo pointer from a dHub instance.
*	Return:	void*	Handle for dHub.HBO
*********************************************************************************************/
void*	dhub_hbo(	void	*hdl		/*!	Handle to HDL_dhub !*/
				);

/********************************************************************************************
*	Function: dhub_hbo_fifoCtl
*	Description: Get HDL_semaphore pointer from the HBO of a dHub instance.
*	Return:	void*		Handle for dHub.HBO.FiFoCtl
*********************************************************************************************/
#define	dhub_hbo_fifoCtl(hdl)		(hbo_fifoCtl(dhub_hbo(hdl)))

/********************************************************************************************
*	DEFINITION - convert from dHub channel ID to HBO FIFO ID & semaphore (interrupt) ID
*********************************************************************************************/
// removed connection from HBO interrupt to semaphore
//#define	dhub_id2intr(id)			((id)+1)
#define	dhub_id2intr(id)			((id))
#define	dhub_id2hbo_cmdQ(id)		((id)*2)
#define	dhub_id2hbo_data(id)		((id)*2+1)

/********************************************************************************************
*	DEFINITION - convert dHub cmdQ/dataQ/channel-done interrupt control to HBO/semaphore control
*********************************************************************************************/
//removed connection from HBO interrupt to semaphore
//#define	dhub_hbo_intr_enable		(hdl,id,empty,full,almostEmpty,almostFull,cpu)
//		  semaphore_intr_enable		(dhub_semaphore(hdl),0,empty,full,almostEmpty,almostFull,cpu)

#define	dhub_channel_intr_enable	(hdl,id,full,cpu)							\
		 semaphore_intr_enable	(dhub_semaphore(hdl),dhub_id2intr(id),0,full,0,0,cpu)

#define	dhub_hbo_cmdQ_intr_enable	(hdl,id,empty,almostEmpty,cpu)					\
		  hbo_queue_intr_enable	(dhub_hbo(hdl),dhub_id2hbo_cmdQ(id),empty,0,almostEmpty,0,cpu)

#define	dhub_hbo_data_intr_enable	(hdl,id,empty,full,almostEmpty,almostFull,cpu)				\
		  hbo_queue_intr_enable	(dhub_hbo(hdl),dhub_id2hbo_data(id),empty,full,almostEmpty,almostFull,cpu)

/********************************************************************************************
*	DEFINITION - convert dHub cmdQ opehdltions to HBO FIFO opehdltions
*********************************************************************************************/
#define	dhub_cmdQ_query		(hdl,id,ptr)					\
		  hbo_queue_query	(dhub_hbo(hdl),dhub_id2hbo_cmdQ(id),0,ptr)

#define	dhub_cmdQ_push		(hdl,id,delta)					\
		  hbo_queue_push	(dhub_hbo(hdl),dhub_id2hbo_cmdQ(id),delta)

/********************************************************************************************
*	DEFINITION - convert dHub dataQ opehdltions to HBO FIFO opehdltions
*********************************************************************************************/
#define	dhub_data_query(hdl,id,master,ptr)\
		  hbo_queue_query(dhub_hbo(hdl),dhub_id2hbo_data(id),master,ptr)

#define	dhub_data_push			(hdl,id,delta)					\
		  hbo_queue_push	(dhub_hbo(hdl),dhub_id2hbo_data(id),delta)

#define	dhub_data_pop			(hdl,id,delta)					\
		  hbo_queue_pop	(dhub_hbo(hdl),dhub_id2hbo_data(id),delta)

/********************************************************************************************
*	Function: dhub_channel_cfg
*	Description: Configurate a dHub channel.
*	Return:		UNSG32		Number of (adr,pair) added to cfgQ, or (when cfgQ==NULL)
*					0 if either cmdQ or dataQ in HBO is still busy
*********************************************************************************************/
UNSG32	dhub_channel_cfg(
		void		*hdl,		/*!Handle to HDL_dhub !*/
		SIGN32		id,		/*!Channel ID in $dHubReg !*/
		UNSG32		baseCmd,	/*!Channel FIFO base address (byte address) for cmdQ !*/
		UNSG32		baseData,	/*!Channel FIFO base address (byte address) for dataQ !*/
		SIGN32		depthCmd,	/*!Channel FIFO depth for cmdQ, in 64b word !*/
		SIGN32		depthData,	/*!Channel FIFO depth for dataQ, in 64b word !*/
		SIGN32		mtu,		/*!See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes !*/
		SIGN32		QoS,		/*!See 'dHubChannel.CFG.QoS' !*/
		SIGN32		selfLoop,	/*!See 'dHubChannel.CFG.selfLoop' !*/
		SIGN32		enable,		/*!0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!Pass NULL to directly init dHub, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						!*/
		);

/********************************************************************************************
*	Function: dhub_channel_enable
*	Description: dHub channel enable/disable.
*	Return:	UNSG32			Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	dhub_channel_enable(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id,		/*!	Channel ID in $dHubReg !*/
		SIGN32		enable,		/*!	0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init dHub, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: dhub_channel_clear
*	Description: Issue dHub channel clear (will NOT wait for finish).
*********************************************************************************************/
void	dhub_channel_clear(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id		/*!	Channel ID in $dHubReg !*/
		);

/********************************************************************************************
*	Function: dhub_channel_flush
*	Description: Issue dHub channel (H2M only) flush (will NOT wait for finish).
*********************************************************************************************/
void	dhub_channel_flush(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id		/*!	Channel ID in $dHubReg !*/
		);

/********************************************************************************************
*	Function: dhub_channel_busy
*	Description: Read dHub 'BUSY' status for all channel FIFOs.
*	Return:		UNSG32		'BUSY' status bits of all channels
*********************************************************************************************/
UNSG32	dhub_channel_busy(
		void		*hdl		/*!	Handle to HDL_dhub !*/
		);

/********************************************************************************************
*	Function: dhub_channel_pending
*	Description: Read dHub 'PENDING' status for all channel FIFOs.
*	Return:		UNSG32			'PENDING' status bits of all channels
*********************************************************************************************/
UNSG32	dhub_channel_pending(
		void		*hdl		/*!	Handle to HDL_dhub !*/
		);

/********************************************************************************************
*	Function: dhub_channel_clear_done
*	Description: Wait for a given channel or all channels to be cleared or flushed.
*********************************************************************************************/
void	dhub_channel_clear_done(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id		/*!	Channel ID in $dHubReg
							-1 to wait for all channel clear done
							!*/
		);

/********************************************************************************************
*	Function: dhub_channel_write_cmd
*	Description: Write a 64b command for a dHub channel.
*	Return:		UNSG32		Number of (adr,pair) added to cfgQ if success, or
*					0 if there're not sufficient space in FIFO
*********************************************************************************************/
UNSG32	dhub_channel_write_cmd(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id,		/*!	Channel ID in $dHubReg !*/
		UNSG32		addr,		/*!	CMD: buffer address !*/
		SIGN32		size,		/*!	CMD: number of bytes to transfer !*/
		SIGN32		semOnMTU,	/*!	CMD: semaphore operation at CMD/MTU (0/1) !*/
		SIGN32		chkSemId,	/*!	CMD: non-zero to check semaphore !*/
		SIGN32		updSemId,	/*!	CMD: non-zero to update semaphore !*/
		SIGN32		interrupt,	/*!	CMD: raise interrupt at CMD finish !*/
		T64b		cfgQ[],		/*!	Pass NULL to directly update dHub, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		UNSG32		*ptr		/*!	Pass in current cmdQ pointer (in 64b word),
							& receive updated new pointer,
							Pass NULL to read from HW
							!*/
		);
/**	ENDOFSECTION
*/

void	dhub_channel_generate_cmd(
		void		*hdl,		/*!	Handle to HDL_dhub !*/
		SIGN32		id,		/*!	Channel ID in $dHubReg !*/
		UNSG32		addr,		/*!	CMD: buffer address !*/
		SIGN32		size,		/*!	CMD: number of bytes to transfer !*/
		SIGN32		semOnMTU,	/*!	CMD: semaphore operation at CMD/MTU (0/1) !*/
		SIGN32		chkSemId,	/*!	CMD: non-zero to check semaphore !*/
		SIGN32		updSemId,	/*!	CMD: non-zero to update semaphore !*/
		SIGN32		interrupt,	/*!	CMD: raise interrupt at CMD finish !*/
		SIGN32		*pData
		);

/********************************************************************************************
*       Function: dhub_channel_big_write_cmd
*       Description: Write a sequence of 64b command for a dHub channel.
*       Return:	UNSG32		Number of (adr,pair) added to cfgQ if success, or
*                                                      0 if there're not sufficient space in FIFO
*********************************************************************************************/
UNSG32  dhub_channel_big_write_cmd(
		void            *hdl,			/*!     Handle to HDL_dhub !*/
		SIGN32          id,			/*!     Channel ID in $dHubReg !*/
		UNSG32          addr,		/*!     CMD: buffer address !*/
		SIGN32          size,		/*!     CMD: number of bytes to transfer !*/
		SIGN32          semOnMTU,		/*!     CMD: semaphore operation at CMD/MTU (0/1) !*/
		SIGN32          chkSemId,		/*!     CMD: non-zero to check semaphore !*/
		SIGN32          updSemId,		/*!     CMD: non-zero to update semaphore !*/
		SIGN32          interrupt,		/*!     CMD: raise interrupt at CMD finish !*/
		T64b            cfgQ[],		/*!     Pass NULL to directly update dHub, or
						Pass non-zero to receive programming sequence
						in (adr,data) pairs
						!*/
		UNSG32          *ptr		/*!     Pass in current cmdQ pointer (in 64b word),
						& receive updated new pointer,
						Pass NULL to read from HW
						!*/
		);

/**	SECTION - API definitions for $dHubReg2D
*/
/********************************************************************************************
*	Function: dhub2d_hdl
*	Description: Initialize HDL_dhub2d with a $dHub2D BIU instance.
*********************************************************************************************/
void	dhub2d_hdl(
		UNSG32		mem,		/*!	Base address of dHub2D.dHub.HBO SRAM !*/
		UNSG32		ra,		/*!	Base address of a BIU instance of $dHub2D !*/
		void		*hdl		/*!	Handle to HDL_dhub2d !*/
		);

/********************************************************************************************
*	Function: dhub2d_channel_cfg
*	Description: Configurate a dHub2D channel.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	dhub2d_channel_cfg(
		void		*hdl,		/*!	Handle to HDL_dhub2d !*/
		SIGN32		id,		/*!	Channel ID in $dHubReg2D !*/
		UNSG32		addr,		/*!	CMD: 2D-buffer address !*/
		SIGN32		stride,		/*!	CMD: line stride size in bytes !*/
		SIGN32		width,		/*!	CMD: buffer width in bytes !*/
		SIGN32		height,		/*!	CMD: buffer height in lines !*/
		SIGN32		semLoop,	/*!	CMD: loop size (1~4) of semaphore operations !*/
		SIGN32		semOnMTU,	/*!	CMD: semaphore operation at CMD/MTU (0/1) !*/
		SIGN32		chkSemId[],	/*!	CMD: semaphore loop pattern - non-zero to check !*/
		SIGN32		updSemId[],	/*!	CMD: semaphore loop pattern - non-zero to update !*/
		SIGN32		interrupt,	/*!	CMD: raise interrupt at CMD finish !*/
		SIGN32		enable,		/*!	0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init dHub2D, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: dhub2d_channel_enable
*	Description: dHub2D channel enable/disable.
*	Return:	UNSG32		Number of (adr,pair) added to cfgQ
*********************************************************************************************/
UNSG32	dhub2d_channel_enable(
		void		*hdl,		/*!	Handle to HDL_dhub2d !*/
		SIGN32		id,		/*!	Channel ID in $dHubReg2D !*/
		SIGN32		enable,		/*!	0 to disable, 1 to enable !*/
		T64b		cfgQ[]		/*!	Pass NULL to directly init dHub2D, or
							Pass non-zero to receive programming sequence
							in (adr,data) pairs
							!*/
		);

/********************************************************************************************
*	Function: dhub2d_channel_clear
*	Description: Issue dHub2D channel clear (will NOT wait for finish).
*********************************************************************************************/
void	dhub2d_channel_clear(
		void		*hdl,		/*!	Handle to HDL_dhub2d !*/
		SIGN32		id		/*!	Channel ID in $dHubReg2D !*/
		);

/********************************************************************************************
*	Function: dhub2d_channel_busy
*	Description: Read dHub2D 'BUSY' status for all channel FIFOs.
*	Return:	UNSG32		'BUSY' status bits of all channels
*********************************************************************************************/
UNSG32	dhub2d_channel_busy(
		void		*hdl		/*!	Handle to HDL_dhub2d !*/
		);

/********************************************************************************************
*	Function: dhub2d_channel_clear_done
*	Description: Wait for a given channel or all channels to be cleared.
*********************************************************************************************/
void	dhub2d_channel_clear_done(
		void		*hdl,		/*!	Handle to HDL_dhub2d !*/
		SIGN32		id		/*!	Channel ID in $dHubReg2D
							-1 to wait for all channel clear done
							!*/
		);
/**	ENDOFSECTION
 */

typedef enum {
	BCM_SCHED_Q0 = 0,
	BCM_SCHED_Q1,
	BCM_SCHED_Q2,
	BCM_SCHED_Q3,
	BCM_SCHED_Q4,
	BCM_SCHED_Q5,
	BCM_SCHED_Q6,
	BCM_SCHED_Q7,
	BCM_SCHED_Q8,
	BCM_SCHED_Q9,
	BCM_SCHED_Q10,
	BCM_SCHED_Q11,
	BCM_SCHED_Q12,
	BCM_SCHED_Q13,
	BCM_SCHED_Q14,
	BCM_SCHED_Q15,
	BCM_SCHED_Q16,
	BCM_SCHED_Q17,
	BCM_SCHED_Q18,
} ENUM_BCM_SCHED_QID;

typedef enum {
	BCM_SCHED_TRIG_CPCB0_VBI = 0,
	BCM_SCHED_TRIG_CPCB1_VBI,
	BCM_SCHED_TRIG_CPCB2_VBI,
	BCM_SCHED_TRIG_CPCB0_VDE,
	BCM_SCHED_TRIG_CPCB1_VDE,
	BCM_SCHED_TRIG_CPCB2_VDE,
	BCM_SCHED_TRIG_AUD_PRIM,
	BCM_SCHED_TRIG_AUD_SEC,
	BCM_SCHED_TRIG_AUD_HDMI,
	BCM_SCHED_TRIG_AUD_SPDIF,
	BCM_SCHED_TRIG_AUD_MIC, //0xA
	BCM_SCHED_TRIG_VBI_VDE,
	BCM_SCHED_TRIG_DVI_VDE,
	BCM_SCHED_TRIG_SD_WRE,
	BCM_SCHED_TRIG_SD_RDE,
	BCM_SCHED_TRIG_SD_ERR,  //0xF
	BCM_SCHED_TRIG_NONE = 0x1f,

} ENUM_BCM_SCHED_TRIG_EVENT;

void BCM_SCHED_Open(void);
void BCM_SCHED_Close(void);
void BCM_SCHED_SetMux(UNSG32 QID, UNSG32 TrigEvent);
int BCM_SCHED_PushCmd(UNSG32 QID, UNSG32 *pCmd, UNSG32 *cfgQ);
void BCM_SCHED_GetEmptySts(UNSG32 QID, UNSG32 *EmptySts);

void dhub2d_channel_clear_seq(void *hdl, SIGN32 id);
void dhub2d_channel_start_seq(void *hdl, SIGN32 id);

#ifdef	__cplusplus
	}
#endif



/**	DHUB_API
 */
#endif

/**	ENDOFFILE: api_dhub.h
 */

