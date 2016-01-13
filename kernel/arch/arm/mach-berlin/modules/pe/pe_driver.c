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

/*******************************************************************************
  System head files
  */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <linux/proc_fs.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>

#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/reboot.h>

#include <linux/of.h>
#include <linux/of_irq.h>
/*******************************************************************************
  Local head files
  */

#include <mach/galois_generic.h>

#include "galois_io.h"
#include "cinclude.h"
#include "zspWrapper.h"

#include "api_avio_dhub.h"
#include "shm_api.h"
#include "cc_msgq.h"
#include "cc_error.h"

#include "pe_driver.h"
#include "pe_agent_driver.h"
#include "pe_memmap.h"
#include "api_dhub.h"

#ifdef CONFIG_BERLIN_FASTLOGO
#include "../fastlogo/fastlogo_driver.h"
static int pe_cpu_id = 0;
static int dhub_init_flag =0;
#endif

enum {
        IRQ_ZSPINT,
        IRQ_DHUBINTRAVIO0,
        IRQ_DHUBINTRAVIO1,
        IRQ_DHUBINTRAVIO2,
        IRQ_DHUBINTRVPRO,
        IRQ_SM_CEC,
        IRQ_PE_MAX,
};
static int pe_irqs[IRQ_PE_MAX];
/*******************************************************************************
  Module API defined
  */

// set 1 to enable message by ioctl
// set 0 to enable ICC message queue
#define CONFIG_VPP_IOCTL_MSG                1
// when CONFIG_VPP_IOCTL_MSG is 1
// set 1 to use internal message queue between isr and ioctl function
// set 0 to use no queue
#define CONFIG_VPP_ISR_MSGQ                 1

// only enable when VPP use ioctl
#if CONFIG_VPP_IOCTL_MSG
// set 1 to enable message by ioctl
// set 0 to enable ICC message queue
#define CONFIG_VIP_IOCTL_MSG                1
// when CONFIG_VIP_IOCTL_MSG is 1
// set 1 to use internal message queue between isr and ioctl function
// set 0 to use no queue
#define CONFIG_VIP_ISR_MSGQ                 1
#endif

#define CONFIG_VDEC_UNBLC_IRQ_FIX           0

#define PE_DEVICE_NAME                      "galois_pe"
#define PE_DEVICE_TAG                       "[Galois][pe_driver] "
#define PE_DEVICE_PATH                      ("/dev/" PE_DEVICE_NAME)

#define PE_DEVICE_PROCFILE_STATUS           "status"
#define PE_DEVICE_PROCFILE_DETAIL           "detail"

#define MV_BERLIN_CPU0                      0
#define PE_MODULE_MSG_ID_VPP                MV_GS_VPP_Serv
#define PE_MODULE_MSG_ID_VDEC               MV_GS_VDEC_Serv
#define PE_MODULE_MSG_ID_AUD                MV_GS_AUD_Serv
#define PE_MODULE_MSG_ID_ZSP                MV_GS_ZSP_Serv
#define PE_MODULE_MSG_ID_RLE                MV_GS_RLE_Serv
#define PE_MODULE_MSG_ID_VIP                MV_GS_VIP_Serv
#define PE_MODULE_MSG_ID_AIP                MV_GS_AIP_Serv

#define VPP_CC_MSG_TYPE_VPP 0x00
#define VPP_CC_MSG_TYPE_CEC 0x01

#define ADSP_ZSPINT2Soc_IRQ0                0

#define VPP_IOCTL_VBI_DMA_CFGQ  0xbeef0001
#define VPP_IOCTL_VBI_BCM_CFGQ  0xbeef0002
#define VPP_IOCTL_VDE_BCM_CFGQ  0xbeef0003
#define VPP_IOCTL_GET_MSG       0xbeef0004
#define VPP_IOCTL_START_BCM_TRANSACTION 0xbeef0005
#define VPP_IOCTL_BCM_SCHE_CMD  0xbeef0006
#define VPP_IOCTL_INTR_MSG      0xbeef0007
#define CEC_IOCTL_RX_MSG_BUF_MSG     0xbeef0008

#define VDEC_IOCTL_ENABLE_INT   0xbeef1001
#define AOUT_IOCTL_START_CMD    0xbeef2001
#define AIP_IOCTL_START_CMD     0xbeef2002
#define AIP_IOCTL_STOP_CMD      0xbeef2003
#define AOUT_IOCTL_STOP_CMD    	0xbeef2004
#define AOUT_IOCTL_DISABLE_INT_MSG 0xbeef2005

#define APP_IOCTL_INIT_CMD      0xbeef3001
#define APP_IOCTL_START_CMD     0xbeef3002

#define VIP_IOCTL_GET_MSG       0xbeef4001
#define VIP_IOCTL_VBI_BCM_CFGQ  0xbeef4002
#define VIP_IOCTL_SD_WRE_CFGQ   0xbeef4003
#define VIP_IOCTL_SD_RDE_CFGQ   0xbeef4004
#define VIP_IOCTL_SEND_MSG      0xbeef4005
#define VIP_IOCTL_VDE_BCM_CFGQ  0xbeef4006
#define VIP_IOCTL_INTR_MSG      0xbeef4007

//CEC MACRO
#define SM_APB_ICTL1_BASE                       0xf7fce000
#define SM_APB_GPIO0_BASE                       0xf7fcc000
#define APB_GPIO_PORTA_EOI                      0x4c
#define APB_GPIO_INTSTATUS                      0x40
#define APB_ICTL_IRQ_STATUS_L                   0x20
#define BE_CEC_INTR_TX_SFT_FAIL                 0x2008 // puneet
#define BE_CEC_INTR_TX_FAIL                     0x200F
#define BE_CEC_INTR_TX_COMPLETE                 0x0010
#define BE_CEC_INTR_RX_COMPLETE                 0x0020
#define BE_CEC_INTR_RX_FAIL                     0x00C0
#define SOC_SM_CEC_BASE                         0xF7FE1000
#define CEC_INTR_STATUS0_REG_ADDR               0x0058
#define CEC_INTR_STATUS1_REG_ADDR               0x0059
#define CEC_INTR_ENABLE0_REG_ADDR               0x0048
#define CEC_INTR_ENABLE1_REG_ADDR               0x0049
#define CEC_RDY_ADDR                            0x0008
#define CEC_RX_BUF_READ_REG_ADDR            	0x0068
#define CEC_RX_EOM_READ_REG_ADDR                0x0069
#define CEC_TOGGLE_FOR_READ_REG_ADDR        	0x0004
#define CEC_RX_RDY_ADDR                      	0x000c //01
#define CEC_RX_FIFO_DPTR                     	0x0087
static int pe_driver_open(struct inode *inode, struct file *filp);
static int pe_driver_release(struct inode *inode, struct file *filp);
static long pe_driver_ioctl_unlocked(struct file *filp, unsigned int cmd,
				     unsigned long arg);

/*******************************************************************************
  Macro Defined
  */

#ifdef ENABLE_DEBUG
#define pe_debug(...)   printk(KERN_DEBUG PE_DEVICE_TAG __VA_ARGS__)
#else
#define pe_debug(...)
#endif

#define pe_trace(...)   printk(KERN_WARNING PE_DEVICE_TAG __VA_ARGS__)
#define pe_error(...)   printk(KERN_ERR PE_DEVICE_TAG __VA_ARGS__)

//#define VPP_DBG

#define DEBUG_TIMER_VALUE					(0xFFFFFFFF)

/*******************************************************************************
  Module Variable
  */

#if CONFIG_VDEC_UNBLC_IRQ_FIX
static int vdec_int_cnt;
static int vdec_enable_int_cnt;
#endif

static struct cdev pe_dev;
static struct class *pe_dev_class;

static struct file_operations pe_ops = {
	.open = pe_driver_open,
	.release = pe_driver_release,
	.unlocked_ioctl = pe_driver_ioctl_unlocked,
	.owner = THIS_MODULE,
};

typedef struct VPP_DMA_INFO_T {
	UINT32 DmaAddr;
	UINT32 DmaLen;
	UINT32 cpcbID;
} VPP_DMA_INFO;

typedef struct VPP_CEC_RX_MSG_BUF_T {
	UINT8 buf[16];
	UINT8 len;
} VPP_CEC_RX_MSG_BUF;

static VPP_DMA_INFO dma_info[3];
static VPP_DMA_INFO vbi_bcm_info[3];
static VPP_DMA_INFO vde_bcm_info[3];
static unsigned int bcm_sche_cmd[6][2];
static VPP_CEC_RX_MSG_BUF rx_buf;

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
typedef struct VIP_DMA_INFO_T {
	UINT32 DmaAddr;
	UINT32 DmaLen;
} VIP_DMA_INFO;

static VIP_DMA_INFO vip_dma_info;
static VIP_DMA_INFO vip_vbi_info;
static VIP_DMA_INFO vip_sd_wr_info;
static VIP_DMA_INFO vip_sd_rd_info;

/////////////////////////////////////////NEW_ISR related
#define NEW_ISR
//This array stores all the VIP intrs enable/disable status
#define MAX_INTR_NUM 0x20
static UINT32 vip_intr_status[MAX_INTR_NUM];
static UINT32 vpp_intr_status[MAX_INTR_NUM];

typedef struct INTR_MSG_T {
	UINT32 DhubSemMap;
	UINT32 Enable;
} INTR_MSG;
/////////////////////////////////////////End of NEW_ISR

static void pe_vip_do_tasklet(unsigned long);
DECLARE_TASKLET_DISABLED(pe_vip_tasklet, pe_vip_do_tasklet, 0);

#define VIP_MSG_DESTROY_ISR_TASK   1
#endif

static AOUT_PATH_CMD_FIFO *p_ma_fifo;
static AOUT_PATH_CMD_FIFO *p_sa_fifo;
static AOUT_PATH_CMD_FIFO *p_spdif_fifo;
static AOUT_PATH_CMD_FIFO *p_hdmi_fifo;
static HWAPP_CMD_FIFO *p_app_fifo;
static AIP_DMA_CMD_FIFO *p_aip_fifo;

static int aip_i2s_pair;
static int disable_hdmi_int_msg = 0;

static struct proc_dir_entry *pe_driver_procdir;
static int vpp_cpcb0_vbi_int_cnt;
static int vpp_hdmi_audio_int_cnt;
//static int vpp_cpcb2_vbi_int_cnt = 0;

static DEFINE_SPINLOCK(bcm_spinlock);
static DEFINE_SPINLOCK(aout_spinlock);
static DEFINE_SPINLOCK(app_spinlock);
static DEFINE_SPINLOCK(aip_spinlock);
static DEFINE_SPINLOCK(msgQ_spinlock);

static void pe_vpp_do_tasklet(unsigned long);
static void pe_vpp_cec_do_tasklet(unsigned long);	// cec tasklet added
static void pe_vdec_do_tasklet(unsigned long);

static void pe_ma_do_tasklet(unsigned long);
static void pe_sa_do_tasklet(unsigned long);
static void pe_spdif_do_tasklet(unsigned long);
static void pe_aip_do_tasklet(unsigned long);
static void pe_hdmi_do_tasklet(unsigned long);
static void pe_app_do_tasklet(unsigned long);
static void pe_zsp_do_tasklet(unsigned long);
static void pe_pg_dhub_done_tasklet(unsigned long);
static void pe_rle_do_err_tasklet(unsigned long);
static void pe_rle_do_done_tasklet(unsigned long);
static void aout_resume_cmd(int path_id);
static void aip_resume_cmd(void);

static DECLARE_TASKLET_DISABLED(pe_vpp_tasklet, pe_vpp_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_vpp_cec_tasklet, pe_vpp_cec_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_vdec_tasklet, pe_vdec_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_ma_tasklet, pe_ma_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_sa_tasklet, pe_sa_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_spdif_tasklet, pe_spdif_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_aip_tasklet, pe_aip_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_hdmi_tasklet, pe_hdmi_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_app_tasklet, pe_app_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_zsp_tasklet, pe_zsp_do_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_pg_done_tasklet, pe_pg_dhub_done_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_rle_err_tasklet, pe_rle_do_err_tasklet, 0);
static DECLARE_TASKLET_DISABLED(pe_rle_done_tasklet, pe_rle_do_done_tasklet, 0);

