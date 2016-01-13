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

#ifndef _THINVPP_COMMON_H_
#define _THINVPP_COMMON_H_

#include "com_type.h"

/* maximum number of VPP objects */
#define MAX_NUM_OBJECTS  1

/* maximum number of integer parameters of callback functions */
#define MAX_NUM_PARAMS    10

/* maximum number of frames in frame buffer queue */
#define MAX_NUM_FRAMES    20

/* maximum number of commands in command queue */
#define MAX_NUM_CMDS      100

/* maximum number of events */
#define MAX_NUM_EVENTS    32

/* BCM buffer size */
/* DV1_BCM_BUFFER_SIZE + DV2_BCM_BUFFER_SIZE + DV3_BCM_BUFFER_SIZE = BCM_BUFFER_SIZE */
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
#define BCM_BUFFER_SIZE   0xA000//20480
#else //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
#define BCM_BUFFER_SIZE   0x17700 //96000
#define DV1_BCM_BUFFER_SIZE   0x7d00 //32000
#define DV2_BCM_BUFFER_SIZE   0x7d00 //32000
#define DV3_BCM_BUFFER_SIZE   0x7d00 //32000
#define VDE_BCM_BUFFER_SIZE   0x800
#endif //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)

/* 3D De-interlacer field buffer size */
#define DEINT_BUFFER_SIZE   (1920*1081*26*3/8) // 6 1080I frame (26-bit per pixel)

/* offline scaling threshold ratio */
#define OFFLINE_SCL_THRESH_VRATIO 2

/* offline frame buffer size */
#define OFFLINE_BUFFER_SIZE   (1920*1080*2/(OFFLINE_SCL_THRESH_VRATIO))  // 2 offline frames
#define AUX_OFFLINE_BUFFER_SIZE   (720*576*2)  // 2 aux offline frames

/* CC data queue size */
#define CC_DATA_QUEUE_SIZE   (100)

/* WSS data queue size */
#define WSS_DATA_QUEUE_SIZE   (10)

/* CGMS data queue size */
#define CGMS_DATA_QUEUE_SIZE   (10)

/* TT data length per frame */
#define TT_DATA_LEN_PER_FRAME   (40 * 4)

/* TT data queue size */
#define TT_DATA_QUEUE_SIZE   (TT_DATA_LEN_PER_FRAME * 10)

/* DMA command buffer size */
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
#define DMA_CMD_BUFFER_SIZE   (20 * 8)
#else //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
#define DMA_CMD_BUFFER_SIZE   (100 * 8)
#endif //(BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)

/* Hardware interface constants */

/* Register width in Berlin is 32 bits */
typedef unsigned int REG_WIDTH;
#define SIZE_OF_REG     sizeof(REG_WIDTH)

// Packed structure and packed array attributes
#ifndef PACKED_STRUCT
    #ifdef __LINUX__
        #define PACKED_STRUCT __attribute__((packed))
        #define PACKED_ARRAY  __attribute__((aligned(1)))
    #else
        #define PACKED_STRUCT
        #define PACKED_ARRAY
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* callback function type definition */
typedef int (*FUNCPTR)(void *, void *);
#ifdef __cplusplus
}
#endif

#endif
