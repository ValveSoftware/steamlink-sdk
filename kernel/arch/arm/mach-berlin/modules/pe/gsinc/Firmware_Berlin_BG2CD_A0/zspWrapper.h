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


#ifndef zspWrapper_h
#define zspWrapper_h (){}

    #define     RA_ZspGlobalRegs_subsys_reset                  0x0000
    #define     RA_ZspGlobalRegs_core_reset                    0x0004
    #define     RA_ZspGlobalRegs_clock_ctrl                    0x0008
    #define     RA_ZspGlobalRegs_ext_bp                        0x000C
    #define     RA_ZspGlobalRegs_halt                          0x0010
    #define     RA_ZspGlobalRegs_status                        0x0014
    #define     RA_ZspGlobalRegs_svtaddr                       0x001C
    #define     RA_ZspInt2Zsp_status                           0x0000
    #define     RA_ZspInt2Zsp_set                              0x0004
    #define     RA_ZspInt2Zsp_clear                            0x0008
    #define     RA_ZspInt2Soc_status                           0x0000
    #define     RA_ZspInt2Soc_set                              0x0004
    #define     RA_ZspInt2Soc_clear                            0x0008
    #define     RA_ZspInt2Soc_enable                           0x000C
    #define     RA_ZspDmaRegs_srcAddr                          0x0000
    #define     RA_ZspDmaRegs_desAddr                          0x0004
    #define     RA_ZspDmaRegs_config                           0x0008
    #define     RA_ZspDmaRegs_status                           0x000C
    #define     RA_ZspRegs_ITCM                                0x0000
    #define     RA_ZspRegs_DTCM                                0x20000
    #define     RA_ZspRegs_PMEM                                0x40000
    #define     RA_ZspRegs_ZXBAR                               0x60000
    #define     RA_ZspRegs_Global                              0x64000
    #define     RA_ZspRegs_Int2Zsp                             0x64400
    #define     RA_ZspRegs_Int2Soc                             0x64600
    #define     RA_ZspRegs_DMA                                 0x64800


    typedef union  T32ZspInt2Soc_status
          { UNSG32 u32;
            //struct w32ZspInt2Soc_status;
                 } T32ZspInt2Soc_status;


#endif