#if (0)
#define CLEAR_FIGO_INTR    \
{(*(volatile UINT*)(MEMMAP_TSI_REG_BASE+RA_DRMDMX_figoSys+RA_FigoSys_FIGO0+BA_FigoReg_figoIntrLvl_st)) = 1;}
#define INTRA_MSG_DMX_FIGO_IRQ              0x3000
#endif

#if CONFIG_VPP_IOCTL_MSG
static INT vpp_instat;
static UINT vpp_intr_timestamp;
static DEFINE_SEMAPHORE(vpp_sem);
#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
#if CONFIG_VIP_IOCTL_MSG
static UINT vip_intr_timestamp;
static DEFINE_SEMAPHORE(vip_sem);
#endif
#endif

#if CONFIG_VPP_ISR_MSGQ

struct pe_message_queue;
typedef struct pe_message_queue {
	UINT q_length;
	UINT rd_number;
	UINT wr_number;
	MV_CC_MSG_t *pMsg;

	HRESULT(*Add) (struct pe_message_queue *pMsgQ, MV_CC_MSG_t *pMsg);
	HRESULT(*ReadTry) (struct pe_message_queue *pMsgQ,
			    MV_CC_MSG_t *pMsg);
	HRESULT(*ReadFin) (struct pe_message_queue *pMsgQ);
	HRESULT(*Destroy) (struct pe_message_queue *pMsgQ);

} PEMsgQ_t;

#define VPP_ISR_MSGQ_SIZE           8

static PEMsgQ_t hPEMsgQ;

static HRESULT PEMsgQ_Add(PEMsgQ_t *pMsgQ, MV_CC_MSG_t *pMsg)
{
	INT wr_offset;

	if (NULL == pMsgQ->pMsg || pMsg == NULL)
		return S_FALSE;

	wr_offset = pMsgQ->wr_number - pMsgQ->rd_number;

	if (wr_offset == -1 || wr_offset == (pMsgQ->q_length - 2)) {
		return S_FALSE;
	} else {
		memcpy((CHAR *) & pMsgQ->pMsg[pMsgQ->wr_number], (CHAR *) pMsg,
		       sizeof(MV_CC_MSG_t));
		pMsgQ->wr_number++;
		pMsgQ->wr_number %= pMsgQ->q_length;
	}

	return S_OK;
}

static HRESULT PEMsgQ_ReadTry(PEMsgQ_t *pMsgQ, MV_CC_MSG_t *pMsg)
{
	INT rd_offset;

	if (NULL == pMsgQ->pMsg || pMsg == NULL)
		return S_FALSE;

	rd_offset = pMsgQ->rd_number - pMsgQ->wr_number;

	if (rd_offset != 0) {
		memcpy((CHAR *) pMsg, (CHAR *) & pMsgQ->pMsg[pMsgQ->rd_number],
		       sizeof(MV_CC_MSG_t));
		return S_OK;
	} else {
		return S_FALSE;
	}
}

static HRESULT PEMsgQ_ReadFinish(PEMsgQ_t *pMsgQ)
{
	INT rd_offset;

	rd_offset = pMsgQ->rd_number - pMsgQ->wr_number;

	if (rd_offset != 0) {
		pMsgQ->rd_number++;
		pMsgQ->rd_number %= pMsgQ->q_length;

		return S_OK;
	} else {
		return S_FALSE;
	}
}

#define	MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int PEMsgQ_Dequeue(PEMsgQ_t *pMsgQ, int cnt)
{
	INT fullness;

	if (cnt <= 0)
		return -1;

	fullness = pMsgQ->wr_number - pMsgQ->rd_number;
	if (fullness < 0) {
		fullness += pMsgQ->q_length;
	}

	cnt = MIN(fullness, cnt);
	if (cnt) {
		pMsgQ->rd_number += cnt;
		if (pMsgQ->rd_number >= pMsgQ->q_length)
			pMsgQ->rd_number -= pMsgQ->q_length;
	}
	return cnt;
}

static int PEMsgQ_DequeueRead(PEMsgQ_t *pMsgQ, MV_CC_MSG_t *pMsg)
{
	INT fullness;

	if (NULL == pMsgQ->pMsg || pMsg == NULL)
		return S_FALSE;

	fullness = pMsgQ->wr_number - pMsgQ->rd_number;
	if (fullness < 0) {
		fullness += pMsgQ->q_length;
	}

	if (fullness) {
		if (pMsg)
			memcpy((void *)pMsg,
			       (void *)&pMsgQ->pMsg[pMsgQ->rd_number],
			       sizeof(MV_CC_MSG_t));

		pMsgQ->rd_number++;
		if (pMsgQ->rd_number >= pMsgQ->q_length)
			pMsgQ->rd_number -= pMsgQ->q_length;

		return 1;
	}

	return 0;
}

static int PEMsgQ_Fullness(PEMsgQ_t *pMsgQ)
{
	INT fullness;

	fullness = pMsgQ->wr_number - pMsgQ->rd_number;
	if (fullness < 0) {
		fullness += pMsgQ->q_length;
	}

	return fullness;
}

static HRESULT PEMsgQ_Destroy(PEMsgQ_t *pMsgQ)
{
	if (pMsgQ == NULL) {
		return E_FAIL;
	}

	if (pMsgQ->pMsg)
		kfree(pMsgQ->pMsg);

	return S_OK;
}

static HRESULT PEMsgQ_Init(PEMsgQ_t *pPEMsgQ, UINT q_length)
{
	pPEMsgQ->q_length = q_length;
	pPEMsgQ->rd_number = pPEMsgQ->wr_number = 0;
	pPEMsgQ->pMsg =
	    (MV_CC_MSG_t *) kmalloc(sizeof(MV_CC_MSG_t) * q_length, GFP_ATOMIC);

	if (pPEMsgQ->pMsg == NULL) {
		return E_OUTOFMEMORY;
	}

	pPEMsgQ->Destroy = PEMsgQ_Destroy;
	pPEMsgQ->Add = PEMsgQ_Add;
	pPEMsgQ->ReadTry = PEMsgQ_ReadTry;
	pPEMsgQ->ReadFin = PEMsgQ_ReadFinish;
//    pPEMsgQ->Fullness =   PEMsgQ_Fullness;

	return S_OK;
}

#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)

#if CONFIG_VIP_ISR_MSGQ
// MSGQ for VIP
#define VIP_ISR_MSGQ_SIZE           64
static PEMsgQ_t hPEVIPMsgQ;
#endif

static void pe_vip_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0 };

	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_VIP, &msg);
}
#endif

static void pe_vpp_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0 };
	UINT32 val;

	msg.m_MsgID = VPP_CC_MSG_TYPE_VPP;

	msg.m_Param1 = unused;

	GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14, &val);
	msg.m_Param2 = DEBUG_TIMER_VALUE - val;

	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_VPP, &msg);
}

static void pe_vpp_cec_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0 };
	UINT32 val;

	msg.m_MsgID = VPP_CC_MSG_TYPE_CEC;

	msg.m_Param1 = unused;

	GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14, &val);
	msg.m_Param2 = DEBUG_TIMER_VALUE - val;

	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_VPP, &msg);
}

static void pe_vdec_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0 };

	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_VDEC, &msg);
}

static void MV_VPP_action(struct softirq_action *h)
{
	MV_CC_MSG_t msg = { 0, };

	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_VPP, &msg);
//    pe_trace("ISR conext irq:%d\n", vpp_cpcb0_vbi_int_cnt);
}

static int vbi_bcm_cmd_fullcnt;
static int vde_bcm_cmd_fullcnt;

static void start_vbi_bcm_transaction(int cpcbID)
{
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	unsigned int bcm_sched_cmd[2];

	if (vbi_bcm_info[cpcbID].DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vbi_bcm_info[cpcbID].DmaAddr,
					  (INT) vbi_bcm_info[cpcbID].DmaLen * 8,
					  0, 0, 0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) {
			vbi_bcm_cmd_fullcnt++;
		}
	}
#else
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) vbi_bcm_info[cpcbID].DmaAddr;
	for (cnt = 0; cnt < vbi_bcm_info[cpcbID].DmaLen; cnt++) {
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, (INT) * ptr,
				       (INT) * (ptr + 1), 0, 0, 0, 1, 0, 0);
		ptr += 2;
	}
#endif
	/* invalid vbi_bcm_info */
	vbi_bcm_info[cpcbID].DmaLen = 0;
}

static void start_vde_bcm_transaction(int cpcbID)
{
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	unsigned int bcm_sched_cmd[2];

	if (vde_bcm_info[cpcbID].DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vde_bcm_info[cpcbID].DmaAddr,
					  (INT) vde_bcm_info[cpcbID].DmaLen * 8,
					  0, 0, 0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) {
			vde_bcm_cmd_fullcnt++;
		}
	}
#else
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) vde_bcm_info[cpcbID].DmaAddr;
	for (cnt = 0; cnt < vde_bcm_info[cpcbID].DmaLen; cnt++) {
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, (INT) * ptr,
				       (INT) * (ptr + 1), 0, 0, 0, 1, 0, 0);
		ptr += 2;
	}
#endif
	/* invalid vde_bcm_info */
	vde_bcm_info[cpcbID].DmaLen = 0;
}

static void start_vbi_dma_transaction(int cpcbID)
{
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) dma_info[cpcbID].DmaAddr;
	for (cnt = 0; cnt < dma_info[cpcbID].DmaLen; cnt++) {
		*((volatile int *)*(ptr + 1)) = *ptr;
		ptr += 2;
	}
	/* invalid dma_info */
	dma_info[cpcbID].DmaLen = 0;
}

static void send_bcm_sche_cmd(int q_id)
{
	if ((bcm_sche_cmd[q_id][0] !=0) && (bcm_sche_cmd[q_id][1] !=0))
		BCM_SCHED_PushCmd(q_id, bcm_sche_cmd[q_id], NULL);
}

static void pe_ma_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

	msg.m_MsgID = 1 << avioDhubChMap_ag_MA0_R;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AUD, &msg);

}

static void pe_sa_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	msg.m_MsgID = 1 << avioDhubChMap_ag_SA_R;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AUD, &msg);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
}

static void pe_spdif_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	msg.m_MsgID = 1 << avioDhubChMap_ag_SPDIF_R;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AUD, &msg);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
}

static void pe_aip_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
	msg.m_MsgID = 1 << avioDhubChMap_vip_MIC0_W;
#else
	msg.m_MsgID = 1 << avioDhubChMap_ag_MIC_W;
#endif
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AIP, &msg);
}

static void pe_hdmi_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

	if(!disable_hdmi_int_msg)
	{
		msg.m_MsgID = 1 << avioDhubChMap_vpp_HDMI_R;
		MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AUD, &msg);
	}
}

static void pe_app_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	msg.m_MsgID = 1 << avioDhubSemMap_ag_app_intr2;
#else
	msg.m_MsgID = 1 << avioDhubChMap_ag_APPDAT_W;
#endif
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AUD, &msg);
}

static void pe_zsp_do_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

	msg.m_MsgID = 1 << ADSP_ZSPINT2Soc_IRQ0;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_ZSP, &msg);
}

static void pe_pg_dhub_done_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	msg.m_MsgID = 1 << avioDhubChMap_ag_PG_ENG_W;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_RLE, &msg);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
}

static void pe_rle_do_err_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

	msg.m_MsgID = 1 << avioDhubSemMap_ag_spu_intr0;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_RLE, &msg);
}

static void pe_rle_do_done_tasklet(unsigned long unused)
{
	MV_CC_MSG_t msg = { 0, };

	msg.m_MsgID = 1 << avioDhubSemMap_ag_spu_intr1;
	MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_RLE, &msg);
}

