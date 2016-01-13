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



#define _VPP_ISR_C_

#include "linux/sched.h" // for cpu_clock()
#include "thinvpp_module.h"
#include "thinvpp_apifuncs.h"
#include "thinvpp_isr.h"

#include "vpp.h"
#include "maddr.h"

#include "api_avio_dhub.h"
#include "avioDhub.h"

extern logo_device_t fastlogo_ctx;

extern int stop_flag;
extern int cpcb_start_flag;
extern unsigned last_isr_interval;

static int vbi_init=0;

#if LOGO_USE_SHM
static void set_bcm_cmd_0(THINVPP_OBJ *vpp_obj, unsigned bytes)
{
    // first vpp commands are pre-loaded
    BCMBUF *pbcmbuf = vpp_obj->pVbiBcmBuf;
    pbcmbuf->writer += (bytes/sizeof(*pbcmbuf->writer));
}
#endif
static void set_bcm_cmd(THINVPP_OBJ *vpp_obj, unsigned * cmd, unsigned bytes)
{
    BCMBUF *pbcmbuf = vpp_obj->pVbiBcmBuf;

    memcpy(pbcmbuf->writer, cmd, bytes);
    pbcmbuf->writer += (bytes/sizeof(*pbcmbuf->writer));
}

static void dma_logo_frame(THINVPP_OBJ *vpp_obj)
{
    DHUB_CFGQ *src_cfgQ = vpp_obj->dv[CPCB_1].curr_cpcb_vbi_dma_cfgQ;
    unsigned bytes = fastlogo_ctx.logo_dma_cmd_len;
#if LOGO_USE_SHM
    // logo dma commands are pre-loaded
#else
    memcpy(src_cfgQ->addr, fastlogo_ctx.logo_frame_dma_cmd, bytes);
#endif
    src_cfgQ->len = bytes/8;
}


typedef struct _rect_win { unsigned left, top, right, bottom; } RECT_WIN;
typedef struct _RES_T { unsigned int HRes, VRes; } VPP_FE_DLR_INPUT_RES, *PVPP_FE_DLR_INPUT_RES;

/*the offset plus with input resolution to set dummy TG size*/
#define VPP_FE_DUMMY_TG_SIZE_H_OFF_P 20
#define VPP_FE_DUMMY_TG_SIZE_V_OFF_P 10
#define VPP_FE_DUMMY_TG_SIZE_H_OFF_I 22
#define VPP_FE_DUMMY_TG_SIZE_V_OFF_I 10
#define VPP_FE_DUMMY_TG_HS_FE 1
#define VPP_FE_DUMMY_TG_HS_BE 3
#define VPP_FE_DUMMY_TG_HB_FE_OFF 8
#define VPP_FE_DUMMY_TG_HB_BE     7
#define VPP_FE_DUMMY_TG_VB0_FE_OFF 3
#define VPP_FE_DUMMY_TG_VB0_BE     2

#define PIXEL_PER_BEAT_YUV_422   4

static const unsigned int VppFeDlrRegOff[] = {
    RA_LDR_BG,
    RA_LDR_MAIN,
    RA_LDR_PIP,
    RA_LDR_PG,
    RA_LDR_IG,
    RA_LDR_CURSOR,
    RA_LDR_MOSD
};

typedef enum VPP_OVL_PLANE_UNIT_NUM_T
{
    Vpp_OVL_PLANE0 = 0,
    Vpp_OVL_PLANE1,
    Vpp_OVL_PLANE2,
    Vpp_OVL_PLANE3,
    Vpp_OVL_PLANE3A,
    Vpp_OVL_PLANE3B,
    Vpp_OVL_PLANE3C,
    Vpp_OVL_PLANE3D,
}VPP_OVL_PLANE_UNIT_NUM;

