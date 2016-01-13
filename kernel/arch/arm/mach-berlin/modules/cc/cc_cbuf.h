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

#ifndef _MV_CC_CBUF_H_
#define _MV_CC_CBUF_H_

#include "cc_type.h"
#include "cc_dss.h"


typedef struct _MV_CC_CBuf_CBufBody {
	volatile UINT32 m_StartOffset;
	volatile UINT32 m_RdOffset;
	volatile UINT32 m_WrOffset;
	volatile UINT32 m_BufSize;
	volatile UINT32 m_EndBlockSize;
	volatile UINT32 m_Flags;
} MV_CC_CBufBody_t, *pMV_CC_CBufBody_t;


typedef struct _MV_CC_CBufCtrl {
	Buffer_Notifier_t	m_BufNtf;

	MV_CC_CBufCtrlType_t	m_type;
	MV_CC_ServiceID_U32_t	m_id;

	UINT32			m_seqid;
	pMV_CC_CBufBody_t	m_pCBufBody;
	UINT32			m_BufSize;
	UINT32			m_EndBlockSize;
	UINT32			m_Flags;

	UINT32			m_CBufBody_SHMOffset;

	MV_CC_ServiceID_U32_t	m_NoticeMsgQSID;
	UINT32			m_EndBlockDataSize;
	MV_CC_HANDLE_MsgQ_t	m_NoticeMsgQHandle;

	UINT32			m_RWCount;
	UINT32			m_RWErrCount;
} MV_CC_CBufCtrl_t, *pMV_CC_CBufCtrl_t;

#define CBuf_BufferStart(x)	((UCHAR *)((UCHAR *)((x)->m_pCBufBody) + \
					(x)->m_pCBufBody->m_StartOffset))


HRESULT  MV_CC_CBufBody_Create(UINT32 *pSHMOffset,
			   	   UINT32 BufSize,
			   	   UINT32 EndBlockSize,
			   	   UINT32 Flags);

HRESULT  MV_CC_CBufBody_Destroy(UINT32	SHMOffset);


HRESULT MV_CC_CBufSrv_Create( pMV_CC_DSS_ServiceInfo_CBuf_t pSrvInfo,
					MV_CC_Task *cc_task);
HRESULT MV_CC_CBufSrv_Destroy(pMV_CC_DSS_ServiceInfo_CBuf_t pSrvInfo,
						MV_CC_Task *cc_task);
HRESULT MV_CC_CBufSrv_Release_By_Taskid(MV_CC_Task *cc_task);

void * MV_SHM_GetCacheVirtAddr(size_t Offset);
int MV_SHM_Free( size_t Offset);
size_t MV_SHM_Malloc( size_t Size, size_t Alignment);


#endif