static irqreturn_t pe_devices_vpp_cec_isr(int irq, void *dev_id)
{
	UINT16 value = 0;
	UINT16 reg = 0;
	INT intr;
	INT val;
	INT i;
	INT dptr_len = 0;
	HRESULT ret = S_OK;
	//printk("cec intr recvd\n");
#if (BERLIN_CHIP_VERSION < BERLIN_BG2_A0)
	GA_REG_WORD32_READ(SM_APB_GPIO0_BASE + APB_GPIO_INTSTATUS, &val);
	if (val & 0x10)		// check gpio status for cec intr
	{
#endif
#if CONFIG_VPP_IOCTL_MSG
		GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14,
				   &vpp_intr_timestamp);
		vpp_intr_timestamp = DEBUG_TIMER_VALUE - vpp_intr_timestamp;
#endif

		// Read CEC status register
		GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
				 (CEC_INTR_STATUS0_REG_ADDR << 2), &value);
		reg = (UINT16) value;
		GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
				 (CEC_INTR_STATUS1_REG_ADDR << 2), &value);
		reg |= ((UINT16) value << 8);
		// Clear CEC interrupt
		if (reg & BE_CEC_INTR_TX_FAIL) {
			intr = BE_CEC_INTR_TX_FAIL;
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					  (CEC_RDY_ADDR << 2),
					  0x0);
#endif
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					 (CEC_INTR_ENABLE0_REG_ADDR << 2),
					 &value);
			value &= ~(intr & 0x00ff);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					  (CEC_INTR_ENABLE0_REG_ADDR << 2),
					  value);
	//printk("cec intr recvd tx failed\n");
		}
		if (reg & BE_CEC_INTR_TX_COMPLETE) {
			intr = BE_CEC_INTR_TX_COMPLETE;
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					 (CEC_INTR_ENABLE0_REG_ADDR << 2),
					 &value);
			value &= ~(intr & 0x00ff);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					  (CEC_INTR_ENABLE0_REG_ADDR << 2),
					  value);
	//printk("cec intr recvd tx complete \n");
		}
		if (reg & BE_CEC_INTR_RX_FAIL) {
			intr = BE_CEC_INTR_RX_FAIL;
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					 (CEC_INTR_ENABLE0_REG_ADDR << 2),
					 &value);
			value &= ~(intr & 0x00ff);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					  (CEC_INTR_ENABLE0_REG_ADDR << 2),
					  value);
		}
		if (reg & BE_CEC_INTR_RX_COMPLETE) {
			intr = BE_CEC_INTR_RX_COMPLETE;
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					(CEC_INTR_ENABLE0_REG_ADDR << 2),
					&value);
			value &= ~(intr & 0x00ff);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					(CEC_INTR_ENABLE0_REG_ADDR << 2),
					value);
			// read cec mesg from rx buffer
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					(CEC_RX_FIFO_DPTR << 2),
					&dptr_len);
			rx_buf.len = dptr_len;
			for (i = 0; i < dptr_len; i++) {
				GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
						(CEC_RX_BUF_READ_REG_ADDR << 2),
						&rx_buf.buf[i]);
				GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
						(CEC_TOGGLE_FOR_READ_REG_ADDR << 2),
						0x01);
			}
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					(CEC_RX_RDY_ADDR << 2),
					0x00);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					(CEC_RX_RDY_ADDR << 2),
					0x01);
			GA_REG_BYTE_READ(SOC_SM_CEC_BASE +
					(CEC_INTR_ENABLE0_REG_ADDR << 2),
					&value);
			value |= (intr & 0x00ff);
			GA_REG_BYTE_WRITE(SOC_SM_CEC_BASE +
					(CEC_INTR_ENABLE0_REG_ADDR << 2),
					value);
		}
#if (BERLIN_CHIP_VERSION < BERLIN_BG2_A0)
		//Clear GPIO interrupt
		GA_REG_WORD32_READ(SM_APB_GPIO0_BASE + APB_GPIO_PORTA_EOI, &val);	// will add macro soon
		val |= 0x10;	// clear 4 bit
		GA_REG_WORD32_WRITE(SM_APB_GPIO0_BASE + APB_GPIO_PORTA_EOI, val);	// will add macro soon
#endif
		// schedule tasklet with intr status as param
#if CONFIG_VPP_IOCTL_MSG
#if CONFIG_VPP_ISR_MSGQ
		{
			MV_CC_MSG_t msg =
			    { VPP_CC_MSG_TYPE_CEC, reg, vpp_intr_timestamp };
			spin_lock(&msgQ_spinlock);
			ret = PEMsgQ_Add(&hPEMsgQ, &msg);
			spin_unlock(&msgQ_spinlock);
			if (ret != S_OK)
			{
				return IRQ_HANDLED;
			}

		}
#else
		vpp_instat = reg;
#endif
		up(&vpp_sem);
#else
		pe_vpp_cec_tasklet.data = reg;	// bug fix puneet
		tasklet_hi_schedule(&pe_vpp_cec_tasklet);
#endif

		//pe_vpp_cec_tasklet.data = reg;
		//tasklet_hi_schedule(&pe_vpp_cec_tasklet);
		return IRQ_HANDLED;
#if (BERLIN_CHIP_VERSION < BERLIN_BG2_A0)
	}
	return IRQ_NONE;
#endif
}

#ifdef CONFIG_IRQ_LATENCY_PROFILE

typedef struct pe_irq_profiler {
	unsigned long long vppCPCB0_intr_curr;
	unsigned long long vppCPCB0_intr_last;
	unsigned long long vpp_task_sched_last;
	unsigned long long vpp_isr_start;

	unsigned long long vpp_isr_end;
	unsigned long vpp_isr_time_last;

	unsigned long vpp_isr_time_max;
	unsigned long vpp_isr_instat_max;

	INT vpp_isr_last_instat;

} pe_irq_profiler_t;

static pe_irq_profiler_t pe_irq_profiler;

#endif

static atomic_t vpp_isr_msg_err_cnt = ATOMIC_INIT(0);

static irqreturn_t pe_devices_vpp_isr(int irq, void *dev_id)
{
	INT instat;
	HDL_semaphore *pSemHandle;
	INT vpp_intr = 0;
	INT instat_used = 0;
	HRESULT ret = S_OK;
	/* disable interrupt */
#if CONFIG_VPP_IOCTL_MSG
	GA_REG_WORD32_READ(0xf7e82C00 + 0x04 + 7 * 0x14, &vpp_intr_timestamp);
	vpp_intr_timestamp = DEBUG_TIMER_VALUE - vpp_intr_timestamp;
#endif

#ifdef CONFIG_IRQ_LATENCY_PROFILE
	pe_irq_profiler.vpp_isr_start = cpu_clock(smp_processor_id());
#endif

	/* VPP interrupt handling  */
	pSemHandle = dhub_semaphore(&VPP_dhubHandle.dhub);
	instat = semaphore_chk_full(pSemHandle, -1);

	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppCPCB0_intr])
#endif
           ) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB0_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB0_intr);

#ifdef CONFIG_IRQ_LATENCY_PROFILE
		pe_irq_profiler.vppCPCB0_intr_curr =
		    cpu_clock(smp_processor_id());
#endif
		/* CPCB-0 interrupt */
		vpp_cpcb0_vbi_int_cnt++;
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
		semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB0_intr);

		/* Clear the bits for CPCB0 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT4_intr)) {
			semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT4_intr, 1);
			semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT4_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT4_intr);
		}

		start_vbi_dma_transaction(0);
		start_vbi_bcm_transaction(0);
		send_bcm_sche_cmd(BCM_SCHED_Q0);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB1_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppCPCB1_intr])
#endif
           ) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB1_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB1_intr);

		/* CPCB-1 interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB1_intr, 1);
		semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB1_intr);

		/* Clear the bits for CPCB1 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT5_intr)) {
			semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT5_intr, 1);
			semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT5_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT5_intr);
		}
		start_vbi_dma_transaction(1);
		start_vbi_bcm_transaction(1);
		send_bcm_sche_cmd(BCM_SCHED_Q1);
	}


	if (bTST(instat, avioDhubSemMap_vpp_vppCPCB2_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppCPCB2_intr])
#endif
           ) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppCPCB2_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppCPCB2_intr);

		/* CPCB-2 interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB2_intr, 1);
		semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vpp_vppCPCB2_intr);

		/* Clear the bits for CPCB2 VDE interrupt */
		if (bTST(instat, avioDhubSemMap_vpp_vppOUT6_intr)) {
			semaphore_pop(pSemHandle,
				      avioDhubSemMap_vpp_vppOUT6_intr, 1);
			semaphore_clr_full(pSemHandle,
					   avioDhubSemMap_vpp_vppOUT6_intr);
			bCLR(instat, avioDhubSemMap_vpp_vppOUT6_intr);
		}

		start_vbi_dma_transaction(2);
		start_vbi_bcm_transaction(2);
		send_bcm_sche_cmd(BCM_SCHED_Q2);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT4_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT4_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT4_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT4_intr);

		/* CPCB-0 VDE interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT4_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT4_intr);

		start_vde_bcm_transaction(0);
		send_bcm_sche_cmd(BCM_SCHED_Q3);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT5_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT5_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT5_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT5_intr);

		/* CPCB-1 VDE interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT5_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT5_intr);

		start_vde_bcm_transaction(1);
		send_bcm_sche_cmd(BCM_SCHED_Q4);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT6_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT6_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT6_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT6_intr);

		/* CPCB-2 VDE interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT6_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT6_intr);

		start_vde_bcm_transaction(2);
		send_bcm_sche_cmd(BCM_SCHED_Q5);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT3_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT3_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT3_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT3_intr);

		/* VOUT3 interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT3_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT3_intr);
	}

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	if (bTST(instat, avioDhubSemMap_vpp_CH10_intr)
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
	if (bTST(instat, avioDhubSemMap_vpp_CH7_intr)
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#ifdef NEW_ISR
                &&
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
                (vpp_intr_status[avioDhubSemMap_vpp_CH10_intr])
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
                (vpp_intr_status[avioDhubSemMap_vpp_CH7_intr])
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#endif
		) {
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		bSET(instat_used, avioDhubSemMap_vpp_CH10_intr);
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
		bSET(instat_used, avioDhubSemMap_vpp_CH7_intr);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
		/* HDMI audio interrupt */
		vpp_hdmi_audio_int_cnt++;
		/* clear interrupt */
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_CH10_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_CH10_intr);
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_CH7_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_CH7_intr);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
		aout_resume_cmd(HDMI_PATH);
		tasklet_hi_schedule(&pe_hdmi_tasklet);
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		bCLR(instat, avioDhubSemMap_vpp_CH10_intr);
#else /* (BERLIN_CHIP_VERSION == BERLIN_BG2CD_A0) */
		bCLR(instat, avioDhubSemMap_vpp_CH7_intr);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT0_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT0_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT0_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT0_intr);

		/* VOUT0 interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT0_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT0_intr);
	}

	if (bTST(instat, avioDhubSemMap_vpp_vppOUT1_intr)
#ifdef NEW_ISR
                &&
                (vpp_intr_status[avioDhubSemMap_vpp_vppOUT1_intr])
#endif
		) {
#ifdef NEW_ISR
		vpp_intr |= bSETMASK(avioDhubSemMap_vpp_vppOUT1_intr);
#else
		vpp_intr = 1;
#endif
		bSET(instat_used, avioDhubSemMap_vpp_vppOUT1_intr);

		/* VOUT1 interrupt */
		/* clear interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppOUT1_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppOUT1_intr);
	}
#ifdef VPP_DBG
//    pe_trace("ISR instat:%x\n", instat);
#endif

	if (vpp_intr) {
#if CONFIG_VPP_IOCTL_MSG

#if CONFIG_VPP_ISR_MSGQ
		MV_CC_MSG_t msg =
		    { VPP_CC_MSG_TYPE_VPP,
#ifdef NEW_ISR
			vpp_intr,
#else
			instat,
#endif
			vpp_intr_timestamp };
 		spin_lock(&msgQ_spinlock);
		ret = PEMsgQ_Add(&hPEMsgQ, &msg);
		spin_unlock(&msgQ_spinlock);
		if(ret != S_OK) {
			if (!atomic_read(&vpp_isr_msg_err_cnt)) {
				pe_error(" E/[vpp isr] MsgQ full, task deadlock or segment fault\n");
			}
			atomic_inc(&vpp_isr_msg_err_cnt);
			return IRQ_HANDLED;
		}

#else
		vpp_instat = instat;
#endif
		up(&vpp_sem);
#else
		pe_vpp_tasklet.data = instat;
		tasklet_hi_schedule(&pe_vpp_tasklet);
#endif

#ifdef CONFIG_IRQ_LATENCY_PROFILE

		pe_irq_profiler.vpp_isr_end = cpu_clock(smp_processor_id());
		unsigned long isr_time =
		    pe_irq_profiler.vpp_isr_end - pe_irq_profiler.vpp_isr_start;
		int32_t jitter = 0;
		isr_time /= 1000;

		if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)) {
			if (pe_irq_profiler.vppCPCB0_intr_last) {
				jitter =
				    (int64_t) pe_irq_profiler.
				    vppCPCB0_intr_curr -
				    (int64_t) pe_irq_profiler.
				    vppCPCB0_intr_last;

				//nanosec_rem = do_div(interval, 1000000000);
				// transform to us unit
				jitter /= 1000;
				jitter -= 16667;
			}
			pe_irq_profiler.vppCPCB0_intr_last =
			    pe_irq_profiler.vppCPCB0_intr_curr;
		}

		if ((jitter > 670) || (jitter < -670) || (isr_time > 1000)) {
			pe_trace
			    (" W/[vpp isr] jitter:%6d > +-670 us, instat:0x%x last_instat:0x%0x max_instat:0x%0x, isr_time:%d us last:%d max:%d \n",
			     jitter, instat_used,
			     pe_irq_profiler.vpp_isr_last_instat,
			     pe_irq_profiler.vpp_isr_instat_max, isr_time,
			     pe_irq_profiler.vpp_isr_time_last,
			     pe_irq_profiler.vpp_isr_time_max);
		}

		pe_irq_profiler.vpp_isr_last_instat = instat_used;
		pe_irq_profiler.vpp_isr_time_last = isr_time;

		if (isr_time > pe_irq_profiler.vpp_isr_time_max) {
			pe_irq_profiler.vpp_isr_time_max = isr_time;
			pe_irq_profiler.vpp_isr_instat_max = instat_used;
		}
#endif

#if CONFIG_VPP_ISR_MSGQ
/*
        INT fullness = PEMsgQ_Fullness(&hPEMsgQ);

        if ( (!atomic_read(&vpp_isr_msg_err_cnt)) && fullness > 1)
            pe_trace(" W/[vpp isr] message queued:%d, task lag\n", fullness);
*/
#endif
	}
