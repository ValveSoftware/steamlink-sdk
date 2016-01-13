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
#include "cc_udp.h"
#include "cc_error.h"


HRESULT MV_CC_MsgQ_PostMsgByID(MV_CC_ServiceID_U32_t 	ServiceID,
						PVOID pMSG )
{
	HRESULT res;
	MV_CC_DSS_ServiceInfo_MsgQ_t SrvInfo;

	/* Parameter Check */
	if (pMSG == NULL)
		MV_CC_DBG_Error(E_INVALIDARG, "MV_CC_MsgQ_PostMsgByID", NULL);

	SrvInfo.m_ServiceID = ServiceID;
	res = MV_CC_DSS_Inquiry((pMV_CC_DSS_ServiceInfo_t)&SrvInfo);
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_MsgQ_PostMsgByID"
					" MV_CC_DSS_Inquiry", NULL);
	}

	res = MV_CC_UDP_SendMsg(NULL, ServiceID, pMSG, sizeof(MV_CC_MSG_t));
	if (res != S_OK) {
		MV_CC_DBG_Error(res, "MV_CC_MsgQ_PostMsgByID"
					" MV_CC_UDP_SendMsg", NULL);
	}

	return res;
}

EXPORT_SYMBOL(MV_CC_MsgQ_PostMsgByID);

