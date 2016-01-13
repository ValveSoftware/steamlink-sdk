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



#ifndef _VPP_MODULE_H_
#define _VPP_MODULE_H_

#include "api_avio_dhub.h"
#include "thinvpp_common.h"
#include "thinvpp_api.h"
#include "thinvpp_bcmbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/slab.h>
#define THINVPP_MALLOC(s) kmalloc(s, GFP_KERNEL)
#define THINVPP_MEMSET memset
#define THINVPP_MEMCPY memcpy
#define THINVPP_FREE   kfree


/* helper MACROs */
#define CPCB_OF_CHAN(vpp_obj, chanID)      (vpp_obj->chan[chanID].dvID)
#define CPCB_OF_VOUT(vpp_obj, voutID)      (vpp_obj->vout[voutID].dvID)

#define START_1DDMA(dhubID, dmaID, start_addr, size, cfgQ, ptr) \
    dhub_channel_big_write_cmd(&(((HDL_dhub2d *)dhubID)->dhub), dmaID, (int)start_addr, size, 0, 0, 0, 1, cfgQ, ptr)

#define START_2DDMA(dhubID, dmaID, start_addr, stride, width, height, cfgQ) \
    dhub2d_channel_cfg((HDL_dhub2d *)dhubID, dmaID, start_addr, stride, width, height, 1, 0, 0, 0, 0, 1, cfgQ)

#define VPP_REG_WRITE32(reg, val) THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+(reg), (val))
#define VPP_REG_WRITE8(reg, val) THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr+((reg)<<2), (val))

#define GLB_REG_WRITE32(reg, val) *(volatile int *)(reg) = (val)
#define GLB_REG_READ32(reg)  (*(volatile int *)(reg))
#define VPP_REG_READ32_RAW(reg) (*(volatile int *)(vpp_obj->base_addr+(reg)))
#define VPP_REG_READ8_RAW(reg)  (*(volatile int *)(vpp_obj->base_addr+((reg)<<2)))
#define VPP_REG_WRITE32_RAW(reg, val) *(volatile int *)(vpp_obj->base_addr+(reg)) = (val)
#define VPP_REG_WRITE8_RAW(reg, val)  *(volatile int *)(vpp_obj->base_addr+((reg)<<2)) = (val)

/***************************************************************
* DESCR: get the one byte of a unsigned int
* PARAMS: uint32 - an unsigned int
****************************************************************/
#define GET_LOWEST_BYTE(uint32) ((uint32) & 0xFF)
#define GET_SECOND_BYTE(uint32) (((uint32) & 0xFF00) >> 8)
#define GET_THIRD_BYTE(uint32) (((uint32) & 0xFF0000) >> 16)

/***************************************************************
* DESCR: load register default value by name
* PARAMS: RegName - register name in maddr.h
*               DefValues - register default value
****************************************************************/
#define WRITE_REG_DEFAULT_VAL(RegName, DefValues)\
do{\
    THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr + (RegName << 2), DefValues.VPP_##RegName);\
}while(0)