#ifdef CONFIG_IRQ_LATENCY_PROFILE
	else {
		pe_irq_profiler.vpp_isr_end = cpu_clock(smp_processor_id());
		unsigned long isr_time =
		    pe_irq_profiler.vpp_isr_end - pe_irq_profiler.vpp_isr_start;
		isr_time /= 1000;

		if (isr_time > 1000) {
			pe_trace("###isr_time:%d us instat:%x last:%x\n",
				 isr_time, vpp_intr, instat,
				 pe_irq_profiler.vpp_isr_last_instat);
		}

		pe_irq_profiler.vpp_isr_time_last = isr_time;
	}
#endif

	return IRQ_HANDLED;
}

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
static void start_vip_vbi_bcm(void)
{
	unsigned int bcm_sched_cmd[2];

	if (vip_vbi_info.DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vip_vbi_info.DmaAddr,
					  (INT) vip_vbi_info.DmaLen * 8, 0, 0,
					  0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) ;
	}

	/* invalid vbi_bcm_info */
	vip_vbi_info.DmaLen = 0;
}
#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
static void start_vip_dvi_bcm(void)
{
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	unsigned int bcm_sched_cmd[2];

	if (vip_dma_info.DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vip_dma_info.DmaAddr,
					  (INT) vip_dma_info.DmaLen * 8, 0, 0,
					  0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) ;
	}
#else
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) vip_dma_info.DmaAddr;
	for (cnt = 0; cnt < vip_dma_info.DmaLen; cnt++) {
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, (INT) * ptr,
				       (INT) * (ptr + 1), 0, 0, 0, 1, 0, 0);
		ptr += 2;
	}
#endif

	/* invalid vbi_bcm_info */
	vip_dma_info.DmaLen = 0;
}

static void start_vip_sd_wr_bcm(void)
{
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	unsigned int bcm_sched_cmd[2];

	if (vip_sd_wr_info.DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vip_sd_wr_info.DmaAddr,
					  (INT) vip_sd_wr_info.DmaLen * 8, 0, 0,
					  0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) ;
	}
#else
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) vip_sd_wr_info.DmaAddr;
	for (cnt = 0; cnt < vip_sd_wr_info.DmaLen; cnt++) {
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, (INT) * ptr,
				       (INT) * (ptr + 1), 0, 0, 0, 1, 0, 0);
		ptr += 2;
	}
#endif

	/* invalid vbi_bcm_info */
	vip_sd_wr_info.DmaLen = 0;
}

static void start_vip_sd_rd_bcm(void)
{
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	unsigned int bcm_sched_cmd[2];

	if (vip_sd_rd_info.DmaLen) {
		dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub),
					  avioDhubChMap_vpp_BCM_R,
					  (INT) vip_sd_rd_info.DmaAddr,
					  (INT) vip_sd_rd_info.DmaLen * 8, 0, 0,
					  0, 1, bcm_sched_cmd);
		while (!BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL)) ;
	}
#else
	UINT32 cnt;
	UINT32 *ptr;

	ptr = (UINT32 *) vip_sd_rd_info.DmaAddr;
	for (cnt = 0; cnt < vip_sd_rd_info.DmaLen; cnt++) {
		//FIXME: does this use Q12?
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, (INT) * ptr,
				       (INT) * (ptr + 1), 0, 0, 0, 1, 0, 0);
		ptr += 2;
	}
#endif

	/* invalid vbi_bcm_info */
	vip_sd_rd_info.DmaLen = 0;
}

static irqreturn_t pe_devices_vip_isr(int irq, void *dev_id)
{
	INT instat;
	HDL_semaphore *pSemHandle;
	INT vip_intr = 0;
	INT chanId;

	/* VIP interrupt handling  */
	pSemHandle = dhub_semaphore(&VIP_dhubHandle.dhub);
	instat = semaphore_chk_full(pSemHandle, -1);

	if (bTST(instat, avioDhubSemMap_vip_wre_event_intr)
#ifdef NEW_ISR
                &&
                (vip_intr_status[avioDhubSemMap_vip_wre_event_intr])
#endif
		) {
		/* SD decoder write enable interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vip_wre_event_intr, 1);
		semaphore_clr_full(pSemHandle,
				   avioDhubSemMap_vip_wre_event_intr);

#ifdef NEW_ISR
		vip_intr |= bSETMASK(avioDhubSemMap_vip_wre_event_intr);
#else
		vip_intr = 1;
#endif
	}

	if (bTST(instat, avioDhubSemMap_vip_dvi_vde_intr)
#ifdef NEW_ISR
                &&
                (vip_intr_status[avioDhubSemMap_vip_dvi_vde_intr])
#endif
		) {
		/* DVI VDE interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vip_dvi_vde_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vip_dvi_vde_intr);

		start_vip_dvi_bcm();
#ifdef NEW_ISR
		vip_intr |= bSETMASK(avioDhubSemMap_vip_dvi_vde_intr);
#else
		vip_intr = 1;
#endif
	}
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
	if (bTST(instat, avioDhubSemMap_vip_vbi_vde_intr)
#ifdef NEW_ISR
                &&
                (vip_intr_status[avioDhubSemMap_vip_vbi_vde_intr])
#endif
		) {
		/* VBI VDE interrupt */
		semaphore_pop(pSemHandle, avioDhubSemMap_vip_vbi_vde_intr, 1);
		semaphore_clr_full(pSemHandle, avioDhubSemMap_vip_vbi_vde_intr);

		start_vip_vbi_bcm();
#ifdef NEW_ISR
		vip_intr |= bSETMASK(avioDhubSemMap_vip_vbi_vde_intr);
#else
		vip_intr = 1;
#endif
	}

	for (chanId = avioDhubChMap_vip_MIC0_W;
	     chanId <= avioDhubChMap_vip_MIC3_W; chanId++) {
		if (bTST(instat, chanId)) {
			semaphore_pop(pSemHandle, chanId, 1);
			semaphore_clr_full(pSemHandle, chanId);
			if (chanId == avioDhubChMap_vip_MIC0_W) {
				aip_resume_cmd();
				tasklet_hi_schedule(&pe_aip_tasklet);
			}
		}
	}
#endif

	if (vip_intr) {
#if CONFIG_VIP_IOCTL_MSG
#if CONFIG_VIP_ISR_MSGQ
		MV_CC_MSG_t msg =
		    { /*VPP_CC_MSG_TYPE_VPP */ 0,
#ifdef NEW_ISR
                        vip_intr,
#else
                        instat,
#endif
                        vip_intr_timestamp };
		PEMsgQ_Add(&hPEVIPMsgQ, &msg);
#endif
		up(&vip_sem);
#else
		tasklet_hi_schedule(&pe_vip_tasklet);
#endif
	}

	return IRQ_HANDLED;
}
#endif

static irqreturn_t pe_devices_vdec_isr(int irq, void *dev_id)
{
	/* disable interrupt */
	disable_irq_nosync(irq);

#if CONFIG_VDEC_UNBLC_IRQ_FIX
	vdec_int_cnt++;
#endif

	tasklet_hi_schedule(&pe_vdec_tasklet);

	return IRQ_HANDLED;
}

static void *AoutFifoGetKernelRdDMAInfo(AOUT_PATH_CMD_FIFO *p_aout_cmd_fifo,
					int pair)
{
	void *pHandle;
	pHandle =
	    &(p_aout_cmd_fifo->
	      aout_dma_info[pair][p_aout_cmd_fifo->kernel_rd_offset]);
	return pHandle;
}

static void AoutFifoKernelRdUpdate(AOUT_PATH_CMD_FIFO *p_aout_cmd_fifo,
				   int adv)
{
	p_aout_cmd_fifo->kernel_rd_offset += adv;
	if (p_aout_cmd_fifo->kernel_rd_offset >= p_aout_cmd_fifo->size)
		p_aout_cmd_fifo->kernel_rd_offset -= p_aout_cmd_fifo->size;
	return;
}

static int AoutFifoCheckKernelFullness(AOUT_PATH_CMD_FIFO *p_aout_cmd_fifo)
{
	int full;
	full = p_aout_cmd_fifo->wr_offset - p_aout_cmd_fifo->kernel_rd_offset;
	if (full < 0)
		full += p_aout_cmd_fifo->size;
	return full;
}

static void aout_disable_int_msg_update(int *aout_info)
{
	int *p = aout_info;
	if(*p == HDMI_PATH)
		disable_hdmi_int_msg = *(p+1);

	return;	
}

