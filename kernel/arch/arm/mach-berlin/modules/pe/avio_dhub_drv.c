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

#include "api_avio_dhub.h"

HDL_dhub2d AG_dhubHandle;
HDL_dhub2d VPP_dhubHandle;
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
HDL_dhub2d VIP_dhubHandle;
#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
DHUB_channel_config  VIP_config[VIP_NUM_OF_CHANNELS] = {
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	// BANK0
	{ avioDhubChMap_vip_SD_WR, VIP_DHUB_BANK0_START_ADDR, VIP_DHUB_BANK0_START_ADDR+32, 32, (6144-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
   	{ avioDhubChMap_vip_SD_RE, VIP_DHUB_BANK0_START_ADDR + 6144, VIP_DHUB_BANK0_START_ADDR + 6144+32, 32, (6144-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#if (BERLIN_CHIP_VERSION == BERLIN_BG2) // Z1
	// BANK1
	{ avioDhubChMap_vip_HDMI_DWR, VIP_DHUB_BANK1_START_ADDR, VIP_DHUB_BANK1_START_ADDR+32, 32, (1024*4-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_HDMI_AWR0, VIP_DHUB_BANK1_START_ADDR + 4096, VIP_DHUB_BANK1_START_ADDR + 4096+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_HDMI_AWR1, VIP_DHUB_BANK1_START_ADDR + 6144, VIP_DHUB_BANK1_START_ADDR + 6144+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_HDMI_AWR2, VIP_DHUB_BANK1_START_ADDR + 8192, VIP_DHUB_BANK1_START_ADDR + 8192+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_HDMI_AWR3, VIP_DHUB_BANK1_START_ADDR + 10240, VIP_DHUB_BANK1_START_ADDR + 10240+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK2
	{ avioDhubChMap_vip_DVI_WR, VIP_DHUB_BANK2_START_ADDR, VIP_DHUB_BANK2_START_ADDR+32, 32, (1024*12-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK3
	{ avioDhubChMap_vip_VBI_WR, VIP_DHUB_BANK3_START_ADDR, VIP_DHUB_BANK3_START_ADDR+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#elif (BERLIN_CHIP_VERSION == BERLIN_BG2_Z2) //Z2
	// BANK1
	{ avioDhubChMap_vip_DVI_WR, VIP_DHUB_BANK1_START_ADDR, VIP_DHUB_BANK1_START_ADDR+32, 32, (1024*12-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK2
	{ avioDhubChMap_vip_VBI_WR, VIP_DHUB_BANK2_START_ADDR, VIP_DHUB_BANK2_START_ADDR+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	
#else // (BERLIN_CHIP_VERSION == BERLIN_BG2_A0) //A0
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	// BANK1
        { avioDhubChMap_vip_DVI_WR, VIP_DHUB_BANK1_START_ADDR, VIP_DHUB_BANK1_START_ADDR+32, 32, (1024*12-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
        // BANK2
        //use 8-byte MTU for VBI data workaround!
        { avioDhubChMap_vip_VBI_WR, VIP_DHUB_BANK2_START_ADDR, VIP_DHUB_BANK2_START_ADDR+32, 32, (1024-32), dHubChannel_CFG_MTU_8byte, 0, 0, 1},
	{ avioDhubChMap_vip_MIC0_W, VIP_DHUB_BANK2_START_ADDR+1024, VIP_DHUB_BANK2_START_ADDR+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_MIC1_W, VIP_DHUB_BANK2_START_ADDR+1024*2, VIP_DHUB_BANK2_START_ADDR+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}, 
	{ avioDhubChMap_vip_MIC2_W, VIP_DHUB_BANK2_START_ADDR+1024*3, VIP_DHUB_BANK2_START_ADDR+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
        { avioDhubChMap_vip_MIC3_W, VIP_DHUB_BANK2_START_ADDR+1024*4, VIP_DHUB_BANK2_START_ADDR+1024*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
	// BANK1
        { avioDhubChMap_vip_DVI_WR, VIP_DHUB_BANK0_START_ADDR, VIP_DHUB_BANK0_START_ADDR+32, 32, (1024*12-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
        // BANK2
	{ avioDhubChMap_vip_MIC0_W, VIP_DHUB_BANK1_START_ADDR+1024, VIP_DHUB_BANK1_START_ADDR+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vip_MIC1_W, VIP_DHUB_BANK1_START_ADDR+1024*2, VIP_DHUB_BANK1_START_ADDR+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}, 
	{ avioDhubChMap_vip_MIC2_W, VIP_DHUB_BANK1_START_ADDR+1024*3, VIP_DHUB_BANK1_START_ADDR+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
        { avioDhubChMap_vip_MIC3_W, VIP_DHUB_BANK1_START_ADDR+1024*4, VIP_DHUB_BANK1_START_ADDR+1024*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#endif
};
#endif

DHUB_channel_config  VPP_config[VPP_NUM_OF_CHANNELS] = {
#if (BERLIN_CHIP_VERSION >= BERLIN_B_0)
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	// BANK0
	{ avioDhubChMap_vpp_MV_R, DHUB_BANK0_START_ADDR, DHUB_BANK0_START_ADDR+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
   	{ avioDhubChMap_vpp_MV_FRC_R, DHUB_BANK0_START_ADDR + 2048*2, DHUB_BANK0_START_ADDR + 2048*2+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_MV_FRC_W, DHUB_BANK0_START_ADDR + 2048*3, DHUB_BANK0_START_ADDR + 2048*3+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK1
	{ avioDhubChMap_vpp_PIP_R, DHUB_BANK1_START_ADDR, DHUB_BANK1_START_ADDR+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
 	{ avioDhubChMap_vpp_PIP_FRC_R,DHUB_BANK1_START_ADDR +2048*2, DHUB_BANK1_START_ADDR + 2048*2+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_PIP_FRC_W,DHUB_BANK1_START_ADDR + 2048*3, DHUB_BANK1_START_ADDR + 2048*3+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK2
	{ avioDhubChMap_vpp_DINT0_R,DHUB_BANK2_START_ADDR, DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// BANK3
	{ avioDhubChMap_vpp_DINT1_R,DHUB_BANK3_START_ADDR, DHUB_BANK3_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// BANK4
	{ avioDhubChMap_vpp_DINT_W,DHUB_BANK4_START_ADDR, DHUB_BANK4_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank5
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	{ avioDhubChMap_vpp_BCM_R, DHUB_BANK5_START_ADDR, DHUB_BANK5_START_ADDR+512, 512, (1024-512), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_HDMI_R, DHUB_BANK5_START_ADDR+1024, DHUB_BANK5_START_ADDR+1024+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#else
	{ avioDhubChMap_vpp_BCM_R, DHUB_BANK5_START_ADDR, DHUB_BANK5_START_ADDR+128, 128, (512-128), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_HDMI_R, DHUB_BANK5_START_ADDR+512, DHUB_BANK5_START_ADDR+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#endif
	{ avioDhubChMap_vpp_AUX_FRC_R,DHUB_BANK5_START_ADDR+1024+512, DHUB_BANK5_START_ADDR+1024+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_AUX_FRC_W,DHUB_BANK5_START_ADDR+2048+512, DHUB_BANK5_START_ADDR+2048+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_BG_R,DHUB_BANK5_START_ADDR+2048+512*3, DHUB_BANK5_START_ADDR+2048+512*3+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_TT_R,DHUB_BANK5_START_ADDR+2048*3+512*3, DHUB_BANK5_START_ADDR+2048*3+512*3+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
	// BANK0
	{ avioDhubChMap_vpp_MV_R, VPP_DHUB_BANK0_START_ADDR, VPP_DHUB_BANK0_START_ADDR+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_MV_FRC_R, VPP_DHUB_BANK0_START_ADDR + 2048*2, VPP_DHUB_BANK0_START_ADDR + 2048*2+32, 32, (1024*3-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_MV_FRC_W, VPP_DHUB_BANK0_START_ADDR + 1024*7, VPP_DHUB_BANK0_START_ADDR + 1024*7+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// BANK1
	{ avioDhubChMap_vpp_DINT0_R,VPP_DHUB_BANK1_START_ADDR, VPP_DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// BANK2
	{ avioDhubChMap_vpp_DINT1_R,VPP_DHUB_BANK2_START_ADDR, VPP_DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// BANK3
	{ avioDhubChMap_vpp_DINT_W,VPP_DHUB_BANK3_START_ADDR, VPP_DHUB_BANK3_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank4
	{ avioDhubChMap_vpp_BCM_R, VPP_DHUB_BANK4_START_ADDR, VPP_DHUB_BANK4_START_ADDR+128, 128, (1024-128), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_HDMI_R, VPP_DHUB_BANK4_START_ADDR+1024, VPP_DHUB_BANK4_START_ADDR+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_SPDIF_W,VPP_DHUB_BANK4_START_ADDR+1024*2, VPP_DHUB_BANK4_START_ADDR+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#else
	{ avioDhubChMap_vpp_MV_R, 0, 0+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_vpp_MV_PBO_R, 2048*2, 2048*2+0, 0, 0, dHubChannel_CFG_MTU_128byte, 0, 0, 1},
   	{ avioDhubChMap_vpp_MV_FRC_R, 1024*4, 1024*4+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_MV_FRC_W, 2048*3, 2048*3+32, 32, (2048-32), dHubChannel_CFG_MTU_32byte, 0, 0, 1},
	{ avioDhubChMap_vpp_PIP_R, 2048*4, 2048*4+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_PIP_PBO_R,2048*5, 2048*5+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
 	{ avioDhubChMap_vpp_PIP_FRC_R,2048*6, 2048*6+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_PIP_FRC_W,2048*7, 2048*7+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_DINT0_R,2048*8, 2048*8+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_DINT1_R,2048*9, 2048*9+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_DINT_W,2048*10, 2048*10+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_AUX_FRC_R,2048*12, 2048*12+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_AUX_FRC_W,2048*13, 2048*14+32, 32, (2048-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_vpp_BG_R,2048*14, 2048*14+32, 32, (2048*2-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#endif
};
DHUB_channel_config  AG_config[AG_NUM_OF_CHANNELS] = {
#if (BERLIN_CHIP_VERSION >= BERLIN_B_0)
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	// Bank0
	{ avioDhubChMap_ag_APPCMD_R, DHUB_BANK0_START_ADDR, DHUB_BANK0_START_ADDR+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA0_R, DHUB_BANK0_START_ADDR+512, DHUB_BANK0_START_ADDR+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA1_R, DHUB_BANK0_START_ADDR+512+1024,DHUB_BANK0_START_ADDR+512+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}, 
	{ avioDhubChMap_ag_MA2_R, DHUB_BANK0_START_ADDR+512+1024*2,DHUB_BANK0_START_ADDR+512+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA3_R, DHUB_BANK0_START_ADDR+512+1024*3,DHUB_BANK0_START_ADDR+512+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_SA_R, DHUB_BANK0_START_ADDR+512+1024*4,DHUB_BANK0_START_ADDR+512+1024*4+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_SPDIF_R, DHUB_BANK0_START_ADDR+512+1024*5, DHUB_BANK0_START_ADDR+512+1024*5+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MIC_W, DHUB_BANK0_START_ADDR+1024*6, DHUB_BANK0_START_ADDR+1024*6+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_APPDAT_R, DHUB_BANK0_START_ADDR+512+1024*6, DHUB_BANK0_START_ADDR+512+1024*6+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_APPDAT_W, DHUB_BANK0_START_ADDR+1024*7, DHUB_BANK0_START_ADDR+1024*7+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// Bank1
	{ avioDhubChMap_ag_MOSD_R, DHUB_BANK1_START_ADDR, DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank2
	{ avioDhubChMap_ag_CSR_R, DHUB_BANK2_START_ADDR, DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank3
	{ avioDhubChMap_ag_GFX_R, DHUB_BANK3_START_ADDR, DHUB_BANK3_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank4
	{ avioDhubChMap_ag_PG_R, DHUB_BANK4_START_ADDR, DHUB_BANK4_START_ADDR+32, 32, (2048*3-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{ avioDhubChMap_ag_PG_ENG_R, DHUB_BANK4_START_ADDR + (2048 * 3), DHUB_BANK4_START_ADDR + (2048 *3) + 32, 32, (1024 - 32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_PG_ENG_W, DHUB_BANK4_START_ADDR + (2048 * 3) + 1024, DHUB_BANK4_START_ADDR + (2048 * 3) + 1024 + 32, 32, (1024 - 32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
	// Bank0
	{ avioDhubChMap_ag_APPCMD_R, AG_DHUB_BANK0_START_ADDR, AG_DHUB_BANK0_START_ADDR+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA0_R, AG_DHUB_BANK0_START_ADDR+512, AG_DHUB_BANK0_START_ADDR+512+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA1_R, AG_DHUB_BANK0_START_ADDR+512+1024,AG_DHUB_BANK0_START_ADDR+512+1024+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1}, 
	{ avioDhubChMap_ag_MA2_R, AG_DHUB_BANK0_START_ADDR+512+1024*2,AG_DHUB_BANK0_START_ADDR+512+1024*2+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_MA3_R, AG_DHUB_BANK0_START_ADDR+512+1024*3,AG_DHUB_BANK0_START_ADDR+512+1024*3+32, 32, (1024-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_SPDIF_R, AG_DHUB_BANK0_START_ADDR+512+1024*5, AG_DHUB_BANK0_START_ADDR+512+1024*5+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_APPDAT_R, AG_DHUB_BANK0_START_ADDR+512+1024*6, AG_DHUB_BANK0_START_ADDR+512+1024*6+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{ avioDhubChMap_ag_APPDAT_W, AG_DHUB_BANK0_START_ADDR+1024*7, AG_DHUB_BANK0_START_ADDR+1024*7+32, 32, (512-32), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	// Bank1
	{ avioDhubChMap_ag_CSR_R, AG_DHUB_BANK1_START_ADDR, AG_DHUB_BANK1_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	// Bank2
	{ avioDhubChMap_ag_GFX_R, AG_DHUB_BANK2_START_ADDR, AG_DHUB_BANK2_START_ADDR+32, 32, (8192-32), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#else
	{	avioDhubChMap_ag_BCM_R, 0, 0+64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_MA0_R, 512, 512 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_MA1_R, 512*2,512*2 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1}, 
	{	avioDhubChMap_ag_MA2_R, 512*3,512*3 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_MA3_R, 512*4,512*4 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_SA_R, 512*5,512*5 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_SPDIF_R, 512*6, 512*6 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_MIC_W, 512*7, 512*7 + 64, 64, (512-64), dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	/*
	{	avioDhubChMap_ag_MOSD_R, 512*8, (512*8+64), 64, (256-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{	avioDhubChMap_ag_CSR_R, (4096+256 ), (4096+256+64), 64, (4096-256-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	*/
	{	avioDhubChMap_ag_MOSD_R, 512*8, (512*8+64), 64, (4096-256-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{	avioDhubChMap_ag_CSR_R, (512*8+4096-256), (512*8+4096-256+64), 64, (256-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{	avioDhubChMap_ag_GFX_R, (4096 + 4096), (4096+4096 + 64), 64, (2048*4 -64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{	avioDhubChMap_ag_GFX_PBO_R, (4096 + 4096 + 2048*4), (4096+4096 + 2048*4 ), 0, 0, dHubChannel_CFG_MTU_128byte, 0, 0, 0},
	{	avioDhubChMap_ag_PG_R, (4096 +4096 + 2048*4), (4096+4096 +2048*4 + 64), 64, (2048*4-64), dHubChannel_CFG_MTU_128byte, 1, 0, 1},
	{	avioDhubChMap_ag_PG_ENG_R, (4096 + 4096 + 8192 ), (4096+4096 +8192 ), 0, 0, dHubChannel_CFG_MTU_128byte, 0, 0, 1},
	{	avioDhubChMap_ag_PG_ENG_W, (4096 + 4096 + 8192 ), (4096+4096 +8192 ), 0, 0, dHubChannel_CFG_MTU_128byte, 0, 0, 1},
#endif
};
/******************************************************************************************************************
 *	Function: DhubInitialization 
 *	Description: Initialize DHUB .
 *	Parameter : cpuId ------------- cpu ID
 *             dHubBaseAddr -------------  dHub Base address.
 *             hboSramAddr ----- Sram Address for HBO.
 *             pdhubHandle ----- pointer to 2D dhubHandle 
 *             dhub_config ----- configuration of AG 
 *             numOfChans	 ----- number of channels
 *	Return:		void
******************************************************************************************************************/
void DhubInitialization(SIGN32 cpuId, UNSG32 dHubBaseAddr, UNSG32 hboSramAddr, HDL_dhub2d *pdhubHandle, DHUB_channel_config *dhub_config,SIGN32 numOfChans)
{
	HDL_semaphore *pSemHandle;
	SIGN32 i;
	SIGN32 chanId;
	
	//Initialize HDL_dhub with a $dHub BIU instance.
	dhub2d_hdl(	hboSramAddr,			/*!	Base address of dHub.HBO SRAM !*/
			 	dHubBaseAddr,			/*!	Base address of a BIU instance of $dHub !*/
			 	pdhubHandle				/*!	Handle to HDL_dhub2d !*/
			);
	//set up semaphore to trigger cmd done interrupt
	//note that this set of semaphores are different from the HBO semaphores
	//the ID must match the dhub ID because they are hardwired.
	pSemHandle = dhub_semaphore(&pdhubHandle->dhub);

	for (i = 0; i< numOfChans; i++) {
		//Configurate a dHub channel
		//note that in this function, it also configured right HBO channels(cmdQ and dataQ) and semaphores
		chanId = dhub_config[i].chanId;	
		{
			dhub_channel_cfg(
						&pdhubHandle->dhub,					/*!	Handle to HDL_dhub !*/
						chanId,						/*!	Channel ID in $dHubReg !*/
						dhub_config[chanId].chanCmdBase,		//UNSG32 baseCmd,	/*!	Channel FIFO base address (byte address) for cmdQ !*/
						dhub_config[chanId].chanDataBase,		//UNSG32 baseData,	/*!	Channel FIFO base address (byte address) for dataQ !*/
						dhub_config[chanId].chanCmdSize/8,	//SIGN32		depthCmd,			/*!	Channel FIFO depth for cmdQ, in 64b word !*/
						dhub_config[chanId].chanDataSize/8,	//SIGN32		depthData,			/*!	Channel FIFO depth for dataQ, in 64b word !*/
						dhub_config[chanId].chanMtuSize,						/*!	See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes !*/
						dhub_config[chanId].chanQos,								/*!	See 'dHubChannel.CFG.QoS' !*/
						dhub_config[chanId].chanSelfLoop,								/*!	See 'dHubChannel.CFG.selfLoop' !*/
						dhub_config[chanId].chanEnable,								/*!	0 to disable, 1 to enable !*/
						0								/*!	Pass NULL to directly init dHub, or
															Pass non-zero to receive programming sequence
															in (adr,data) pairs
															!*/
						);
			// setup interrupt for channel chanId
			//configure the semaphore depth to be 1
			semaphore_cfg(pSemHandle, chanId, 1, 0);
#if 0
  			// enable interrupt from this semaphore
		  	semaphore_intr_enable (
    			pSemHandle, // semaphore handler
		    	chanId, 
    			0,       // empty
		    	1,       // full
    			0,       // almost_empty
		    	0,       // almost_full
    			cpuId        // 0~2, depending on which CPU the interrupt is enabled for.
		  	);
#endif
		}
	}
}

/******************************************************************************************************************
 *	Function: DhubChannelClear
 *	Description: Clear corresponding DHUB channel.
 *	Parameter:	hdl  ---------- handle to HDL_dhub
 *			id   ---------- channel ID in dHubReg
 *			cfgQ ---------- pass null to directly init dhub, or pass non-zero to receive programming
 *					sequence in (adr, data) pairs
 *	Return:		void
******************************************************************************************************************/
void DhubChannelClear(void *hdl, SIGN32 id, T64b cfgQ[])
{
	UNSG32	cmdID = dhub_id2hbo_cmdQ(id);
	UNSG32	dataID = dhub_id2hbo_data(id);
	HDL_dhub *dhub = (HDL_dhub *)hdl;
	HDL_hbo *hbo = &(dhub->hbo);

	/* 1.Software stops the command queue in HBO (please refer to HBO.sxw for details) */
	hbo_queue_enable(hbo, cmdID, 0, cfgQ);
	/* 2.Software stops the channel in dHub by writing zero to  dHubChannel.START.EN */
	dhub_channel_enable(dhub, id, 0, cfgQ);
	/* 3.Software clears the channel in dHub by writing one to  dHubChannel.CLEAR.EN */
	dhub_channel_clear(dhub, id);
	/* 4.Software waits for the register bits dHubChannel.PENDING.ST and dHubChannel.BUSY.ST to be 0 */
	dhub_channel_clear_done(dhub, id);
	/* 5.Software stops and clears the command queue */
	hbo_queue_enable(hbo, cmdID, 0, cfgQ);
	hbo_queue_clear(hbo, cmdID);
	/* 6.Software wait for the corresponding busy bit to be 0 */
	hbo_queue_clear_done(hbo, cmdID);
	/* 7.Software stops and clears the data queue */
	hbo_queue_enable(hbo, dataID, 0, cfgQ);
	hbo_queue_clear(hbo, dataID);
	/* 8.Software wait for the corresponding data Q busy bit to be 0 */
	hbo_queue_clear_done(hbo, dataID);
	/* 9.Software enable dHub and HBO */
	dhub_channel_enable(dhub, id, 1, cfgQ);
	hbo_queue_enable(hbo, cmdID, 1, cfgQ);
	hbo_queue_enable(hbo, dataID, 1, cfgQ);
}

