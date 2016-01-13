/**********************************************************************************************************
*       Copyright (C) 2007-2011
*       Copyright ? 2007 Marvell International Ltd.
*
*       This program is free software; you can redistribute it and/or
*       modify it under the terms of the GNU General Public License
*       as published by the Free Software Foundation; either version 2
*       of the License, or (at your option) any later version.
*
*       This program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY; without even the implied warranty of
*       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*       GNU General Public License for more details.
*
*       You should have received a copy of the GNU General Public License
*       along with this program; if not, write to the Free Software
*       Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
**********************************************************************************************************/
#ifndef avio_h
#define avio_h (){}


#include "ctypes.h"

#pragma pack(1)
#ifdef __cplusplus
  extern "C" {
#endif

#ifndef _DOCC_H_BITOPS_
#define _DOCC_H_BITOPS_ (){}
#define _bSETMASK_(b)		((b)<32 ? (1<<((b)&31)) : 0)
#define _NSETMASK_(msb,lsb)	(_bSETMASK_((msb)+1)-_bSETMASK_(lsb))
#define _bCLRMASK_(b)		(~_bSETMASK_(b))
#define _NCLRMASK_(msb,lsb)	(~_NSETMASK_(msb,lsb))
#define _BFGET_(r,msb,lsb)	(_NSETMASK_((msb)-(lsb),0)&((r)>>(lsb)))
#define _BFSET_(r,msb,lsb,v)	do{ (r)&=_NCLRMASK_(msb,lsb); (r)|=_NSETMASK_(msb,lsb)&((v)<<(lsb)); }while(0)
#endif

#ifndef h_oneReg
#define h_oneReg (){}
#define BA_oneReg_0x00000000                           0x0000
#define B16oneReg_0x00000000                           0x0000
#define LSb32oneReg_0x00000000                              0
#define LSb16oneReg_0x00000000                              0
#define boneReg_0x00000000                           32
#define MSK32oneReg_0x00000000                              0xFFFFFFFF


typedef struct SIE_oneReg {

#define   GET32oneReg_0x00000000(r32)                      _BFGET_(r32,31, 0)
#define   SET32oneReg_0x00000000(r32,v)                    _BFSET_(r32,31, 0,v)

    UNSG32 u_0x00000000                                : 32;

} SIE_oneReg;

SIGN32 oneReg_drvrd(SIE_oneReg *p, UNSG32 base, SIGN32 mem, SIGN32 tst);
SIGN32 oneReg_drvwr(SIE_oneReg *p, UNSG32 base, SIGN32 mem, SIGN32 tst, UNSG32 *pcmd);
void oneReg_reset(SIE_oneReg *p);
SIGN32 oneReg_cmp  (SIE_oneReg *p, SIE_oneReg *pie, char *pfx, void *hLOG, SIGN32 mem, SIGN32 tst);
#define oneReg_check(p,pie,pfx,hLOG) oneReg_cmp(p,pie,pfx,(void*)(hLOG),0,0)
#define oneReg_print(p,    pfx,hLOG) oneReg_cmp(p,0,  pfx,(void*)(hLOG),0,0)

#endif

//////
///
/// $INTERFACE AVIO_REG                                 (4,4)
///     # # ----------------------------------------------------------
///     @ 0x00000                      (P)
///     # 0x00000 dummy
///               $oneReg              dummy             REG      [36]
///     # # ----------------------------------------------------------
/// $ENDOFINTERFACE  # size:     144B, bits:    1152b, padding:     0B
////////////////////////////////////////////////////////////
#ifndef h_AVIO_REG
#define h_AVIO_REG (){}
#define     RA_AVIO_REG_dummy                              0x0000
typedef struct SIE_AVIO_REG {
      SIE_oneReg                                       ie_dummy[36];
} SIE_AVIO_REG;

SIGN32 AVIO_REG_drvrd(SIE_AVIO_REG *p, UNSG32 base, SIGN32 mem, SIGN32 tst);
SIGN32 AVIO_REG_drvwr(SIE_AVIO_REG *p, UNSG32 base, SIGN32 mem, SIGN32 tst, UNSG32 *pcmd);
void AVIO_REG_reset(SIE_AVIO_REG *p);
SIGN32 AVIO_REG_cmp  (SIE_AVIO_REG *p, SIE_AVIO_REG *pie, char *pfx, void *hLOG, SIGN32 mem, SIGN32 tst);
#define AVIO_REG_check(p,pie,pfx,hLOG) AVIO_REG_cmp(p,pie,pfx,(void*)(hLOG),0,0)
#define AVIO_REG_print(p,    pfx,hLOG) AVIO_REG_cmp(p,0,  pfx,(void*)(hLOG),0,0)

#endif

#ifndef h_AVIO
#define h_AVIO (){}

#define     RA_AVIO_cfgReg                                 0x0000
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q0                                 0x0100

#define     BA_AVIO_BCM_Q0_mux                             0x0100
#define     B16AVIO_BCM_Q0_mux                             0x0100
#define   LSb32AVIO_BCM_Q0_mux                                0
#define   LSb16AVIO_BCM_Q0_mux                                0
#define       bAVIO_BCM_Q0_mux                             5
#define   MSK32AVIO_BCM_Q0_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q1                                 0x0104

#define     BA_AVIO_BCM_Q1_mux                             0x0104
#define     B16AVIO_BCM_Q1_mux                             0x0104
#define   LSb32AVIO_BCM_Q1_mux                                0
#define   LSb16AVIO_BCM_Q1_mux                                0
#define       bAVIO_BCM_Q1_mux                             5
#define   MSK32AVIO_BCM_Q1_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q2                                 0x0108

#define     BA_AVIO_BCM_Q2_mux                             0x0108
#define     B16AVIO_BCM_Q2_mux                             0x0108
#define   LSb32AVIO_BCM_Q2_mux                                0
#define   LSb16AVIO_BCM_Q2_mux                                0
#define       bAVIO_BCM_Q2_mux                             5
#define   MSK32AVIO_BCM_Q2_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q3                                 0x010C

#define     BA_AVIO_BCM_Q3_mux                             0x010C
#define     B16AVIO_BCM_Q3_mux                             0x010C
#define   LSb32AVIO_BCM_Q3_mux                                0
#define   LSb16AVIO_BCM_Q3_mux                                0
#define       bAVIO_BCM_Q3_mux                             5
#define   MSK32AVIO_BCM_Q3_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q4                                 0x0110

#define     BA_AVIO_BCM_Q4_mux                             0x0110
#define     B16AVIO_BCM_Q4_mux                             0x0110
#define   LSb32AVIO_BCM_Q4_mux                                0
#define   LSb16AVIO_BCM_Q4_mux                                0
#define       bAVIO_BCM_Q4_mux                             5
#define   MSK32AVIO_BCM_Q4_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q5                                 0x0114

#define     BA_AVIO_BCM_Q5_mux                             0x0114
#define     B16AVIO_BCM_Q5_mux                             0x0114
#define   LSb32AVIO_BCM_Q5_mux                                0
#define   LSb16AVIO_BCM_Q5_mux                                0
#define       bAVIO_BCM_Q5_mux                             5
#define   MSK32AVIO_BCM_Q5_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q6                                 0x0118

#define     BA_AVIO_BCM_Q6_mux                             0x0118
#define     B16AVIO_BCM_Q6_mux                             0x0118
#define   LSb32AVIO_BCM_Q6_mux                                0
#define   LSb16AVIO_BCM_Q6_mux                                0
#define       bAVIO_BCM_Q6_mux                             5
#define   MSK32AVIO_BCM_Q6_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q7                                 0x011C

#define     BA_AVIO_BCM_Q7_mux                             0x011C
#define     B16AVIO_BCM_Q7_mux                             0x011C
#define   LSb32AVIO_BCM_Q7_mux                                0
#define   LSb16AVIO_BCM_Q7_mux                                0
#define       bAVIO_BCM_Q7_mux                             5
#define   MSK32AVIO_BCM_Q7_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q8                                 0x0120

#define     BA_AVIO_BCM_Q8_mux                             0x0120
#define     B16AVIO_BCM_Q8_mux                             0x0120
#define   LSb32AVIO_BCM_Q8_mux                                0
#define   LSb16AVIO_BCM_Q8_mux                                0
#define       bAVIO_BCM_Q8_mux                             5
#define   MSK32AVIO_BCM_Q8_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q9                                 0x0124

#define     BA_AVIO_BCM_Q9_mux                             0x0124
#define     B16AVIO_BCM_Q9_mux                             0x0124
#define   LSb32AVIO_BCM_Q9_mux                                0
#define   LSb16AVIO_BCM_Q9_mux                                0
#define       bAVIO_BCM_Q9_mux                             5
#define   MSK32AVIO_BCM_Q9_mux                                0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q10                                0x0128

#define     BA_AVIO_BCM_Q10_mux                            0x0128
#define     B16AVIO_BCM_Q10_mux                            0x0128
#define   LSb32AVIO_BCM_Q10_mux                               0
#define   LSb16AVIO_BCM_Q10_mux                               0
#define       bAVIO_BCM_Q10_mux                            5
#define   MSK32AVIO_BCM_Q10_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q11                                0x012C

#define     BA_AVIO_BCM_Q11_mux                            0x012C
#define     B16AVIO_BCM_Q11_mux                            0x012C
#define   LSb32AVIO_BCM_Q11_mux                               0
#define   LSb16AVIO_BCM_Q11_mux                               0
#define       bAVIO_BCM_Q11_mux                            5
#define   MSK32AVIO_BCM_Q11_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q14                                0x0130

#define     BA_AVIO_BCM_Q14_mux                            0x0130
#define     B16AVIO_BCM_Q14_mux                            0x0130
#define   LSb32AVIO_BCM_Q14_mux                               0
#define   LSb16AVIO_BCM_Q14_mux                               0
#define       bAVIO_BCM_Q14_mux                            5
#define   MSK32AVIO_BCM_Q14_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q15                                0x0134

#define     BA_AVIO_BCM_Q15_mux                            0x0134
#define     B16AVIO_BCM_Q15_mux                            0x0134
#define   LSb32AVIO_BCM_Q15_mux                               0
#define   LSb16AVIO_BCM_Q15_mux                               0
#define       bAVIO_BCM_Q15_mux                            5
#define   MSK32AVIO_BCM_Q15_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q16                                0x0138

#define     BA_AVIO_BCM_Q16_mux                            0x0138
#define     B16AVIO_BCM_Q16_mux                            0x0138
#define   LSb32AVIO_BCM_Q16_mux                               0
#define   LSb16AVIO_BCM_Q16_mux                               0
#define       bAVIO_BCM_Q16_mux                            5
#define   MSK32AVIO_BCM_Q16_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q17                                0x013C

#define     BA_AVIO_BCM_Q17_mux                            0x013C
#define     B16AVIO_BCM_Q17_mux                            0x013C
#define   LSb32AVIO_BCM_Q17_mux                               0
#define   LSb16AVIO_BCM_Q17_mux                               0
#define       bAVIO_BCM_Q17_mux                            5
#define   MSK32AVIO_BCM_Q17_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_Q18                                0x0140

#define     BA_AVIO_BCM_Q18_mux                            0x0140
#define     B16AVIO_BCM_Q18_mux                            0x0140
#define   LSb32AVIO_BCM_Q18_mux                               0
#define   LSb16AVIO_BCM_Q18_mux                               0
#define       bAVIO_BCM_Q18_mux                            5
#define   MSK32AVIO_BCM_Q18_mux                               0x0000001F
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_FULL_STS                           0x0144

#define     BA_AVIO_BCM_FULL_STS_Q0                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q0                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q0                           0
#define   LSb16AVIO_BCM_FULL_STS_Q0                           0
#define       bAVIO_BCM_FULL_STS_Q0                        1
#define   MSK32AVIO_BCM_FULL_STS_Q0                           0x00000001

#define     BA_AVIO_BCM_FULL_STS_Q1                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q1                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q1                           1
#define   LSb16AVIO_BCM_FULL_STS_Q1                           1
#define       bAVIO_BCM_FULL_STS_Q1                        1
#define   MSK32AVIO_BCM_FULL_STS_Q1                           0x00000002

#define     BA_AVIO_BCM_FULL_STS_Q2                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q2                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q2                           2
#define   LSb16AVIO_BCM_FULL_STS_Q2                           2
#define       bAVIO_BCM_FULL_STS_Q2                        1
#define   MSK32AVIO_BCM_FULL_STS_Q2                           0x00000004

#define     BA_AVIO_BCM_FULL_STS_Q3                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q3                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q3                           3
#define   LSb16AVIO_BCM_FULL_STS_Q3                           3
#define       bAVIO_BCM_FULL_STS_Q3                        1
#define   MSK32AVIO_BCM_FULL_STS_Q3                           0x00000008

#define     BA_AVIO_BCM_FULL_STS_Q4                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q4                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q4                           4
#define   LSb16AVIO_BCM_FULL_STS_Q4                           4
#define       bAVIO_BCM_FULL_STS_Q4                        1
#define   MSK32AVIO_BCM_FULL_STS_Q4                           0x00000010

#define     BA_AVIO_BCM_FULL_STS_Q5                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q5                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q5                           5
#define   LSb16AVIO_BCM_FULL_STS_Q5                           5
#define       bAVIO_BCM_FULL_STS_Q5                        1
#define   MSK32AVIO_BCM_FULL_STS_Q5                           0x00000020

#define     BA_AVIO_BCM_FULL_STS_Q6                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q6                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q6                           6
#define   LSb16AVIO_BCM_FULL_STS_Q6                           6
#define       bAVIO_BCM_FULL_STS_Q6                        1
#define   MSK32AVIO_BCM_FULL_STS_Q6                           0x00000040

#define     BA_AVIO_BCM_FULL_STS_Q7                        0x0144
#define     B16AVIO_BCM_FULL_STS_Q7                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q7                           7
#define   LSb16AVIO_BCM_FULL_STS_Q7                           7
#define       bAVIO_BCM_FULL_STS_Q7                        1
#define   MSK32AVIO_BCM_FULL_STS_Q7                           0x00000080

#define     BA_AVIO_BCM_FULL_STS_Q8                        0x0145
#define     B16AVIO_BCM_FULL_STS_Q8                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q8                           8
#define   LSb16AVIO_BCM_FULL_STS_Q8                           8
#define       bAVIO_BCM_FULL_STS_Q8                        1
#define   MSK32AVIO_BCM_FULL_STS_Q8                           0x00000100

#define     BA_AVIO_BCM_FULL_STS_Q9                        0x0145
#define     B16AVIO_BCM_FULL_STS_Q9                        0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q9                           9
#define   LSb16AVIO_BCM_FULL_STS_Q9                           9
#define       bAVIO_BCM_FULL_STS_Q9                        1
#define   MSK32AVIO_BCM_FULL_STS_Q9                           0x00000200

#define     BA_AVIO_BCM_FULL_STS_Q10                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q10                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q10                          10
#define   LSb16AVIO_BCM_FULL_STS_Q10                          10
#define       bAVIO_BCM_FULL_STS_Q10                       1
#define   MSK32AVIO_BCM_FULL_STS_Q10                          0x00000400

#define     BA_AVIO_BCM_FULL_STS_Q11                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q11                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q11                          11
#define   LSb16AVIO_BCM_FULL_STS_Q11                          11
#define       bAVIO_BCM_FULL_STS_Q11                       1
#define   MSK32AVIO_BCM_FULL_STS_Q11                          0x00000800

#define     BA_AVIO_BCM_FULL_STS_Q12                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q12                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q12                          12
#define   LSb16AVIO_BCM_FULL_STS_Q12                          12
#define       bAVIO_BCM_FULL_STS_Q12                       1
#define   MSK32AVIO_BCM_FULL_STS_Q12                          0x00001000

#define     BA_AVIO_BCM_FULL_STS_Q13                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q13                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q13                          13
#define   LSb16AVIO_BCM_FULL_STS_Q13                          13
#define       bAVIO_BCM_FULL_STS_Q13                       1
#define   MSK32AVIO_BCM_FULL_STS_Q13                          0x00002000

#define     BA_AVIO_BCM_FULL_STS_Q14                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q14                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q14                          14
#define   LSb16AVIO_BCM_FULL_STS_Q14                          14
#define       bAVIO_BCM_FULL_STS_Q14                       1
#define   MSK32AVIO_BCM_FULL_STS_Q14                          0x00004000

#define     BA_AVIO_BCM_FULL_STS_Q15                       0x0145
#define     B16AVIO_BCM_FULL_STS_Q15                       0x0144
#define   LSb32AVIO_BCM_FULL_STS_Q15                          15
#define   LSb16AVIO_BCM_FULL_STS_Q15                          15
#define       bAVIO_BCM_FULL_STS_Q15                       1
#define   MSK32AVIO_BCM_FULL_STS_Q15                          0x00008000

#define     BA_AVIO_BCM_FULL_STS_Q16                       0x0146
#define     B16AVIO_BCM_FULL_STS_Q16                       0x0146
#define   LSb32AVIO_BCM_FULL_STS_Q16                          16
#define   LSb16AVIO_BCM_FULL_STS_Q16                          0
#define       bAVIO_BCM_FULL_STS_Q16                       1
#define   MSK32AVIO_BCM_FULL_STS_Q16                          0x00010000

#define     BA_AVIO_BCM_FULL_STS_Q17                       0x0146
#define     B16AVIO_BCM_FULL_STS_Q17                       0x0146
#define   LSb32AVIO_BCM_FULL_STS_Q17                          17
#define   LSb16AVIO_BCM_FULL_STS_Q17                          1
#define       bAVIO_BCM_FULL_STS_Q17                       1
#define   MSK32AVIO_BCM_FULL_STS_Q17                          0x00020000

#define     BA_AVIO_BCM_FULL_STS_Q18                       0x0146
#define     B16AVIO_BCM_FULL_STS_Q18                       0x0146
#define   LSb32AVIO_BCM_FULL_STS_Q18                          18
#define   LSb16AVIO_BCM_FULL_STS_Q18                          2
#define       bAVIO_BCM_FULL_STS_Q18                       1
#define   MSK32AVIO_BCM_FULL_STS_Q18                          0x00040000
///////////////////////////////////////////////////////////
#define     RA_AVIO_BCM_EMP_STS                            0x0148

#define     BA_AVIO_BCM_EMP_STS_Q0                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q0                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q0                            0
#define   LSb16AVIO_BCM_EMP_STS_Q0                            0
#define       bAVIO_BCM_EMP_STS_Q0                         1
#define   MSK32AVIO_BCM_EMP_STS_Q0                            0x00000001

#define     BA_AVIO_BCM_EMP_STS_Q1                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q1                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q1                            1
#define   LSb16AVIO_BCM_EMP_STS_Q1                            1
#define       bAVIO_BCM_EMP_STS_Q1                         1
#define   MSK32AVIO_BCM_EMP_STS_Q1                            0x00000002

#define     BA_AVIO_BCM_EMP_STS_Q2                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q2                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q2                            2
#define   LSb16AVIO_BCM_EMP_STS_Q2                            2
#define       bAVIO_BCM_EMP_STS_Q2                         1
#define   MSK32AVIO_BCM_EMP_STS_Q2                            0x00000004

#define     BA_AVIO_BCM_EMP_STS_Q3                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q3                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q3                            3
#define   LSb16AVIO_BCM_EMP_STS_Q3                            3
#define       bAVIO_BCM_EMP_STS_Q3                         1
#define   MSK32AVIO_BCM_EMP_STS_Q3                            0x00000008

#define     BA_AVIO_BCM_EMP_STS_Q4                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q4                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q4                            4
#define   LSb16AVIO_BCM_EMP_STS_Q4                            4
#define       bAVIO_BCM_EMP_STS_Q4                         1
#define   MSK32AVIO_BCM_EMP_STS_Q4                            0x00000010

#define     BA_AVIO_BCM_EMP_STS_Q5                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q5                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q5                            5
#define   LSb16AVIO_BCM_EMP_STS_Q5                            5
#define       bAVIO_BCM_EMP_STS_Q5                         1
#define   MSK32AVIO_BCM_EMP_STS_Q5                            0x00000020

#define     BA_AVIO_BCM_EMP_STS_Q6                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q6                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q6                            6
#define   LSb16AVIO_BCM_EMP_STS_Q6                            6
#define       bAVIO_BCM_EMP_STS_Q6                         1
#define   MSK32AVIO_BCM_EMP_STS_Q6                            0x00000040

#define     BA_AVIO_BCM_EMP_STS_Q7                         0x0148
#define     B16AVIO_BCM_EMP_STS_Q7                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q7                            7
#define   LSb16AVIO_BCM_EMP_STS_Q7                            7
#define       bAVIO_BCM_EMP_STS_Q7                         1
#define   MSK32AVIO_BCM_EMP_STS_Q7                            0x00000080

#define     BA_AVIO_BCM_EMP_STS_Q8                         0x0149
#define     B16AVIO_BCM_EMP_STS_Q8                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q8                            8
#define   LSb16AVIO_BCM_EMP_STS_Q8                            8
#define       bAVIO_BCM_EMP_STS_Q8                         1
#define   MSK32AVIO_BCM_EMP_STS_Q8                            0x00000100

#define     BA_AVIO_BCM_EMP_STS_Q9                         0x0149
#define     B16AVIO_BCM_EMP_STS_Q9                         0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q9                            9
#define   LSb16AVIO_BCM_EMP_STS_Q9                            9
#define       bAVIO_BCM_EMP_STS_Q9                         1
#define   MSK32AVIO_BCM_EMP_STS_Q9                            0x00000200

#define     BA_AVIO_BCM_EMP_STS_Q10                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q10                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q10                           10
#define   LSb16AVIO_BCM_EMP_STS_Q10                           10
#define       bAVIO_BCM_EMP_STS_Q10                        1
#define   MSK32AVIO_BCM_EMP_STS_Q10                           0x00000400

#define     BA_AVIO_BCM_EMP_STS_Q11                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q11                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q11                           11
#define   LSb16AVIO_BCM_EMP_STS_Q11                           11
#define       bAVIO_BCM_EMP_STS_Q11                        1
#define   MSK32AVIO_BCM_EMP_STS_Q11                           0x00000800

#define     BA_AVIO_BCM_EMP_STS_Q12                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q12                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q12                           12
#define   LSb16AVIO_BCM_EMP_STS_Q12                           12
#define       bAVIO_BCM_EMP_STS_Q12                        1
#define   MSK32AVIO_BCM_EMP_STS_Q12                           0x00001000

#define     BA_AVIO_BCM_EMP_STS_Q13                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q13                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q13                           13
#define   LSb16AVIO_BCM_EMP_STS_Q13                           13
#define       bAVIO_BCM_EMP_STS_Q13                        1
#define   MSK32AVIO_BCM_EMP_STS_Q13                           0x00002000

#define     BA_AVIO_BCM_EMP_STS_Q14                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q14                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q14                           14
#define   LSb16AVIO_BCM_EMP_STS_Q14                           14
#define       bAVIO_BCM_EMP_STS_Q14                        1
#define   MSK32AVIO_BCM_EMP_STS_Q14                           0x00004000

#define     BA_AVIO_BCM_EMP_STS_Q15                        0x0149
#define     B16AVIO_BCM_EMP_STS_Q15                        0x0148
#define   LSb32AVIO_BCM_EMP_STS_Q15                           15
#define   LSb16AVIO_BCM_EMP_STS_Q15                           15
#define       bAVIO_BCM_EMP_STS_Q15                        1
#define   MSK32AVIO_BCM_EMP_STS_Q15                           0x00008000

#define     BA_AVIO_BCM_EMP_STS_Q16                        0x014A
#define     B16AVIO_BCM_EMP_STS_Q16                        0x014A
#define   LSb32AVIO_BCM_EMP_STS_Q16                           16
#define   LSb16AVIO_BCM_EMP_STS_Q16                           0
#define       bAVIO_BCM_EMP_STS_Q16                        1
#define   MSK32AVIO_BCM_EMP_STS_Q16                           0x00010000

#define     BA_AVIO_BCM_EMP_STS_Q17                        0x014A
#define     B16AVIO_BCM_EMP_STS_Q17                        0x014A
#define   LSb32AVIO_BCM_EMP_STS_Q17                           17
#define   LSb16AVIO_BCM_EMP_STS_Q17                           1
#define       bAVIO_BCM_EMP_STS_Q17                        1
#define   MSK32AVIO_BCM_EMP_STS_Q17                           0x00020000

#define     BA_AVIO_BCM_EMP_STS_Q18                        0x014A
#define     B16AVIO_BCM_EMP_STS_Q18                        0x014A
#define   LSb32AVIO_BCM_EMP_STS_Q18                           18
#define   LSb16AVIO_BCM_EMP_STS_Q18                           2
#define       bAVIO_BCM_EMP_STS_Q18                        1
#define   MSK32AVIO_BCM_EMP_STS_Q18                           0x00040000
///////////////////////////////////////////////////////////

typedef struct SIE_AVIO {
///////////////////////////////////////////////////////////
      SIE_AVIO_REG                                     ie_cfgReg;
     UNSG8 RSVD_cfgReg                                 [112];
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q0_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q0_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q0_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q0_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q0                                 {\
    UNSG32 uBCM_Q0_mux                                 :  5;\
    UNSG32 RSVDx100_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q0;
    struct w32AVIO_BCM_Q0;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q1_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q1_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q1_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q1_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q1                                 {\
    UNSG32 uBCM_Q1_mux                                 :  5;\
    UNSG32 RSVDx104_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q1;
    struct w32AVIO_BCM_Q1;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q2_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q2_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q2_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q2_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q2                                 {\
    UNSG32 uBCM_Q2_mux                                 :  5;\
    UNSG32 RSVDx108_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q2;
    struct w32AVIO_BCM_Q2;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q3_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q3_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q3_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q3_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q3                                 {\
    UNSG32 uBCM_Q3_mux                                 :  5;\
    UNSG32 RSVDx10C_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q3;
    struct w32AVIO_BCM_Q3;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q4_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q4_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q4_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q4_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q4                                 {\
    UNSG32 uBCM_Q4_mux                                 :  5;\
    UNSG32 RSVDx110_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q4;
    struct w32AVIO_BCM_Q4;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q5_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q5_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q5_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q5_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q5                                 {\
    UNSG32 uBCM_Q5_mux                                 :  5;\
    UNSG32 RSVDx114_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q5;
    struct w32AVIO_BCM_Q5;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q6_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q6_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q6_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q6_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q6                                 {\
    UNSG32 uBCM_Q6_mux                                 :  5;\
    UNSG32 RSVDx118_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q6;
    struct w32AVIO_BCM_Q6;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q7_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q7_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q7_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q7_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q7                                 {\
    UNSG32 uBCM_Q7_mux                                 :  5;\
    UNSG32 RSVDx11C_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q7;
    struct w32AVIO_BCM_Q7;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q8_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q8_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q8_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q8_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q8                                 {\
    UNSG32 uBCM_Q8_mux                                 :  5;\
    UNSG32 RSVDx120_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q8;
    struct w32AVIO_BCM_Q8;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q9_mux(r32)                        _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q9_mux(r32,v)                      _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q9_mux(r16)                        _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q9_mux(r16,v)                      _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q9                                 {\
    UNSG32 uBCM_Q9_mux                                 :  5;\
    UNSG32 RSVDx124_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q9;
    struct w32AVIO_BCM_Q9;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q10_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q10_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q10_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q10_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q10                                {\
    UNSG32 uBCM_Q10_mux                                :  5;\
    UNSG32 RSVDx128_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q10;
    struct w32AVIO_BCM_Q10;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q11_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q11_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q11_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q11_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q11                                {\
    UNSG32 uBCM_Q11_mux                                :  5;\
    UNSG32 RSVDx12C_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q11;
    struct w32AVIO_BCM_Q11;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q14_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q14_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q14_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q14_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q14                                {\
    UNSG32 uBCM_Q14_mux                                :  5;\
    UNSG32 RSVDx130_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q14;
    struct w32AVIO_BCM_Q14;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q15_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q15_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q15_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q15_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q15                                {\
    UNSG32 uBCM_Q15_mux                                :  5;\
    UNSG32 RSVDx134_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q15;
    struct w32AVIO_BCM_Q15;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q16_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q16_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q16_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q16_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q16                                {\
    UNSG32 uBCM_Q16_mux                                :  5;\
    UNSG32 RSVDx138_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q16;
    struct w32AVIO_BCM_Q16;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q17_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q17_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q17_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q17_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q17                                {\
    UNSG32 uBCM_Q17_mux                                :  5;\
    UNSG32 RSVDx13C_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q17;
    struct w32AVIO_BCM_Q17;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_Q18_mux(r32)                       _BFGET_(r32, 4, 0)
#define   SET32AVIO_BCM_Q18_mux(r32,v)                     _BFSET_(r32, 4, 0,v)
#define   GET16AVIO_BCM_Q18_mux(r16)                       _BFGET_(r16, 4, 0)
#define   SET16AVIO_BCM_Q18_mux(r16,v)                     _BFSET_(r16, 4, 0,v)

#define     w32AVIO_BCM_Q18                                {\
    UNSG32 uBCM_Q18_mux                                :  5;\
    UNSG32 RSVDx140_b5                                 : 27;\
  }
union { UNSG32 u32AVIO_BCM_Q18;
    struct w32AVIO_BCM_Q18;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_FULL_STS_Q0(r32)                   _BFGET_(r32, 0, 0)
#define   SET32AVIO_BCM_FULL_STS_Q0(r32,v)                 _BFSET_(r32, 0, 0,v)
#define   GET16AVIO_BCM_FULL_STS_Q0(r16)                   _BFGET_(r16, 0, 0)
#define   SET16AVIO_BCM_FULL_STS_Q0(r16,v)                 _BFSET_(r16, 0, 0,v)

#define   GET32AVIO_BCM_FULL_STS_Q1(r32)                   _BFGET_(r32, 1, 1)
#define   SET32AVIO_BCM_FULL_STS_Q1(r32,v)                 _BFSET_(r32, 1, 1,v)
#define   GET16AVIO_BCM_FULL_STS_Q1(r16)                   _BFGET_(r16, 1, 1)
#define   SET16AVIO_BCM_FULL_STS_Q1(r16,v)                 _BFSET_(r16, 1, 1,v)

#define   GET32AVIO_BCM_FULL_STS_Q2(r32)                   _BFGET_(r32, 2, 2)
#define   SET32AVIO_BCM_FULL_STS_Q2(r32,v)                 _BFSET_(r32, 2, 2,v)
#define   GET16AVIO_BCM_FULL_STS_Q2(r16)                   _BFGET_(r16, 2, 2)
#define   SET16AVIO_BCM_FULL_STS_Q2(r16,v)                 _BFSET_(r16, 2, 2,v)

#define   GET32AVIO_BCM_FULL_STS_Q3(r32)                   _BFGET_(r32, 3, 3)
#define   SET32AVIO_BCM_FULL_STS_Q3(r32,v)                 _BFSET_(r32, 3, 3,v)
#define   GET16AVIO_BCM_FULL_STS_Q3(r16)                   _BFGET_(r16, 3, 3)
#define   SET16AVIO_BCM_FULL_STS_Q3(r16,v)                 _BFSET_(r16, 3, 3,v)

#define   GET32AVIO_BCM_FULL_STS_Q4(r32)                   _BFGET_(r32, 4, 4)
#define   SET32AVIO_BCM_FULL_STS_Q4(r32,v)                 _BFSET_(r32, 4, 4,v)
#define   GET16AVIO_BCM_FULL_STS_Q4(r16)                   _BFGET_(r16, 4, 4)
#define   SET16AVIO_BCM_FULL_STS_Q4(r16,v)                 _BFSET_(r16, 4, 4,v)

#define   GET32AVIO_BCM_FULL_STS_Q5(r32)                   _BFGET_(r32, 5, 5)
#define   SET32AVIO_BCM_FULL_STS_Q5(r32,v)                 _BFSET_(r32, 5, 5,v)
#define   GET16AVIO_BCM_FULL_STS_Q5(r16)                   _BFGET_(r16, 5, 5)
#define   SET16AVIO_BCM_FULL_STS_Q5(r16,v)                 _BFSET_(r16, 5, 5,v)

#define   GET32AVIO_BCM_FULL_STS_Q6(r32)                   _BFGET_(r32, 6, 6)
#define   SET32AVIO_BCM_FULL_STS_Q6(r32,v)                 _BFSET_(r32, 6, 6,v)
#define   GET16AVIO_BCM_FULL_STS_Q6(r16)                   _BFGET_(r16, 6, 6)
#define   SET16AVIO_BCM_FULL_STS_Q6(r16,v)                 _BFSET_(r16, 6, 6,v)

#define   GET32AVIO_BCM_FULL_STS_Q7(r32)                   _BFGET_(r32, 7, 7)
#define   SET32AVIO_BCM_FULL_STS_Q7(r32,v)                 _BFSET_(r32, 7, 7,v)
#define   GET16AVIO_BCM_FULL_STS_Q7(r16)                   _BFGET_(r16, 7, 7)
#define   SET16AVIO_BCM_FULL_STS_Q7(r16,v)                 _BFSET_(r16, 7, 7,v)

#define   GET32AVIO_BCM_FULL_STS_Q8(r32)                   _BFGET_(r32, 8, 8)
#define   SET32AVIO_BCM_FULL_STS_Q8(r32,v)                 _BFSET_(r32, 8, 8,v)
#define   GET16AVIO_BCM_FULL_STS_Q8(r16)                   _BFGET_(r16, 8, 8)
#define   SET16AVIO_BCM_FULL_STS_Q8(r16,v)                 _BFSET_(r16, 8, 8,v)

#define   GET32AVIO_BCM_FULL_STS_Q9(r32)                   _BFGET_(r32, 9, 9)
#define   SET32AVIO_BCM_FULL_STS_Q9(r32,v)                 _BFSET_(r32, 9, 9,v)
#define   GET16AVIO_BCM_FULL_STS_Q9(r16)                   _BFGET_(r16, 9, 9)
#define   SET16AVIO_BCM_FULL_STS_Q9(r16,v)                 _BFSET_(r16, 9, 9,v)

#define   GET32AVIO_BCM_FULL_STS_Q10(r32)                  _BFGET_(r32,10,10)
#define   SET32AVIO_BCM_FULL_STS_Q10(r32,v)                _BFSET_(r32,10,10,v)
#define   GET16AVIO_BCM_FULL_STS_Q10(r16)                  _BFGET_(r16,10,10)
#define   SET16AVIO_BCM_FULL_STS_Q10(r16,v)                _BFSET_(r16,10,10,v)

#define   GET32AVIO_BCM_FULL_STS_Q11(r32)                  _BFGET_(r32,11,11)
#define   SET32AVIO_BCM_FULL_STS_Q11(r32,v)                _BFSET_(r32,11,11,v)
#define   GET16AVIO_BCM_FULL_STS_Q11(r16)                  _BFGET_(r16,11,11)
#define   SET16AVIO_BCM_FULL_STS_Q11(r16,v)                _BFSET_(r16,11,11,v)

#define   GET32AVIO_BCM_FULL_STS_Q12(r32)                  _BFGET_(r32,12,12)
#define   SET32AVIO_BCM_FULL_STS_Q12(r32,v)                _BFSET_(r32,12,12,v)
#define   GET16AVIO_BCM_FULL_STS_Q12(r16)                  _BFGET_(r16,12,12)
#define   SET16AVIO_BCM_FULL_STS_Q12(r16,v)                _BFSET_(r16,12,12,v)

#define   GET32AVIO_BCM_FULL_STS_Q13(r32)                  _BFGET_(r32,13,13)
#define   SET32AVIO_BCM_FULL_STS_Q13(r32,v)                _BFSET_(r32,13,13,v)
#define   GET16AVIO_BCM_FULL_STS_Q13(r16)                  _BFGET_(r16,13,13)
#define   SET16AVIO_BCM_FULL_STS_Q13(r16,v)                _BFSET_(r16,13,13,v)

#define   GET32AVIO_BCM_FULL_STS_Q14(r32)                  _BFGET_(r32,14,14)
#define   SET32AVIO_BCM_FULL_STS_Q14(r32,v)                _BFSET_(r32,14,14,v)
#define   GET16AVIO_BCM_FULL_STS_Q14(r16)                  _BFGET_(r16,14,14)
#define   SET16AVIO_BCM_FULL_STS_Q14(r16,v)                _BFSET_(r16,14,14,v)

#define   GET32AVIO_BCM_FULL_STS_Q15(r32)                  _BFGET_(r32,15,15)
#define   SET32AVIO_BCM_FULL_STS_Q15(r32,v)                _BFSET_(r32,15,15,v)
#define   GET16AVIO_BCM_FULL_STS_Q15(r16)                  _BFGET_(r16,15,15)
#define   SET16AVIO_BCM_FULL_STS_Q15(r16,v)                _BFSET_(r16,15,15,v)

#define   GET32AVIO_BCM_FULL_STS_Q16(r32)                  _BFGET_(r32,16,16)
#define   SET32AVIO_BCM_FULL_STS_Q16(r32,v)                _BFSET_(r32,16,16,v)
#define   GET16AVIO_BCM_FULL_STS_Q16(r16)                  _BFGET_(r16, 0, 0)
#define   SET16AVIO_BCM_FULL_STS_Q16(r16,v)                _BFSET_(r16, 0, 0,v)

#define   GET32AVIO_BCM_FULL_STS_Q17(r32)                  _BFGET_(r32,17,17)
#define   SET32AVIO_BCM_FULL_STS_Q17(r32,v)                _BFSET_(r32,17,17,v)
#define   GET16AVIO_BCM_FULL_STS_Q17(r16)                  _BFGET_(r16, 1, 1)
#define   SET16AVIO_BCM_FULL_STS_Q17(r16,v)                _BFSET_(r16, 1, 1,v)

#define   GET32AVIO_BCM_FULL_STS_Q18(r32)                  _BFGET_(r32,18,18)
#define   SET32AVIO_BCM_FULL_STS_Q18(r32,v)                _BFSET_(r32,18,18,v)
#define   GET16AVIO_BCM_FULL_STS_Q18(r16)                  _BFGET_(r16, 2, 2)
#define   SET16AVIO_BCM_FULL_STS_Q18(r16,v)                _BFSET_(r16, 2, 2,v)

#define     w32AVIO_BCM_FULL_STS                           {\
    UNSG32 uBCM_FULL_STS_Q0                            :  1;\
    UNSG32 uBCM_FULL_STS_Q1                            :  1;\
    UNSG32 uBCM_FULL_STS_Q2                            :  1;\
    UNSG32 uBCM_FULL_STS_Q3                            :  1;\
    UNSG32 uBCM_FULL_STS_Q4                            :  1;\
    UNSG32 uBCM_FULL_STS_Q5                            :  1;\
    UNSG32 uBCM_FULL_STS_Q6                            :  1;\
    UNSG32 uBCM_FULL_STS_Q7                            :  1;\
    UNSG32 uBCM_FULL_STS_Q8                            :  1;\
    UNSG32 uBCM_FULL_STS_Q9                            :  1;\
    UNSG32 uBCM_FULL_STS_Q10                           :  1;\
    UNSG32 uBCM_FULL_STS_Q11                           :  1;\
    UNSG32 uBCM_FULL_STS_Q12                           :  1;\
    UNSG32 uBCM_FULL_STS_Q13                           :  1;\
    UNSG32 uBCM_FULL_STS_Q14                           :  1;\
    UNSG32 uBCM_FULL_STS_Q15                           :  1;\
    UNSG32 uBCM_FULL_STS_Q16                           :  1;\
    UNSG32 uBCM_FULL_STS_Q17                           :  1;\
    UNSG32 uBCM_FULL_STS_Q18                           :  1;\
    UNSG32 RSVDx144_b19                                : 13;\
  }
union { UNSG32 u32AVIO_BCM_FULL_STS;
    struct w32AVIO_BCM_FULL_STS;
  };
///////////////////////////////////////////////////////////
#define   GET32AVIO_BCM_EMP_STS_Q0(r32)                    _BFGET_(r32, 0, 0)
#define   SET32AVIO_BCM_EMP_STS_Q0(r32,v)                  _BFSET_(r32, 0, 0,v)
#define   GET16AVIO_BCM_EMP_STS_Q0(r16)                    _BFGET_(r16, 0, 0)
#define   SET16AVIO_BCM_EMP_STS_Q0(r16,v)                  _BFSET_(r16, 0, 0,v)

#define   GET32AVIO_BCM_EMP_STS_Q1(r32)                    _BFGET_(r32, 1, 1)
#define   SET32AVIO_BCM_EMP_STS_Q1(r32,v)                  _BFSET_(r32, 1, 1,v)
#define   GET16AVIO_BCM_EMP_STS_Q1(r16)                    _BFGET_(r16, 1, 1)
#define   SET16AVIO_BCM_EMP_STS_Q1(r16,v)                  _BFSET_(r16, 1, 1,v)

#define   GET32AVIO_BCM_EMP_STS_Q2(r32)                    _BFGET_(r32, 2, 2)
#define   SET32AVIO_BCM_EMP_STS_Q2(r32,v)                  _BFSET_(r32, 2, 2,v)
#define   GET16AVIO_BCM_EMP_STS_Q2(r16)                    _BFGET_(r16, 2, 2)
#define   SET16AVIO_BCM_EMP_STS_Q2(r16,v)                  _BFSET_(r16, 2, 2,v)

#define   GET32AVIO_BCM_EMP_STS_Q3(r32)                    _BFGET_(r32, 3, 3)
#define   SET32AVIO_BCM_EMP_STS_Q3(r32,v)                  _BFSET_(r32, 3, 3,v)
#define   GET16AVIO_BCM_EMP_STS_Q3(r16)                    _BFGET_(r16, 3, 3)
#define   SET16AVIO_BCM_EMP_STS_Q3(r16,v)                  _BFSET_(r16, 3, 3,v)

#define   GET32AVIO_BCM_EMP_STS_Q4(r32)                    _BFGET_(r32, 4, 4)
#define   SET32AVIO_BCM_EMP_STS_Q4(r32,v)                  _BFSET_(r32, 4, 4,v)
#define   GET16AVIO_BCM_EMP_STS_Q4(r16)                    _BFGET_(r16, 4, 4)
#define   SET16AVIO_BCM_EMP_STS_Q4(r16,v)                  _BFSET_(r16, 4, 4,v)

#define   GET32AVIO_BCM_EMP_STS_Q5(r32)                    _BFGET_(r32, 5, 5)
#define   SET32AVIO_BCM_EMP_STS_Q5(r32,v)                  _BFSET_(r32, 5, 5,v)
#define   GET16AVIO_BCM_EMP_STS_Q5(r16)                    _BFGET_(r16, 5, 5)
#define   SET16AVIO_BCM_EMP_STS_Q5(r16,v)                  _BFSET_(r16, 5, 5,v)

#define   GET32AVIO_BCM_EMP_STS_Q6(r32)                    _BFGET_(r32, 6, 6)
#define   SET32AVIO_BCM_EMP_STS_Q6(r32,v)                  _BFSET_(r32, 6, 6,v)
#define   GET16AVIO_BCM_EMP_STS_Q6(r16)                    _BFGET_(r16, 6, 6)
#define   SET16AVIO_BCM_EMP_STS_Q6(r16,v)                  _BFSET_(r16, 6, 6,v)

#define   GET32AVIO_BCM_EMP_STS_Q7(r32)                    _BFGET_(r32, 7, 7)
#define   SET32AVIO_BCM_EMP_STS_Q7(r32,v)                  _BFSET_(r32, 7, 7,v)
#define   GET16AVIO_BCM_EMP_STS_Q7(r16)                    _BFGET_(r16, 7, 7)
#define   SET16AVIO_BCM_EMP_STS_Q7(r16,v)                  _BFSET_(r16, 7, 7,v)

#define   GET32AVIO_BCM_EMP_STS_Q8(r32)                    _BFGET_(r32, 8, 8)
#define   SET32AVIO_BCM_EMP_STS_Q8(r32,v)                  _BFSET_(r32, 8, 8,v)
#define   GET16AVIO_BCM_EMP_STS_Q8(r16)                    _BFGET_(r16, 8, 8)
#define   SET16AVIO_BCM_EMP_STS_Q8(r16,v)                  _BFSET_(r16, 8, 8,v)

#define   GET32AVIO_BCM_EMP_STS_Q9(r32)                    _BFGET_(r32, 9, 9)
#define   SET32AVIO_BCM_EMP_STS_Q9(r32,v)                  _BFSET_(r32, 9, 9,v)
#define   GET16AVIO_BCM_EMP_STS_Q9(r16)                    _BFGET_(r16, 9, 9)
#define   SET16AVIO_BCM_EMP_STS_Q9(r16,v)                  _BFSET_(r16, 9, 9,v)

#define   GET32AVIO_BCM_EMP_STS_Q10(r32)                   _BFGET_(r32,10,10)
#define   SET32AVIO_BCM_EMP_STS_Q10(r32,v)                 _BFSET_(r32,10,10,v)
#define   GET16AVIO_BCM_EMP_STS_Q10(r16)                   _BFGET_(r16,10,10)
#define   SET16AVIO_BCM_EMP_STS_Q10(r16,v)                 _BFSET_(r16,10,10,v)

#define   GET32AVIO_BCM_EMP_STS_Q11(r32)                   _BFGET_(r32,11,11)
#define   SET32AVIO_BCM_EMP_STS_Q11(r32,v)                 _BFSET_(r32,11,11,v)
#define   GET16AVIO_BCM_EMP_STS_Q11(r16)                   _BFGET_(r16,11,11)
#define   SET16AVIO_BCM_EMP_STS_Q11(r16,v)                 _BFSET_(r16,11,11,v)

#define   GET32AVIO_BCM_EMP_STS_Q12(r32)                   _BFGET_(r32,12,12)
#define   SET32AVIO_BCM_EMP_STS_Q12(r32,v)                 _BFSET_(r32,12,12,v)
#define   GET16AVIO_BCM_EMP_STS_Q12(r16)                   _BFGET_(r16,12,12)
#define   SET16AVIO_BCM_EMP_STS_Q12(r16,v)                 _BFSET_(r16,12,12,v)

#define   GET32AVIO_BCM_EMP_STS_Q13(r32)                   _BFGET_(r32,13,13)
#define   SET32AVIO_BCM_EMP_STS_Q13(r32,v)                 _BFSET_(r32,13,13,v)
#define   GET16AVIO_BCM_EMP_STS_Q13(r16)                   _BFGET_(r16,13,13)
#define   SET16AVIO_BCM_EMP_STS_Q13(r16,v)                 _BFSET_(r16,13,13,v)

#define   GET32AVIO_BCM_EMP_STS_Q14(r32)                   _BFGET_(r32,14,14)
#define   SET32AVIO_BCM_EMP_STS_Q14(r32,v)                 _BFSET_(r32,14,14,v)
#define   GET16AVIO_BCM_EMP_STS_Q14(r16)                   _BFGET_(r16,14,14)
#define   SET16AVIO_BCM_EMP_STS_Q14(r16,v)                 _BFSET_(r16,14,14,v)

#define   GET32AVIO_BCM_EMP_STS_Q15(r32)                   _BFGET_(r32,15,15)
#define   SET32AVIO_BCM_EMP_STS_Q15(r32,v)                 _BFSET_(r32,15,15,v)
#define   GET16AVIO_BCM_EMP_STS_Q15(r16)                   _BFGET_(r16,15,15)
#define   SET16AVIO_BCM_EMP_STS_Q15(r16,v)                 _BFSET_(r16,15,15,v)

#define   GET32AVIO_BCM_EMP_STS_Q16(r32)                   _BFGET_(r32,16,16)
#define   SET32AVIO_BCM_EMP_STS_Q16(r32,v)                 _BFSET_(r32,16,16,v)
#define   GET16AVIO_BCM_EMP_STS_Q16(r16)                   _BFGET_(r16, 0, 0)
#define   SET16AVIO_BCM_EMP_STS_Q16(r16,v)                 _BFSET_(r16, 0, 0,v)

#define   GET32AVIO_BCM_EMP_STS_Q17(r32)                   _BFGET_(r32,17,17)
#define   SET32AVIO_BCM_EMP_STS_Q17(r32,v)                 _BFSET_(r32,17,17,v)
#define   GET16AVIO_BCM_EMP_STS_Q17(r16)                   _BFGET_(r16, 1, 1)
#define   SET16AVIO_BCM_EMP_STS_Q17(r16,v)                 _BFSET_(r16, 1, 1,v)

#define   GET32AVIO_BCM_EMP_STS_Q18(r32)                   _BFGET_(r32,18,18)
#define   SET32AVIO_BCM_EMP_STS_Q18(r32,v)                 _BFSET_(r32,18,18,v)
#define   GET16AVIO_BCM_EMP_STS_Q18(r16)                   _BFGET_(r16, 2, 2)
#define   SET16AVIO_BCM_EMP_STS_Q18(r16,v)                 _BFSET_(r16, 2, 2,v)

#define     w32AVIO_BCM_EMP_STS                            {\
    UNSG32 uBCM_EMP_STS_Q0                             :  1;\
    UNSG32 uBCM_EMP_STS_Q1                             :  1;\
    UNSG32 uBCM_EMP_STS_Q2                             :  1;\
    UNSG32 uBCM_EMP_STS_Q3                             :  1;\
    UNSG32 uBCM_EMP_STS_Q4                             :  1;\
    UNSG32 uBCM_EMP_STS_Q5                             :  1;\
    UNSG32 uBCM_EMP_STS_Q6                             :  1;\
    UNSG32 uBCM_EMP_STS_Q7                             :  1;\
    UNSG32 uBCM_EMP_STS_Q8                             :  1;\
    UNSG32 uBCM_EMP_STS_Q9                             :  1;\
    UNSG32 uBCM_EMP_STS_Q10                            :  1;\
    UNSG32 uBCM_EMP_STS_Q11                            :  1;\
    UNSG32 uBCM_EMP_STS_Q12                            :  1;\
    UNSG32 uBCM_EMP_STS_Q13                            :  1;\
    UNSG32 uBCM_EMP_STS_Q14                            :  1;\
    UNSG32 uBCM_EMP_STS_Q15                            :  1;\
    UNSG32 uBCM_EMP_STS_Q16                            :  1;\
    UNSG32 uBCM_EMP_STS_Q17                            :  1;\
    UNSG32 uBCM_EMP_STS_Q18                            :  1;\
    UNSG32 RSVDx148_b19                                : 13;\
  }
union { UNSG32 u32AVIO_BCM_EMP_STS;
    struct w32AVIO_BCM_EMP_STS;
  };
///////////////////////////////////////////////////////////
     UNSG8 RSVDx14C                                    [180];
///////////////////////////////////////////////////////////
} SIE_AVIO;

typedef union  T32AVIO_BCM_Q0
  { UNSG32 u32;
    struct w32AVIO_BCM_Q0;
         } T32AVIO_BCM_Q0;
typedef union  T32AVIO_BCM_Q1
  { UNSG32 u32;
    struct w32AVIO_BCM_Q1;
         } T32AVIO_BCM_Q1;
typedef union  T32AVIO_BCM_Q2
  { UNSG32 u32;
    struct w32AVIO_BCM_Q2;
         } T32AVIO_BCM_Q2;
typedef union  T32AVIO_BCM_Q3
  { UNSG32 u32;
    struct w32AVIO_BCM_Q3;
         } T32AVIO_BCM_Q3;
typedef union  T32AVIO_BCM_Q4
  { UNSG32 u32;
    struct w32AVIO_BCM_Q4;
         } T32AVIO_BCM_Q4;
typedef union  T32AVIO_BCM_Q5
  { UNSG32 u32;
    struct w32AVIO_BCM_Q5;
         } T32AVIO_BCM_Q5;
typedef union  T32AVIO_BCM_Q6
  { UNSG32 u32;
    struct w32AVIO_BCM_Q6;
         } T32AVIO_BCM_Q6;
typedef union  T32AVIO_BCM_Q7
  { UNSG32 u32;
    struct w32AVIO_BCM_Q7;
         } T32AVIO_BCM_Q7;
typedef union  T32AVIO_BCM_Q8
  { UNSG32 u32;
    struct w32AVIO_BCM_Q8;
         } T32AVIO_BCM_Q8;
typedef union  T32AVIO_BCM_Q9
  { UNSG32 u32;
    struct w32AVIO_BCM_Q9;
         } T32AVIO_BCM_Q9;
typedef union  T32AVIO_BCM_Q10
  { UNSG32 u32;
    struct w32AVIO_BCM_Q10;
         } T32AVIO_BCM_Q10;
typedef union  T32AVIO_BCM_Q11
  { UNSG32 u32;
    struct w32AVIO_BCM_Q11;
         } T32AVIO_BCM_Q11;
typedef union  T32AVIO_BCM_Q14
  { UNSG32 u32;
    struct w32AVIO_BCM_Q14;
         } T32AVIO_BCM_Q14;
typedef union  T32AVIO_BCM_Q15
  { UNSG32 u32;
    struct w32AVIO_BCM_Q15;
         } T32AVIO_BCM_Q15;
typedef union  T32AVIO_BCM_Q16
  { UNSG32 u32;
    struct w32AVIO_BCM_Q16;
         } T32AVIO_BCM_Q16;
typedef union  T32AVIO_BCM_Q17
  { UNSG32 u32;
    struct w32AVIO_BCM_Q17;
         } T32AVIO_BCM_Q17;
typedef union  T32AVIO_BCM_Q18
  { UNSG32 u32;
    struct w32AVIO_BCM_Q18;
         } T32AVIO_BCM_Q18;
typedef union  T32AVIO_BCM_FULL_STS
  { UNSG32 u32;
    struct w32AVIO_BCM_FULL_STS;
         } T32AVIO_BCM_FULL_STS;
typedef union  T32AVIO_BCM_EMP_STS
  { UNSG32 u32;
    struct w32AVIO_BCM_EMP_STS;
         } T32AVIO_BCM_EMP_STS;
///////////////////////////////////////////////////////////

typedef union  TAVIO_BCM_Q0
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q0;
           };
         } TAVIO_BCM_Q0;
typedef union  TAVIO_BCM_Q1
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q1;
           };
         } TAVIO_BCM_Q1;
typedef union  TAVIO_BCM_Q2
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q2;
           };
         } TAVIO_BCM_Q2;
typedef union  TAVIO_BCM_Q3
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q3;
           };
         } TAVIO_BCM_Q3;
typedef union  TAVIO_BCM_Q4
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q4;
           };
         } TAVIO_BCM_Q4;
typedef union  TAVIO_BCM_Q5
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q5;
           };
         } TAVIO_BCM_Q5;
typedef union  TAVIO_BCM_Q6
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q6;
           };
         } TAVIO_BCM_Q6;
typedef union  TAVIO_BCM_Q7
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q7;
           };
         } TAVIO_BCM_Q7;
