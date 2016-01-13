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

#include <linux/sched.h>

#include "cc_std_map.h"
#include "cc_list.h"
#include "cc_dss.h"
#include "cc_driver.h"


pMV_CC_DSP_t pMV_APP_DSS = NULL;

MV_CC_ServiceID_U32_t MV_CC_DSS_GetDynamicSID(pMV_CC_DSP_t self)
{
	if (self == NULL)
		return MV_CC_ServiceID_None;

	MV_OSAL_Mutex_Lock(self->m_SeqIDMutex);

	self->m_SeqID++;
	self->m_Status.m_SeqID = self->m_SeqID;

	if (!(self->m_SeqID & MV_CC_SID_BIT_DYNAMIC))
		MV_CC_DBG_Warning(E_OUTOFRANGE, \
		"MV_CC_DSS_GetDynamicSID rolled !!!", NULL);

	MV_OSAL_Mutex_Unlock(self->m_SeqIDMutex);

	return (self->m_SeqID | MV_CC_SID_BIT_DYNAMIC
		| MV_CC_SID_BIT_NORMAL | MV_CC_SID_BIT_LOCAL);
}

HRESULT MV_CC_DSS_Init(void)
{
	HRESULT res;

	if (pMV_APP_DSS != NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Init", NULL);

	pMV_APP_DSS = MV_OSAL_Malloc(sizeof(MV_CC_DSP_t));
	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_OUTOFMEMORY, "MV_CC_DSS_Init"
				" MV_OSAL_Malloc", NULL);

	pMV_APP_DSS->m_SeqID = MV_CC_ServiceID_DynamicStart;

	MV_OSAL_Mutex_Create(&(pMV_APP_DSS->m_SeqIDMutex));
	MV_OSAL_Mutex_Create(&(pMV_APP_DSS->m_hGSListMutex));

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Init();
	if (res != S_OK)
		MV_CC_DBG_Error(res, "MV_CC_DSS_Init"
		" MV_CC_DSS_GlobalServiceList_Init", NULL);

	pMV_APP_DSS->m_Status.m_RegCount =
	pMV_APP_DSS->m_Status.m_RegErrCount =
	pMV_APP_DSS->m_Status.m_FreeCount =
	pMV_APP_DSS->m_Status.m_FreeErrCount =
	pMV_APP_DSS->m_Status.m_InquiryCount =
	pMV_APP_DSS->m_Status.m_InquiryErrCount =
	pMV_APP_DSS->m_Status.m_UpdateCount =
	pMV_APP_DSS->m_Status.m_UpdateErrCount =
	pMV_APP_DSS->m_Status.m_ServiceCount =
	pMV_APP_DSS->m_Status.m_LastServiceID = 0;
	pMV_APP_DSS->m_Status.m_SeqID = pMV_APP_DSS->m_SeqID;

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return S_OK;
}

HRESULT MV_CC_DSS_Exit(void)
{
	HRESULT res;

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Exit", NULL);

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Exit();
	if (res != S_OK)
		MV_CC_DBG_Error(res, "MV_CC_DSS_Exit" \
		" MV_CC_DSS_GlobalServiceList_Exit", NULL);

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	MV_OSAL_Mutex_Destroy(&(pMV_APP_DSS->m_hGSListMutex));
	MV_OSAL_Mutex_Destroy(&(pMV_APP_DSS->m_SeqIDMutex));

	MV_OSAL_Free(pMV_APP_DSS);

	pMV_APP_DSS = NULL;

	return S_OK;
}


HRESULT singlenode_init(MV_CC_Node **head)
{
	MV_CC_Node *ptmp = NULL;
	if (head == NULL)
		return -1;

	ptmp = (MV_CC_Node*)kzalloc(sizeof(MV_CC_Node),
					GFP_KERNEL);
	if (ptmp == NULL) {
		return -1;
	}
	ptmp->pnext = NULL;
	ptmp->serverid = 5;
	*head = ptmp;
	return 0;
}


