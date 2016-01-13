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

#ifndef __GALOIS_GENERIC_H
#define __GALOIS_GENERIC_H

#define VAL2STR(x)          #x
#define GALOIS_TO_STR(x)    VAL2STR(x)
/*
 * please put your major/minor number in sequence, i.e. right under my comments here.
 * or you are risky to mislead others.
 */

/* pe device for single cpu */
#if defined(BERLIN_SINGLE_CPU)
#define GALOIS_PE_MAJOR            244
#define GALOIS_PE_MINORS           2
#define GALOIS_PE_MINOR            0

/* pe agent device for single cpu */
#define GALOIS_PE_AGENT_MINOR      1
#endif

/* cc device for single cpu */
#define GALOIS_CCNEW_MAJOR		245
#define GALOIS_CCNEW_MINORS		1
#define GALOIS_CCNEW_MINOR	    0

/* shm device for single cpu */
#define GALOIS_SHM_MAJOR		246
#define GALOIS_SHM_MINORS		2
#define GALOIS_SHM0_MINOR	    0
#define GALOIS_SHM1_MINOR	    1

/* device major and minor */
#define MV88DE3100_SERIAL_MAJOR	247
#define MV88DE3100_SERIAL_MINOR	66

#define GALOIS_PWMCH_MAJOR		248
#define GALOIS_PWMCH0_MINOR	0
#define GALOIS_PWMCH1_MINOR	1
#define GALOIS_PWMCH2_MINOR	2
#define GALOIS_PWMCH3_MINOR	3
#define GALOIS_PWMCH0_NAME	"pwmch0"
#define GALOIS_PWMCH1_NAME	"pwmch1"
#define GALOIS_PWMCH2_NAME	"pwmch2"
#define GALOIS_PWMCH3_NAME	"pwmch3"

#define GALOIS_TWSI_MAJOR		249
#define GALOIS_TWSI0_MINOR	0
#define GALOIS_TWSI1_MINOR	1
#define GALOIS_TWSI2_MINOR	2
#define GALOIS_TWSI3_MINOR	3
#define GALOIS_TWSI0_NAME	"twsi0"
#define GALOIS_TWSI1_NAME	"twsi1"
#define GALOIS_TWSI2_NAME	"twsi2"
#define GALOIS_TWSI3_NAME	"twsi3"

/* MAJOR=250 is reserved for fusion */

#define GALOIS_SPI_MAJOR		251
#define GALOIS_SPI0_MINOR	0
#define GALOIS_SPI1_MINOR	1
#define GALOIS_SPI2_MINOR	2
#define GALOIS_SPI0_NAME	"spi0"
#define GALOIS_SPI1_NAME	"spi1"
#define GALOIS_SPI2_NAME	"spi2"

#define GALOIS_CC_MAJOR			252
#define GALOIS_CC_MINORS		4
#define GALOIS_CC_SHM0_MINOR	0
#define GALOIS_CC_SHM1_MINOR	1
#define GALOIS_CC_SHM2_MINOR	2
#define GALOIS_CC_ICC_MINOR		3
#define GALOIS_CC_SHM0_NAME		"MV_CC_SHM0"
#define GALOIS_CC_SHM1_NAME		"MV_CC_SHM1"
#define GALOIS_CC_SHM2_NAME		"MV_CC_SHM2"
#define GALOIS_CC_ICC_NAME		"MV_CC_ICC"

#define	GALOIS_PINMUX_MAJOR		253
#define	GALOIS_PINMUX_MINOR		0
#define	GALOIS_PINMUX_NAME		"pinmux"

/*
 * misc device: major = 10
 */
#define GALOIS_PFTIMER_MISC_MINOR	111
#define GALOIS_PFTIMER_NAME			"pftimer"
#define GALOIS_SATA_MISC_MINOR		112
#define GALOIS_SATA_NAME			"mv61xx"
#define GALOIS_IOMAPPER_MISC_MINOR	113
#define GALOIS_IOMAPPER_NAME		"iomapper"
#define GALOIS_IR_MISC_MINOR		114
#define GALOIS_IR_NAME				"girl"
#define GALOIS_REGMAP_MISC_MINOR	115
#define GALOIS_REGMAP_NAME			"regmap"
#define GALOIS_GPIO_MISC_MINOR		116
#define GALOIS_GPIO_NAME			"gpio"
#define BERLIN_SM_MISC_MINOR		117
#define BERLIN_SM_NAME				"bsm"
#define BERLIN_MVPM_MISC_MINOR		118
#define BERLIN_MVPM_NAME			"mvpm"
#define GALOIS_SOFT_KEYBOARD_MISC_MINOR 119
#define GALOIS_SOFT_KEYBOARD_NAME "soft_keyboard"
#define BERLIN_SM_MISCDEV_MINOR		120
#define BERLIN_SM_MISCDEV_NAME			"bsmd"



int bsm_msg_receive(int iModuleID, void *pMsgBody, int *piLen);
int bsm_msg_send(int iModuleID, void *pMsgBody, int iLen);

#endif
