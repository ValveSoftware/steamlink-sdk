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



#define _THINVPP_API_C_

#include "Galois_memmap.h"
#include "galois_io.h"

#include "thinvpp_module.h"
#include "thinvpp_apifuncs.h"
#include "thinvpp_isr.h"

#include <linux/mm.h>
#include <asm/page.h>
#include <linux/delay.h>

#include "maddr.h"
#include "vpp.h"
#include "api_dhub.h"

#define bTST(x, b) (((x) >> (b)) & 1)

extern logo_device_t fastlogo_ctx;

void VPP_dhub_sem_clear(void)
{
    int instat;
    HDL_dhub2d *pDhubHandle;
    int dhubID;
    int status;

    HDL_semaphore *pSemHandle = thinvpp_obj->pSemHandle;

    instat = semaphore_chk_full(pSemHandle, -1);

    if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)){
        semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
        semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr);
    } else if (bTST(instat, avioDhubSemMap_vpp_vppCPCB2_intr)){
        semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB2_intr, 1);
        semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppCPCB2_intr);
    }


    dhubID = avioDhubChMap_vpp_BCM_R;
    pDhubHandle = &VPP_dhubHandle;

    /* clear BCM interrupt */
    pSemHandle = dhub_semaphore(&(pDhubHandle->dhub));
    status = semaphore_chk_full(pSemHandle, dhubID);
    while (status) {
        semaphore_pop(pSemHandle, dhubID, 1);
        semaphore_clr_full(pSemHandle, dhubID);
        status = semaphore_chk_full(pSemHandle, dhubID);
    }

    return;
}


/***********************************************
 * FUNCTION: create a VPP object
 * PARAMS: base_addr - VPP object base address
 *         *handle - pointer to object handle
 * RETURN: MV_THINVPP_OK - succeed
 *         MV_THINVPP_EUNCONFIG - not initialized
 ***********************************************/
int MV_THINVPP_Create(int base_addr)
{
    if (!(thinvpp_obj = (THINVPP_OBJ *)THINVPP_MALLOC(sizeof(THINVPP_OBJ)))){
        return (MV_THINVPP_ENOMEM);
    }

    THINVPP_MEMSET(thinvpp_obj, 0, sizeof(THINVPP_OBJ));

    thinvpp_obj->base_addr = base_addr;

#if LOGO_USE_SHM
    // do no need double buffers for dhub queues
    THINVPP_BCMBUF_Set(&(thinvpp_obj->vbi_bcm_buf[0]), fastlogo_ctx.bcmQ, fastlogo_ctx.bcmQ_phys, fastlogo_ctx.bcmQ_len);
    THINVPP_CFGQ_Set(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0]), fastlogo_ctx.dmaQ, fastlogo_ctx.dmaQ_phys, fastlogo_ctx.dmaQ_len);
    THINVPP_CFGQ_Set(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0]), fastlogo_ctx.cfgQ, fastlogo_ctx.cfgQ_phys, fastlogo_ctx.cfgQ_len);
#else
    if (THINVPP_BCMBUF_Create(&(thinvpp_obj->vbi_bcm_buf[0]), BCM_BUFFER_SIZE) != MV_THINVPP_OK){
        goto nomem_exit;
    }

    if (THINVPP_BCMBUF_Create(&(thinvpp_obj->vbi_bcm_buf[1]), BCM_BUFFER_SIZE) != MV_THINVPP_OK) {
        goto nomem_exit;
    }

    if (THINVPP_CFGQ_Create(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0]), DMA_CMD_BUFFER_SIZE) != MV_THINVPP_OK) {
        goto nomem_exit;
    }
    if (THINVPP_CFGQ_Create(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[1]), DMA_CMD_BUFFER_SIZE) != MV_THINVPP_OK) {
        goto nomem_exit;
    }

    if (THINVPP_CFGQ_Create(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0]), DMA_CMD_BUFFER_SIZE) != MV_THINVPP_OK) {
        goto nomem_exit;
    }
    if (THINVPP_CFGQ_Create(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[1]), DMA_CMD_BUFFER_SIZE) != MV_THINVPP_OK) {
        goto nomem_exit;
    }
#endif

    return (MV_THINVPP_OK);

nomem_exit:
    if (thinvpp_obj->vbi_bcm_buf[0].addr != 0)
        THINVPP_BCMBUF_Destroy(&(thinvpp_obj->vbi_bcm_buf[0]));
    if (thinvpp_obj->vbi_bcm_buf[1].addr != 0)
        THINVPP_BCMBUF_Destroy(&(thinvpp_obj->vbi_bcm_buf[1]));

    if (thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0].addr != 0)
        THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0]));
    if (thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[1].addr != 0)
        THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[1]));
    if (thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0].addr != 0)
        THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0]));
    if (thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[1].addr != 0)
        THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[1]));

    THINVPP_FREE(thinvpp_obj);

    return (MV_THINVPP_ENOMEM);
}

