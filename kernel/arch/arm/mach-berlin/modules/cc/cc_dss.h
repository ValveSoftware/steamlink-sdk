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

#ifndef _CC_DSS_H_
#define _CC_DSS_H_

#include <linux/spinlock.h>
#include "cc_list.h"

#define MV_CC_SID_BIT_DYNAMIC 				(0x80000000)
#define MV_CC_SID_BIT_NORMAL 				(0x00000000)


#define MV_CC_ServiceID_None				(0x00000000)
#define MV_CC_ServiceID_StaticStart			(0x00000001)
#define MV_CC_GSConfigList_EndFlagID			(0x7FFFFFFF)
#define MV_CC_ServiceID_DynamicStart			(0x80000000)
#define MV_CC_ServiceID_DynamicApply			(0x80000000)

#define MV_OSAL_Mutex_Create(sem)			spin_lock_init(sem)
#define MV_OSAL_Mutex_Lock(sem)				spin_lock_bh(&sem)
#define MV_OSAL_Mutex_Unlock(sem)			spin_unlock_bh(&sem)
#define MV_OSAL_Mutex_Destroy(sem)


typedef struct _MV_CC_DSS_Status {
	UINT32 m_RegCount;
	UINT32 m_RegErrCount;
	UINT32 m_FreeCount;
	UINT32 m_FreeErrCount;
	UINT32 m_InquiryCount;
	UINT32 m_InquiryErrCount;
	UINT32 m_UpdateCount;
	UINT32 m_UpdateErrCount;
	UINT32 m_ServiceCount;
	UINT32 m_LastServiceID;
	UINT32 m_SeqID;
} MV_CC_DSS_Status_t, *pMV_CC_DSS_Status_t;


typedef struct cc_node {
	unsigned int serverid;
	struct cc_node * pnext;
} MV_CC_Node;


typedef struct mv_cc_task {
	pid_t cc_taskid;
	char cc_taskname[16];
	MV_CC_Node *serverid_head;
	MV_CC_Node *cbuf_head;
} MV_CC_Task;


typedef struct _MV_CC_DSP {
	spinlock_t		m_hGSListMutex;
	MV_CC_ServiceID_U32_t	m_SeqID;
	spinlock_t		m_SeqIDMutex;
	MV_CC_DSS_Status_t	m_Status;
} MV_CC_DSP_t, *pMV_CC_DSP_t;


HRESULT MV_CC_DSS_Init(void);
HRESULT MV_CC_DSS_Exit(void);

HRESULT MV_CC_DSS_Reg(pMV_CC_DSS_ServiceInfo_t pSrvInfo,MV_CC_Task *cc_task);
HRESULT MV_CC_DSS_Free(pMV_CC_DSS_ServiceInfo_t pSrvInfo,MV_CC_Task *cc_task);
HRESULT MV_CC_DSS_Release_By_Taskid(MV_CC_Task *cc_task);

HRESULT MV_CC_DSS_Inquiry(pMV_CC_DSS_ServiceInfo_t pSrvInfo);
HRESULT MV_CC_DSS_Update(pMV_CC_DSS_ServiceInfo_t pSrvInfo);
HRESULT MV_CC_DSS_GetList(pMV_CC_DSS_ServiceInfo_DataList_t pSrvInfoList);
HRESULT MV_CC_DSS_GetStatus(pMV_CC_DSS_Status_t pStatus);

HRESULT singlenode_init(MV_CC_Node **head);
HRESULT singlenode_exit(MV_CC_Node *head);
HRESULT singlenode_delete(MV_CC_Node *head, unsigned int serverid);
HRESULT singlenode_add(MV_CC_Node *head, unsigned int serverid);
HRESULT singlenode_display(MV_CC_Node *head);
HRESULT singlenode_checkempty(MV_CC_Node *head);
unsigned int singlenode_getfirstnode(MV_CC_Node *head);


#endif
