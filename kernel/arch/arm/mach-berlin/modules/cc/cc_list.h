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

#ifndef _CC_LIST_H_
#define _CC_LIST_H_

#include "cc_type.h"


#define true		(1)
#define false		(0)


typedef VOID (*pGSList_VisitFunc_t)(void *arg, MV_CC_ServiceID_U32_t uiSID,
					pMV_CC_DSS_ServiceInfo_t pData);

// Public APIs
HRESULT MV_CC_DSS_GlobalServiceList_Init(void);
HRESULT MV_CC_DSS_GlobalServiceList_Exit(void);
HRESULT MV_CC_DSS_GlobalServiceList_Add(MV_CC_ServiceID_U32_t uiSID,
						pMV_CC_DSS_ServiceInfo_t pData);
HRESULT MV_CC_DSS_GlobalServiceList_Delete(MV_CC_ServiceID_U32_t uiSID);
HRESULT MV_CC_DSS_GlobalServiceList_Get(MV_CC_ServiceID_U32_t uiSID,
					pMV_CC_DSS_ServiceInfo_t *ppData );
HRESULT MV_CC_DSS_GlobalServiceList_Traversal(
				pGSList_VisitFunc_t func_visit, void * arg);

void *MV_CC_DSS_GlobalServiceList_SrvInfo_Ctor(void);
void MV_CC_DSS_GlobalServiceList_SrvInfo_Dtor(void *obj);
void GSList_VisitFunc_Demo(PVOID arg, MV_CC_ServiceID_U32_t uiSID,
					pMV_CC_DSS_ServiceInfo_t pData);

#endif