static unsigned CPCB1_VPP_OVL_PlaneColorRegOffset[] = {
    CPCB0_OO_FIX0_0<<2,
    CPCB0_OO_FIX1_0<<2,
    CPCB0_OO_FIX2_0<<2,
    CPCB0_OO_FIX3_0<<2,
    CPCB0_OO_FIX3A_0<<2,
    CPCB0_OO_FIX3B_0<<2,
    CPCB0_OO_FIX3C_0<<2,
    CPCB0_OO_FIX3D_0<<2,
};
static unsigned CPCB3_VPP_OVL_PlaneColorRegOffset[] = {
    CPCB2_OO_FIX0_0<<2,
    CPCB2_OO_FIX1_0<<2,
    ~0,
    ~0,
    ~0,
    ~0,
    ~0,
    ~0,
};
static unsigned CPCB1_VPP_OVL_BorderAlphaRegOffset[] = {
    ~0,
    CPCB0_OO_P5_AL<<2,
    CPCB0_OO_P6_AL<<2,
    CPCB0_OO_P7_AL<<2,
    CPCB0_OO_P7A_AL<<2,
    CPCB0_OO_P7B_AL<<2,
    CPCB0_OO_P7C_AL<<2,
    CPCB0_OO_P7D_AL<<2,
};

static unsigned VPP_OVL_WeightRegOffset[] = {
    CPCB0_VO_WEIGHT << 2,
    CPCB1_VO_WEIGHT << 2,
    ~0,
    ~0
};



static unsigned* set_plane_widnow_helper(unsigned* Que, unsigned plane_addr, unsigned value)
{
    *Que++ = value & 0xff;
    *Que++ = plane_addr;
    *Que++ = (value>>8) & 0xff;
    *Que++ = plane_addr + 4;
    return Que;
}

static unsigned* m_SetPlaneWindow(unsigned* Que, unsigned plane_addr, RECT_WIN *Win)
{
    Que = set_plane_widnow_helper(Que, plane_addr + 0*8, Win->left);
    Que = set_plane_widnow_helper(Que, plane_addr + 1*8, Win->right);
    Que = set_plane_widnow_helper(Que, plane_addr + 2*8, Win->top);
    Que = set_plane_widnow_helper(Que, plane_addr + 3*8, Win->bottom);
    return Que;
}

static unsigned* m_THINVPP_CPCB_SetPlaneAttribute(unsigned* Que, THINVPP_OBJ *vpp_obj, int cpcbID, int layerID, int alpha, int bgcolor)
{
    unsigned RegAddr;

    // set plane background color
    RegAddr = ~0;
    if (cpcbID == CPCB_1)
    {
        RegAddr = CPCB1_VPP_OVL_PlaneColorRegOffset[layerID+1];
    }
    else if (cpcbID == CPCB_3)
    {
        RegAddr = CPCB3_VPP_OVL_PlaneColorRegOffset[layerID+1];
    }

    if (RegAddr == ~0)
        return Que;

    *Que++ = GET_LOWEST_BYTE(bgcolor);
    *Que++ = (vpp_obj->base_addr + RegAddr);
    *Que++ = GET_SECOND_BYTE(bgcolor);
    *Que++ = (vpp_obj->base_addr + RegAddr + 4);
    *Que++ = GET_THIRD_BYTE(bgcolor);
    *Que++ = (vpp_obj->base_addr + RegAddr + 8);

    // set plane source global alpha
    if ((cpcbID > CPCB_1) || (layerID > CPCB1_PLANE_2))
        return Que;

    RegAddr = (CPCB0_OO_P1_AL << 2);
    if (layerID == CPCB1_PLANE_2)
    {
        unsigned alpha2;

        alpha2 = (vpp_obj->chan[CHAN_MAIN].zorder < vpp_obj->chan[CHAN_PIP].zorder)?
             (alpha+1)/2: (256-alpha)/2;
        //VPP_OVL_SetVideoBlendingFactor(vpp_obj, cpcbID, alpha2);
        *Que++ = alpha2;
        *Que++ = (vpp_obj->base_addr + VPP_OVL_WeightRegOffset[cpcbID]);

        RegAddr += 4;
    }

    *Que++ = alpha;
    *Que++ = (vpp_obj->base_addr + RegAddr);

    // set plane border global alpha
    RegAddr = CPCB1_VPP_OVL_BorderAlphaRegOffset[layerID+1];
    *Que++ = alpha;
    *Que++ = (vpp_obj->base_addr + RegAddr);

    return Que;
}