HRESULT singlenode_exit(MV_CC_Node *head)
{
	if (head == NULL)
		return -1;

	if(head->pnext == NULL) {
		kfree(head);
		//printk("singlenode_exit ok\n");
	} else {
		printk("singlenode_exit fail,"
			" the singlenode is not empty\n");
	}
	return 0;
}


HRESULT singlenode_delete(MV_CC_Node *head, unsigned int serverid)
{
	MV_CC_Node *ptmp, *pfront = NULL;

	if (head == NULL)
		return -1;

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	ptmp = head->pnext;
	while (1) {
		if (ptmp->serverid == serverid) {
			break;
		}

		if (ptmp->pnext == NULL) {
			MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);
			printk("serverid [%08x] couldn't be found\n",
					serverid);
			return -1;
		} else {
			pfront = ptmp;
			ptmp = ptmp->pnext;
		}
	}

	if (ptmp == head->pnext) {
		head->pnext = head->pnext->pnext;
		kfree(ptmp);
	} else {
		pfront->pnext = ptmp->pnext;
		kfree(ptmp);
	}
	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);
	//printk("singlenode_delete serverid[%08x]\n",serverid);
	return 0;

}


HRESULT singlenode_display(MV_CC_Node *head)
{
	MV_CC_Node *ptmp;
	if (head == NULL)
		return -1;

	ptmp = head->pnext;
	printk("===========start================\n");
	if (head->pnext == NULL) {
		printk("\033[31msinglenode is empty\033[00m\n");
		printk("==========end===================\n");
		return -1;
	}
	while (1) {
		printk("serverid [%08x]  [%u]\n",
			ptmp->serverid,ptmp->serverid);
		if (ptmp->pnext ==NULL) {
			break;
		}else{
			ptmp = ptmp->pnext;
		}
	}

	printk("==========end===================\n");
	return 0;
}


HRESULT singlenode_add(MV_CC_Node *head, unsigned int serverid)
{
	MV_CC_Node *ptmp = NULL;
	if (head == NULL)
		return -1;

	ptmp = (MV_CC_Node*)kzalloc(sizeof(MV_CC_Node), GFP_KERNEL);
	if (ptmp == NULL) {
		return -1;
	}
	ptmp->serverid = serverid;

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);
	if(head->pnext == NULL) {
		ptmp->pnext = NULL;
		head->pnext = ptmp;
	} else {
		ptmp->pnext = head->pnext->pnext;
		head->pnext->pnext = ptmp;
	}
	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);
	return 0;
}


HRESULT singlenode_checkempty(MV_CC_Node *head)
{
	if(head->pnext == NULL) {
		return 0; //empty
	} else {
		return 1; //not empty
	}
}


unsigned int singlenode_getfirstnode(MV_CC_Node *head)
{
	return head->pnext->serverid;
}


HRESULT MV_CC_DSS_Release_By_Taskid(MV_CC_Task *cc_task)
{
	unsigned int first_serverid = 0;
	MV_CC_DSS_ServiceInfo_t pSrvInfo_Search;
	MV_CC_Node *head = cc_task->serverid_head;

	while (singlenode_checkempty(head)) {
		first_serverid = singlenode_getfirstnode(head);
		pSrvInfo_Search.m_ServiceID = first_serverid;
		MV_CC_DSS_Free(&pSrvInfo_Search, cc_task);
	}

	return singlenode_exit(head);
}