static void aout_start_cmd(int *aout_info)
{
	int *p = aout_info;
	int i, chanId;
	AOUT_DMA_INFO *p_dma_info;

	if (*p == MULTI_PATH) {
		p_ma_fifo =
		    (AOUT_PATH_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
		for (i = 0; i < 4; i++) {
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_ma_fifo, i);
			chanId = pri_audio_chanId[i];
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       i == 0 ? 1 : 0, 0, 0);
		}
	} else if (*p == LoRo_PATH) {
		p_sa_fifo =
		    (AOUT_PATH_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(p_sa_fifo, 0);
		chanId = avioDhubChMap_ag_SA_R;
		dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
	} else if (*p == SPDIF_PATH) {
		p_spdif_fifo =
		    (AOUT_PATH_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(p_spdif_fifo,
								 0);
		chanId = avioDhubChMap_ag_SPDIF_R;
		dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
	} else if (*p == HDMI_PATH) {
		p_hdmi_fifo =
		    (AOUT_PATH_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
		p_dma_info =
		    (AOUT_DMA_INFO *) AoutFifoGetKernelRdDMAInfo(p_hdmi_fifo,
								 0);
		chanId = avioDhubChMap_vpp_HDMI_R;
		dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
				       p_dma_info->addr0, p_dma_info->size0, 0,
				       0, 0, 1, 0, 0);
	}
}

static void aout_resume_cmd(int path_id)
{
	AOUT_DMA_INFO *p_dma_info;
	unsigned int i, chanId;

	if (path_id == MULTI_PATH) {
		if (!p_ma_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_ma_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_ma_fifo)) {
			p_ma_fifo->fifo_underflow = 0;
			for (i = 0; i < 4; i++) {
				p_dma_info =
				    (AOUT_DMA_INFO *)
				    AoutFifoGetKernelRdDMAInfo(p_ma_fifo, i);
				chanId = pri_audio_chanId[i];
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr0,
						       p_dma_info->size0, 0, 0,
						       0,
						       (p_dma_info->size1 == 0
							&& i == 0) ? 1 : 0, 0,
						       0);
				if (p_dma_info->size1)
					dhub_channel_write_cmd(&AG_dhubHandle.
							       dhub, chanId,
							       p_dma_info->
							       addr1,
							       p_dma_info->
							       size1, 0, 0, 0,
							       (i == 0) ? 1 : 0,
							       0, 0);
			}
		} else {
			p_ma_fifo->fifo_underflow = 1;
			for (i = 0; i < 4; i++) {
				chanId = pri_audio_chanId[i];
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_ma_fifo->zero_buffer,
						       p_ma_fifo->
						       zero_buffer_size, 0, 0,
						       0, i == 0 ? 1 : 0, 0, 0);
			}
		}
	} else if (path_id == LoRo_PATH) {
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		if (!p_sa_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_sa_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_sa_fifo)) {
			p_sa_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_sa_fifo, 0);
			chanId = avioDhubChMap_ag_SA_R;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
		} else {
			p_sa_fifo->fifo_underflow = 1;
			chanId = avioDhubChMap_ag_SA_R;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_sa_fifo->zero_buffer,
					       p_sa_fifo->zero_buffer_size, 0,
					       0, 0, 1, 0, 0);
		}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
	} else if (path_id == SPDIF_PATH) {
#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
		if (!p_spdif_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_spdif_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_spdif_fifo)) {
			p_spdif_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_spdif_fifo, 0);
			chanId = avioDhubChMap_ag_SPDIF_R;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
		} else {
			p_spdif_fifo->fifo_underflow = 1;
			chanId = avioDhubChMap_ag_SPDIF_R;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_spdif_fifo->zero_buffer,
					       p_spdif_fifo->zero_buffer_size,
					       0, 0, 0, 1, 0, 0);
		}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
	} else if (path_id == HDMI_PATH) {
		if (!p_hdmi_fifo->fifo_underflow)
			AoutFifoKernelRdUpdate(p_hdmi_fifo, 1);

		if (AoutFifoCheckKernelFullness(p_hdmi_fifo)) {
			p_hdmi_fifo->fifo_underflow = 0;
			p_dma_info =
			    (AOUT_DMA_INFO *)
			    AoutFifoGetKernelRdDMAInfo(p_hdmi_fifo, 0);
			chanId = avioDhubChMap_vpp_HDMI_R;
			dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
					       p_dma_info->addr0,
					       p_dma_info->size0, 0, 0, 0,
					       p_dma_info->size1 ? 0 : 1, 0, 0);
			if (p_dma_info->size1)
				dhub_channel_write_cmd(&VPP_dhubHandle.dhub,
						       chanId,
						       p_dma_info->addr1,
						       p_dma_info->size1, 0, 0,
						       0, 1, 0, 0);
		} else {
			p_hdmi_fifo->fifo_underflow = 1;
			chanId = avioDhubChMap_vpp_HDMI_R;
			dhub_channel_write_cmd(&VPP_dhubHandle.dhub, chanId,
					       p_hdmi_fifo->zero_buffer,
					       p_hdmi_fifo->zero_buffer_size, 0,
					       0, 0, 1, 0, 0);
		}
	}
	return;
}

static void *AIPFifoGetKernelPreRdDMAInfo(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo, int pair)
{
	void *pHandle;
        pHandle = &(p_aip_cmd_fifo->aip_dma_cmd[pair][p_aip_cmd_fifo->kernel_pre_rd_offset]);
        return pHandle;
}

static void AIPFifoKernelPreRdUpdate(AIP_DMA_CMD_FIFO * p_aip_cmd_fifo, int adv)
{
        int tmp;
        tmp = p_aip_cmd_fifo->kernel_pre_rd_offset + adv;
        p_aip_cmd_fifo->kernel_pre_rd_offset = tmp >= p_aip_cmd_fifo->size ?
            tmp - p_aip_cmd_fifo->size : tmp;
        return;
}

static void AIPFifoKernelRdUpdate(AIP_DMA_CMD_FIFO *p_aip_cmd_fifo, int adv)
{
	int tmp;
	tmp = p_aip_cmd_fifo->kernel_rd_offset + adv;
	p_aip_cmd_fifo->kernel_rd_offset = tmp >= p_aip_cmd_fifo->size ?
	    tmp - p_aip_cmd_fifo->size : tmp;
	return;
}

static int AIPFifoCheckKernelFullness(AIP_DMA_CMD_FIFO *p_aip_cmd_fifo)
{
	int full;
	full = p_aip_cmd_fifo->wr_offset - p_aip_cmd_fifo->kernel_pre_rd_offset;
	if (full < 0)
		full += p_aip_cmd_fifo->size;
	return full;
}

static void aip_start_cmd(int *aip_info)
{
	int *p = aip_info;
	int chanId, pair;
	AIP_DMA_CMD *p_dma_cmd;

	if (*p == 1) {
		aip_i2s_pair = 1;
		p_aip_fifo =
            (AIP_DMA_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
		p_dma_cmd =
            (AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(p_aip_fifo, 0);

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
		chanId = avioDhubChMap_vip_MIC0_W;
		dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
				       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
				       0, 1, 0, 0);
#else
		chanId = avioDhubChMap_ag_MIC_W;
		dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
				       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
				       0, 1, 0, 0);
#endif
		AIPFifoKernelPreRdUpdate(p_aip_fifo, 1);

		// push 2nd dHub command
		p_dma_cmd = (AIP_DMA_CMD *) AIPFifoGetKernelPreRdDMAInfo(p_aip_fifo, 0);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
                chanId = avioDhubChMap_vip_MIC0_W;
                dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
                                       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
                                       0, 1, 0, 0);
#else
                chanId = avioDhubChMap_ag_MIC_W;
                dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
                                       p_dma_cmd->addr0, p_dma_cmd->size0, 0, 0,
                                       0, 1, 0, 0);
#endif
		AIPFifoKernelPreRdUpdate(p_aip_fifo, 1);
	} else if (*p == 4) {
		/* 4 I2S will be introduced since BG2 A0 */
		aip_i2s_pair = 4;
		p_aip_fifo = (AIP_DMA_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*(p + 1));
		for (pair = 0; pair < 4; pair++) {
			p_dma_cmd =
                (AIP_DMA_CMD *)AIPFifoGetKernelPreRdDMAInfo(p_aip_fifo, pair);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
			chanId = avioDhubChMap_vip_MIC0_W + pair;
			dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0, 1, 0,
					       0);
#else
			chanId = avioDhubChMap_ag_MIC_W;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0, 1, 0,
					       0);
#endif
		}
		AIPFifoKernelPreRdUpdate(p_aip_fifo, 1);

		// push 2nd dHub command
		for (pair = 0; pair < 4; pair++) {
                        p_dma_cmd = (AIP_DMA_CMD *)AIPFifoGetKernelPreRdDMAInfo(p_aip_fifo, pair);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
                        chanId = avioDhubChMap_vip_MIC0_W + pair;
                        dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
                                               p_dma_cmd->addr0,
                                               p_dma_cmd->size0, 0, 0, 0, 1, 0,
                                               0);
#else
                        chanId = avioDhubChMap_ag_MIC_W;
                        dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
                                               p_dma_cmd->addr0,
                                               p_dma_cmd->size0, 0, 0, 0, 1, 0,
                                               0);
#endif
                }
                AIPFifoKernelPreRdUpdate(p_aip_fifo, 1);
	}
}

static void aip_stop_cmd(void)
{
	return;
}

static void aip_resume_cmd()
{
	AIP_DMA_CMD *p_dma_cmd;
	unsigned int chanId;
	int pair;

	if (!p_aip_fifo->fifo_overflow)
		AIPFifoKernelRdUpdate(p_aip_fifo, 1);

	if (AIPFifoCheckKernelFullness(p_aip_fifo)) {
		p_aip_fifo->fifo_overflow = 0;
		for (pair = 0; pair < aip_i2s_pair; pair++) {
			p_dma_cmd = (AIP_DMA_CMD *)AIPFifoGetKernelPreRdDMAInfo(p_aip_fifo, pair);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
			chanId = avioDhubChMap_vip_MIC0_W + pair;
			dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0,
					       p_dma_cmd->addr1 ? 0 : 1, 0, 0);
			if (p_dma_cmd->addr1) {
				dhub_channel_write_cmd(&VIP_dhubHandle.dhub,
						       chanId, p_dma_cmd->addr1,
						       p_dma_cmd->size1, 0, 0,
						       0, 1, 0, 0);
			}
#else
			chanId = avioDhubChMap_ag_MIC_W;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_dma_cmd->addr0,
					       p_dma_cmd->size0, 0, 0, 0,
					       p_dma_cmd->addr1 ? 0 : 1, 0, 0);
			if (p_dma_cmd->addr1) {
				dhub_channel_write_cmd(&AG_dhubHandle.dhub,
						       chanId, p_dma_cmd->addr1,
						       p_dma_cmd->size1, 0, 0,
						       0, 1, 0, 0);
			}
#endif
		}
		AIPFifoKernelPreRdUpdate(p_aip_fifo, 1);
	} else {
		p_aip_fifo->fifo_overflow = 1;
		p_aip_fifo->fifo_overflow_cnt++;
		for (pair = 0; pair < aip_i2s_pair; pair++) {
			/* FIXME:
			 *chanid should be changed if 4 pair is supported
			 */
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
			chanId = avioDhubChMap_vip_MIC0_W + pair;
			dhub_channel_write_cmd(&VIP_dhubHandle.dhub, chanId,
					       p_aip_fifo->overflow_buffer,
					       p_aip_fifo->overflow_buffer_size,
					       0, 0, 0, 1, 0, 0);
#else
			chanId = avioDhubChMap_ag_MIC_W;
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId,
					       p_aip_fifo->overflow_buffer,
					       p_aip_fifo->overflow_buffer_size,
					       0, 0, 0, 1, 0, 0);
#endif
		}
	}
}

static int HwAPPFifoCheckKernelFullness(HWAPP_CMD_FIFO *p_app_cmd_fifo)
{
	int full;
	full = p_app_cmd_fifo->wr_offset - p_app_cmd_fifo->kernel_rd_offset;
	if (full < 0)
		full += p_app_cmd_fifo->size;
	return full;
}

static void *HwAPPFifoGetKernelCoefRdCmdBuf(HWAPP_CMD_FIFO *p_app_cmd_fifo)
{
	void *pHandle;
	pHandle = &(p_app_cmd_fifo->coef_cmd[p_app_cmd_fifo->kernel_rd_offset]);
	return pHandle;
}

static void *HwAPPFifoGetKernelDataRdCmdBuf(HWAPP_CMD_FIFO *p_app_cmd_fifo)
{
	void *pHandle;
	pHandle = &(p_app_cmd_fifo->data_cmd[p_app_cmd_fifo->kernel_rd_offset]);
	return pHandle;
}