static unsigned *m_THINVPP_CPCB_SetPlaneSourceWindow(unsigned *Que, THINVPP_OBJ *vpp_obj, int cpcbID, int layerID, int x, int y, int width, int height)
{
    unsigned PlaneAddr;
    RECT_WIN window;
    window.left = x;
    window.top = y;
    window.right = x + width;
    window.bottom = y + height;

    switch (cpcbID){
        case CPCB_1:
            switch (layerID){
                case CPCB1_PLANE_1: /* CPCB plane 1 */
                    PlaneAddr = vpp_obj->base_addr + (CPCB0_P1_SX_L << 2); // CPCB_1::TG_PLANE1_MAIN
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    PlaneAddr = vpp_obj->base_addr + (CPCB0_P1_CR1_SX_L << 2); // CPCB_1::TG_PLANE1_MAIN_CROP1
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    PlaneAddr = vpp_obj->base_addr + (CPCB0_P1_CR2_SX_L << 2); // CPCB_1::TG_PLANE1_MAIN_CROP1
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    break;
                case CPCB1_PLANE_2: /* CPCB plane 2 */
                    PlaneAddr = vpp_obj->base_addr + (CPCB0_P2_SX_L << 2); // CPCB_1::TG_PLANE2_PIP
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    PlaneAddr = vpp_obj->base_addr + (CPCB0_P2_CR1_SX_L << 2); // CPCB_1::TG_PLANE2_PIP_CROP1
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    break;
            }
            break;
        case CPCB_3:
            switch (layerID){
                case CPCB1_PLANE_1: /* CPCB plane 1 */
                    PlaneAddr = vpp_obj->base_addr + (CPCB2_P1_SX_L << 2); // CPCB_3::TG_PLANE1_MAIN
                    Que = m_SetPlaneWindow(Que, PlaneAddr, &window);
                    break;
            }
            break;
    }

    return Que;
}

static unsigned *m_FE_DLR_SetVPDMX(unsigned *Que, THINVPP_OBJ *vpp_obj, unsigned HRes, unsigned VRes)
{
    *Que++ = (HRes-1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_HRES);

    *Que++ = ((VRes/2)-1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_VRES);

    *Que++ = (HRes + VPP_FE_DUMMY_TG_SIZE_H_OFF_I - 1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_HT);

    *Que++ = (VRes + VPP_FE_DUMMY_TG_SIZE_V_OFF_I - 1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_IVT);

    *Que++ = (VRes + VPP_FE_DUMMY_TG_SIZE_V_OFF_I - 1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_VT);

    *Que++ = (1);
    *Que++ = (vpp_obj->base_addr + RA_Vpp_VP_DMX_CTRL);

    return Que;
}

static unsigned *m_FE_DLR_SetPlaneSize(unsigned *Que, THINVPP_OBJ *vpp_obj, int Channel, unsigned HRes, unsigned VRes, unsigned CropWpl)
{
    unsigned int RegAddr = vpp_obj->base_addr + RA_Vpp_LDR + VppFeDlrRegOff[Channel];

    /*write plane size to BCM buffer*/
    *Que++ = (VRes + (HRes << 16));
    *Que++ = (RegAddr + RA_PLANE_SIZE);

    /*write crop WPL to BCM buffer*/
    *Que++ = CropWpl;
    *Que++ = (RegAddr + RA_PLANE_CROP);

    return Que;
}

static unsigned *m_FE_DLR_SetDummyTG(unsigned *Que, THINVPP_OBJ *vpp_obj, unsigned HRes, unsigned VRes)
{
    unsigned int RegAddr;

    RegAddr = vpp_obj->base_addr + RA_Vpp_VP_TG;

    /*write TG size*/
    *Que++ = ((VRes + VPP_FE_DUMMY_TG_SIZE_V_OFF_P) + ((HRes + VPP_FE_DUMMY_TG_SIZE_H_OFF_P) << 16));
    *Que++ = (RegAddr + RA_TG_SIZE);

    /*write the H blank front edge value*/
    *Que++ = (HRes + VPP_FE_DUMMY_TG_HB_FE_OFF + (VPP_FE_DUMMY_TG_HB_BE << 16));
    *Que++ = (RegAddr + RA_TG_HB);

    /*write the V blank front edge value*/
    *Que++ = (VRes + VPP_FE_DUMMY_TG_VB0_FE_OFF + (VPP_FE_DUMMY_TG_VB0_BE << 16));
    *Que++ = (RegAddr + RA_TG_VB0);

    return Que;
}

