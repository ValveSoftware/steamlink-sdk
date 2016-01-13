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
    #define     RA_dHubReg_axiCmdCol                           0x0C14
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
    #define        avioDhubChMap_vpp_MV_R                                   0x0
    #define        avioDhubChMap_vpp_MV_FRC_R                               0x1
    #define        avioDhubChMap_vpp_MV_FRC_W                               0x2
    #define        avioDhubChMap_vpp_PIP_R                                  0x3
    #define        avioDhubChMap_vpp_PIP_FRC_R                              0x4
    #define        avioDhubChMap_vpp_PIP_FRC_W                              0x5
    #define        avioDhubChMap_vpp_DINT0_R                                0x6
    #define        avioDhubChMap_vpp_DINT1_R                                0x7
    #define        avioDhubChMap_vpp_DINT_W                                 0x8
    #define        avioDhubChMap_vpp_BCM_R                                  0x9
    #define        avioDhubChMap_vpp_HDMI_R                                 0xA
    #define        avioDhubChMap_vpp_AUX_FRC_R                              0xB
    #define        avioDhubChMap_vpp_AUX_FRC_W                              0xC
    #define        avioDhubChMap_vpp_BG_R                                   0xD
    #define        avioDhubChMap_vpp_TT_R                                   0xE
    #define        avioDhubChMap_ag_APPCMD_R                                0x0
    #define        avioDhubChMap_ag_MA0_R                                   0x1
    #define        avioDhubChMap_ag_MA1_R                                   0x2
    #define        avioDhubChMap_ag_MA2_R                                   0x3
    #define        avioDhubChMap_ag_MA3_R                                   0x4
    #define        avioDhubChMap_ag_SA_R                                    0x5
    #define        avioDhubChMap_ag_SPDIF_R                                 0x6
    #define        avioDhubChMap_ag_MIC_W                                   0x7
    #define        avioDhubChMap_ag_APPDAT_R                                0x8
    #define        avioDhubChMap_ag_APPDAT_W                                0x9
    #define        avioDhubChMap_ag_MOSD_R                                  0xA
    #define        avioDhubChMap_ag_CSR_R                                   0xB
    #define        avioDhubChMap_ag_GFX_R                                   0xC
    #define        avioDhubChMap_ag_PG_R                                    0xD
    #define        avioDhubChMap_ag_PG_ENG_R                                0xE
    #define        avioDhubChMap_ag_PG_ENG_W                                0xF
    #define        avioDhubSemMap_vpp_CH0_intr                              0x0
    #define        avioDhubSemMap_vpp_CH1_intr                              0x1
    #define        avioDhubSemMap_vpp_CH2_intr                              0x2
    #define        avioDhubSemMap_vpp_CH3_intr                              0x3
    #define        avioDhubSemMap_vpp_CH4_intr                              0x4
    #define        avioDhubSemMap_vpp_CH5_intr                              0x5
    #define        avioDhubSemMap_vpp_CH6_intr                              0x6
    #define        avioDhubSemMap_vpp_CH7_intr                              0x7
    #define        avioDhubSemMap_vpp_CH8_intr                              0x8
    #define        avioDhubSemMap_vpp_CH9_intr                              0x9
    #define        avioDhubSemMap_vpp_CH10_intr                             0xA
    #define        avioDhubSemMap_vpp_CH11_intr                             0xB
    #define        avioDhubSemMap_vpp_CH12_intr                             0xC
    #define        avioDhubSemMap_vpp_CH13_intr                             0xD
    #define        avioDhubSemMap_vpp_CH14_intr                             0xE
    #define        avioDhubSemMap_vpp_CH15_intr                             0xF
    #define        avioDhubSemMap_vpp_MV_R_SEM                              0x10
    #define        avioDhubSemMap_vpp_PIP_R_SEM                             0x11
    #define        avioDhubSemMap_vpp_BCM_R_SEM                             0x12
    #define        avioDhubSemMap_vpp_vppCPCB0_intr                         0x13
    #define        avioDhubSemMap_vpp_vppCPCB1_intr                         0x14
    #define        avioDhubSemMap_vpp_vppCPCB2_intr                         0x15
    #define        avioDhubSemMap_vpp_vppOUT0_intr                          0x16
    #define        avioDhubSemMap_vpp_vppOUT1_intr                          0x17
    #define        avioDhubSemMap_vpp_vppOUT2_intr                          0x18
    #define        avioDhubSemMap_vpp_vppOUT3_intr                          0x19
    #define        avioDhubSemMap_vpp_vppOUT4_intr                          0x1A
    #define        avioDhubSemMap_vpp_vppOUT5_intr                          0x1B
    #define        avioDhubSemMap_vpp_vppOUT6_intr                          0x1C
    #define        avioDhubSemMap_ag_CH0_intr                               0x0
    #define        avioDhubSemMap_ag_CH1_intr                               0x1
    #define        avioDhubSemMap_ag_CH2_intr                               0x2
    #define        avioDhubSemMap_ag_CH3_intr                               0x3
    #define        avioDhubSemMap_ag_CH4_intr                               0x4
    #define        avioDhubSemMap_ag_CH5_intr                               0x5
    #define        avioDhubSemMap_ag_CH6_intr                               0x6
    #define        avioDhubSemMap_ag_CH7_intr                               0x7
    #define        avioDhubSemMap_ag_CH8_intr                               0x8
    #define        avioDhubSemMap_ag_CH9_intr                               0x9
    #define        avioDhubSemMap_ag_CH10_intr                              0xA
    #define        avioDhubSemMap_ag_CH11_intr                              0xB
    #define        avioDhubSemMap_ag_CH12_intr                              0xC
    #define        avioDhubSemMap_ag_CH13_intr                              0xD
    #define        avioDhubSemMap_ag_CH14_intr                              0xE
    #define        avioDhubSemMap_ag_CH15_intr                              0xF
    #define        avioDhubSemMap_ag_audio_intr                             0x10
    #define        avioDhubSemMap_ag_app_intr0                              0x11
    #define        avioDhubSemMap_ag_app_intr1                              0x12
    #define        avioDhubSemMap_ag_app_intr2                              0x13
    #define        avioDhubSemMap_ag_app_intr3                              0x14
    #define        avioDhubSemMap_ag_MA0_R_SEM                              0x15
    #define        avioDhubSemMap_ag_MA1_R_SEM                              0x16
    #define        avioDhubSemMap_ag_MA2_R_SEM                              0x17
    #define        avioDhubSemMap_ag_MA3_R_SEM                              0x18
    #define        avioDhubSemMap_ag_SA_R_SEM                               0x19
    #define        avioDhubSemMap_ag_SPDIF_R_SEM                            0x1A
    #define        avioDhubSemMap_ag_MIC_W_SEM                              0x1B
    #define        avioDhubSemMap_ag_spu_intr0                              0x1C
    #define        avioDhubSemMap_ag_spu_intr1                              0x1D

#endif