static void HwAPPFifoUpdateIdleFlag(HWAPP_CMD_FIFO *p_app_cmd_fifo, int flag)
{
	p_app_cmd_fifo->kernel_idle = flag;
}

static void HwAPPFifoKernelRdUpdate(HWAPP_CMD_FIFO *p_app_cmd_fifo, int adv)
{
	p_app_cmd_fifo->kernel_rd_offset += adv;
	if (p_app_cmd_fifo->kernel_rd_offset >= p_app_cmd_fifo->size)
		p_app_cmd_fifo->kernel_rd_offset -= p_app_cmd_fifo->size;
}

static void app_start_cmd(HWAPP_CMD_FIFO *p_app_cmd_fifo)
{
	APP_CMD_BUFFER *p_coef_cmd;
	APP_CMD_BUFFER *p_data_cmd;
	unsigned int chanId, PA, cmdSize;

	if (HwAPPFifoCheckKernelFullness(p_app_cmd_fifo)) {
		HwAPPFifoUpdateIdleFlag(p_app_cmd_fifo, 0);
		p_coef_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelCoefRdCmdBuf(p_app_cmd_fifo);
		chanId = avioDhubChMap_ag_APPCMD_R;
		if (p_coef_cmd->cmd_len) {
			PA = p_coef_cmd->cmd_buffer_hw_base;
			cmdSize = p_coef_cmd->cmd_len * sizeof(int);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
		p_data_cmd =
		    (APP_CMD_BUFFER *)
		    HwAPPFifoGetKernelDataRdCmdBuf(p_app_cmd_fifo);
		if (p_data_cmd->cmd_len) {
			PA = p_data_cmd->cmd_buffer_hw_base;
			cmdSize = p_data_cmd->cmd_len * sizeof(int);
			dhub_channel_write_cmd(&AG_dhubHandle.dhub, chanId, PA,
					       cmdSize, 0, 0, 0, 0, 0, 0);
		}
	} else {
		HwAPPFifoUpdateIdleFlag(p_app_cmd_fifo, 1);
	}
}

static void app_resume_cmd(HWAPP_CMD_FIFO *p_app_cmd_fifo)
{
	HwAPPFifoKernelRdUpdate(p_app_cmd_fifo, 1);
	app_start_cmd(p_app_cmd_fifo);
}

static irqreturn_t pe_devices_aout_isr(int irq, void *dev_id)
{
	int instat;
	UNSG32 chanId;
	HDL_semaphore *pSemHandle;

	pSemHandle = dhub_semaphore(&AG_dhubHandle.dhub);
	instat = semaphore_chk_full(pSemHandle, -1);

	for (chanId = avioDhubChMap_ag_MA0_R; chanId <= avioDhubChMap_ag_MA3_R;
	     chanId++) {
		if (bTST(instat, chanId)) {
			semaphore_pop(pSemHandle, chanId, 1);
			semaphore_clr_full(pSemHandle, chanId);
			if (chanId == avioDhubChMap_ag_MA0_R) {
				aout_resume_cmd(MULTI_PATH);
				tasklet_hi_schedule(&pe_ma_tasklet);
			}
		}
	}

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	chanId = avioDhubChMap_ag_SA_R;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aout_resume_cmd(LoRo_PATH);
		tasklet_hi_schedule(&pe_sa_tasklet);
	}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	chanId = avioDhubChMap_ag_SPDIF_R;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aout_resume_cmd(SPDIF_PATH);
		tasklet_hi_schedule(&pe_spdif_tasklet);
	}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */
#if(BERLIN_CHIP_VERSION < BERLIN_BG2_A0)
	chanId = avioDhubChMap_ag_MIC_W;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		aip_resume_cmd();
		tasklet_hi_schedule(&pe_aip_tasklet);
	}
#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	chanId = avioDhubSemMap_ag_app_intr2;
#else
	chanId = avioDhubChMap_ag_APPDAT_W;
#endif
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		app_resume_cmd(p_app_fifo);
		tasklet_hi_schedule(&pe_app_tasklet);
	}

#if (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	chanId = avioDhubChMap_ag_PG_ENG_W;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		tasklet_hi_schedule(&pe_pg_done_tasklet);
	}
#endif /* (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0) */

	chanId = avioDhubSemMap_ag_spu_intr0;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		tasklet_hi_schedule(&pe_rle_err_tasklet);
	}

	chanId = avioDhubSemMap_ag_spu_intr1;
	if (bTST(instat, chanId)) {
		semaphore_pop(pSemHandle, chanId, 1);
		semaphore_clr_full(pSemHandle, chanId);
		tasklet_hi_schedule(&pe_rle_done_tasklet);
	}

	return IRQ_HANDLED;
}

static irqreturn_t pe_devices_zsp_isr(int irq, void *dev_id)
{
	UNSG32 addr, v_id;
	T32ZspInt2Soc_status reg;

	addr = MEMMAP_ZSP_REG_BASE + RA_ZspRegs_Int2Soc + RA_ZspInt2Soc_status;
	GA_REG_WORD32_READ(addr, &(reg.u32));

	addr = MEMMAP_ZSP_REG_BASE + RA_ZspRegs_Int2Soc + RA_ZspInt2Soc_clear;
	v_id = ADSP_ZSPINT2Soc_IRQ0;
	if ((reg.u32) & (1 << v_id)) {
		GA_REG_WORD32_WRITE(addr, v_id);
	}

	tasklet_hi_schedule(&pe_zsp_tasklet);	// this is audio zsp, video zsp interrupt will use another tasklet to post msg.

	return IRQ_HANDLED;
}

#if (BERLIN_CHIP_VERSION >=  BERLIN_BG2_A0)
#define  MEMMAP_VPP_REG_BASE       0xF7F60000
#define  RA_Vpp_HDMI_ctrl    0x100C0
#define  MSK32HDMI_ctrl_DAMP 0x7FF80000
#define  MSK32HDMI_ctrl_EAMP 0x00000FFF

static void pe_power_off_hdmi(void)
{
    UNSG32 addr, value;
    HDL_semaphore *pSemHandle;

    addr = MEMMAP_VPP_REG_BASE + RA_Vpp_HDMI_ctrl;
	  GA_REG_WORD32_READ(addr, &value);
    value &= (~MSK32HDMI_ctrl_DAMP);
    GA_REG_WORD32_WRITE(addr, value);
    addr = MEMMAP_VPP_REG_BASE + RA_Vpp_HDMI_ctrl+ 0x4;
	  GA_REG_WORD32_READ(addr, &value);
    value &= (~MSK32HDMI_ctrl_EAMP);
    GA_REG_WORD32_WRITE(addr, value);
     /* disable CPCB0 interrupt */
#ifdef CONFIG_BERLIN_FASTLOGO
    if(dhub_init_flag)
#endif
    {
        pSemHandle = dhub_semaphore(&VPP_dhubHandle.dhub);
        semaphore_intr_enable(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr,
        0/*empty*/, 0/*full*/, 0/*almost empty*/, 0/*almost full*/, 0/*cpu id*/);
    }
}
#endif

static int pe_reboot_notify_sys(struct notifier_block *this, unsigned long code,
                            void *unused)
{
#if (BERLIN_CHIP_VERSION >=  BERLIN_BG2_A0)
    pe_power_off_hdmi();
#endif
    return NOTIFY_DONE;
}

static struct notifier_block pe_reboot_notifier = {
        .notifier_call = pe_reboot_notify_sys
};


static int pe_device_init(unsigned int cpu_id, void *pHandle)
{
	unsigned int vec_num;
	int err = 0;

	vpp_cpcb0_vbi_int_cnt = 0;
	vpp_hdmi_audio_int_cnt =0;
	err = register_reboot_notifier(&pe_reboot_notifier);
	if(err) return err;
	tasklet_enable(&pe_vpp_tasklet);
	tasklet_enable(&pe_vpp_cec_tasklet);
	tasklet_enable(&pe_vdec_tasklet);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	tasklet_enable(&pe_vip_tasklet);
#endif
	tasklet_enable(&pe_ma_tasklet);
	tasklet_enable(&pe_sa_tasklet);
	tasklet_enable(&pe_spdif_tasklet);
	tasklet_enable(&pe_aip_tasklet);
	tasklet_enable(&pe_hdmi_tasklet);
	tasklet_enable(&pe_app_tasklet);
	tasklet_enable(&pe_zsp_tasklet);
	tasklet_enable(&pe_pg_done_tasklet);
	tasklet_enable(&pe_rle_err_tasklet);
	tasklet_enable(&pe_rle_done_tasklet);

#ifdef CONFIG_BERLIN_FASTLOGO
    /* defer the DhubInitialization to the pe_driver_open() after fastlogo_stop(); */
	pe_cpu_id = cpu_id;
#else
	/* initialize dhub */
	DhubInitialization(cpu_id, VPP_DHUB_BASE, VPP_HBO_SRAM_BASE,
			   &VPP_dhubHandle, VPP_config, VPP_NUM_OF_CHANNELS);
	DhubInitialization(cpu_id, AG_DHUB_BASE, AG_HBO_SRAM_BASE,
			   &AG_dhubHandle, AG_config, AG_NUM_OF_CHANNELS);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2) && (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	DhubInitialization(cpu_id, VIP_DHUB_BASE, VIP_HBO_SRAM_BASE,
			   &VIP_dhubHandle, VIP_config, VIP_NUM_OF_CHANNELS);
#endif
#endif //CONFIG_BERLIN_FASTLOGO

#if CONFIG_VPP_IOCTL_MSG
	sema_init(&vpp_sem, 0);
#endif

#if CONFIG_VPP_ISR_MSGQ
	err = PEMsgQ_Init(&hPEMsgQ, VPP_ISR_MSGQ_SIZE);
	if (unlikely(err != S_OK)) {
		pe_trace("PEMsgQ_Init: falied, err:%8x\n", err);
		unregister_reboot_notifier(&pe_reboot_notifier);
		return err;
	}
#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
#if CONFIG_VIP_IOCTL_MSG
	sema_init(&vip_sem, 0);
#endif
#if CONFIG_VIP_ISR_MSGQ
	err = PEMsgQ_Init(&hPEVIPMsgQ, VIP_ISR_MSGQ_SIZE);
	if (unlikely(err != S_OK)) {
		pe_trace("PEVIPMsgQ_Init: falied, err:%8x\n", err);
		unregister_reboot_notifier(&pe_reboot_notifier);
		return err;
	}
#endif
#endif

	return S_OK;
}

static int pe_device_exit(unsigned int cpu_id, void *pHandle)
{
	int err = 0;
	unregister_reboot_notifier(&pe_reboot_notifier);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
#if CONFIG_VIP_ISR_MSGQ
	err = PEMsgQ_Destroy(&hPEVIPMsgQ);
	if (unlikely(err != S_OK)) {
		pe_trace("vip MsgQ Destroy: falied, err:%8x\n",err);
		return err;
	}
#endif
#endif
#if CONFIG_VPP_ISR_MSGQ
	err = PEMsgQ_Destroy(&hPEMsgQ);
	if (unlikely(err != S_OK)) {
		pe_trace("vpp MsgQ Destroy: falied, err:%8x\n", err);
		return err;
	}
#endif

	tasklet_disable(&pe_vpp_tasklet);
	tasklet_disable(&pe_vpp_cec_tasklet);
	tasklet_disable(&pe_vdec_tasklet);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	tasklet_disable(&pe_vip_tasklet);
#endif
	tasklet_disable(&pe_ma_tasklet);
	tasklet_disable(&pe_sa_tasklet);
	tasklet_disable(&pe_spdif_tasklet);
	tasklet_disable(&pe_aip_tasklet);
	tasklet_disable(&pe_hdmi_tasklet);
	tasklet_disable(&pe_app_tasklet);
	tasklet_disable(&pe_zsp_tasklet);
	tasklet_disable(&pe_pg_done_tasklet);
	tasklet_disable(&pe_rle_err_tasklet);
	tasklet_disable(&pe_rle_done_tasklet);

	return S_OK;
}