static unsigned* m_UpdateVPSize(unsigned* Que, THINVPP_OBJ *vpp_obj, unsigned ihRes, unsigned ivRes)
{
    unsigned int TotalInpPix, TotalOutPix;
    unsigned int TotPixForWriteClient, TotPixForReadClient;

    TotalInpPix =
    TotalOutPix = ivRes * ihRes;

    //THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+RA_Vpp_vpIn_pix, TotalInpPix);
    *Que++ = TotalInpPix;
    *Que++ = (vpp_obj->base_addr+RA_Vpp_vpIn_pix);
    //THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+RA_Vpp_vpOut_pix, TotalOutPix);
    *Que++ = TotalOutPix;
    *Que++ = (vpp_obj->base_addr+RA_Vpp_vpOut_pix);

    TotPixForWriteClient = (ivRes + 1) * ihRes;
    TotPixForReadClient = (TotPixForWriteClient * 26 + 63) / 64;

    //THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+RA_Vpp_diW_pix, TotPixForWriteClient);
    *Que++ = TotPixForWriteClient;
    *Que++ = (vpp_obj->base_addr+RA_Vpp_diW_pix);
    //THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+RA_Vpp_diR_word, TotPixForReadClient);
    *Que++ = TotPixForReadClient;
    *Que++ = (vpp_obj->base_addr+RA_Vpp_diR_word);

    return Que;
}

void init_vbi(THINVPP_OBJ *vpp_obj)
{
    PLANE *plane = &vpp_obj->plane[PLANE_MAIN];
    VBUF_INFO *pinfo = plane->pinfo;
    unsigned *Que, *Cmd, *CmdEnd;
    unsigned CropWpl;

#if LOGO_USE_SHM
    Cmd = (unsigned *) fastlogo_ctx.bcmQ;
    Que = (unsigned *) fastlogo_ctx.dmaQ;
#else
    Cmd = (unsigned *) fastlogo_ctx.bcm_cmd_0;
    Que = (unsigned *) logo_frame_dma_cmd;
#endif
    CmdEnd = Cmd + (fastlogo_ctx.bcm_cmd_0_len/(sizeof(unsigned)));
    //pinfo->m_active_left &= ~1;
    //pinfo->m_active_width = ((pinfo->m_active_left + pinfo->m_active_width) & ~1) - pinfo->m_active_left;
    CropWpl = (((pinfo->m_active_width + PIXEL_PER_BEAT_YUV_422 - 1) / PIXEL_PER_BEAT_YUV_422) << 16);
    plane->wpl = (CropWpl>>16);

    while (Cmd < CmdEnd)
    {
        switch (*Cmd & 0xf0000000)
        {
        case 0x80000000:
            Cmd = m_THINVPP_CPCB_SetPlaneAttribute(Cmd, vpp_obj, 0, 0, pinfo->alpha, pinfo->bgcolor);
            break;
        case 0x90000000:
            Cmd = m_FE_DLR_SetPlaneSize(Cmd, vpp_obj, 1, pinfo->m_active_width, pinfo->m_active_height, CropWpl);
            break;
        case 0xa0000000:
            Cmd = m_FE_DLR_SetVPDMX(Cmd, vpp_obj, pinfo->m_active_width, pinfo->m_active_height);
            break;
        case 0xb0000000:
            Cmd = m_FE_DLR_SetDummyTG(Cmd, vpp_obj, pinfo->m_active_width, pinfo->m_active_height);
            break;
        case 0xc0000000:
            Cmd = m_UpdateVPSize(Cmd, vpp_obj, pinfo->m_active_width, pinfo->m_active_height);
            break;
        case 0xd0000000:
            Cmd = m_THINVPP_CPCB_SetPlaneSourceWindow(Cmd, vpp_obj, 0, 0,
                    pinfo->m_active_left, pinfo->m_active_top, pinfo->m_active_width, pinfo->m_active_height);
            break;
        default:
            Cmd += 2;
            break;
        }
    }

    START_2DDMA(plane->dmaRdhubID, plane->dmaRID,
        (unsigned) pinfo->m_pbuf_start + (unsigned) pinfo->m_disp_offset, pinfo->m_buf_stride,
        plane->wpl*8, plane->actv_win.height,
        (unsigned int (*)[2])Que);
}