typedef union  TAVIO_BCM_Q8
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q8;
           };
         } TAVIO_BCM_Q8;
typedef union  TAVIO_BCM_Q9
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q9;
           };
         } TAVIO_BCM_Q9;
typedef union  TAVIO_BCM_Q10
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q10;
           };
         } TAVIO_BCM_Q10;
typedef union  TAVIO_BCM_Q11
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q11;
           };
         } TAVIO_BCM_Q11;
typedef union  TAVIO_BCM_Q14
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q14;
           };
         } TAVIO_BCM_Q14;
typedef union  TAVIO_BCM_Q15
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q15;
           };
         } TAVIO_BCM_Q15;
typedef union  TAVIO_BCM_Q16
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q16;
           };
         } TAVIO_BCM_Q16;
typedef union  TAVIO_BCM_Q17
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q17;
           };
         } TAVIO_BCM_Q17;
typedef union  TAVIO_BCM_Q18
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_Q18;
           };
         } TAVIO_BCM_Q18;
typedef union  TAVIO_BCM_FULL_STS
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_FULL_STS;
           };
         } TAVIO_BCM_FULL_STS;
typedef union  TAVIO_BCM_EMP_STS
  { UNSG32 u32[1];
    struct {
    struct w32AVIO_BCM_EMP_STS;
           };
         } TAVIO_BCM_EMP_STS;

///////////////////////////////////////////////////////////
SIGN32 AVIO_drvrd(SIE_AVIO *p, UNSG32 base, SIGN32 mem, SIGN32 tst);
SIGN32 AVIO_drvwr(SIE_AVIO *p, UNSG32 base, SIGN32 mem, SIGN32 tst, UNSG32 *pcmd);
void AVIO_reset(SIE_AVIO *p);
SIGN32 AVIO_cmp  (SIE_AVIO *p, SIE_AVIO *pie, char *pfx, void *hLOG, SIGN32 mem, SIGN32 tst);
#define AVIO_check(p,pie,pfx,hLOG) AVIO_cmp(p,pie,pfx,(void*)(hLOG),0,0)
#define AVIO_print(p,    pfx,hLOG) AVIO_cmp(p,0,  pfx,(void*)(hLOG),0,0)

#endif
//////
/// ENDOFINTERFACE: AVIO
////////////////////////////////////////////////////////////



#ifdef __cplusplus
  }
#endif
#pragma  pack()

#endif
//////
/// ENDOFFILE: avio.h
////////////////////////////////////////////////////////////