/*******************************************************************************
  Module API
  */

#define  MEMMAP_VPP_REG_BASE  0xF7F60000
#define  RA_Vpp_HDMI_ctrl     0x100C0
#define  MSK32HDMI_ctrl_PD_TX 0x0000003C

static int pe_driver_open(struct inode *inode, struct file *filp)
{
	unsigned int vec_num;
	void *pHandle = &pe_dev;
	int err = 0;
    UINT32 val;

#ifdef CONFIG_BERLIN_FASTLOGO
	printk(KERN_NOTICE PE_DEVICE_TAG "drv stop fastlogo\n");
	fastlogo_stop();

    GA_REG_WORD32_READ((MEMMAP_VPP_REG_BASE + RA_Vpp_HDMI_ctrl), &val);
    val = val | MSK32HDMI_ctrl_PD_TX ;
    GA_REG_WORD32_WRITE((MEMMAP_VPP_REG_BASE + RA_Vpp_HDMI_ctrl),val);

	/* initialize dhub */
	dhub_init_flag =1;
	DhubInitialization(pe_cpu_id, VPP_DHUB_BASE, VPP_HBO_SRAM_BASE,
			   &VPP_dhubHandle, VPP_config, VPP_NUM_OF_CHANNELS);
	DhubInitialization(pe_cpu_id, AG_DHUB_BASE, AG_HBO_SRAM_BASE,
			   &AG_dhubHandle, AG_config, AG_NUM_OF_CHANNELS);
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2) && (BERLIN_CHIP_VERSION != BERLIN_BG2CD_A0)
	DhubInitialization(pe_cpu_id, VIP_DHUB_BASE, VIP_HBO_SRAM_BASE,
			   &VIP_dhubHandle, VIP_config, VIP_NUM_OF_CHANNELS);
#endif

#endif

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
		/* register and enable cec interrupt */
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
		vec_num = pe_irqs[IRQ_SM_CEC];
		err =
			request_irq(vec_num, pe_devices_vpp_cec_isr, IRQF_DISABLED,
				"pe_module_vpp_cec", pHandle);
		if (unlikely(err < 0)) {
			pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
			return err;
		}
#else
		vec_num = IRQ_SM_GPIO0;
		err =
			request_irq(vec_num, pe_devices_vpp_cec_isr,
				IRQF_DISABLED | IRQF_SHARED, "pe_module_vpp_cec",
				pHandle);
		if (unlikely(err < 0)) {
			pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
			return err;
		}
#endif
#endif

	/* register and enable VPP ISR */
	vec_num = pe_irqs[IRQ_DHUBINTRAVIO0];
	err = request_irq(vec_num, pe_devices_vpp_isr, IRQF_DISABLED, "pe_module_vpp", pHandle);
	if (unlikely(err < 0)) {
		pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	/* register and enable VIP ISR */
	vec_num = pe_irqs[IRQ_DHUBINTRAVIO2];
	err = request_irq(vec_num, pe_devices_vip_isr, IRQF_DISABLED, "pe_module_vip", pHandle);
	if (unlikely(err < 0)) {
		pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}
#endif

	/* register and enable VDEC ISR */
	vec_num = pe_irqs[IRQ_DHUBINTRVPRO];
	err = request_irq(vec_num, pe_devices_vdec_isr, IRQF_DISABLED, "pe_module_vdec", pHandle);
	if (unlikely(err < 0)) {
		pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}

	/* register and enable audio out ISR */
	vec_num = pe_irqs[IRQ_DHUBINTRAVIO1];
	err = request_irq(vec_num, pe_devices_aout_isr, IRQF_DISABLED, "pe_module_aout", pHandle);
	if (unlikely(err < 0)) {
		pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}

	/* register and enable ZSP ISR */
	vec_num = pe_irqs[IRQ_ZSPINT];
	err = request_irq(vec_num, pe_devices_zsp_isr, IRQF_DISABLED, "pe_module_zsp", pHandle);
	if (unlikely(err < 0))
	{
		pe_trace("vec_num:%5d, err:%8x\n", vec_num, err);
		return err;
	}

	disable_irq(pe_irqs[IRQ_DHUBINTRVPRO]);

	pe_debug("pe_driver_open ok\n");

	return 0;
}

static int pe_driver_release(struct inode *inode, struct file *filp)
{
	void *pHandle = &pe_dev;
	unsigned int vec_num;
	int err = 0;
	/* unregister cec interrupt */
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
#if (BERLIN_CHIP_VERSION >= BERLIN_BG2_A0)
	free_irq(pe_irqs[IRQ_SM_CEC], pHandle);
#else
	free_irq(pe_irqs[IRQ_SM_GPIO0], pHandle);
#endif
#endif

	/* unregister VPP interrupt */
	free_irq(pe_irqs[IRQ_DHUBINTRAVIO0], pHandle);
	/* unregister VIP interrupt */
	free_irq(pe_irqs[IRQ_DHUBINTRAVIO2], pHandle);
	/* unregister VDEC interrupt */
	free_irq(pe_irqs[IRQ_DHUBINTRVPRO], pHandle);
	/* unregister audio out interrupt */
	free_irq(pe_irqs[IRQ_DHUBINTRAVIO1], pHandle);
	/* unregister ZSP interrupt */
	free_irq(pe_irqs[IRQ_ZSPINT], pHandle);

	pe_debug("pe_driver_release ok\n");

	return 0;
}

