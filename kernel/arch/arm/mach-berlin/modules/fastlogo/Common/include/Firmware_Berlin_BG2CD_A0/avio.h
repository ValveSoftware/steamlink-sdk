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


#ifndef avio_h
#define avio_h (){}

    #define     RA_AVIO_REG_dummy                              0x0000
    #define     RA_AVIO_cfgReg                                 0x0000
    #define     RA_AVIO_BCM_Q0                                 0x0100
    #define     RA_AVIO_BCM_Q1                                 0x0104
    #define     RA_AVIO_BCM_Q2                                 0x0108
    #define     RA_AVIO_BCM_Q3                                 0x010C
    #define     RA_AVIO_BCM_Q4                                 0x0110
    #define     RA_AVIO_BCM_Q5                                 0x0114
    #define     RA_AVIO_BCM_Q6                                 0x0118
    #define     RA_AVIO_BCM_Q7                                 0x011C
    #define     RA_AVIO_BCM_Q8                                 0x0120
    #define     RA_AVIO_BCM_Q9                                 0x0124
    #define     RA_AVIO_BCM_Q10                                0x0128
    #define     RA_AVIO_BCM_Q11                                0x012C
    #define     RA_AVIO_BCM_Q14                                0x0130
    #define     RA_AVIO_BCM_Q15                                0x0134
    #define     RA_AVIO_BCM_Q16                                0x0138
    #define     RA_AVIO_BCM_Q17                                0x013C
    #define     RA_AVIO_BCM_Q18                                0x0140
    #define     RA_AVIO_BCM_FULL_STS                           0x0144
    #define     RA_AVIO_BCM_EMP_STS                            0x0148
    #define     RA_AVIO_BCM_FLUSH                              0x014C
    #define     RA_AVIO_BCM_AUTOPUSH_CNT                       0x0150
    #define     RA_AVIO_Q0                                     0x0154
    #define     RA_AVIO_Q1                                     0x0158
    #define     RA_AVIO_Q2                                     0x015C
    #define     RA_AVIO_Q3                                     0x0160
    #define     RA_AVIO_Q4                                     0x0164
    #define     RA_AVIO_Q5                                     0x0168
    #define     RA_AVIO_Q6                                     0x016C
    #define     RA_AVIO_Q7                                     0x0170
    #define     RA_AVIO_Q8                                     0x0174
    #define     RA_AVIO_Q9                                     0x0178
    #define     RA_AVIO_Q10                                    0x017C
    #define     RA_AVIO_Q11                                    0x0180
    #define     RA_AVIO_Q14                                    0x0184
    #define     RA_AVIO_Q15                                    0x0188
    #define     RA_AVIO_Q16                                    0x018C
    #define     RA_AVIO_Q17                                    0x0190
    #define     RA_AVIO_Q18                                    0x0194
    #define     RA_AVIO_BCM_AUTOPUSH                           0x0198
    #define     RA_AVIO_BCM_FULL_STS_STICKY                    0x019C
    #define     RA_AVIO_BCM_ERROR                              0x01A0
    #define     RA_AVIO_BCM_LOG_ADDR                           0x01A4
    #define     RA_AVIO_BCM_ERROR_DATA                         0x01A8

#endif
