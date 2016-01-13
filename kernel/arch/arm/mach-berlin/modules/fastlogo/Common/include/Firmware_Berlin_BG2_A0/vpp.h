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


#ifndef vpp_h
#define vpp_h (){}

    #define     RA_VPP_REG_dummy                               0x0000
    #define     RA_CLK_ctrl                                    0x0000
    #define     RA_FE_BE_FIFO_CTRL                             0x0000
    #define     RA_FE_BE_LRST                                  0x0004
    #define     RA_FE_BE_FRST                                  0x0008
    #define     RA_FE_BE_SYNC_CTRL                             0x000C
    #define     RA_DVI_FE_BE_FIFO_CTRL                         0x0000
    #define     RA_DVI_FE_BE_LRST                              0x0004
    #define     RA_DVI_FE_BE_FRST                              0x0008
    #define     RA_DVI_FE_BE_SYNC_CTRL                         0x000C
    #define     RA_DAC_ctrl_ctrl0                              0x0000
    #define     RA_DAC_ctrl_ctrl1                              0x0004
    #define     RA_VDAC_ctrl_a                                 0x0000
    #define     RA_VDAC_ctrl_b                                 0x0008
    #define     RA_VDAC_ctrl_c                                 0x0010
    #define     RA_VDAC_ctrl_d                                 0x0018
    #define     RA_VDAC_ctrl_clk                               0x0020
    #define     RA_VDAC_ctrl_misc1                             0x0024
    #define     RA_VDAC_ctrl_misc2                             0x0028
    #define     RA_VDAC_ctrl_misc21                            0x002C
    #define     RA_VDAC_sts_a                                  0x0000
    #define     RA_VDAC_sts_b                                  0x0004
    #define     RA_VDAC_sts_c                                  0x0008
    #define     RA_VDAC_sts_d                                  0x000C
    #define     RA_LVDS_ctrl_ctrl0                             0x0000
    #define     RA_LVDS_ctrl_ctrl1                             0x0004
    #define     RA_DUAL_LVDS_ctrl_a                            0x0000
    #define     RA_DUAL_LVDS_ctrl_b                            0x0008
    #define     RA_DUAL_LVDS_ctrl_PLL                          0x0010
    #define     RA_DUAL_LVDS_ctrl_PLL1                         0x0014
    #define     RA_DUAL_LVDS_ctrl_SSC                          0x0018
    #define     RA_DUAL_LVDS_ctrl_SSC1                         0x001C
    #define     RA_DUAL_LVDS_ctrl_FREQ_OFFSET                  0x0020
    #define     RA_PLANE_SIZE                                  0x0000
    #define     RA_PLANE_CROP                                  0x0004
    #define     RA_PLANE_FORMAT                                0x0008
    #define     RA_PLANE_START                                 0x000C
    #define     RA_PLANE_CLEAR                                 0x0010
    #define     RA_CQUAD_ARGB                                  0x0000
    #define     RA_CLUT_CLUT                                   0x0000
    #define     RA_CSC_const_C0                                0x0000
    #define     RA_CSC_const_C1                                0x0004
    #define     RA_CSC_const_C2                                0x0008
    #define     RA_CSC_const_C3                                0x000C
    #define     RA_CSC_const_C4                                0x0010
    #define     RA_CSC_const_C5                                0x0014
    #define     RA_CSC_const_C6                                0x0018
    #define     RA_CSC_const_C7                                0x001C
    #define     RA_CSC_const_C8                                0x0020
    #define     RA_CSC_off_A0                                  0x0024
    #define     RA_CSC_off_A1                                  0x0028
    #define     RA_CSC_off_A2                                  0x002C
    #define     RA_ICSC_const_C0                               0x0000
    #define     RA_ICSC_const_C1                               0x0004
    #define     RA_ICSC_const_C2                               0x0008
    #define     RA_ICSC_const_C3                               0x000C
    #define     RA_ICSC_const_C4                               0x0010
    #define     RA_ICSC_const_C5                               0x0014
    #define     RA_ICSC_const_C6                               0x0018
    #define     RA_ICSC_const_C7                               0x001C
    #define     RA_ICSC_const_C8                               0x0020
    #define     RA_ICSC_off_A0                                 0x0024
    #define     RA_ICSC_off_A1                                 0x0028
    #define     RA_ICSC_off_A2                                 0x002C
    #define     RA_BG_PLANE                                    0x0000
    #define     RA_MAIN_PLANE                                  0x0000
    #define     RA_PIP_PLANE                                   0x0000
    #define     RA_PG_PLANE                                    0x0000
    #define     RA_PG_LUT                                      0x0400
    #define     RA_IG_PLANE                                    0x0000
    #define     RA_IG_LUT                                      0x0400
    #define     RA_CURSOR_PLANE                                0x0000
    #define     RA_CURSOR_LUT                                  0x0400
    #define     RA_CURSOR_CSC                                  0x0800
    #define     RA_MOSD_PLANE                                  0x0000
    #define     RA_MOSD_LUT                                    0x0400
    #define     RA_LDR_BG                                      0x0000
    #define     RA_LDR_MAIN                                    0x0014
    #define     RA_LDR_PIP                                     0x0028
    #define     RA_LDR_PG                                      0x0400
    #define     RA_LDR_IG                                      0x0C00
    #define     RA_LDR_CURSOR                                  0x1400
    #define     RA_LDR_MOSD                                    0x2000
    #define     RA_TG_INIT                                     0x0000
    #define     RA_TG_SIZE                                     0x0004
    #define     RA_TG_HS                                       0x0008
    #define     RA_TG_HB                                       0x000C
    #define     RA_TG_VS0                                      0x0010
    #define     RA_TG_VS1                                      0x0014
    #define     RA_TG_VB0                                      0x0018
    #define     RA_TG_VB1                                      0x001C
    #define     RA_TG_SCAN                                     0x0020
    #define     RA_TG_INTPOS                                   0x0024
    #define     RA_TG_MODE                                     0x0028
    #define     RA_TG_HVREF                                    0x002C
    #define     RA_VIP_ctrl_DVI_FE_BE                          0x0024
    #define     RA_VIP_ctrl_SCL_CLKEN_CTRL                     0x0034
    #define     RA_VIP_ctrl_DVI_LSIZE                          0x0038
    #define     RA_VIP_ctrl_OFRST_SEL                          0x003C
    #define     RA_VIP_ctrl_OFRST_SW                           0x0040
    #define     RA_VIP_ctrl_intr_sel                           0x0044
    #define     RA_VIP_ctrl_vbi_vde_cnt                        0x0048
    #define     RA_Vpp_cfgReg                                  0x0000
    #define     RA_Vpp_cpcb0Clk                                0x10000
    #define     RA_Vpp_cpcb1Clk                                0x10004
    #define     RA_Vpp_cpcb2Clk                                0x10008
    #define     RA_Vpp_FE_BE                                   0x1000C
    #define     RA_Vpp_VP_CLKEN_CTRL                           0x1001C
    #define     RA_Vpp_FE_MAIN_CTRL                            0x10020
    #define     RA_Vpp_FE_PIP_CTRL                             0x10024
    #define     RA_Vpp_FE_OSD_CTRL                             0x10028
    #define     RA_Vpp_FE_PG_CTRL                              0x1002C
    #define     RA_Vpp_CPCB0_FLD                               0x10030
    #define     RA_Vpp_CPCB2_FLD                               0x10034
    #define     RA_Vpp_FE_PAT_SEL                              0x10038
    #define     RA_Vpp_mainW                                   0x1003C
    #define     RA_Vpp_mainR                                   0x10040
    #define     RA_Vpp_pipW                                    0x10044
    #define     RA_Vpp_pipR                                    0x10048
    #define     RA_Vpp_auxW                                    0x1004C
    #define     RA_Vpp_auxR                                    0x10050
    #define     RA_Vpp_rst                                     0x10054
    #define     RA_Vpp_VDAC_ctrl                               0x10058
    #define     RA_Vpp_VDAC_sts                                0x10088
    #define     RA_Vpp_DUAL_LVDS__ctrl                         0x10098
    #define     RA_Vpp_DUAL_LVDS_sts                           0x100BC
    #define     RA_Vpp_HDMI_ctrl                               0x100C0
    #define     RA_Vpp_HDMI_sts                                0x100D0
    #define     RA_Vpp_regIfCtrl                               0x100D4
    #define     RA_Vpp_MAIN_LSIZE                              0x100D8
    #define     RA_Vpp_PIP_LSIZE                               0x100DC
    #define     RA_Vpp_OSD_LSIZE                               0x100E0
    #define     RA_Vpp_PG_LSIZE                                0x100E4
    #define     RA_Vpp_AUX_LSIZE                               0x100E8
    #define     RA_Vpp_vpIn_pix                                0x100EC
    #define     RA_Vpp_vpOut_pix                               0x100F0
    #define     RA_Vpp_pip_pix                                 0x100F4
    #define     RA_Vpp_osd_pix                                 0x100F8
    #define     RA_Vpp_pg_pix                                  0x100FC
    #define     RA_Vpp_diW_pix                                 0x10100
    #define     RA_Vpp_diR_word                                0x10104
    #define     RA_Vpp_mainW_pix                               0x10108
    #define     RA_Vpp_mainR_word                              0x1010C
    #define     RA_Vpp_pipW_pix                                0x10110
    #define     RA_Vpp_pipR_word                               0x10114
    #define     RA_Vpp_auxW_pix                                0x10118
    #define     RA_Vpp_auxR_word                               0x1011C
    #define     RA_Vpp_main_ols                                0x10120
    #define     RA_Vpp_pip_ols                                 0x10124
    #define     RA_Vpp_enc_hsvs_sel                            0x10128
    #define     RA_Vpp_CPCB_FIFO_UF                            0x1012C
    #define     RA_Vpp_VP_TG                                   0x10130
    #define     RA_Vpp_LDR                                     0x10400
    #define     RA_Vpp_HDMI2DVAO                               0x12C00
    #define     RA_Vpp_VP_DMX_CTRL                             0x12C04
    #define     RA_Vpp_VP_DMX_HRES                             0x12C08
    #define     RA_Vpp_VP_DMX_HT                               0x12C0C
    #define     RA_Vpp_VP_DMX_VRES                             0x12C10
    #define     RA_Vpp_VP_DMX_VT                               0x12C14
    #define     RA_Vpp_VP_DMX_IVT                              0x12C18
    #define     RA_Vpp_CPCB0_PL_EN                             0x12C1C
    #define     RA_Vpp_CPCB1_PL_EN                             0x12C20
    #define     RA_Vpp_MAIN_WCLIENT                            0x12C24
    #define     RA_Vpp_PIP_WCLIENT                             0x12C28
    #define     RA_Vpp_DIW_CLIENT                              0x12C2C
    #define     RA_Vpp_DIR0_CLIENT                             0x12C30
    #define     RA_Vpp_DIR1_CLIENT                             0x12C34
    #define     RA_Vpp_SD_TT_CLIENT                            0x12C38
    #define     RA_Vpp_SD_TT_BYTE                              0x12C3C
    #define     RA_Vpp_DAC_RAMP_CTRL                           0x12C40
    #define     RA_Vpp_DAC_TEST_CTRL                           0x12C44
    #define     RA_Vpp_SCL_CLKEN_CTRL                          0x12C48
    #define     RA_Vpp_PAT_DNS_CTRL                            0x12C4C
    #define     RA_Vpp_CPCB0_FLD_STS                           0x12C50
    #define     RA_Vpp_CPCB2_FLD_STS                           0x12C54
    #define     RA_Vpp_SD_TT_TEST                              0x12C58
    #define     RA_Vpp_DUMMY0                                  0x12C5C
    #define     RA_Vpp_DEBUG0                                  0x12C60
    #define     RA_Vpp_MAIN_SCL_CROP                           0x12C64
    #define     RA_Vpp_PIP_SCL_CROP                            0x12C68
    #define     RA_Vpp_PIP_LUMA_KEY                            0x12C6C
    #define     RA_Vpp_PIP_AL_IN                               0x12C70
    #define     RA_Vpp_PIP_AL_OUT                              0x12C74
    #define     RA_Vpp_mosd_pix                                0x12C78
    #define     RA_Vpp_MOSD_LSIZE                              0x12C7C
    #define     RA_Vpp_FE_MOSD_CTRL                            0x12C80
    #define     RA_Vpp_OVERLAY_MUX                             0x12C84
    #define     RA_Vpp_MAIN_OV_FXD_IMG                         0x12C88
    #define     RA_Vpp_PIP_OV_FXD_IMG                          0x12C8C
    #define     RA_Vpp_IG_OV_FXD_IMG                           0x12C90
    #define     RA_Vpp_PG_OV_FXD_IMG                           0x12C94
    #define     RA_Vpp_CURSOR_OV_FXD_IMG                       0x12C98
    #define     RA_Vpp_MOSD_OV_FXD_IMG                         0x12C9C
    #define     RA_Vpp_BG_OV_FXD_IMG                           0x12CA0
    #define     RA_Vpp_DET_OV_FXD_IMG                          0x12CA4
    #define     RA_Vpp_SENSIO_CTRL0                            0x12CA8
    #define     RA_Vpp_SENSIO_CTRL                             0x12CAC
    #define     RA_Vpp_ICSC_DEGAMMA                            0x12CC8
    #define     RA_Vpp_ICSC_GAMMA                              0x12CF8
    #define     RA_Vpp_GAMMA_DEGAMMA_PATH_bypass               0x12D28
    #define     RA_Vpp_VIP_ctrl                                0x18000

#endif