static long pe_driver_ioctl_unlocked(struct file *filp, unsigned int cmd,
				     unsigned long arg)
{
	VPP_DMA_INFO user_dma_info;
	int bcmbuf_info[2];
	int aout_info[2];
	int app_info[2];
	int aip_info[2];
	unsigned long irqstat, aoutirq, appirq, aipirq;
	unsigned int bcm_sche_cmd_info[3], q_id;

	switch (cmd) {
	case VPP_IOCTL_BCM_SCHE_CMD:
		if (copy_from_user
		    (bcm_sche_cmd_info, (void __user *)arg, 3*sizeof(unsigned int)))
			return -EFAULT;
		q_id = bcm_sche_cmd_info[2];
		if (q_id > BCM_SCHED_Q5) {
			pe_trace("error BCM queue ID = %d\n", q_id);
			return -EFAULT;
		}
		bcm_sche_cmd[q_id][0] = bcm_sche_cmd_info[0];
		bcm_sche_cmd[q_id][1] = bcm_sche_cmd_info[1];
		break;

	case VPP_IOCTL_VBI_DMA_CFGQ:
		if (copy_from_user
		    (&user_dma_info, (void __user *)arg, sizeof(VPP_DMA_INFO)))
			return -EFAULT;

		dma_info[user_dma_info.cpcbID].DmaAddr =
		    (UINT32) MV_SHM_GetCacheVirtAddr(user_dma_info.DmaAddr);
		dma_info[user_dma_info.cpcbID].DmaLen = user_dma_info.DmaLen;
		break;

	case VPP_IOCTL_VBI_BCM_CFGQ:
		if (copy_from_user
		    (&user_dma_info, (void __user *)arg, sizeof(VPP_DMA_INFO)))
			return -EFAULT;

		vbi_bcm_info[user_dma_info.cpcbID].DmaAddr =
		    (UINT32) MV_SHM_GetCachePhysAddr(user_dma_info.DmaAddr);
		vbi_bcm_info[user_dma_info.cpcbID].DmaLen =
		    user_dma_info.DmaLen;
		break;

	case VPP_IOCTL_VDE_BCM_CFGQ:
		if (copy_from_user
		    (&user_dma_info, (void __user *)arg, sizeof(VPP_DMA_INFO)))
			return -EFAULT;

		vde_bcm_info[user_dma_info.cpcbID].DmaAddr =
		    (UINT32) MV_SHM_GetCachePhysAddr(user_dma_info.DmaAddr);
		vde_bcm_info[user_dma_info.cpcbID].DmaLen =
		    user_dma_info.DmaLen;
		break;

	case VPP_IOCTL_GET_MSG:
		{
#if CONFIG_VPP_IOCTL_MSG
			MV_CC_MSG_t msg = { 0 };
			HRESULT rc = S_OK;

			rc = down_interruptible(&vpp_sem);
			if (rc < 0)
				return rc;

#ifdef CONFIG_IRQ_LATENCY_PROFILE
			pe_irq_profiler.vpp_task_sched_last =
			    cpu_clock(smp_processor_id());
#endif
#if CONFIG_VPP_ISR_MSGQ

			// check fullness, clear message queue once.
			// only send latest message to task.
#if 1
			if (PEMsgQ_Fullness(&hPEMsgQ) <= 0) {
				//pe_trace(" E/[vpp isr task]  message queue empty\n");
				return -EFAULT;
			}

			PEMsgQ_DequeueRead(&hPEMsgQ, &msg);

			if (!atomic_read(&vpp_isr_msg_err_cnt)) {
				// msgQ get full, if isr task can run here, reset msgQ
				//fullness--;
				//PEMsgQ_Dequeue(&hPEMsgQ, fullness);
				atomic_set(&vpp_isr_msg_err_cnt, 0);
			}
#else
			rc = PEMsgQ_ReadTry(&hPEMsgQ, &msg);
			if (unlikely(rc != S_OK)) {
				pe_trace("read message queue failed\n");
				return -EFAULT;
			}
			PEMsgQ_ReadFinish(&hPEMsgQ);
#endif
#else
			msg.m_MsgID = VPP_CC_MSG_TYPE_VPP;
			msg.m_Param1 = vpp_instat;
			msg.m_Param2 = vpp_intr_timestamp;
#endif
			if (copy_to_user
			    ((void __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
				return -EFAULT;
			break;
#else
			return -EFAULT;
#endif
		}
	case CEC_IOCTL_RX_MSG_BUF_MSG:// copy cec rx message to user space buffer
		if (copy_to_user
		    ((void __user *)arg, &rx_buf, sizeof(VPP_CEC_RX_MSG_BUF)))
			return -EFAULT;

		return S_OK;
		break;
	case VPP_IOCTL_START_BCM_TRANSACTION:
		if (copy_from_user
		    (bcmbuf_info, (void __user *)arg, 2 * sizeof(int)))
			return -EFAULT;
		spin_lock_irqsave(&bcm_spinlock, irqstat);
		dhub_channel_write_cmd(&(VPP_dhubHandle.dhub),
				       avioDhubChMap_vpp_BCM_R, bcmbuf_info[0],
				       bcmbuf_info[1], 0, 0, 0, 1, 0, 0);
		spin_unlock_irqrestore(&bcm_spinlock, irqstat);
		break;

	case VPP_IOCTL_INTR_MSG:
		//get VPP INTR MASK info
		{
		    INTR_MSG vpp_intr_info = { 0, 0 };

			if (copy_from_user
			    (&vpp_intr_info, (void __user *)arg, sizeof(INTR_MSG)))
				return -EFAULT;

			if (vpp_intr_info.DhubSemMap < MAX_INTR_NUM)
				vpp_intr_status[vpp_intr_info.DhubSemMap] = vpp_intr_info.Enable;
			else
				return -EFAULT;

			break;
		}

#if (BERLIN_CHIP_VERSION >= BERLIN_BG2)
	    /**************************************
             * VIP IOCTL
             **************************************/
	case VIP_IOCTL_GET_MSG:
		{
#if CONFIG_VIP_IOCTL_MSG
			MV_CC_MSG_t msg = { 0 };
			HRESULT rc = S_OK;

			rc = down_interruptible(&vip_sem);
			if (rc < 0)
				return rc;

#if CONFIG_VIP_ISR_MSGQ
			rc = PEMsgQ_ReadTry(&hPEVIPMsgQ, &msg);
			if (unlikely(rc != S_OK)) {
				pe_trace("VIP read message queue failed\n");
				return -EFAULT;
			}
			PEMsgQ_ReadFinish(&hPEVIPMsgQ);
#else
#if 0
			//msg.m_MsgID = VPP_CC_MSG_TYPE_VPP;
			//msg.m_Param1 = MV_Time_GetTIMER7(); it's not in kernel symbol temp use others
			msg.m_Param1 = vip_instat;
			msg.m_Param2 = vip_intr_timestamp;
#endif
#endif
			if (copy_to_user
			    ((void __user *)arg, &msg, sizeof(MV_CC_MSG_t)))
				return -EFAULT;
			break;
#else
			return -EFAULT;
#endif
		}
	case VIP_IOCTL_VDE_BCM_CFGQ:
		//TODO: get VBI data BCM CFGQ from user space
		{
			VIP_DMA_INFO user_vip_info;

			if (copy_from_user
			    (&user_vip_info, (void __user *)arg,
			     sizeof(VIP_DMA_INFO)))
				return -EFAULT;

			vip_vbi_info.DmaAddr =
			    (UINT32) MV_SHM_GetCachePhysAddr(user_vip_info.
							     DmaAddr);
			vip_vbi_info.DmaLen = user_vip_info.DmaLen;

			break;
		}
	case VIP_IOCTL_VBI_BCM_CFGQ:
		//TODO: get BCM CFGQ from user space
		{
			VIP_DMA_INFO user_vip_info;

			if (copy_from_user
			    (&user_vip_info, (void __user *)arg,
			     sizeof(VIP_DMA_INFO)))
				return -EFAULT;

			vip_dma_info.DmaAddr =
			    (UINT32) MV_SHM_GetCachePhysAddr(user_vip_info.
							     DmaAddr);
			vip_dma_info.DmaLen = user_vip_info.DmaLen;

			break;
		}

	case VIP_IOCTL_SD_WRE_CFGQ:
		//TODO: get BCM CFGQ from user space
		{
			VIP_DMA_INFO user_vip_info;

			if (copy_from_user
			    (&user_vip_info, (void __user *)arg,
			     sizeof(VIP_DMA_INFO)))
				return -EFAULT;

			vip_sd_wr_info.DmaAddr =
			    (UINT32) MV_SHM_GetCachePhysAddr(user_vip_info.
							     DmaAddr);
			vip_sd_wr_info.DmaLen = user_vip_info.DmaLen;

			//setup WR DMA in advance!
			start_vip_sd_wr_bcm();

			break;
		}
	case VIP_IOCTL_SD_RDE_CFGQ:
		//TODO: get BCM CFGQ from user space
		{
			VIP_DMA_INFO user_vip_info;

			if (copy_from_user
			    (&user_vip_info, (void __user *)arg,
			     sizeof(VIP_DMA_INFO)))
				return -EFAULT;

			vip_sd_rd_info.DmaAddr =
			    (UINT32) MV_SHM_GetCachePhysAddr(user_vip_info.
							     DmaAddr);
			vip_sd_rd_info.DmaLen = user_vip_info.DmaLen;

			//setup RD DMA in advance!
			start_vip_sd_rd_bcm();

			break;
		}

	case VIP_IOCTL_SEND_MSG:
		//get msg from VIP
		{
			int vip_msg = 0;
			if (copy_from_user
			    (&vip_msg, (void __user *)arg, sizeof(int)))
				return -EFAULT;

			if (vip_msg == VIP_MSG_DESTROY_ISR_TASK) {
				//force one more INT to VIP to destroy ISR task
				up(&vip_sem);
			}

			break;
		}

	case VIP_IOCTL_INTR_MSG:
		//get VIP INTR MASK info
		{
		    INTR_MSG vip_intr_info = { 0, 0 };

			if (copy_from_user
			    (&vip_intr_info, (void __user *)arg, sizeof(INTR_MSG)))
				return -EFAULT;

			if (vip_intr_info.DhubSemMap < MAX_INTR_NUM)
				vip_intr_status[vip_intr_info.DhubSemMap] = vip_intr_info.Enable;
			else
				return -EFAULT;

			break;
		}
#endif

	case VDEC_IOCTL_ENABLE_INT:
		/* special handle for Vdec interrupt */
#if CONFIG_VDEC_UNBLC_IRQ_FIX
		if (vdec_enable_int_cnt - vdec_int_cnt == 0) {
			enable_irq(pe_irqs[IRQ_DHUBINTRVPRO]);
			vdec_enable_int_cnt++;
		}
		if (vdec_enable_int_cnt - vdec_int_cnt > 1) {
			pe_trace("enable_irq vdec, vdec_int_depth:%d, %d\n",
				 vdec_int_cnt, vdec_enable_int_cnt);
		}
#else
		enable_irq(pe_irqs[IRQ_DHUBINTRVPRO]);
#endif
		break;

	case AOUT_IOCTL_DISABLE_INT_MSG:
                if (copy_from_user
                    (aout_info, (void __user *)arg, 2 * sizeof(int)))
                        return -EFAULT;
                spin_lock_irqsave(&aout_spinlock, aoutirq);
                aout_disable_int_msg_update(aout_info);
                spin_unlock_irqrestore(&aout_spinlock, aoutirq);
                break;

	case AOUT_IOCTL_START_CMD:
		if (copy_from_user
		    (aout_info, (void __user *)arg, 2 * sizeof(int)))
			return -EFAULT;
		spin_lock_irqsave(&aout_spinlock, aoutirq);
		aout_start_cmd(aout_info);
		spin_unlock_irqrestore(&aout_spinlock, aoutirq);
		break;

	case AIP_IOCTL_START_CMD:
		if (copy_from_user
		    (aip_info, (void __user *)arg, 2 * sizeof(int))) {
			return -EFAULT;
		}
		spin_lock_irqsave(&aip_spinlock, aipirq);
		aip_start_cmd(aip_info);
		spin_unlock_irqrestore(&aip_spinlock, aipirq);
		break;

	case AIP_IOCTL_STOP_CMD:
		aip_stop_cmd();
		break;

	case APP_IOCTL_INIT_CMD:
		if (copy_from_user(app_info, (void __user *)arg, sizeof(int)))
			return -EFAULT;
		p_app_fifo =
		    (HWAPP_CMD_FIFO *) MV_SHM_GetNonCacheVirtAddr(*app_info);
		break;

	case APP_IOCTL_START_CMD:
		spin_lock_irqsave(&app_spinlock, appirq);
		app_start_cmd(p_app_fifo);
		spin_unlock_irqrestore(&app_spinlock, appirq);
	default:
		break;
	}

	return 0;
}

static int read_proc_status(char *page, char **start, off_t offset,
			    int count, int *eof, void *data)
{
	int len = 0;
	int cnt;

	len += snprintf(page + len, count, "%-25s : %d \n", "PE Module IRQ cnt",
		    vpp_cpcb0_vbi_int_cnt);

	pe_debug("%s OK. (%d / %d)\n", __func__, len, count);

	*eof = 1;

	return ((count < len) ? count : len);
}

static int read_proc_detail(char *page, char **start, off_t offset,
			    int count, int *eof, void *data)
{
	int len = 0;

	len +=
	    snprintf(page + len, count,"%-25s : %d aud %d \n", "PE Module IRQ cnt",
		    vpp_cpcb0_vbi_int_cnt, vpp_hdmi_audio_int_cnt);

	pe_debug("%s OK. (%d / %d)\n", __func__, len, count);

	*eof = 1;

	return ((count < len) ? count : len);
}

/*******************************************************************************
  Module Register API
  */
static int pe_driver_setup_cdev(struct cdev *dev, int major, int minor,
				struct file_operations *fops)
{
	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	return cdev_add(dev, MKDEV(major, minor), 1);
}

static int __init pe_driver_init(void)
{
	int i, res;	
	struct device_node *np, *iter;
	struct resource r;

	np = of_find_compatible_node(NULL, NULL, "mrvl,berlin-pe");
	if (!np)
		return -ENODEV;
	for (i = 0; i <= IRQ_DHUBINTRVPRO; i++) {
		of_irq_to_resource(np, i, &r);
		pe_irqs[i] = r.start;
	}
	for_each_child_of_node(np, iter) {
		if (of_device_is_compatible(iter, "mrvl,berlin-cec")) {
			of_irq_to_resource(iter, 0, &r);
			pe_irqs[IRQ_SM_CEC] = r.start;
		}
	}
	of_node_put(np);

	/* Figure out our device number. */
	res =
	    register_chrdev_region(MKDEV(GALOIS_PE_MAJOR, 0), GALOIS_PE_MINORS,
				   PE_DEVICE_NAME);
	if (res < 0) {
		pe_error("unable to get pe device major [%d]\n",
			 GALOIS_PE_MAJOR);
		goto err_reg_device;
	}
	pe_debug("register cdev device major [%d]\n", GALOIS_PE_MAJOR);

	/* Now setup cdevs. */
	res =
	    pe_driver_setup_cdev(&pe_dev, GALOIS_PE_MAJOR, GALOIS_PE_MINOR,
				 &pe_ops);
	if (res) {
		pe_error("pe_driver_setup_cdev failed.\n");
		goto err_add_device;
	}
	pe_debug("setup cdevs device minor [%d]\n", GALOIS_PE_MINOR);

	/* add PE devices to sysfs */
	pe_dev_class = class_create(THIS_MODULE, PE_DEVICE_NAME);
	if (IS_ERR(pe_dev_class)) {
		pe_error("class_create failed.\n");
		res = -ENODEV;
		goto err_add_device;
	}

	device_create(pe_dev_class, NULL,
		      MKDEV(GALOIS_PE_MAJOR, GALOIS_PE_MINOR),
		      NULL, PE_DEVICE_NAME);
	pe_debug("create device sysfs [%s]\n", PE_DEVICE_NAME);

	/* create PE device */
	res = pe_device_init(CPUINDEX, &pe_dev);
	if (res != 0)
		pe_error("pe_int_init failed !!! res = 0x%08X\n", res);

	/* create PE device proc file */
	pe_driver_procdir = proc_mkdir(PE_DEVICE_NAME, NULL);
	//pe_driver_procdir->owner = THIS_MODULE;
	create_proc_read_entry(PE_DEVICE_PROCFILE_STATUS, 0, pe_driver_procdir,
			       read_proc_status, NULL);
	create_proc_read_entry(PE_DEVICE_PROCFILE_DETAIL, 0, pe_driver_procdir,
			       read_proc_detail, NULL);

	res = pe_agent_driver_init();
	if (res != 0)
		pe_error("pe_agent_driver_init failed !!! res = 0x%08X\n", res);

	pe_trace("pe_driver_init OK\n");

	return 0;

 err_add_device:

	cdev_del(&pe_dev);

	unregister_chrdev_region(MKDEV(GALOIS_PE_MAJOR, 0), GALOIS_PE_MINORS);

 err_reg_device:

	pe_trace("pe_driver_init failed !!! (%d)\n", res);

	return res;
}

static void __exit pe_driver_exit(void)
{
	int res;

	/* destroy PE kernel API */
	res = pe_device_exit(0, 0);
	if (res != 0)
		pe_error("pe_device_exit failed !!! res = 0x%08X\n", res);

	/* remove PE device proc file */
	remove_proc_entry(PE_DEVICE_PROCFILE_DETAIL, pe_driver_procdir);
	remove_proc_entry(PE_DEVICE_PROCFILE_STATUS, pe_driver_procdir);
	remove_proc_entry(PE_DEVICE_NAME, NULL);

	/* del sysfs entries */
	device_destroy(pe_dev_class, MKDEV(GALOIS_PE_MAJOR, GALOIS_PE_MINOR));
	pe_debug("delete device sysfs [%s]\n", PE_DEVICE_NAME);

	class_destroy(pe_dev_class);

	/* del cdev */
	cdev_del(&pe_dev);

	pe_agent_driver_exit();

	unregister_chrdev_region(MKDEV(GALOIS_PE_MAJOR, 0), GALOIS_PE_MINORS);
	pe_debug("unregister cdev device major [%d]\n", GALOIS_PE_MAJOR);

	pe_trace("pe_driver_exit OK\n");
}

module_init(pe_driver_init);
module_exit(pe_driver_exit);

/*******************************************************************************
    Module Descrption
*/
MODULE_AUTHOR("marvell");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pe module template");
