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


#ifndef _THINVPP_H_
#define _THINVPP_H_

#include "thinvpp_module.h"

#ifdef __cplusplus
extern "C" {
#endif


void THINVPP_CPCB_ISR_service(THINVPP_OBJ *vpp_obj, int cpcbID);
void THINVPP_Enable_ISR_Interrupt(THINVPP_OBJ *vpp_obj, int cpcbID, int flag);

#ifdef __cplusplus
}
#endif

#endif
