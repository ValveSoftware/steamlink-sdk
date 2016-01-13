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



#define _VPP_APIFUNCS_C_

#include "thinvpp_module.h"
#include "thinvpp_apifuncs.h"

/*******************************************************************
 * FUNCTION: set CPCB(for Berlin) or DV(for Galois) output resolution
 * INPUT: cpcbID - CPCB(for Berlin) or DV(for Galois) id
 *        resID - id of DV output resolution
 * RETURN: MV_THINVPP_OK - SUCCEED
 *         MV_EBADPARAM - invalid parameters
 *         MV_EUNCONFIG - VPP not configured or plane not active
 *         MV_EFRAMEQFULL - frame queue is full
 * Note: this function has to be called before enabling a plane
 *       which belongs to that DV.
 *******************************************************************/
int VPP_SetCPCBOutputResolution(THINVPP_OBJ *vpp_obj, int *in_params)
{
    int cpcbID = in_params[0];
    int resID = in_params[1];
    DV *cpcb;

    cpcb = &(vpp_obj->dv[cpcbID]);

    if(resID == RES_RESET){
        vpp_obj->dv[cpcbID].vbi_num = 0; // reset VBI counter
        vpp_obj->dv[cpcbID].status = STATUS_INACTIVE;
        return (MV_THINVPP_OK);
    }

    cpcb->output_res = resID;

    return (MV_THINVPP_OK);
}