/***********************************************
 * FUNCTION: destroy a VPP object
 * PARAMS: handle - VPP object handle
 * RETURN: MV_THINVPP_OK - succeed
 *         MV_THINVPP_EUNCONFIG - not initialized
 *         MV_THINVPP_ENODEV - no device
 *         MV_THINVPP_ENOMEM - no memory
 ***********************************************/
int MV_THINVPP_Destroy(void)
{
    if (thinvpp_obj == NULL)
        return (MV_THINVPP_ENODEV);

    /* free BCM buffer memory */
    THINVPP_BCMBUF_Destroy(&(thinvpp_obj->vbi_bcm_buf[0]));
    THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0]));
    THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0]));
#if !LOGO_USE_SHM
    // do no need double buffers for dhub queues
    THINVPP_BCMBUF_Destroy(&(thinvpp_obj->vbi_bcm_buf[1]));
    THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[1]));
    THINVPP_CFGQ_Destroy(&(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[1]));
#endif

    /* free vpp object memory */
    THINVPP_FREE(thinvpp_obj);
    thinvpp_obj = NULL;

    return (MV_THINVPP_OK);
}

/***************************************
 * FUNCTION: VPP reset
 * INPUT: NONE
 * RETURN: NONE
 **************************************/
int MV_THINVPP_Reset(void)
{
    int i;

    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    // reset VPP object variable
    thinvpp_obj->status = STATUS_INACTIVE;

    /* reset planes */
    for (i=FIRST_PLANE; i<MAX_NUM_PLANES; i++){
        thinvpp_obj->plane[i].status = STATUS_INACTIVE;
        thinvpp_obj->plane[i].mode = -1; // invalid
        thinvpp_obj->plane[i].srcfmt = -1; // invalid
        thinvpp_obj->plane[i].order = -1; // invalid

        thinvpp_obj->plane[i].actv_win.x = 0;
        thinvpp_obj->plane[i].actv_win.y = 0;
        thinvpp_obj->plane[i].actv_win.width = 0;
        thinvpp_obj->plane[i].actv_win.height = 0;
    }

    /* reset channels */
    for (i=FIRST_CHAN; i<MAX_NUM_CHANS; i++) {
        thinvpp_obj->chan[i].status = STATUS_INACTIVE;
        thinvpp_obj->chan[i].dvID = -1; // invalid
        thinvpp_obj->chan[i].dvlayerID = -1; // invalid
        thinvpp_obj->chan[i].zorder = -1; // invalid

        thinvpp_obj->chan[i].disp_win.x = 0;
        thinvpp_obj->chan[i].disp_win.y = 0;
        thinvpp_obj->chan[i].disp_win.width = 0;
        thinvpp_obj->chan[i].disp_win.height = 0;
    }

    /* reset DVs */
    for (i=FIRST_CPCB; i<MAX_NUM_CPCBS; i++) {
        thinvpp_obj->dv[i].status = STATUS_INACTIVE;
        thinvpp_obj->dv[i].output_res = RES_INVALID; // invalid
        thinvpp_obj->dv[i].num_of_vouts = 0;
        thinvpp_obj->dv[i].vbi_num = 0;
    }

    /* reset VOUTs */
    for (i=FIRST_VOUT; i<MAX_NUM_VOUTS; i++) {
        thinvpp_obj->vout[i].status = STATUS_INACTIVE;
        thinvpp_obj->vout[i].dvID = -1; // invalid
    }

    /* reset VBI BCM buffer */
    THINVPP_BCMBUF_Reset(&thinvpp_obj->vbi_bcm_buf[0]);
    thinvpp_obj->pVbiBcmBuf = &(thinvpp_obj->vbi_bcm_buf[0]);
    thinvpp_obj->hdmi_mute = 0;

#if !LOGO_USE_SHM
    // do no need double buffers for dhub queues
    THINVPP_BCMBUF_Reset(&thinvpp_obj->vbi_bcm_buf[1]);
#endif
    thinvpp_obj->pVbiBcmBufCpcb[CPCB_1] = &(thinvpp_obj->vbi_bcm_buf[0]);

    thinvpp_obj->dv[CPCB_1].curr_cpcb_vbi_dma_cfgQ = &(thinvpp_obj->dv[CPCB_1].vbi_dma_cfgQ[0]);
    thinvpp_obj->dv[CPCB_1].curr_cpcb_vbi_bcm_cfgQ = &(thinvpp_obj->dv[CPCB_1].vbi_bcm_cfgQ[0]);

    /* reset dHub cmdQ */
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
    for (i = 0; i < avioDhubChMap_vpp_TT_R; i++)
#else /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
    for (i = 0; i < avioDhubChMap_vpp_SPDIF_W; i++)
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
        thinvpp_obj->dhub_cmdQ[i] = 0;

    /* select BCM sub-buffer to dump register settings */
    THINVPP_BCMBUF_Select(thinvpp_obj->pVbiBcmBuf, -1);

    return (MV_THINVPP_OK);
}

