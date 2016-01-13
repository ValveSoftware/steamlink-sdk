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

#ifndef _OSAL_DRIVER_H_
#define _OSAL_DRIVER_H_

#define MV_CC_SID_BIT_LOCAL			0

#define NETLINK_GALOIS_CC_SINGLECPU		(29) /* Galois CC module transports by netlink*/
#define NETLINK_GALOIS_CC_GROUP_SINGLECPU	(0)  /* Galois CC module transports by netlink*/
#define MV_CC_ICCFIFO_FRAME_SIZE		(128)


#define CC_DEVICE_NAME                      "galois_cc"
#define CC_DEVICE_TAG                       "[Galois][cc_driver] "


#define CC_DEVICE_PROCFILE_STATUS           "status"
#define CC_DEVICE_PROCFILE_DETAIL           "detail"

#define CC_DEVICE_LIST_NUM                  (100)


#define CC_DEVICE_CMD_REG                  (0x1E01)
#define CC_DEVICE_CMD_FREE                 (0x1E02)
#define CC_DEVICE_CMD_INQUIRY              (0x1E03)
#define CC_DEVICE_CMD_GET_LIST             (0x1E04)
#define CC_DEVICE_CMD_GET_STATUS           (0x1E05)
#define CC_DEVICE_CMD_TEST_MSG             (0x1E06)
#define CC_DEVICE_CMD_UPDATE               (0x1E07)
#define CC_DEVICE_CMD_CREATE_CBUF          (0x1E08)
#define CC_DEVICE_CMD_DESTROY_CBUF         (0x1E09)


#endif