#define WRITE_REG_DEFAULT_VAL_32BITS(RegName, DefValues)\
do{\
    THINVPP_BCMBUF_Write(vpp_obj->pVbiBcmBuf, vpp_obj->base_addr + RegName, DefValues.VPP_##RegName);\
}while(0)

/*Enum for maintaining CPCB 0,1 and 2 at Backend*/
typedef enum VPP_CPCB_UNIT_NUM/////////////////////////////////temp put here
{
    VPP_CPCB0  = 0,
    VPP_CPCB1,
    VPP_CPCB2,
    VPP_CPCB_MAX
}VPP_CPCB_UNIT_NUM, *PVPP_CPCB_UNIT_NUM;

/* default window attributes */
#define DEFAULT_BGCOLOR     0 // green
#define DEFAULT_ALPHA       0xff // opaque

/* definition of video resolution type */
enum {
    TYPE_SD = 0,
    TYPE_HD = 1,
};

/* definition of video scan mode */
enum {
    SCAN_PROGRESSIVE = 0,
    SCAN_INTERLACED  = 1,
};

/* definition of video frame-rate */
enum {
    FRAME_RATE_23P98 = 0,
    FRAME_RATE_24    = 1,
    FRAME_RATE_25    = 2,
    FRAME_RATE_29P97 = 3,
    FRAME_RATE_30    = 4,
    FRAME_RATE_47P96 = 5,
    FRAME_RATE_48    = 6,
    FRAME_RATE_50    = 7,
    FRAME_RATE_59P94 = 8,
    FRAME_RATE_60    = 9,
    FRAME_RATE_100   = 10,
    FRAME_RATE_119P88 = 11,
    FRAME_RATE_120   = 12,
};

/* definition of EE working mode */
enum {
    EE_MODE_OFF   = 0, /* disable EE */
    EE_MODE_ON    = 1, /* enable EE*/
};

/* definition of VPP in-line and off-line modes */
enum {
    MODE_INLINE     = 0,
    MODE_OFFLINE    = 1,
};

/* definition of HV scaling coeff selection */
enum {
    MODE_COEFF_SEL_AUTO      = 0,
    MODE_COEFF_SEL_FORCE_H   = 1,
    MODE_COEFF_SEL_FORCE_V   = 2,
    MODE_COEFF_SEL_FORCE_HV  = 3,
};

/* definition of stages of a pipeline */
enum {
    STAGE_FE    = 0,
    STAGE_SCL   = 1,
    STAGE_CPCB  = 2,
    STAGE_BE    = 3,
    MAX_NUM_STAGES = 4
};

enum {
    SENSIO_DISABLE     = 0,
    SENSIO_SCALE_ONLY  = 1,
    SENSIO_FILTER_ONLY = 2,
    SENSIO_FULL        = 3,
};

typedef struct EVENT_CALLBACK_T{
    volatile int status;         // registered or not
    FUNCPTR cb_func;    // callback function
    int params[MAX_NUM_PARAMS];     // function parameters
} EVENT_CALLBACK;

typedef struct OFFLINE_BUFF_DESCR_T{
    void *addr; // offline buffer address
    int size; // data size in byte
    VPP_WIN disp_win; // display window in end display for content in this offline buffer
    VPP_WIN content_win; // content window in end display for content in this offline buffer
}OFFLINE_BUFF_DESCR;

typedef struct PLANE_T {
    volatile int status;         // active or not
    int mode;           // off-line or in-line mode, only valid for main & PIP video
    int srcfmt;         // video component data format in input frame buffer
    int order;          // video component data order in input frame buffer

    int wpl;            // beat per line of current frame

    int dmaRID;         // plane inline read DMA channel ID
    int dmaRdhubID;     // ID of Dhub on which inline read DMA channel is connected

    /* offline write-back/read-back DMAs are only available for Berlin Main/PIP/AUX plane */
    int offline_dmaWID;  // plane offline write-back DMA channel ID
    int offline_dmaWdhubID;     // ID of Dhub on which offline write-back DMA channel is connected
    int offline_dmaRID;  // plane offline read-back DMA channel ID
    int offline_dmaRdhubID;     // ID of Dhub on which offline read-back DMA channel is connected

    VPP_WIN actv_win;   // plane display content location and size in reference window
                        // this information is a copy from frame descriptor for register updating purpose.
    VBUF_INFO * pinfo;
} PLANE;


typedef struct CHAN_T {
    volatile int status;          // active or not
    int dvID;            // ID of the DV, which this channel belongs to
    int dvlayerID;       // ID of DV layer, which this channel connects to
    int zorder;          // Z-order of DV plane, which this channel connects to
    int scl_in_out_mode;

    VPP_WIN disp_win;                // channel display window in end display
    VPP_WIN_ATTR disp_win_attr;      // channel display window attribute: bgcolor and alpha
} CHAN;

typedef struct DV_T {
    volatile int status;      // active or not
    int num_of_chans;   // number of the channels, which belong to this channel
    int chanID[MAX_NUM_CHANS]; // ID of the channels, which belong to this DV
    int num_of_vouts;   // number of the VOUTs, which belong to this channel
    int voutID[MAX_NUM_VOUTS]; // ID of the VOUTs, which belong to this DV
    int output_res;            // ID of CPCB(for Berlin) or DV(for Galois) output resolution, i.e. end display resolution

    DHUB_CFGQ vbi_dma_cfgQ[2];
    DHUB_CFGQ *curr_cpcb_vbi_dma_cfgQ;
    DHUB_CFGQ vbi_bcm_cfgQ[2];
    DHUB_CFGQ *curr_cpcb_vbi_bcm_cfgQ;

    volatile int vbi_num;     // VBI interrupt counter
} DV;

typedef struct VOUT_T {
    volatile int status;      // active or not
    int dvID;        // ID of the DV, which this VOUT belongs to
} VOUT;


/************************************************/
/*********** VPP object data structures *********/
/************************************************/
typedef struct THINVPP_OBJ_T {
    volatile int    status;                 // active or not

    PLANE  plane[MAX_NUM_PLANES];  // CURSOR, OSD, IG, PG, PIP, MAIN, BG
    CHAN   chan[MAX_NUM_CHANS];    // OSD, PG, PIP, MAIN and BG
    DV     dv[MAX_NUM_CPCBS];        // CPCB-0, CPCB-1, and CPCB-2
    VOUT   vout[MAX_NUM_VOUTS];    // HDMI, Component, S-Video and CVBS

    unsigned int base_addr;       // VPP object hardware base address
    HDL_semaphore *pSemHandle; // semaphore handler of CPCB semaphores

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
    int dhub_cmdQ[avioDhubChMap_vpp_TT_R];
#else //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
    UINT32 dhub_cmdQ[avioDhubChMap_vpp_SPDIF_W];
#endif //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)

    int hdmi_mute;
    BCMBUF vbi_bcm_buf[2];
    BCMBUF *pVbiBcmBufCpcb[VPP_CPCB_MAX];
    BCMBUF *pVbiBcmBuf;    //pointer to the VBI BCM buffer in use

} THINVPP_OBJ;


/************************************************/
/*********** global VPP container ***************/
/************************************************/
#ifdef _THINVPP_API_C_
THINVPP_OBJ *thinvpp_obj = NULL;
#else
extern THINVPP_OBJ *thinvpp_obj;
#endif

/**************************************************************************/
/****************** Common VPP object Function Calls **********************/
/**************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