/***************************************
 * FUNCTION: VPP profile configuration
 * INPUT: NONE
 * RETURN: NONE
 **************************************/
int MV_THINVPP_Config(void)
{
    int i;

    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    /* VPP module has been configured */
    if (thinvpp_obj->status == STATUS_ACTIVE)
        return (MV_THINVPP_OK);

    thinvpp_obj->pSemHandle = dhub_semaphore(&VPP_dhubHandle.dhub);

    /* config planes */
    for (i=FIRST_PLANE; i<MAX_NUM_PLANES; i++){

        /* set plane's DMA channel ID */
        if (i == PLANE_MAIN){
            thinvpp_obj->plane[i].dmaRID = avioDhubChMap_vpp_MV_R; // inline read DMA
            thinvpp_obj->plane[i].dmaRdhubID = (int)(&VPP_dhubHandle);
            thinvpp_obj->plane[i].offline_dmaWID = avioDhubChMap_vpp_MV_FRC_W; // offline write-back DMA
            thinvpp_obj->plane[i].offline_dmaWdhubID = (int)(&VPP_dhubHandle);
            thinvpp_obj->plane[i].offline_dmaRID = avioDhubChMap_vpp_MV_FRC_R; // offline readd-back DMA
            thinvpp_obj->plane[i].offline_dmaRdhubID = (int)(&VPP_dhubHandle);
        }

    } // <- config FE planes

    /* config channels */
    thinvpp_obj->chan[CHAN_MAIN].dvID = CPCB_1;
    thinvpp_obj->chan[CHAN_MAIN].zorder = CPCB_ZORDER_2;
    thinvpp_obj->chan[CHAN_MAIN].dvlayerID = CPCB1_PLANE_1;

    thinvpp_obj->chan[CHAN_AUX].dvID = CPCB_3;
    thinvpp_obj->chan[CHAN_AUX].zorder = CPCB_ZORDER_1;
    thinvpp_obj->chan[CHAN_AUX].dvlayerID = CPCB1_PLANE_1; // PLANE-1 of CPCB-2

    thinvpp_obj->dv[CPCB_1].num_of_chans = 1;
    thinvpp_obj->dv[CPCB_1].chanID[0] = CHAN_MAIN;

    thinvpp_obj->dv[CPCB_1].num_of_vouts = 1;
    thinvpp_obj->dv[CPCB_1].voutID[0] = VOUT_HDMI;

    thinvpp_obj->dv[CPCB_3].num_of_chans = 0;

    /* config VOUTs */
    thinvpp_obj->vout[VOUT_HDMI].dvID = CPCB_1;

    /* select BCM sub-buffer to dump register settings */
    THINVPP_BCMBUF_Select(thinvpp_obj->pVbiBcmBuf, -1);

    BCM_SCHED_SetMux(BCM_SCHED_Q0, 0); /* CPCB0 VBI -> Q0 */

    /* set VPP module to be configured */
    thinvpp_obj->status = STATUS_ACTIVE;
    return (MV_THINVPP_OK);
}

/*******************************************************************
 * FUNCTION: set CPCB or DV output resolution
 * INPUT: cpcbID - CPCB(for Berlin) or DV(for Galois) id
 *        resID - id of output resolution
 *        bit_depth - HDMI deep color bit depth
 * RETURN: MV_THINVPP_OK - SUCCEED
 *         MV_EBADPARAM - invalid parameters
 *         MV_EUNCONFIG - VPP not configured or plane not active
 *         MV_EFRAMEQFULL - frame queue is full
 * Note: this function has to be called before enabling a plane
 *       which belongs to that CPCB or DV.
 *******************************************************************/
int MV_THINVPP_SetCPCBOutputResolution(int cpcbID, int resID, int bit_depth)
{
    int params[3];

    if (!thinvpp_obj) {
        return (MV_THINVPP_ENODEV);
    }

    if (resID<FIRST_RES || resID>=MAX_NUM_RESS) {
        return (MV_THINVPP_EBADPARAM);
    }

    if (thinvpp_obj->status == STATUS_INACTIVE){
        /* VPP module is not configured */
        return (MV_THINVPP_EUNCONFIG);
    }

    if (cpcbID<FIRST_CPCB || cpcbID>=MAX_NUM_CPCBS) {
        return (MV_THINVPP_EBADPARAM);
    }


    if (resID != RES_RESET) {

        /* select BCM sub-buffer to dump register settings */
        THINVPP_BCMBUF_Select(thinvpp_obj->pVbiBcmBuf, cpcbID);

        /* set CPCB resolution */
        params[0] = cpcbID;
        params[1] = resID;
        VPP_SetCPCBOutputResolution(thinvpp_obj, params);

        /* set DV status to active */
        thinvpp_obj->dv[cpcbID].status = STATUS_ACTIVE;
    }
   return (MV_THINVPP_OK);
}