HRESULT MV_CC_DSS_Reg(pMV_CC_DSS_ServiceInfo_t pSrvInfo,
					MV_CC_Task *cc_task )
{
	HRESULT res;
	pMV_CC_DSS_ServiceInfo_t pSrvInfo_copy;

	if (pSrvInfo == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_Reg", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Reg", NULL);

	pSrvInfo_copy = MV_CC_DSS_GlobalServiceList_SrvInfo_Ctor();
	if (pSrvInfo_copy == NULL) {
		MV_CC_DBG_Error(E_OUTOFMEMORY, "MV_CC_DSS_Reg", NULL);
	}

	// check service id need dynamic generator?
	if (pSrvInfo->m_ServiceID == MV_CC_ServiceID_DynamicApply) {
		//get a new dynamic service id
		pSrvInfo->m_ServiceID = MV_CC_DSS_GetDynamicSID(pMV_APP_DSS);
		if (pSrvInfo->m_ServiceID == MV_CC_ServiceID_None) {
			MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);
			pMV_APP_DSS->m_Status.m_RegErrCount++;
			MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);
			MV_CC_DSS_GlobalServiceList_SrvInfo_Dtor(pSrvInfo_copy);
			MV_CC_DBG_Error(E_FAIL, "MV_CC_DSS_Reg", NULL);
		}
	}

	GaloisMemcpy(pSrvInfo_copy, pSrvInfo, sizeof(MV_CC_DSS_ServiceInfo_t));

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Add(pSrvInfo->m_ServiceID,
						pSrvInfo_copy);
	if (res != S_OK) {
		pMV_APP_DSS->m_Status.m_RegErrCount++;
		MV_CC_DBG_Warning(res, "MV_CC_DSS_Reg"
			" MV_CC_DSS_GlobalServiceList_Add", NULL);
		MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);
		MV_CC_DSS_GlobalServiceList_SrvInfo_Dtor(pSrvInfo_copy);
		return res;
	}

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	singlenode_add(cc_task->serverid_head, pSrvInfo->m_ServiceID);

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);
	pMV_APP_DSS->m_Status.m_RegCount++;
	pMV_APP_DSS->m_Status.m_ServiceCount++;
	pMV_APP_DSS->m_Status.m_LastServiceID = pSrvInfo->m_ServiceID;
	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return res;
}


HRESULT MV_CC_DSS_Update(pMV_CC_DSS_ServiceInfo_t pSrvInfo)
{
	HRESULT res;
	pMV_CC_DSS_ServiceInfo_t pSrvInfo_Search;

	if (pSrvInfo == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_Update", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Update", NULL);

	// check service id
	if (pSrvInfo->m_ServiceID == MV_CC_ServiceID_DynamicApply)
		MV_CC_DBG_Error(E_BADVALUE, "MV_CC_DSS_Update", NULL);

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Get(pSrvInfo->m_ServiceID, \
					&pSrvInfo_Search);
	if (res != S_OK) {
		pMV_APP_DSS->m_Status.m_UpdateErrCount++;
		goto MV_CC_DSS_Update_Failure;
	}

	if (pSrvInfo_Search != NULL) {
		GaloisMemcpy(pSrvInfo_Search, pSrvInfo, \
		sizeof(MV_CC_DSS_ServiceInfo_t));
	} else {
		pMV_APP_DSS->m_Status.m_UpdateErrCount++;
		goto MV_CC_DSS_Update_Failure;
	}

	pMV_APP_DSS->m_Status.m_UpdateCount++;
	pMV_APP_DSS->m_Status.m_LastServiceID = pSrvInfo->m_ServiceID;

MV_CC_DSS_Update_Failure:

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return res;
}


HRESULT MV_CC_DSS_Free(pMV_CC_DSS_ServiceInfo_t pSrvInfo,
						MV_CC_Task *cc_task)
{
	HRESULT res;

	if (pSrvInfo == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_Free", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Free", NULL);



	singlenode_delete(cc_task->serverid_head, pSrvInfo->m_ServiceID);

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Delete(pSrvInfo->m_ServiceID);
	if (res != S_OK) {
		pMV_APP_DSS->m_Status.m_FreeErrCount++;
		MV_CC_DBG_Warning(res, "MV_CC_DSS_Free"
			" MV_CC_DSS_GlobalServiceList_Delete", NULL);
		goto MV_CC_DSS_Free_Failure;
	}

	pMV_APP_DSS->m_Status.m_FreeCount++;
	pMV_APP_DSS->m_Status.m_ServiceCount--;
	pMV_APP_DSS->m_Status.m_LastServiceID = pSrvInfo->m_ServiceID;

MV_CC_DSS_Free_Failure:

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return res;
}