/////////////////////////////////////////////////////
//                                                 //
//  Interrupt Service Routines for Berlin          //
//                                                 //
/////////////////////////////////////////////////////

/********************************************************************************
 * FUNCTION: start all active plane data loader of a VPP FE channel
 * PARAMS: *vpp_obj - pointer to VPP object
 *         chanID - channel
 * RETURN: MV_THINVPP_OK - succeed
 *         MV_THINVPP_EBADPARAM - invalid parameter
 *         MV_THINVPP_EUNCONFIG - plane not activated yet
 *******************************************************************************/
static int startChannelDataLoader(THINVPP_OBJ *vpp_obj, int chanID)
{
    int cpcbID;
    CHAN *chan;
    PLANE *plane;

    cpcbID = CPCB_OF_CHAN(vpp_obj, chanID);
    chan = &vpp_obj->chan[chanID];

    if (chanID == CHAN_MAIN || chanID == CHAN_PIP) { /* none AUX channel */
        int planeID = chanID;

        plane = &vpp_obj->plane[planeID];

        switch (plane->status) {
        case STATUS_ACTIVE:
#if LOGO_USE_SHM
            // first vpp commands are pre-loaded
            set_bcm_cmd_0(vpp_obj, fastlogo_ctx.bcm_cmd_0_len);
#else
            set_bcm_cmd(vpp_obj, fastlogo_ctx.bcm_cmd_0, fastlogo_ctx.bcm_cmd_0_len);
#endif
            dma_logo_frame(vpp_obj);
            plane->status = STATUS_DISP;
            break;

        case STATUS_DISP:
            /* start data loader DMA to load display content */
            dma_logo_frame(vpp_obj);

            /* start read-back data loader */
            set_bcm_cmd(vpp_obj, fastlogo_ctx.bcm_cmd_n, fastlogo_ctx.bcm_cmd_n_len);

            break;

        default:
            break;
        }
    }

    return (MV_THINVPP_OK);
}

static void toggleQ(THINVPP_OBJ *vpp_obj, int cpcbID)
{
    THINVPP_BCMBUF_To_CFGQ(vpp_obj->pVbiBcmBuf, vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ);
    THINVPP_BCMDHUB_CFGQ_Commit(vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ, cpcbID);

#if !LOGO_USE_SHM
    // do no need double buffers for dhub queues
    if (vpp_obj->pVbiBcmBufCpcb[cpcbID] == &vpp_obj->vbi_bcm_buf[0])
        vpp_obj->pVbiBcmBufCpcb[cpcbID] = &vpp_obj->vbi_bcm_buf[1];
    else
        vpp_obj->pVbiBcmBufCpcb[cpcbID] = &vpp_obj->vbi_bcm_buf[0];

    if (vpp_obj->dv[cpcbID].curr_cpcb_vbi_dma_cfgQ == &vpp_obj->dv[cpcbID].vbi_dma_cfgQ[0])
        vpp_obj->dv[cpcbID].curr_cpcb_vbi_dma_cfgQ = &(vpp_obj->dv[cpcbID].vbi_dma_cfgQ[1]);
    else
        vpp_obj->dv[cpcbID].curr_cpcb_vbi_dma_cfgQ = &(vpp_obj->dv[cpcbID].vbi_dma_cfgQ[0]);

    if (vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ == &vpp_obj->dv[cpcbID].vbi_bcm_cfgQ[0])
        vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ = &(vpp_obj->dv[cpcbID].vbi_bcm_cfgQ[1]);
    else
        vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ = &(vpp_obj->dv[cpcbID].vbi_bcm_cfgQ[0]);
#endif
}

