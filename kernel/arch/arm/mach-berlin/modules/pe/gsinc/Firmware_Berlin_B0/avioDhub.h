/*******************************************************************************
* Copyright (C) Marvell International Ltd. and its affiliates
*
* Marvell GPL License Option
*
* If you received this File from Marvell, you may opt to use, redistribute and/or
* modify this File in accordance with the terms and conditions of the General
* Public License Version 2, June 1991 (the GPL License), a copy of which is
* available along with the File in the license.txt file or by writing to the Free
* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
* on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
*
* THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
* WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
* DISCLAIMED.  The GPL License provides additional details about this warranty
* disclaimer.
********************************************************************************/


#ifndef avioDhub_h
#define avioDhub_h (){}

    #define     RA_SemaINTR_mask                               0x0000
    #define     RA_Semaphore_CFG                               0x0000
    #define     RA_Semaphore_INTR                              0x0004
    #define     RA_Semaphore_mask                              0x0010
    #define     RA_SemaQuery_RESP                              0x0000
    #define     RA_SemaQueryMap_ADDR                           0x0000
    #define     RA_SemaHub_Query                               0x0000
    #define     RA_SemaHub_counter                             0x0000
    #define     RA_SemaHub_ARR                                 0x0100
    #define     RA_SemaHub_cell                                0x0100
    #define     RA_SemaHub_PUSH                                0x0380
    #define     RA_SemaHub_POP                                 0x0384
    #define     RA_SemaHub_empty                               0x0388
    #define     RA_SemaHub_full                                0x038C
    #define     RA_SemaHub_almostEmpty                         0x0390
    #define     RA_SemaHub_almostFull                          0x0394
    #define     RA_FiFo_CFG                                    0x0000
    #define     RA_FiFo_START                                  0x0004
    #define     RA_FiFo_CLEAR                                  0x0008
    #define     RA_FiFo_FLUSH                                  0x000C
    #define     RA_HBO_FiFoCtl                                 0x0000
    #define     RA_HBO_ARR                                     0x0400
    #define     RA_HBO_FiFo                                    0x0400
    #define     RA_HBO_BUSY                                    0x0600
    #define     RA_LLDesFmt_mem                                0x0000
    #define     RA_dHubCmdHDR_DESC                             0x0000
    #define     RA_dHubCmd_MEM                                 0x0000
    #define     RA_dHubCmd_HDR                                 0x0004
    #define     RA_dHubChannel_CFG                             0x0000
    #define     RA_dHubChannel_START                           0x0004
    #define     RA_dHubChannel_CLEAR                           0x0008
    #define     RA_dHubChannel_FLUSH                           0x000C
    #define     RA_dHubReg_SemaHub                             0x0000
    #define     RA_dHubReg_HBO                                 0x0400
    #define     RA_dHubReg_ARR                                 0x0B00
    #define     RA_dHubReg_channelCtl                          0x0B00
    #define     RA_dHubReg_BUSY                                0x0C00
    #define     RA_dHubReg_PENDING                             0x0C04
    #define     RA_dHubReg_busRstEn                            0x0C08
    #define     RA_dHubReg_busRstDone                          0x0C0C
    #define     RA_dHubReg_flowCtl                             0x0C10
    #define     RA_dHubCmd2D_MEM                               0x0000
    #define     RA_dHubCmd2D_DESC                              0x0004
    #define     RA_dHubCmd2D_START                             0x0008
    #define     RA_dHubCmd2D_CLEAR                             0x000C
    #define     RA_dHubCmd2D_HDR                               0x0010
    #define     RA_dHubQuery_RESP                              0x0000
    #define     RA_dHubReg2D_dHub                              0x0000
    #define     RA_dHubReg2D_ARR                               0x0D00
    #define     RA_dHubReg2D_Cmd2D                             0x0D00
    #define     RA_dHubReg2D_BUSY                              0x0F00
    #define     RA_dHubReg2D_CH_ST                             0x0F40
    #define     RA_avioDhubTcmMap_dummy                        0x0000
    #define     RA_vppDhub_tcm0                                0x0000
    #define     RA_vppDhub_dHub0                               0x10000
    #define     RA_agDhub_tcm0                                 0x0000
    #define     RA_agDhub_dHub0                                0x10000

#endif