int MV_THINVPP_IsCPCBActive(int cpcbID)
{
    int vtotal;

    if (cpcbID == CPCB_1) {
        vtotal = (GLB_REG_READ32(MEMMAP_VPP_REG_BASE+(CPCB0_VT_H << 2)) & 0x0ff);
        vtotal <<= 8;
        vtotal |= (GLB_REG_READ32(MEMMAP_VPP_REG_BASE+(CPCB0_VT_L << 2)) & 0x0ff);
    } else if (cpcbID == CPCB_3) {
        vtotal = (GLB_REG_READ32(MEMMAP_VPP_REG_BASE+(CPCB2_VT_H << 2)) & 0x0ff);
        vtotal <<= 8;
        vtotal |= (GLB_REG_READ32(MEMMAP_VPP_REG_BASE+(CPCB2_VT_L << 2)) & 0x0ff);
    } else
        vtotal = 0;

    return (vtotal);
}

int MV_THINVPP_SetMainDisplayFrame(VBUF_INFO *pinfo)
{
    PLANE *plane;

    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    if (!pinfo)
        return (MV_THINVPP_EBADPARAM);

    plane = &(thinvpp_obj->plane[PLANE_MAIN]);
    plane->pinfo = pinfo;

    plane->actv_win.x = pinfo->m_active_left;
    plane->actv_win.y = pinfo->m_active_top;
    plane->actv_win.width  = pinfo->m_active_width;
    plane->actv_win.height = pinfo->m_active_height;

    return (MV_THINVPP_OK);
}

/******************************************************************************
 * FUNCTION: open a window of a video/graphics plane for display.
 *           the window is defined in end display resolution
 * INPUT: planeID - id of a video/grahpics plane
 *        *win - pointer to a vpp window struct
 *        *attr - pointer to a vpp window attribute struct
 * RETURN: MV_THINVPP_OK - SUCCEED
 *         MV_EBADPARAM - invalid parameters
 *         MV_EUNCONFIG - VPP not configured
 *         MV_EUNSUPPORT - plane not connected in configuration
 *         MV_ECMDQFULL - command queue is full
 ******************************************************************************/
int MV_THINVPP_OpenDispWindow(int planeID, VPP_WIN *win, VPP_WIN_ATTR *attr)
{
    int chanID;
    int cpcbID;
    PLANE *plane;
    CHAN *chan;


    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    if (planeID<FIRST_PLANE || planeID>=MAX_NUM_PLANES)
        return (MV_THINVPP_EBADPARAM);

    if (!win)
        return (MV_THINVPP_EBADPARAM);

    if ((win->width<=0) || (win->height<=0))
        return (MV_THINVPP_EBADPARAM);

    if (thinvpp_obj->status == STATUS_INACTIVE){
        /* VPP module is not configured */
        return (MV_THINVPP_EUNCONFIG);
    }

    plane = &(thinvpp_obj->plane[planeID]);
    chanID = planeID;
    chan = &(thinvpp_obj->chan[chanID]);
    cpcbID = chan->dvID;

    /* update video/graphics channel display window */
    chan->disp_win.x = win->x;
    chan->disp_win.y = win->y;
    chan->disp_win.width = win->width;
    chan->disp_win.height = win->height;

    if (attr){
        chan->disp_win_attr.bgcolor = attr->bgcolor;
        chan->disp_win_attr.alpha = attr->alpha;
    } else {
        chan->disp_win_attr.bgcolor = DEFAULT_BGCOLOR;
        chan->disp_win_attr.alpha = DEFAULT_ALPHA;
    }
    /* set video/graphics plane & channel in active status */
    plane->status = STATUS_ACTIVE;
    chan->status = STATUS_ACTIVE;

    return (MV_THINVPP_OK);
}

volatile int stop_flag=0;
int MV_THINVPP_CloseDispWindow(void)
{
    int n;
    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    return (MV_THINVPP_OK);
}

extern int cpcb_isr_count;
extern int curr_logo_tp_count;

int MV_THINVPP_Stop(void)
{
    int n;
    if (!thinvpp_obj)
        return (MV_THINVPP_ENODEV);

    printk(KERN_INFO "set stop_flag to 1\n");
    stop_flag = 1;

    while(1) {
        if(stop_flag > 1) break;

        msleep(20);
    }

    printk(KERN_INFO "VPP_dhub_sem_clear stop_flag=%d\n", stop_flag);
    VPP_dhub_sem_clear();

    return (MV_THINVPP_OK);
}