static void prepareQ(THINVPP_OBJ *vpp_obj, int cpcbID)
{
    vpp_obj->dv[cpcbID].curr_cpcb_vbi_bcm_cfgQ->len = 0;
    vpp_obj->dv[cpcbID].curr_cpcb_vbi_dma_cfgQ->len = 0;
    vpp_obj->pVbiBcmBuf = vpp_obj->pVbiBcmBufCpcb[cpcbID];
    THINVPP_BCMBUF_Select(vpp_obj->pVbiBcmBuf, cpcbID);
}

/* interrupt service routine for CPCB TG interrupt */
void THINVPP_CPCB_ISR_service(THINVPP_OBJ *vpp_obj, int cpcbID)
{
    DV *pDV;
    int params[2];
	static int cpcb_isr_count=0;
	cpcb_isr_count++;

    if (!vpp_obj)
        return;

	if (cpcbID != CPCB_1)
		return;

    pDV = &(vpp_obj->dv[cpcbID]);
    pDV->vbi_num ++; // VBI counter

    switch (pDV->status) {
    case STATUS_INACTIVE:
        break;

    case STATUS_ACTIVE:
        /*when change cpcb output resolution, status was set to inactive, Dhub should be stopped at this moment*/
        if (cpcbID != CPCB_1)
            break;

		/* stop fast-logo */
		if(stop_flag == 1)
		{
			prepareQ(vpp_obj, cpcbID);

			/* set resolution to RESET mode */
			params[0] = CPCB_1;
			params[1] = RES_RESET;

			VPP_SetCPCBOutputResolution(vpp_obj, params);
			set_bcm_cmd(vpp_obj, &fastlogo_ctx.bcm_cmd_z[0], fastlogo_ctx.bcm_cmd_z_len);

			toggleQ(vpp_obj, cpcbID);

			/* disable interrupt */
			THINVPP_Enable_ISR_Interrupt(vpp_obj, CPCB_1, 0);

			pDV->status = STATUS_INACTIVE;
			stop_flag++;

			break;
		}

		/* display fast-logo */
		if (cpcb_start_flag == 3)
		{
			cpcb_start_flag = 4;
		}
		else
		{
			cpcb_start_flag++;
			prepareQ(vpp_obj, cpcbID);
		}

        startChannelDataLoader(vpp_obj, CHAN_MAIN);

		THINVPP_CFGQ_To_CFGQ(vpp_obj->dv[CPCB_1].curr_cpcb_vbi_dma_cfgQ, vpp_obj->dv[CPCB_1].curr_cpcb_vbi_bcm_cfgQ);
        toggleQ(vpp_obj, cpcbID);
        break;

    case STATUS_STOP:
        if (cpcbID != CPCB_1)
            break;

        prepareQ(vpp_obj, cpcbID);

        params[0] = CPCB_1;
        params[1] = RES_RESET;
        VPP_SetCPCBOutputResolution(vpp_obj, params);

        if(!(stop_flag > 0))
            THINVPP_CFGQ_To_CFGQ(vpp_obj->dv[CPCB_1].curr_cpcb_vbi_dma_cfgQ, vpp_obj->dv[CPCB_1].curr_cpcb_vbi_bcm_cfgQ);

        toggleQ(vpp_obj, cpcbID);
        break;

    default:
        break;
    }
}

/*************************************************************
 * FUNCTION: register VPP interrupt service routine
 * PARAMS: cpcbID - CPCB ID
 * RETURN: none
 *************************************************************/
void THINVPP_Enable_ISR_Interrupt(THINVPP_OBJ *vpp_obj, int cpcbID, int flag)
{
    if (cpcbID == CPCB_1){
        /* configure and enable CPCB0 interrupt */
        semaphore_cfg(vpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1, 0);
        semaphore_pop(vpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
        semaphore_clr_full(vpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr);

        if (flag)
        {
            if (!vbi_init)
            {
                init_vbi(vpp_obj);
            }
            vbi_init = 1;
            flag = 1;
        }
        semaphore_intr_enable(vpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 0, flag, 0, 0, 0);
    }

    return;
}