HRESULT MV_CC_DSS_Inquiry(pMV_CC_DSS_ServiceInfo_t pSrvInfo)
{
	HRESULT res;
	pMV_CC_DSS_ServiceInfo_t pSrvInfo_Search;

	if (pSrvInfo == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_Inquiry", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_Inquiry", NULL);

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Get(pSrvInfo->m_ServiceID, \
						&pSrvInfo_Search);
	if (res != S_OK) {
		pMV_APP_DSS->m_Status.m_InquiryErrCount++;
		goto MV_CC_DSS_Inquiry_Failure;
	}

	if (pSrvInfo_Search != NULL)
		GaloisMemcpy(pSrvInfo, pSrvInfo_Search, \
			sizeof(MV_CC_DSS_ServiceInfo_t));
	else {
		pMV_APP_DSS->m_Status.m_InquiryErrCount++;
		goto MV_CC_DSS_Inquiry_Failure;
	}

	pMV_APP_DSS->m_Status.m_InquiryCount++;
	pMV_APP_DSS->m_Status.m_LastServiceID = pSrvInfo->m_ServiceID;

MV_CC_DSS_Inquiry_Failure:

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return res;
}

VOID GSList_VisitFunc_GetList(PVOID arg,
				MV_CC_ServiceID_U32_t uiSID,
				pMV_CC_DSS_ServiceInfo_t pData )
{
	pMV_CC_DSS_ServiceInfo_DataList_t 	pSrvInfoList;

	if (((pSrvInfoList = (pMV_CC_DSS_ServiceInfo_DataList_t)arg ) == NULL)
		|| (pData == NULL)) {
		MV_CC_DBG_Warning(E_INVALIDARG, "GSList_VisitFunc_GetList", NULL);
		return;
	}

	if (pSrvInfoList->m_DataNum < pSrvInfoList->m_ListNum) {
		GaloisMemcpy(&(pSrvInfoList->m_SrvInfo[pSrvInfoList->m_DataNum]),
				pData, sizeof(MV_CC_DSS_ServiceInfo_t));
		pSrvInfoList->m_DataNum++;
	}

	pSrvInfoList->m_MaxNum++;

	return;
}

HRESULT MV_CC_DSS_GetList(pMV_CC_DSS_ServiceInfo_DataList_t pSrvInfoList)
{
	HRESULT res;

	if ((pSrvInfoList == NULL) || (pSrvInfoList->m_ListNum == 0))
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_GetList", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_GetList", NULL);

	pSrvInfoList->m_DataNum = 0;
	pSrvInfoList->m_MaxNum = 0;

	MV_OSAL_Mutex_Lock(pMV_APP_DSS->m_hGSListMutex);

	res = MV_CC_DSS_GlobalServiceList_Traversal(GSList_VisitFunc_GetList,
							pSrvInfoList);
	if (res != S_OK)
		MV_CC_DBG_Warning(res, "MV_CC_DSS_GetList" \
		" MV_CC_DSS_GlobalServiceList_Traversal", NULL);

	MV_OSAL_Mutex_Unlock(pMV_APP_DSS->m_hGSListMutex);

	return res;
}

HRESULT MV_CC_DSS_GetStatus(pMV_CC_DSS_Status_t pStatus)
{
	if (pStatus == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_DSS_GetStatus", NULL);

	if (pMV_APP_DSS == NULL)
		MV_CC_DBG_Error(E_NOTREADY, "MV_CC_DSS_GetStatus", NULL);

	GaloisMemcpy(pStatus, &(pMV_APP_DSS->m_Status),
				sizeof(MV_CC_DSS_Status_t));

	return S_OK;
}
