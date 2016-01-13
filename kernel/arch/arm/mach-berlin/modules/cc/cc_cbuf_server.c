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

#include "cc_dss.h"
#include "cc_cbuf.h"
#include "cc_error.h"

HRESULT MV_CC_CBufSrv_Create( pMV_CC_DSS_ServiceInfo_CBuf_t pSrvInfo,
					MV_CC_Task *cc_task)
{
	HRESULT res;

	/* Parameter Check */
	if ((pSrvInfo == NULL))
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_CBufSrv_Create", NULL);

	if (pSrvInfo->m_ServiceID != MV_CC_ServiceID_DynamicApply) {
		MV_CC_DSS_ServiceInfo_CBuf_t 	SrvInfo_CBuf;
		SrvInfo_CBuf.m_ServiceID = pSrvInfo->m_ServiceID;

		res = MV_CC_DSS_Inquiry((pMV_CC_DSS_ServiceInfo_t)&SrvInfo_CBuf);
		if (res == S_OK) {
			// update service
			SrvInfo_CBuf.m_seqid++;

			res = MV_CC_DSS_Update((pMV_CC_DSS_ServiceInfo_t)&SrvInfo_CBuf);
			if (res != S_OK) {
				MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Create"
					" MV_CC_DSS_Update", NULL);
			}

			GaloisMemcpy(pSrvInfo, &SrvInfo_CBuf,
				sizeof(MV_CC_DSS_ServiceInfo_CBuf_t));
			singlenode_add(cc_task->cbuf_head, pSrvInfo->m_ServiceID);

			return S_OK;
		}
	}

	// create new cbuf body
	res = MV_CC_CBufBody_Create(&(pSrvInfo->m_CBufBody_SHMOffset),
	                        pSrvInfo->m_BufSize,
	                        pSrvInfo->m_EndBlockSize,
	                        pSrvInfo->m_Flags);
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Create"
			" MV_CC_CBufBody_Create", NULL);
	}

	pSrvInfo->m_SrvType = MV_CC_SrvType_CBuf;
	pSrvInfo->m_SrvLevel = MV_CC_SrvLevel_ICC;
	pSrvInfo->m_Pad1 = pSrvInfo->m_Pad2 = 0;

	pSrvInfo->m_seqid = 1;

	res = MV_CC_DSS_Reg((pMV_CC_DSS_ServiceInfo_t)pSrvInfo,cc_task);
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Create"
			" MV_CC_DSS_Reg", NULL);
	}

	MV_CC_DBG_Info("MV_CC_CBufSrv_Create"
		" ServiceID = 0x%08X\n", pSrvInfo->m_ServiceID);

	singlenode_add(cc_task->cbuf_head, pSrvInfo->m_ServiceID);

	return S_OK;
}


HRESULT MV_CC_CBufSrv_Destroy(pMV_CC_DSS_ServiceInfo_CBuf_t pSrvInfo,
					MV_CC_Task *cc_task)
{
	HRESULT res;

	MV_CC_DBG_Info("MV_CC_CBufSrv_Destroy ServiceID = 0x%08X\n",
				pSrvInfo->m_ServiceID);

	/* Parameter Check */
	if ((pSrvInfo == NULL))
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_CBufSrv_Destroy", NULL);

	singlenode_delete(cc_task->cbuf_head, pSrvInfo->m_ServiceID);

	if (pSrvInfo->m_ServiceID == MV_CC_ServiceID_DynamicApply)
		MV_CC_DBG_Error(E_BADVALUE, "MV_CC_CBufSrv_Destroy", NULL);

	res = MV_CC_DSS_Inquiry((pMV_CC_DSS_ServiceInfo_t)pSrvInfo);
	if (res != S_OK) {
	    	MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Destroy"
			" MV_CC_DSS_Inquiry", NULL);
	}

	pSrvInfo->m_seqid--;

	if (pSrvInfo->m_seqid > 0) {
		// update service
		res = MV_CC_DSS_Update((pMV_CC_DSS_ServiceInfo_t)pSrvInfo);
		if (res != S_OK) {
			MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Destroy"
				" MV_CC_DSS_Update", NULL);
		}
		return S_OK;
	}

	// destroy cbuf body
	res = MV_CC_CBufBody_Destroy(pSrvInfo->m_CBufBody_SHMOffset);
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Create"
			" MV_CC_CBufBody_Destroy", NULL);
	}

	res = MV_CC_DSS_Free((pMV_CC_DSS_ServiceInfo_t)pSrvInfo,cc_task);
	if (res != S_OK){
		MV_CC_DBG_Error(res, "MV_CC_CBufSrv_Create"
			" MV_CC_DSS_Reg", NULL);
	}
	return S_OK;
}


HRESULT MV_CC_CBufSrv_Release_By_Taskid(MV_CC_Task *cc_task)
{
	unsigned int first_serverid = 0;
	MV_CC_DSS_ServiceInfo_t pSrvInfo_Search;
	MV_CC_Node *head = cc_task->cbuf_head;

	while (singlenode_checkempty(head)) {
		first_serverid = singlenode_getfirstnode(head);
		pSrvInfo_Search.m_ServiceID = first_serverid;
		MV_CC_CBufSrv_Destroy(&pSrvInfo_Search, cc_task);
	}

	return singlenode_exit(head);
}
