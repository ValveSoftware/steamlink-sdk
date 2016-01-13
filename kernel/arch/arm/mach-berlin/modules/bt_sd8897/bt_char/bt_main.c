/** @file bt_main.c
  *
  * @brief This file contains the major functions in BlueTooth
  * driver. It includes init, exit, open, close and main
  * thread etc..
  *
  * Copyright (C) 2007-2015, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available along with the File in the gpl.txt file or by writing to
  * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */
/**
  * @mainpage M-BT Linux Driver
  *
  * @section overview_sec Overview
  *
  * The M-BT is a Linux reference driver for Marvell Bluetooth chipset.
  *
  * @section copyright_sec Copyright
  *
  * Copyright (C) 2007-2015, Marvell International Ltd.
  *
  */

#include <linux/module.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include <linux/mmc/sdio_func.h>

#include "bt_drv.h"
#include "mbt_char.h"
#include "bt_sdio.h"

/** Version */
#define VERSION "C3X14113"

/** Driver version */
static char mbt_driver_version[] = "SD8XXX-%s-" VERSION "-(" "FP" FPNUM ")"
#ifdef DEBUG_LEVEL2
	"-dbg"
#endif
	" ";

/** SD8787 Card */
#define CARD_SD8787     "SD8787"
/** SD8777 Card */
#define CARD_SD8777     "SD8777"
/** SD8887 Card */
#define CARD_SD8887     "SD8887"
/** SD8897 Card */
#define CARD_SD8897     "SD8897"
/** SD8797 Card */
#define CARD_SD8797     "SD8797"
/** SD8977 Card */
#define CARD_SD8977     "SD8977"

/** Declare and initialize fw_version */
static char fw_version[32] = "0.0.0.p0";

#define AID_SYSTEM        1000	/* system server */

#define AID_BLUETOOTH     1002	/* bluetooth subsystem */

#define AID_NET_BT_STACK  3008	/* bluetooth stack */

/** Define module name */
#define MODULE_NAME  "bt_fm_nfc"

/** Declaration of chardev class */
static struct class *chardev_class;

/** Interface specific variables */
static int mbtchar_minor;
static int fmchar_minor;
static int nfcchar_minor;
static int debugchar_minor;

/** Default Driver mode */
static int drv_mode = (DRV_MODE_BT | DRV_MODE_FM | DRV_MODE_NFC);

/** BT interface name */
static char *bt_name;
/** FM interface name */
static char *fm_name;
/** NFC interface name */
static char *nfc_name;
/** BT debug interface name */
static char *debug_name;

/** Firmware flag */
static int fw = 1;
/** default powermode */
static int psmode = 1;
/** Default CRC check control */
static int fw_crc_check = 1;
/** Init config file (MAC address, register etc.) */
static char *init_cfg;
/** Calibration config file (MAC address, init powe etc.) */
static char *cal_cfg;
/** Calibration config file EXT */
static char *cal_cfg_ext;
/** Init MAC address */
static char *bt_mac;
static int mbt_gpio_pin;

/** Setting mbt_drvdbg value based on DEBUG level */
#ifdef DEBUG_LEVEL1
#ifdef DEBUG_LEVEL2
#define DEFAULT_DEBUG_MASK  (0xffffffff & ~DBG_EVENT)
#else
#define DEFAULT_DEBUG_MASK  (DBG_MSG | DBG_FATAL | DBG_ERROR)
#endif /* DEBUG_LEVEL2 */
u32 mbt_drvdbg = DEFAULT_DEBUG_MASK;
#endif

#ifdef CONFIG_OF
static int dts_enable = 1;
#endif

#ifdef SDIO_SUSPEND_RESUME
/** PM keep power */
int mbt_pm_keep_power = 1;
#endif

static int debug_intf = 1;

/**
 *  @brief Alloc bt device
 *
 *  @return    pointer to structure mbt_dev or NULL
 */
struct mbt_dev *
alloc_mbt_dev(void)
{
	struct mbt_dev *mbt_dev;
	ENTER();

	mbt_dev = kzalloc(sizeof(struct mbt_dev), GFP_KERNEL);
	if (!mbt_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return mbt_dev;
}

/**
 *  @brief Alloc fm device
 *
 *  @return    pointer to structure fm_dev or NULL
 */
struct fm_dev *
alloc_fm_dev(void)
{
	struct fm_dev *fm_dev;
	ENTER();

	fm_dev = kzalloc(sizeof(struct fm_dev), GFP_KERNEL);
	if (!fm_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return fm_dev;
}

/**
 *  @brief Alloc nfc device
 *
 *  @return    pointer to structure nfc_dev or NULL
 */
struct nfc_dev *
alloc_nfc_dev(void)
{
	struct nfc_dev *nfc_dev;
	ENTER();

	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (!nfc_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return nfc_dev;
}

/**
 *  @brief Alloc debug device
 *
 *  @return    pointer to structure debug_level or NULL
 */
struct debug_dev *
alloc_debug_dev(void)
{
	struct debug_dev *debug_dev;
	ENTER();

	debug_dev = kzalloc(sizeof(struct debug_dev), GFP_KERNEL);
	if (!debug_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return debug_dev;
}

/**
 *  @brief Frees m_dev
 *
 *  @return    N/A
 */
void
free_m_dev(struct m_dev *m_dev)
{
	ENTER();
	kfree(m_dev->dev_pointer);
	m_dev->dev_pointer = NULL;
	LEAVE();
}

/**
 *  @brief clean up m_devs
 *
 *  @return    N/A
 */
void
clean_up_m_devs(bt_private *priv)
{
	struct m_dev *m_dev = NULL;
	int i;

	ENTER();
	if (priv->bt_dev.m_dev[BT_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
		PRINTM(MSG, "BT: Delete %s\n", m_dev->name);
		if (m_dev->spec_type == IANYWHERE_SPEC) {
			if ((drv_mode & DRV_MODE_BT) && (mbtchar_minor > 0))
				mbtchar_minor--;
			m_dev->close(m_dev);
			for (i = 0; i < 3; i++)
				kfree_skb(((struct mbt_dev *)
					   (m_dev->dev_pointer))->
					  reassembly[i]);
			/**  unregister m_dev to char_dev */
			if (chardev_class)
				chardev_cleanup_one(m_dev, chardev_class);
			free_m_dev(m_dev);
		}
		priv->bt_dev.m_dev[BT_SEQ].dev_pointer = NULL;
	}
	if (priv->bt_dev.m_dev[FM_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[FM_SEQ]);
		PRINTM(MSG, "BT: Delete %s\n", m_dev->name);
		if ((drv_mode & DRV_MODE_FM) && (fmchar_minor > 0))
			fmchar_minor--;
		m_dev->close(m_dev);
		/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
		priv->bt_dev.m_dev[FM_SEQ].dev_pointer = NULL;
	}
	if (priv->bt_dev.m_dev[NFC_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[NFC_SEQ]);
		PRINTM(MSG, "BT: Delete %s\n", m_dev->name);
		if ((drv_mode & DRV_MODE_NFC) && (nfcchar_minor > 0))
			nfcchar_minor--;
		m_dev->close(m_dev);
		/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
		priv->bt_dev.m_dev[NFC_SEQ].dev_pointer = NULL;
	}
	if (priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[DEBUG_SEQ]);
		PRINTM(MSG, "BT: Delete %s\n", m_dev->name);
		if ((debug_intf) && (debugchar_minor > 0))
			debugchar_minor--;
		/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
		priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer = NULL;
	}
	LEAVE();
	return;
}

/**
 *  @brief This function verify the received event pkt
 *
 *  Event format:
 *  +--------+--------+--------+--------+--------+
 *  | Event  | Length |  ncmd  |      Opcode     |
 *  +--------+--------+--------+--------+--------+
 *  | 1-byte | 1-byte | 1-byte |      2-byte     |
 *  +--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to rx skb
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
check_evtpkt(bt_private *priv, struct sk_buff *skb)
{
	struct hci_event_hdr *hdr = (struct hci_event_hdr *)skb->data;
	struct hci_ev_cmd_complete *ec;
	u16 opcode, ocf;
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	if (!priv->bt_dev.sendcmdflag) {
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	if (hdr->evt == HCI_EV_CMD_COMPLETE) {
		ec = (struct hci_ev_cmd_complete *)
			(skb->data + HCI_EVENT_HDR_SIZE);
		opcode = __le16_to_cpu(ec->opcode);
		ocf = hci_opcode_ocf(opcode);
		PRINTM(CMD,
		       "BT: CMD_COMPLTE opcode=0x%x, ocf=0x%x, send_cmd_opcode=0x%x\n",
		       opcode, ocf, priv->bt_dev.send_cmd_opcode);
		if (opcode != priv->bt_dev.send_cmd_opcode) {
			ret = BT_STATUS_FAILURE;
			goto exit;
		}
		switch (ocf) {
		case BT_CMD_MODULE_CFG_REQ:
		case BT_CMD_BLE_DEEP_SLEEP:
		case BT_CMD_CONFIG_MAC_ADDR:
		case BT_CMD_CSU_WRITE_REG:
		case BT_CMD_LOAD_CONFIG_DATA:
		case BT_CMD_LOAD_CONFIG_DATA_EXT:
		case BT_CMD_AUTO_SLEEP_MODE:
		case BT_CMD_HOST_SLEEP_CONFIG:
		case BT_CMD_SDIO_PULL_CFG_REQ:
		case BT_CMD_SET_EVT_FILTER:
		case BT_CMD_ENABLE_WRITE_SCAN:
			// case BT_CMD_ENABLE_DEVICE_TESTMODE:
		case BT_CMD_RESET:
		case BT_CMD_SET_GPIO_PIN:
			priv->bt_dev.sendcmdflag = FALSE;
			priv->adapter->cmd_complete = TRUE;
			wake_up_interruptible(&priv->adapter->cmd_wait_q);
			break;
		case BT_CMD_GET_FW_VERSION:
			{
				u8 *pos = (skb->data + HCI_EVENT_HDR_SIZE +
					   sizeof(struct hci_ev_cmd_complete) +
					   1);
				snprintf(fw_version, sizeof(fw_version),
					 "%u.%u.%u.p%u", pos[2], pos[1], pos[0],
					 pos[3]);
				priv->bt_dev.sendcmdflag = FALSE;
				priv->adapter->cmd_complete = TRUE;
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
				break;
			}
#ifdef SDIO_SUSPEND_RESUME
		case FM_CMD:
			{
				u8 *pos =
					(skb->data + HCI_EVENT_HDR_SIZE +
					 sizeof(struct hci_ev_cmd_complete) +
					 1);
				if (*pos == FM_SET_INTR_MASK) {
					priv->bt_dev.sendcmdflag = FALSE;
					priv->adapter->cmd_complete = TRUE;
					wake_up_interruptible(&priv->adapter->
							      cmd_wait_q);
				}
			}
			break;
#endif
		case BT_CMD_HOST_SLEEP_ENABLE:
			priv->bt_dev.sendcmdflag = FALSE;
			break;
		default:
			ret = BT_STATUS_FAILURE;
			break;
		}
	}
exit:
	if (ret == BT_STATUS_SUCCESS)
		kfree_skb(skb);
	LEAVE();
	return ret;
}

/**
 *  @brief This function process the received event
 *
 *  Event format:
 *  +--------+--------+--------+--------+-----+
 *  |   EC   | Length |           Data        |
 *  +--------+--------+--------+--------+-----+
 *  | 1-byte | 1-byte |          n-byte       |
 *  +--------+--------+--------+--------+-----+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to rx skb
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_process_event(bt_private *priv, struct sk_buff *skb)
{
	int ret = BT_STATUS_SUCCESS;
	struct m_dev *m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	BT_EVENT *pevent;

	ENTER();
	pevent = (BT_EVENT *)skb->data;
	if (pevent->EC != 0xff) {
		PRINTM(CMD, "BT: Not Marvell Event=0x%x\n", pevent->EC);
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	switch (pevent->data[0]) {
	case BT_CMD_AUTO_SLEEP_MODE:
		if (pevent->data[2] == BT_STATUS_SUCCESS) {
			if (pevent->data[1] == BT_PS_ENABLE)
				priv->adapter->psmode = 1;
			else
				priv->adapter->psmode = 0;
			PRINTM(CMD, "BT: PS Mode %s:%s\n", m_dev->name,
			       (priv->adapter->psmode) ? "Enable" : "Disable");

		} else {
			PRINTM(CMD, "BT: PS Mode Command Fail %s\n",
			       m_dev->name);
		}
		break;
	case BT_CMD_HOST_SLEEP_CONFIG:
		if (pevent->data[3] == BT_STATUS_SUCCESS) {
			PRINTM(CMD, "BT: %s: gpio=0x%x, gap=0x%x\n",
			       m_dev->name, pevent->data[1], pevent->data[2]);
		} else {
			PRINTM(CMD, "BT: %s: HSCFG Command Fail\n",
			       m_dev->name);
		}
		break;
	case BT_CMD_HOST_SLEEP_ENABLE:
		if (pevent->data[1] == BT_STATUS_SUCCESS) {
			priv->adapter->hs_state = HS_ACTIVATED;
			if (priv->adapter->suspend_fail == FALSE) {
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
#ifdef MMC_PM_FUNC_SUSPENDED
				bt_is_suspended(priv);
#endif
#endif
#endif
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
			}
			if (priv->adapter->psmode)
				priv->adapter->ps_state = PS_SLEEP;
			PRINTM(CMD, "BT: EVENT %s: HS ACTIVATED!\n",
			       m_dev->name);

		} else {
			PRINTM(CMD, "BT: %s: HS Enable Fail\n", m_dev->name);
		}
		break;
	case BT_CMD_MODULE_CFG_REQ:
		if ((priv->bt_dev.sendcmdflag == TRUE) &&
		    ((pevent->data[1] == MODULE_BRINGUP_REQ)
		     || (pevent->data[1] == MODULE_SHUTDOWN_REQ))) {
			if (pevent->data[1] == MODULE_BRINGUP_REQ) {
				PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
				       (pevent->data[2] && (pevent->data[2] !=
							    MODULE_CFG_RESP_ALREADY_UP))
				       ? "Bring up Fail" : "Bring up success");
				priv->bt_dev.devType = pevent->data[3];
				PRINTM(CMD, "devType:%s\n",
				       (pevent->data[3] ==
					DEV_TYPE_AMP) ? "AMP controller" :
				       "BR/EDR controller");
				priv->bt_dev.devFeature = pevent->data[4];
				PRINTM(CMD,
				       "devFeature:  %s,    %s,    %s,    %s,    %s\n",
				       ((pevent->
					 data[4] & DEV_FEATURE_BT) ?
					"BT Feature" : "No BT Feature"),
				       ((pevent->
					 data[4] & DEV_FEATURE_BTAMP) ?
					"BTAMP Feature" : "No BTAMP Feature"),
				       ((pevent->
					 data[4] & DEV_FEATURE_BLE) ?
					"BLE Feature" : "No BLE Feature"),
				       ((pevent->
					 data[4] & DEV_FEATURE_FM) ?
					"FM Feature" : "No FM Feature"),
				       ((pevent->
					 data[4] & DEV_FEATURE_NFC) ?
					"NFC Feature" : "No NFC Feature"));
			}
			if (pevent->data[1] == MODULE_SHUTDOWN_REQ) {
				PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
				       (pevent->data[2]) ? "Shut down Fail"
				       : "Shut down success");

			}
			if (pevent->data[2]) {
				priv->bt_dev.sendcmdflag = FALSE;
				priv->adapter->cmd_complete = TRUE;
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
			}
		} else {
			PRINTM(CMD, "BT_CMD_MODULE_CFG_REQ resp for APP\n");
			ret = BT_STATUS_FAILURE;
		}
		break;
	case BT_EVENT_POWER_STATE:
		if (pevent->data[1] == BT_PS_SLEEP)
			priv->adapter->ps_state = PS_SLEEP;
		if (priv->adapter->ps_state == PS_SLEEP
		    && (priv->card_type == CARD_TYPE_SD8887)
			)
			os_sched_timeout(5);
		PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
		       (priv->adapter->ps_state) ? "PS_SLEEP" : "PS_AWAKE");

		break;
	case BT_CMD_SDIO_PULL_CFG_REQ:
		if (pevent->data[pevent->length - 1] == BT_STATUS_SUCCESS)
			PRINTM(CMD, "BT: %s: SDIO pull configuration success\n",
			       m_dev->name);

		else {
			PRINTM(CMD, "BT: %s: SDIO pull configuration fail\n",
			       m_dev->name);

		}
		break;
	default:
		PRINTM(CMD, "BT: Unknown Event=%d %s\n", pevent->data[0],
		       m_dev->name);
		ret = BT_STATUS_FAILURE;
		break;
	}
exit:
	if (ret == BT_STATUS_SUCCESS)
		kfree_skb(skb);
	LEAVE();
	return ret;
}

/**
 *  @brief This function save the dump info to file
 *
 *
 *  @param dir_name     directory name
 *  @param file_name    file_name
 *  @return    0 --success otherwise fail
 */
int
bt_save_dump_info_to_file(char *dir_name, char *file_name, u8 *buf, u32 buf_len)
{
	int ret = BT_STATUS_SUCCESS;
	struct file *pfile = NULL;
	u8 name[64];
	mm_segment_t fs;
	loff_t pos;

	ENTER();

	if (!dir_name || !file_name || !buf) {
		PRINTM(ERROR, "Can't save dump info to file\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}

	memset(name, 0, sizeof(name));
	sprintf((char *)name, "%s/%s", dir_name, file_name);
	pfile = filp_open((const char *)name, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(pfile)) {
		PRINTM(MSG,
		       "Create file %s error, try to save dump file in /var\n",
		       name);
		memset(name, 0, sizeof(name));
		sprintf((char *)name, "%s/%s", "/var", file_name);
		pfile = filp_open((const char *)name, O_CREAT | O_RDWR, 0644);
	}
	if (IS_ERR(pfile)) {
		PRINTM(ERROR, "Create Dump file for %s error\n", name);
		ret = BT_STATUS_FAILURE;
		goto done;
	}

	PRINTM(MSG, "Dump data %s saved in %s\n", file_name, name);

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, (const char __user *)buf, buf_len, &pos);
	filp_close(pfile, NULL);
	set_fs(fs);

	PRINTM(MSG, "Dump data %s saved in %s successfully\n", file_name, name);

done:
	LEAVE();
	return ret;
}

#define DEBUG_HOST_READY		0xEE
#define DEBUG_FW_DONE			0xFF
#define MAX_POLL_TRIES			100

#define DEBUG_DUMP_CTRL_REG_8897               0xE2
#define DEBUG_DUMP_START_REG_8897              0xE3
#define DEBUG_DUMP_END_REG_8897                0xEA
#define DEBUG_DUMP_CTRL_REG_8887               0xA2
#define DEBUG_DUMP_START_REG_8887              0xA3
#define DEBUG_DUMP_END_REG_8887                0xAA
#define DEBUG_DUMP_CTRL_REG_8977               0xF0
#define DEBUG_DUMP_START_REG_8977              0xF1
#define DEBUG_DUMP_END_REG_8977                0xF8

typedef enum {
	DUMP_TYPE_ITCM = 0,
	DUMP_TYPE_DTCM = 1,
	DUMP_TYPE_SQRAM = 2,
	DUMP_TYPE_APU_REGS = 3,
	DUMP_TYPE_CIU_REGS = 4,
	DUMP_TYPE_ICU_REGS = 5,
	DUMP_TYPE_MAC_REGS = 6,
	DUMP_TYPE_EXTEND_7 = 7,
	DUMP_TYPE_EXTEND_8 = 8,
	DUMP_TYPE_EXTEND_9 = 9,
	DUMP_TYPE_EXTEND_10 = 10,
	DUMP_TYPE_EXTEND_11 = 11,
	DUMP_TYPE_EXTEND_12 = 12,
	DUMP_TYPE_EXTEND_13 = 13,
	DUMP_TYPE_EXTEND_LAST = 14
} dumped_mem_type;

#define MAX_NAME_LEN               8
#define MAX_FULL_NAME_LEN               32

typedef struct {
	u8 mem_name[MAX_NAME_LEN];
	u8 *mem_Ptr;
	struct file *pfile_mem;
	u8 done_flag;
} memory_type_mapping;

memory_type_mapping bt_mem_type_mapping_tbl[] = {
	{"ITCM", NULL, NULL, 0xF0},
	{"DTCM", NULL, NULL, 0xF1},
	{"SQRAM", NULL, NULL, 0xF2},
	{"APU", NULL, NULL, 0xF3},
	{"CIU", NULL, NULL, 0xF4},
	{"ICU", NULL, NULL, 0xF5},
	{"MAC", NULL, NULL, 0xF6},
	{"EXT7", NULL, NULL, 0xF7},
	{"EXT8", NULL, NULL, 0xF8},
	{"EXT9", NULL, NULL, 0xF9},
	{"EXT10", NULL, NULL, 0xFA},
	{"EXT11", NULL, NULL, 0xFB},
	{"EXT12", NULL, NULL, 0xFC},
	{"EXT13", NULL, NULL, 0xFD},
	{"EXTLAST", NULL, NULL, 0xFE},
};

typedef enum {
	RDWR_STATUS_SUCCESS = 0,
	RDWR_STATUS_FAILURE = 1,
	RDWR_STATUS_DONE = 2
} rdwr_status;

/**
 *  @brief This function read/write firmware via cmd52
 *
 *  @param phandle   A pointer to moal_handle
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
rdwr_status
bt_cmd52_rdwr_firmware(bt_private *priv, u8 doneflag)
{
	int ret = 0;
	int tries = 0;
	u8 ctrl_data = 0;
	u8 dbg_dump_ctrl_reg = 0;

	if (priv->card_type == CARD_TYPE_SD8887)
		dbg_dump_ctrl_reg = DEBUG_DUMP_CTRL_REG_8887;
	else if (priv->card_type == CARD_TYPE_SD8897)
		dbg_dump_ctrl_reg = DEBUG_DUMP_CTRL_REG_8897;
	else if (priv->card_type == CARD_TYPE_SD8977)
		dbg_dump_ctrl_reg = DEBUG_DUMP_CTRL_REG_8977;

	sdio_writeb(((struct sdio_mmc_card *)priv->bt_dev.card)->func,
		    DEBUG_HOST_READY, dbg_dump_ctrl_reg, &ret);
	if (ret) {
		PRINTM(ERROR, "SDIO Write ERR\n");
		return RDWR_STATUS_FAILURE;
	}
	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		ctrl_data =
			sdio_readb(((struct sdio_mmc_card *)priv->bt_dev.card)->
				   func, dbg_dump_ctrl_reg, &ret);
		if (ret) {
			PRINTM(ERROR, "SDIO READ ERR\n");
			return RDWR_STATUS_FAILURE;
		}
		if (ctrl_data == DEBUG_FW_DONE)
			break;
		if (doneflag && ctrl_data == doneflag)
			return RDWR_STATUS_DONE;
		if (ctrl_data != DEBUG_HOST_READY) {
			PRINTM(INFO,
			       "The ctrl reg was changed, re-try again!\n");
			sdio_writeb(((struct sdio_mmc_card *)priv->bt_dev.
				     card)->func, DEBUG_HOST_READY,
				    dbg_dump_ctrl_reg, &ret);
			if (ret) {
				PRINTM(ERROR, "SDIO Write ERR\n");
				return RDWR_STATUS_FAILURE;
			}
		}
		udelay(100);
	}
	if (ctrl_data == DEBUG_HOST_READY) {
		PRINTM(ERROR, "Fail to pull ctrl_data\n");
		return RDWR_STATUS_FAILURE;
	}

	return RDWR_STATUS_SUCCESS;
}

/**
 *  @brief This function dump firmware memory to file
 *
 *  @param phandle   A pointer to moal_handle
 *
 *  @return         N/A
 */
void
bt_dump_firmware_info_v2(bt_private *priv)
{
	int ret = 0;
	unsigned int reg, reg_start, reg_end;
	u8 *dbg_ptr = NULL;
	u8 dump_num = 0;
	u8 idx = 0;
	u8 doneflag = 0;
	rdwr_status stat;
	u8 i = 0;
	u8 read_reg = 0;
	u32 memory_size = 0;
	u8 path_name[64], file_name[32];
	u8 *end_ptr = NULL;
	u8 dbg_dump_start_reg = 0;
	u8 dbg_dump_end_reg = 0;

	if (!priv) {
		PRINTM(ERROR, "Could not dump firmwware info\n");
		return;
	}

	if ((priv->card_type != CARD_TYPE_SD8887) &&
	    (priv->card_type != CARD_TYPE_SD8897)
	    && ((priv->card_type != CARD_TYPE_SD8977))) {
		PRINTM(MSG, "card_type %d don't support FW dump\n",
		       priv->card_type);
		return;
	}

	memset(path_name, 0, sizeof(path_name));
	strcpy((char *)path_name, "/data");
	PRINTM(MSG, "Create DUMP directory success:dir_name=%s\n", path_name);

	if (priv->card_type == CARD_TYPE_SD8887) {
		dbg_dump_start_reg = DEBUG_DUMP_START_REG_8887;
		dbg_dump_end_reg = DEBUG_DUMP_END_REG_8887;
	} else if (priv->card_type == CARD_TYPE_SD8897) {
		dbg_dump_start_reg = DEBUG_DUMP_START_REG_8897;
		dbg_dump_end_reg = DEBUG_DUMP_END_REG_8897;
	} else if (priv->card_type == CARD_TYPE_SD8977) {
		dbg_dump_start_reg = DEBUG_DUMP_START_REG_8977;
		dbg_dump_end_reg = DEBUG_DUMP_END_REG_8977;
	}

	sbi_wakeup_firmware(priv);
	sdio_claim_host(((struct sdio_mmc_card *)priv->bt_dev.card)->func);
	/* start dump fw memory */
	PRINTM(MSG, "==== DEBUG MODE OUTPUT START ====\n");
	/* read the number of the memories which will dump */
	if (RDWR_STATUS_FAILURE == bt_cmd52_rdwr_firmware(priv, doneflag))
		goto done;
	reg = dbg_dump_start_reg;
	dump_num =
		sdio_readb(((struct sdio_mmc_card *)priv->bt_dev.card)->func,
			   reg, &ret);
	if (ret) {
		PRINTM(MSG, "SDIO READ MEM NUM ERR\n");
		goto done;
	}

	/* read the length of every memory which will dump */
	for (idx = 0; idx < dump_num; idx++) {
		if (RDWR_STATUS_FAILURE ==
		    bt_cmd52_rdwr_firmware(priv, doneflag))
			goto done;
		memory_size = 0;
		reg = dbg_dump_start_reg;
		for (i = 0; i < 4; i++) {
			read_reg =
				sdio_readb(((struct sdio_mmc_card *)priv->
					    bt_dev.card)->func, reg, &ret);
			if (ret) {
				PRINTM(MSG, "SDIO READ ERR\n");
				goto done;
			}
			memory_size |= (read_reg << i * 8);
			reg++;
		}
		if (memory_size == 0) {
			PRINTM(MSG, "Firmware Dump Finished!\n");
			break;
		} else {
			PRINTM(MSG, "%s_SIZE=0x%x\n",
			       bt_mem_type_mapping_tbl[idx].mem_name,
			       memory_size);
			bt_mem_type_mapping_tbl[idx].mem_Ptr =
				vmalloc(memory_size + 1);
			if ((ret != BT_STATUS_SUCCESS) ||
			    !bt_mem_type_mapping_tbl[idx].mem_Ptr) {
				PRINTM(ERROR,
				       "Error: vmalloc %s buffer failed!!!\n",
				       bt_mem_type_mapping_tbl[idx].mem_name);
				goto done;
			}
			dbg_ptr = bt_mem_type_mapping_tbl[idx].mem_Ptr;
			end_ptr = dbg_ptr + memory_size;
		}
		doneflag = bt_mem_type_mapping_tbl[idx].done_flag;
		PRINTM(MSG, "Start %s output, please wait...\n",
		       bt_mem_type_mapping_tbl[idx].mem_name);
		do {
			stat = bt_cmd52_rdwr_firmware(priv, doneflag);
			if (RDWR_STATUS_FAILURE == stat)
				goto done;

			reg_start = dbg_dump_start_reg;
			reg_end = dbg_dump_end_reg;
			for (reg = reg_start; reg <= reg_end; reg++) {
				*dbg_ptr =
					sdio_readb(((struct sdio_mmc_card *)
						    priv->bt_dev.card)->func,
						   reg, &ret);
				if (ret) {
					PRINTM(MSG, "SDIO READ ERR\n");
					goto done;
				}
				if (dbg_ptr < end_ptr)
					dbg_ptr++;
				else
					PRINTM(MSG,
					       "pre-allocced buf is not enough\n");
			}
			if (RDWR_STATUS_DONE == stat) {
				PRINTM(MSG, "%s done:"
				       "size = 0x%x\n",
				       bt_mem_type_mapping_tbl[idx].mem_name,
				       (unsigned int)(dbg_ptr -
						      bt_mem_type_mapping_tbl
						      [idx].mem_Ptr));
				memset(file_name, 0, sizeof(file_name));
				sprintf((char *)file_name, "%s%s", "file_bt_",
					bt_mem_type_mapping_tbl[idx].mem_name);
				if (BT_STATUS_SUCCESS !=
				    bt_save_dump_info_to_file((char *)path_name,
							      (char *)file_name,
							      bt_mem_type_mapping_tbl
							      [idx].mem_Ptr,
							      memory_size))
					PRINTM(MSG,
					       "Can't save dump file %s in %s\n",
					       file_name, path_name);
				vfree(bt_mem_type_mapping_tbl[idx].mem_Ptr);
				bt_mem_type_mapping_tbl[idx].mem_Ptr = NULL;
				break;
			}
		} while (1);
	}
	PRINTM(MSG, "==== DEBUG MODE OUTPUT END ====\n");
	/* end dump fw memory */
done:
	sdio_release_host(((struct sdio_mmc_card *)priv->bt_dev.card)->func);
	for (idx = 0; idx < dump_num; idx++) {
		if (bt_mem_type_mapping_tbl[idx].mem_Ptr) {
			vfree(bt_mem_type_mapping_tbl[idx].mem_Ptr);
			bt_mem_type_mapping_tbl[idx].mem_Ptr = NULL;
		}
	}
	PRINTM(MSG, "==== DEBUG MODE END ====\n");
	return;
}

/**
 *  @brief This function shows debug info for timeout of command sending.
 *
 *  @param adapter  A pointer to bt_private
 *  @param cmd      Timeout command id
 *
 *  @return         N/A
 */
static void
bt_cmd_timeout_func(bt_private *priv, u16 cmd)
{
	bt_adapter *adapter = priv->adapter;
	ENTER();

	adapter->num_cmd_timeout++;

	PRINTM(ERROR, "Version = %s\n", adapter->drv_ver);
	PRINTM(ERROR, "Timeout Command id = 0x%x\n", cmd);
	PRINTM(ERROR, "Number of command timeout = %d\n",
	       adapter->num_cmd_timeout);
	PRINTM(ERROR, "Interrupt counter = %d\n", adapter->IntCounter);
	PRINTM(ERROR, "Power Save mode = %d\n", adapter->psmode);
	PRINTM(ERROR, "Power Save state = %d\n", adapter->ps_state);
	PRINTM(ERROR, "Host Sleep state = %d\n", adapter->hs_state);
	PRINTM(ERROR, "hs skip count = %d\n", adapter->hs_skip);
	PRINTM(ERROR, "suspend_fail flag = %d\n", adapter->suspend_fail);
	PRINTM(ERROR, "suspended flag = %d\n", adapter->is_suspended);
	PRINTM(ERROR, "Number of wakeup tries = %d\n", adapter->WakeupTries);
	PRINTM(ERROR, "Host Cmd complet state = %d\n", adapter->cmd_complete);
	PRINTM(ERROR, "Last irq recv = %d\n", adapter->irq_recv);
	PRINTM(ERROR, "Last irq processed = %d\n", adapter->irq_done);
	PRINTM(ERROR, "tx pending = %d\n", adapter->skb_pending);
	PRINTM(ERROR, "sdio int status = %d\n", adapter->sd_ireg);
	bt_dump_sdio_regs(priv);
	LEAVE();
}

/**
 *  @brief This function send reset cmd to firmware
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return	       BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_reset_command(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_HCI_CMD *)skb->data;
	pcmd->ocf_ogf = __cpu_to_le16((RESET_OGF << 10) | BT_CMD_RESET);
	pcmd->length = 0x00;
	pcmd->cmd_type = 0x00;
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, 3);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "Queue Reset Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Reset timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_RESET);
	} else {
		PRINTM(CMD, "BT: Reset Command done\n");
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends module cfg cmd to firmware
 *
 *  Command format:
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     OCF OGF     | Length |                Data               |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     2-byte      | 1-byte |               4-byte              |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param subcmd  sub command
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_module_cfg_cmd(bt_private *priv, int subcmd)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "BT: No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_MODULE_CFG_REQ);
	pcmd->length = 1;
	pcmd->data[0] = subcmd;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "Queue module cfg Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	wake_up_interruptible(&priv->MainThread.waitQ);
	/*
	   On some Android platforms certain delay is needed for HCI daemon to
	   remove this module and close itself gracefully. Otherwise it hangs.
	   This 10ms delay is a workaround for such platforms as the root cause
	   has not been found yet. */
	mdelay(10);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: module_cfg_cmd(%#x): timeout sendcmdflag=%d\n",
		       subcmd, priv->bt_dev.sendcmdflag);
		bt_cmd_timeout_func(priv, BT_CMD_MODULE_CFG_REQ);
	} else {
		PRINTM(CMD, "BT: module cfg Command done\n");
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function enables power save mode
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_ps(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_AUTO_SLEEP_MODE);
	if (priv->bt_dev.psmode)
		pcmd->data[0] = BT_PS_ENABLE;
	else
		pcmd->data[0] = BT_PS_DISABLE;
	if (priv->bt_dev.idle_timeout) {
		pcmd->length = 3;
		pcmd->data[1] = (u8)(priv->bt_dev.idle_timeout & 0x00ff);
		pcmd->data[2] = (priv->bt_dev.idle_timeout & 0xff00) >> 8;
	} else {
		pcmd->length = 1;
	}
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue PSMODE Command(0x%x):%d\n",
	       __le16_to_cpu(pcmd->ocf_ogf), pcmd->data[0]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: psmode timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_AUTO_SLEEP_MODE);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends hscfg command
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_hscfg_cmd(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_HOST_SLEEP_CONFIG);
	pcmd->length = 2;
	pcmd->data[0] = (priv->bt_dev.gpio_gap & 0xff00) >> 8;
	pcmd->data[1] = (u8)(priv->bt_dev.gpio_gap & 0x00ff);
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue HSCFG Command(0x%x),gpio=0x%x,gap=0x%x\n",
	       __le16_to_cpu(pcmd->ocf_ogf), pcmd->data[0], pcmd->data[1]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: HSCFG timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_HOST_SLEEP_CONFIG);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends sdio pull ctrl command
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_sdio_pull_ctrl_cmd(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_SDIO_PULL_CFG_REQ);
	pcmd->length = 4;
	pcmd->data[0] = (priv->bt_dev.sdio_pull_cfg & 0x000000ff);
	pcmd->data[1] = (priv->bt_dev.sdio_pull_cfg & 0x0000ff00) >> 8;
	pcmd->data[2] = (priv->bt_dev.sdio_pull_cfg & 0x00ff0000) >> 16;
	pcmd->data[3] = (priv->bt_dev.sdio_pull_cfg & 0xff000000) >> 24;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD,
	       "Queue SDIO PULL CFG Command(0x%x), PullUp=0x%x%x,PullDown=0x%x%x\n",
	       __le16_to_cpu(pcmd->ocf_ogf), pcmd->data[1], pcmd->data[0],
	       pcmd->data[3], pcmd->data[2]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: SDIO PULL CFG timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_SDIO_PULL_CFG_REQ);
	}
exit:
	LEAVE();
	return ret;
}

#ifdef SDIO_SUSPEND_RESUME
/**
 *  @brief This function set FM interrupt mask
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @param priv    FM interrupt mask value
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
fm_set_intr_mask(bt_private *priv, u32 mask)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;

	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf = __cpu_to_le16((VENDOR_OGF << 10) | FM_CMD);
	pcmd->length = 0x05;
	pcmd->data[0] = FM_SET_INTR_MASK;
	PRINTM(CMD, "FM set intr mask=0x%x\n", mask);
	mask = __cpu_to_le32(mask);
	memcpy(&pcmd->data[1], &mask, sizeof(mask));
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[FM_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "FM: set intr mask=%d timeout\n",
		       (int)__cpu_to_le32(mask));
		bt_cmd_timeout_func(priv, FM_CMD);
	}
exit:
	LEAVE();
	return ret;
}
#endif

/**
 *  @brief This function enables host sleep
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_hs(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	priv->adapter->suspend_fail = FALSE;
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_HOST_SLEEP_ENABLE);
	pcmd->length = 0;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	PRINTM(CMD, "Queue hs enable Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->hs_state,
	     WAIT_UNTIL_HS_STATE_CHANGED)) {
		PRINTM(MSG, "BT: Enable host sleep timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_HOST_SLEEP_ENABLE);
	}
	OS_INT_DISABLE;
	if ((priv->adapter->hs_state == HS_ACTIVATED) ||
	    (priv->adapter->is_suspended == TRUE)) {
		OS_INT_RESTORE;
		PRINTM(MSG, "BT: suspend success! skip=%d\n",
		       priv->adapter->hs_skip);
	} else {
		priv->adapter->suspend_fail = TRUE;
		OS_INT_RESTORE;
		priv->adapter->hs_skip++;
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG,
		       "BT: suspend skipped! "
		       "state=%d skip=%d ps_state= %d WakeupTries=%d\n",
		       priv->adapter->hs_state, priv->adapter->hs_skip,
		       priv->adapter->ps_state, priv->adapter->WakeupTries);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function Set Evt Filter
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_evt_filter(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf = __cpu_to_le16((0x03 << 10) | BT_CMD_SET_EVT_FILTER);
	pcmd->length = 0x03;
	pcmd->data[0] = 0x02;
	pcmd->data[1] = 0x00;
	pcmd->data[2] = 0x03;
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue Set Evt Filter Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set Evt Filter timeout\n");
		bt_cmd_timeout_func(priv, BT_CMD_SET_EVT_FILTER);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function Enable Write Scan - Page and Inquiry
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_write_scan(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf = __cpu_to_le16((0x03 << 10) | BT_CMD_ENABLE_WRITE_SCAN);
	pcmd->length = 0x01;
	pcmd->data[0] = 0x03;
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue Enable Write Scan Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Enable Write Scan timeout\n");
		bt_cmd_timeout_func(priv, BT_CMD_ENABLE_WRITE_SCAN);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function Enable Device under test mode
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_device_under_testmode(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((0x06 << 10) | BT_CMD_ENABLE_DEVICE_TESTMODE);
	pcmd->length = 0x00;
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue enable device under testmode Command(0x%x)\n",
	       __le16_to_cpu(pcmd->ocf_ogf));
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Enable Device under TEST mode timeout\n");
		bt_cmd_timeout_func(priv, BT_CMD_ENABLE_DEVICE_TESTMODE);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function enables test mode and send cmd
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_test_mode(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;

	ENTER();

	/** Set Evt Filter Command */
	ret = bt_set_evt_filter(priv);
	if (ret != BT_STATUS_SUCCESS) {
		PRINTM(ERROR, "BT test_mode: Set Evt filter fail\n");
		goto exit;
	}

	/** Enable Write Scan Command */
	ret = bt_enable_write_scan(priv);
	if (ret != BT_STATUS_SUCCESS) {
		PRINTM(ERROR, "BT test_mode: Enable Write Scan fail\n");
		goto exit;
	}

	/** Enable Device under test mode */
	ret = bt_enable_device_under_testmode(priv);
	if (ret != BT_STATUS_SUCCESS)
		PRINTM(ERROR,
		       "BT test_mode: Enable device under testmode fail\n");

exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function set GPIO pin
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_gpio_pin(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	/**Interrupt falling edge **/
	u8 gpio_int_edge = INT_FALLING_EDGE;
	/**Delay 50 usec **/
	u8 gpio_pulse_width = DELAY_50_US;
	BT_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf = __cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_SET_GPIO_PIN);
	pcmd->data[0] = mbt_gpio_pin;
	pcmd->data[1] = gpio_int_edge;
	pcmd->data[2] = gpio_pulse_width;
	pcmd->length = 3;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set GPIO pin: timeout!\n");
		bt_cmd_timeout_func(priv, BT_CMD_SET_GPIO_PIN);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sets ble deepsleep mode
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mode    TRUE/FALSE
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_ble_deepsleep(bt_private *priv, int mode)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_BLE_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_BLE_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_BLE_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_BLE_DEEP_SLEEP);
	pcmd->length = 1;
	pcmd->deepsleep = mode;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_BLE_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set BLE deepsleep = %d (0x%x)\n", mode,
	       __le16_to_cpu(pcmd->ocf_ogf));
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set BLE deepsleep timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_BLE_DEEP_SLEEP);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function gets FW version
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_get_fw_version(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_HCI_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_GET_FW_VERSION);
	pcmd->length = 0x01;
	pcmd->cmd_type = 0x00;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, 4);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Get FW version: timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_GET_FW_VERSION);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sets mac address
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mac     A pointer to mac address
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_mac_address(bt_private *priv, u8 *mac)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pcmd;
	int i = 0;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_HCI_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_CONFIG_MAC_ADDR);
	pcmd->length = 8;
	pcmd->cmd_type = MRVL_VENDOR_PKT;
	pcmd->cmd_len = 6;
	for (i = 0; i < 6; i++)
		pcmd->data[i] = mac[5 - i];
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_HCI_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set mac addr " MACSTR " (0x%x)\n", MAC2STR(mac),
	       __le16_to_cpu(pcmd->ocf_ogf));
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set mac addr: timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_CONFIG_MAC_ADDR);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function load the calibrate data
 *
 *  @param priv    A pointer to bt_private structure
 *  @param config_data     A pointer to calibrate data
 *  @param mac     A pointer to mac address
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_load_cal_data(bt_private *priv, u8 *config_data, u8 *mac)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;
	int i = 0;
	/* u8 config_data[28] = {0x37 0x01 0x1c 0x00 0xFF 0xFF 0xFF 0xFF 0x01
	   0x7f 0x04 0x02 0x00 0x00 0xBA 0xCE 0xC0 0xC6 0x2D 0x00 0x00 0x00
	   0x00 0x00 0x00 0x00 0xF0}; */

	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_LOAD_CONFIG_DATA);
	pcmd->length = 0x20;
	pcmd->data[0] = 0x00;
	pcmd->data[1] = 0x00;
	pcmd->data[2] = 0x00;
	pcmd->data[3] = 0x1C;
	/* swip cal-data byte */
	for (i = 4; i < 32; i++)
		pcmd->data[i] = config_data[(i / 4) * 8 - 1 - i];
	if (mac != NULL) {
		pcmd->data[2] = 0x01;	/* skip checksum */
		for (i = 24; i < 30; i++)
			pcmd->data[i] = mac[29 - i];
	}
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;

	DBG_HEXDUMP(DAT_D, "calirate data: ", pcmd->data, 32);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(ERROR, "BT: Load calibrate data: timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_LOAD_CONFIG_DATA);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function load the calibrate EXT data
 *
 *  @param priv    A pointer to bt_private structure
 *  @param config_data     A pointer to calibrate data
 *  @param mac     A pointer to mac address
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_load_cal_data_ext(bt_private *priv, u8 *config_data, u32 cfg_data_len)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CMD *pcmd;

	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_LOAD_CONFIG_DATA_EXT);
	pcmd->length = cfg_data_len;

	memcpy(pcmd->data, config_data, cfg_data_len);
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, BT_CMD_HEADER_SIZE + pcmd->length);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;

	DBG_HEXDUMP(DAT_D, "calirate ext data", pcmd->data, pcmd->length);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(ERROR, "BT: Load calibrate ext data: timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_LOAD_CONFIG_DATA_EXT);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function writes value to CSU registers
 *
 *  @param priv    A pointer to bt_private structure
 *  @param type    reg type
 *  @param offset  register address
 *  @param value   register value to write
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_write_reg(bt_private *priv, u8 type, u32 offset, u16 value)
{
	struct sk_buff *skb = NULL;
	int ret = BT_STATUS_SUCCESS;
	BT_CSU_CMD *pcmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CSU_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pcmd = (BT_CSU_CMD *)skb->data;
	pcmd->ocf_ogf =
		__cpu_to_le16((VENDOR_OGF << 10) | BT_CMD_CSU_WRITE_REG);
	pcmd->length = 7;
	pcmd->type = type;
	pcmd->offset[0] = (offset & 0x000000ff);
	pcmd->offset[1] = (offset & 0x0000ff00) >> 8;
	pcmd->offset[2] = (offset & 0x00ff0000) >> 16;
	pcmd->offset[3] = (offset & 0xff000000) >> 24;
	pcmd->value[0] = (value & 0x00ff);
	pcmd->value[1] = (value & 0xff00) >> 8;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CSU_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_opcode = pcmd->ocf_ogf;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set CSU reg type=%d reg=0x%x value=0x%x\n",
	       type, offset, value);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(ERROR, "BT: Set CSU reg timeout:\n");
		bt_cmd_timeout_func(priv, BT_CMD_CSU_WRITE_REG);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function used to restore tx_queue
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        N/A
 */
void
bt_restore_tx_queue(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	while (!skb_queue_empty(&priv->adapter->pending_queue)) {
		skb = skb_dequeue(&priv->adapter->pending_queue);
		if (skb)
			skb_queue_tail(&priv->adapter->tx_queue, skb);
	}
	wake_up_interruptible(&priv->MainThread.waitQ);
}

/**
 *  @brief This function used to send command to firmware
 *
 *  Command format:
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     OCF OGF     | Length |                Data               |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     2-byte      | 1-byte |               4-byte              |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_prepare_command(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	if (priv->bt_dev.hscfgcmd) {
		priv->bt_dev.hscfgcmd = 0;
		ret = bt_send_hscfg_cmd(priv);
	}
	if (priv->bt_dev.pscmd) {
		priv->bt_dev.pscmd = 0;
		ret = bt_enable_ps(priv);
	}
	if (priv->bt_dev.sdio_pull_ctrl) {
		priv->bt_dev.sdio_pull_ctrl = 0;
		ret = bt_send_sdio_pull_ctrl_cmd(priv);
	}
	if (priv->bt_dev.hscmd) {
		priv->bt_dev.hscmd = 0;
		if (priv->bt_dev.hsmode)
			ret = bt_enable_hs(priv);
		else {
			ret = sbi_wakeup_firmware(priv);
			priv->adapter->hs_state = HS_DEACTIVATED;
		}
	}
	if (priv->bt_dev.test_mode) {
		priv->bt_dev.test_mode = 0;
		ret = bt_enable_test_mode(priv);
	}
	LEAVE();
	return ret;
}

/** @brief This function processes a single packet
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to skb which includes TX packet
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
send_single_packet(bt_private *priv, struct sk_buff *skb)
{
	int ret;
	int has_realloc = 0;
	ENTER();
	if (!skb || !skb->data) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	if (!skb->len || ((skb->len + BT_HEADER_LEN) > BT_UPLD_SIZE)) {
		PRINTM(ERROR, "Tx Error: Bad skb length %d : %d\n", skb->len,
		       BT_UPLD_SIZE);
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	if (skb_headroom(skb) < BT_HEADER_LEN) {
		skb = skb_realloc_headroom(skb, BT_HEADER_LEN);
		if (!skb) {
			PRINTM(ERROR, "TX error: realloc_headroom failed %d\n",
			       BT_HEADER_LEN);
			LEAVE();
			return BT_STATUS_FAILURE;
		}
		has_realloc = 1;
	}
	/* This is SDIO specific header length: byte[3][2][1], * type: byte[0]
	   (HCI_COMMAND = 1, ACL_DATA = 2, SCO_DATA = 3, 0xFE = Vendor) */
	skb_push(skb, BT_HEADER_LEN);
	skb->data[0] = (skb->len & 0x0000ff);
	skb->data[1] = (skb->len & 0x00ff00) >> 8;
	skb->data[2] = (skb->len & 0xff0000) >> 16;
	skb->data[3] = bt_cb(skb)->pkt_type;
	if (bt_cb(skb)->pkt_type == MRVL_VENDOR_PKT)
		PRINTM(CMD, "DNLD_CMD: ocf_ogf=0x%x len=%d\n",
		       __le16_to_cpu(*((u16 *) & skb->data[4])), skb->len);
	ret = sbi_host_to_card(priv, skb->data, skb->len);
	if (has_realloc)
		kfree_skb(skb);
	LEAVE();
	return ret;
}

#ifdef CONFIG_OF
/**
 *  @brief This function read the initial parameter from device tress
 *
 *
 *  @return         N/A
 */
static void
bt_init_from_dev_tree(void)
{
	struct device_node *dt_node = NULL;
	struct property *prop;
	u32 data;
	const char *string_data;

	ENTER();

	if (!dts_enable) {
		PRINTM(CMD, "DTS is disabled!");
		return;
	}

	dt_node = of_find_node_by_name(NULL, "sd8xxx-bt");
	if (!dt_node) {
		LEAVE();
		return;
	}
	for_each_property_of_node(dt_node, prop) {
		if (!strncmp(prop->name, "mbt_drvdbg", strlen("mbt_drvdbg"))) {
			if (!of_property_read_u32(dt_node, prop->name, &data)) {
				PRINTM(CMD, "mbt_drvdbg=0x%x\n", data);
				mbt_drvdbg = data;
			}
		} else if (!strncmp(prop->name, "init_cfg", strlen("init_cfg"))) {
			if (!of_property_read_string
			    (dt_node, prop->name, &string_data)) {
				init_cfg = (char *)string_data;
				PRINTM(CMD, "init_cfg=%s\n", init_cfg);
			}
		} else if (!strncmp
			   (prop->name, "cal_cfg_ext", strlen("cal_cfg_ext"))) {
			if (!of_property_read_string
			    (dt_node, prop->name, &string_data)) {
				cal_cfg_ext = (char *)string_data;
				PRINTM(CMD, "cal_cfg_ext=%s\n", cal_cfg_ext);
			}
		} else if (!strncmp(prop->name, "cal_cfg", strlen("cal_cfg"))) {
			if (!of_property_read_string
			    (dt_node, prop->name, &string_data)) {
				cal_cfg = (char *)string_data;
				PRINTM(CMD, "cal_cfg=%s\n", cal_cfg);
			}
		} else if (!strncmp(prop->name, "bt_mac", strlen("bt_mac"))) {
			if (!of_property_read_string
			    (dt_node, prop->name, &string_data)) {
				bt_mac = (char *)string_data;
				PRINTM(CMD, "bt_mac=%s\n", bt_mac);
			}
		} else if (!strncmp
			   (prop->name, "mbt_gpio_pin",
			    strlen("mbt_gpio_pin"))) {
			if (!of_property_read_u32(dt_node, prop->name, &data)) {
				mbt_gpio_pin = data;
				PRINTM(CMD, "mbt_gpio_pin=%d\n", mbt_gpio_pin);
			}
		}
	}
	LEAVE();
	return;
}
#endif

/**
 *  @brief This function initializes the adapter structure
 *  and set default value to the member of adapter.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    N/A
 */
static void
bt_init_adapter(bt_private *priv)
{
	ENTER();
#ifdef CONFIG_OF
	bt_init_from_dev_tree();
#endif
	skb_queue_head_init(&priv->adapter->tx_queue);
	skb_queue_head_init(&priv->adapter->pending_queue);
	priv->adapter->tx_lock = FALSE;
	priv->adapter->ps_state = PS_AWAKE;
	priv->adapter->suspend_fail = FALSE;
	priv->adapter->is_suspended = FALSE;
	priv->adapter->hs_skip = 0;
	priv->adapter->num_cmd_timeout = 0;
	init_waitqueue_head(&priv->adapter->cmd_wait_q);
	LEAVE();
}

/**
 *  @brief This function initializes firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_init_fw(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	if (fw == 0) {
		sd_enable_host_int(priv);
		goto done;
	}
	sd_disable_host_int(priv);
	if ((priv->card_type == CARD_TYPE_SD8787) ||
	    (priv->card_type == CARD_TYPE_SD8777))
		priv->fw_crc_check = fw_crc_check;
	if (sbi_download_fw(priv)) {
		PRINTM(ERROR, " FW failed to be download!\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function frees the structure of adapter
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    N/A
 */
void
bt_free_adapter(bt_private *priv)
{
	bt_adapter *adapter = priv->adapter;
	ENTER();
	skb_queue_purge(&priv->adapter->tx_queue);
	kfree(adapter->tx_buffer);
	kfree(adapter->hw_regs_buf);
	/* Free the adapter object itself */
	kfree(adapter);
	priv->adapter = NULL;

	LEAVE();
}

/**
 *  @brief This function handles the wrapper_dev ioctl
 *
 *  @param hev     A pointer to wrapper_dev structure
 *  @cmd            ioctl cmd
 *  @arg            argument
 *  @return    -ENOIOCTLCMD
 */
static int
mdev_ioctl(struct m_dev *m_dev, unsigned int cmd, void *arg)
{
	ENTER();
	LEAVE();
	return -ENOIOCTLCMD;
}

/**
 *  @brief This function handles wrapper device destruct
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    N/A
 */
static void
mdev_destruct(struct m_dev *m_dev)
{
	ENTER();
	LEAVE();
	return;
}

/**
 *  @brief This function handles the wrapper device transmit
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @param skb     A pointer to sk_buff structure
 *
 *  @return    BT_STATUS_SUCCESS or other error no.
 */
static int
mdev_send_frame(struct m_dev *m_dev, struct sk_buff *skb)
{
	bt_private *priv = NULL;

	ENTER();
	if (!m_dev || !m_dev->driver_data) {
		PRINTM(ERROR, "Frame for unknown HCI device (m_dev=NULL)\n");
		LEAVE();
		return -ENODEV;
	}
	priv = (bt_private *)m_dev->driver_data;
	if (!test_bit(HCI_RUNNING, &m_dev->flags)) {
		PRINTM(ERROR, "Fail test HCI_RUNNING, flag=0x%lx\n",
		       m_dev->flags);
		LEAVE();
		return -EBUSY;
	}
	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		m_dev->stat.cmd_tx++;
		break;
	case HCI_ACLDATA_PKT:
		m_dev->stat.acl_tx++;
		break;
	case HCI_SCODATA_PKT:
		m_dev->stat.sco_tx++;
		break;
	}

	if (m_dev->dev_type == DEBUG_TYPE) {
		/* remember the ogf_ocf */
		priv->debug_device_pending = 1;
		priv->debug_ocf_ogf[0] = skb->data[0];
		priv->debug_ocf_ogf[1] = skb->data[1];
		PRINTM(CMD, "debug_ocf_ogf[0]=0x%x debug_ocf_ogf[1]=0x%x\n",
		       priv->debug_ocf_ogf[0], priv->debug_ocf_ogf[1]);
	}

	if (priv->adapter->tx_lock == TRUE)
		skb_queue_tail(&priv->adapter->pending_queue, skb);
	else
		skb_queue_tail(&priv->adapter->tx_queue, skb);
	wake_up_interruptible(&priv->MainThread.waitQ);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function flushes the transmit queue
 *
 *  @param m_dev     A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS
 */
static int
mdev_flush(struct m_dev *m_dev)
{
	bt_private *priv = (bt_private *)m_dev->driver_data;
	ENTER();
	skb_queue_purge(&priv->adapter->tx_queue);
	skb_queue_purge(&priv->adapter->pending_queue);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function closes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS
 */
static int
mdev_close(struct m_dev *m_dev)
{
	ENTER();
	mdev_req_lock(m_dev);
	if (!test_and_clear_bit(HCI_UP, &m_dev->flags)) {
		mdev_req_unlock(m_dev);
		LEAVE();
		return 0;
	}

	if (m_dev->flush)
		m_dev->flush(m_dev);
	/* wait up pending read and unregister char dev */
	wake_up_interruptible(&m_dev->req_wait_q);
	/* Drop queues */
	skb_queue_purge(&m_dev->rx_q);

	if (!test_and_clear_bit(HCI_RUNNING, &m_dev->flags)) {
		mdev_req_unlock(m_dev);
		LEAVE();
		return 0;
	}
	module_put(THIS_MODULE);
	m_dev->flags = 0;
	mdev_req_unlock(m_dev);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function opens the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
static int
mdev_open(struct m_dev *m_dev)
{
	ENTER();

	if (try_module_get(THIS_MODULE) == 0)
		return BT_STATUS_FAILURE;

	set_bit(HCI_RUNNING, &m_dev->flags);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function queries the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @param arg     arguement
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
void
mdev_query(struct m_dev *m_dev, void *arg)
{
	struct mbt_dev *mbt_dev = (struct mbt_dev *)m_dev->dev_pointer;

	ENTER();
	if (copy_to_user(arg, &mbt_dev->type, sizeof(mbt_dev->type)))
		PRINTM(ERROR, "IOCTL_QUERY_TYPE: Fail copy to user\n");

	LEAVE();
}

/**
 *  @brief This function initializes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
void
init_m_dev(struct m_dev *m_dev)
{
	m_dev->dev_pointer = NULL;
	m_dev->driver_data = NULL;
	m_dev->dev_type = 0;
	m_dev->spec_type = 0;
	skb_queue_head_init(&m_dev->rx_q);
	init_waitqueue_head(&m_dev->req_wait_q);
	init_waitqueue_head(&m_dev->rx_wait_q);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
	init_MUTEX(&m_dev->req_lock);
#else
	sema_init(&m_dev->req_lock, 1);
#endif
	memset(&m_dev->stat, 0, sizeof(struct hci_dev_stats));
	m_dev->open = mdev_open;
	m_dev->close = mdev_close;
	m_dev->flush = mdev_flush;
	m_dev->send = mdev_send_frame;
	m_dev->destruct = mdev_destruct;
	m_dev->ioctl = mdev_ioctl;
	m_dev->query = mdev_query;
	m_dev->owner = THIS_MODULE;

}

/**
 *  @brief This function handles the major job in bluetooth driver.
 *  it handles the event generated by firmware, rx data received
 *  from firmware and tx data sent from kernel.
 *
 *  @param data    A pointer to bt_thread structure
 *  @return        BT_STATUS_SUCCESS
 */
static int
bt_service_main_thread(void *data)
{
	bt_thread *thread = data;
	bt_private *priv = thread->priv;
	bt_adapter *adapter = priv->adapter;
	wait_queue_t wait;
	struct sk_buff *skb;
	ENTER();
	bt_activate_thread(thread);
	init_waitqueue_entry(&wait, current);
	current->flags |= PF_NOFREEZE;

	for (;;) {
		add_wait_queue(&thread->waitQ, &wait);
		OS_SET_THREAD_STATE(TASK_INTERRUPTIBLE);
		if (priv->adapter->WakeupTries ||
		    ((!priv->adapter->IntCounter) &&
		     (!priv->bt_dev.tx_dnld_rdy ||
		      skb_queue_empty(&priv->adapter->tx_queue)))) {
			PRINTM(INFO, "Main: Thread sleeping...\n");
			schedule();
		}
		OS_SET_THREAD_STATE(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);
		if (kthread_should_stop() || adapter->SurpriseRemoved) {
			PRINTM(INFO, "main-thread: break from main thread: "
			       "SurpriseRemoved=0x%x\n",
			       adapter->SurpriseRemoved);
			break;
		}

		PRINTM(INFO, "Main: Thread waking up...\n");

		if (priv->adapter->IntCounter) {
			OS_INT_DISABLE;
			adapter->IntCounter = 0;
			OS_INT_RESTORE;
			sbi_get_int_status(priv);
		} else if ((priv->adapter->ps_state == PS_SLEEP) &&
			   !skb_queue_empty(&priv->adapter->tx_queue)) {
			priv->adapter->WakeupTries++;
			sbi_wakeup_firmware(priv);
			continue;
		}
		if (priv->adapter->ps_state == PS_SLEEP)
			continue;
		if (priv->bt_dev.tx_dnld_rdy == TRUE) {
			if (!skb_queue_empty(&priv->adapter->tx_queue)) {
				skb = skb_dequeue(&priv->adapter->tx_queue);
				if (skb) {
					if (send_single_packet(priv, skb))
						((struct m_dev *)skb->dev)->
							stat.err_tx++;
					else
						((struct m_dev *)skb->dev)->
							stat.byte_tx +=
							skb->len;
					kfree_skb(skb);
				}
			}
		}
	}
	bt_deactivate_thread(thread);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the interrupt. it will change PS
 *  state if applicable. it will wake up main_thread to handle
 *  the interrupt event as well.
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @return        N/A
 */
void
bt_interrupt(struct m_dev *m_dev)
{
	bt_private *priv = (bt_private *)m_dev->driver_data;
	ENTER();
	if (!priv || !priv->adapter) {
		LEAVE();
		return;
	}
	PRINTM(INTR, "*\n");
	priv->adapter->ps_state = PS_AWAKE;
	if (priv->adapter->hs_state == HS_ACTIVATED) {
		PRINTM(CMD, "BT: %s: HS DEACTIVATED in ISR!\n", m_dev->name);
		priv->adapter->hs_state = HS_DEACTIVATED;
	}
	priv->adapter->WakeupTries = 0;
	priv->adapter->IntCounter++;
	wake_up_interruptible(&priv->MainThread.waitQ);
	LEAVE();
}

static void
char_dev_release_dynamic(struct kobject *kobj)
{
	struct char_dev *cdev = container_of(kobj, struct char_dev, kobj);
	ENTER();
	PRINTM(INFO, "free char_dev\n");
	kfree(cdev);
	LEAVE();
}

static struct kobj_type ktype_char_dev_dynamic = {
	.release = char_dev_release_dynamic,
};

static struct char_dev *
alloc_char_dev(void)
{
	struct char_dev *cdev;
	ENTER();
	cdev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
	if (cdev) {
		kobject_init(&cdev->kobj, &ktype_char_dev_dynamic);
		PRINTM(INFO, "alloc char_dev\n");
	}
	return cdev;
}

static void
bt_private_dynamic_release(struct kobject *kobj)
{
	bt_private *priv = container_of(kobj, bt_private, kobj);
	ENTER();
	PRINTM(INFO, "free bt priv\n");
	kfree(priv);
	LEAVE();
}

static struct kobj_type ktype_bt_private_dynamic = {
	.release = bt_private_dynamic_release,
};

static bt_private *
bt_alloc_priv(void)
{
	bt_private *priv;
	ENTER();
	priv = kzalloc(sizeof(bt_private), GFP_KERNEL);
	if (priv) {
		kobject_init(&priv->kobj, &ktype_bt_private_dynamic);
		PRINTM(INFO, "alloc bt priv\n");
	}
	LEAVE();
	return priv;
}

struct kobject *
bt_priv_get(bt_private *priv)
{
	PRINTM(INFO, "bt priv get object");
	return kobject_get(&priv->kobj);
}

void
bt_priv_put(bt_private *priv)
{
	PRINTM(INFO, "bt priv put object");
	kobject_put(&priv->kobj);
}

/**
 *  @brief Module configuration and register device
 *
 *  @param priv      A Pointer to bt_private structure
 *  @return      BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_register_conf_dpc(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	struct mbt_dev *mbt_dev = NULL;
	struct fm_dev *fm_dev = NULL;
	struct nfc_dev *nfc_dev = NULL;
	struct debug_dev *debug_dev = NULL;
	int i = 0;
	struct char_dev *char_dev = NULL;
	char dev_file[DEV_NAME_LEN + 5];
	unsigned char dev_type = 0;

	ENTER();

	priv->bt_dev.tx_dnld_rdy = TRUE;

	if (drv_mode & DRV_MODE_BT) {
		mbt_dev = alloc_mbt_dev();
		if (!mbt_dev) {
			PRINTM(FATAL, "Can not allocate mbt dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		init_m_dev(&(priv->bt_dev.m_dev[BT_SEQ]));
		priv->bt_dev.m_dev[BT_SEQ].dev_type = BT_TYPE;
		priv->bt_dev.m_dev[BT_SEQ].spec_type = IANYWHERE_SPEC;
		priv->bt_dev.m_dev[BT_SEQ].dev_pointer = (void *)mbt_dev;
		priv->bt_dev.m_dev[BT_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[BT_SEQ].read_continue_flag = 0;
	}

	dev_type = HCI_SDIO;

	if (mbt_dev)
		mbt_dev->type = dev_type;
	if (mbt_gpio_pin) {
		ret = bt_set_gpio_pin(priv);
		if (ret < 0) {
			PRINTM(FATAL, "GPIO pin set failed!\n");
			goto done;
		}
	}

	ret = bt_send_module_cfg_cmd(priv, MODULE_BRINGUP_REQ);
	if (ret < 0) {
		PRINTM(FATAL, "Module cfg command send failed!\n");
		goto done;
	}
	ret = bt_set_ble_deepsleep(priv, TRUE);
	if (ret < 0) {
		PRINTM(FATAL, "Enable BLE deepsleep failed!\n");
		goto done;
	}
	if (psmode) {
		priv->bt_dev.psmode = TRUE;
		priv->bt_dev.idle_timeout = DEFAULT_IDLE_TIME;
		ret = bt_enable_ps(priv);
		if (ret < 0) {
			PRINTM(FATAL, "Enable PS mode failed!\n");
			goto done;
		}
	}
#ifdef SDIO_SUSPEND_RESUME
	priv->bt_dev.gpio_gap = 0xffff;
	ret = bt_send_hscfg_cmd(priv);
	if (ret < 0) {
		PRINTM(FATAL, "Send HSCFG failed!\n");
		goto done;
	}
#endif
	priv->bt_dev.sdio_pull_cfg = 0xffffffff;
	priv->bt_dev.sdio_pull_ctrl = 0;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (mbt_dev && priv->bt_dev.devType == DEV_TYPE_AMP) {
		mbt_dev->type |= HCI_BT_AMP;
		priv->bt_dev.m_dev[BT_SEQ].dev_type = BT_AMP_TYPE;
	}
	/** Process device tree init parameters before register hci device.
	 *  Since uplayer device has not yet registered, no need to block tx queue.
	 * */
	if (init_cfg)
		if (BT_STATUS_SUCCESS != bt_init_config(priv, init_cfg)) {
			PRINTM(FATAL,
			       "BT: Set user init data and param failed\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
	if (cal_cfg) {
		if (BT_STATUS_SUCCESS != bt_cal_config(priv, cal_cfg, bt_mac)) {
			PRINTM(FATAL, "BT: Set cal data failed\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
	} else if (bt_mac) {
		PRINTM(INFO,
		       "Set BT mac_addr from insmod parametre bt_mac = %s\n",
		       bt_mac);
		if (BT_STATUS_SUCCESS != bt_init_mac_address(priv, bt_mac)) {
			PRINTM(FATAL,
			       "BT: Fail to set mac address from insmod parametre\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
	}
	if (cal_cfg_ext && (priv->card_type == CARD_TYPE_SD8787)
		) {
		if (BT_STATUS_SUCCESS != bt_cal_config_ext(priv, cal_cfg_ext)) {
			PRINTM(FATAL, "BT: Set cal ext data failed\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
	}

	if (mbt_dev) {
		/** init mbt_dev */
		mbt_dev->flags = 0;
		mbt_dev->pkt_type = (HCI_DM1 | HCI_DH1 | HCI_HV1);
		mbt_dev->esco_type = (ESCO_HV1);
		mbt_dev->link_mode = (HCI_LM_ACCEPT);

		mbt_dev->idle_timeout = 0;
		mbt_dev->sniff_max_interval = 800;
		mbt_dev->sniff_min_interval = 80;
		for (i = 0; i < 3; i++)
			mbt_dev->reassembly[i] = NULL;
		atomic_set(&mbt_dev->promisc, 0);

		/** alloc char dev node */
		char_dev = alloc_char_dev();
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = MBTCHAR_MINOR_BASE + mbtchar_minor;
		if (mbt_dev->type & HCI_BT_AMP)
			char_dev->dev_type = BT_AMP_TYPE;
		else
			char_dev->dev_type = BT_TYPE;

		if (bt_name)
			snprintf(mbt_dev->name, sizeof(mbt_dev->name), "%s%d",
				 bt_name, mbtchar_minor);
		else
			snprintf(mbt_dev->name, sizeof(mbt_dev->name),
				 "mbtchar%d", mbtchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", mbt_dev->name);
		mbtchar_minor++;
		PRINTM(MSG, "BT: Create %s\n", dev_file);

		/** register m_dev to BT char device */
		priv->bt_dev.m_dev[BT_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);

		/** create BT char device node */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  mbt_dev->name);

		/** chmod & chown for BT char device */
		mbtchar_chown(dev_file, AID_SYSTEM, AID_NET_BT_STACK);
		mbtchar_chmod(dev_file, 0660);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[BT_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[BT_SEQ].name),
			 mbt_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[BT_SEQ]), BT_SEQ);
	}

	if ((drv_mode & DRV_MODE_FM) &&
	    (!(priv->bt_dev.devType == DEV_TYPE_AMP)) &&
	    (priv->bt_dev.devFeature & DEV_FEATURE_FM)) {

		/** alloc fm_dev */
		fm_dev = alloc_fm_dev();
		if (!fm_dev) {
			PRINTM(FATAL, "Can not allocate fm dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[FM_SEQ]));
		priv->bt_dev.m_dev[FM_SEQ].dev_type = FM_TYPE;
		priv->bt_dev.m_dev[FM_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[FM_SEQ].dev_pointer = (void *)fm_dev;
		priv->bt_dev.m_dev[FM_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[FM_SEQ].read_continue_flag = 0;

		/** create char device for FM */
		char_dev = alloc_char_dev();
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = FMCHAR_MINOR_BASE + fmchar_minor;
		char_dev->dev_type = FM_TYPE;

		if (fm_name)
			snprintf(fm_dev->name, sizeof(fm_dev->name), "%s%d",
				 fm_name, fmchar_minor);
		else
			snprintf(fm_dev->name, sizeof(fm_dev->name),
				 "mfmchar%d", fmchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", fm_dev->name);
		PRINTM(MSG, "BT: Create %s\n", dev_file);
		fmchar_minor++;

		/** register m_dev to FM char device */
		priv->bt_dev.m_dev[FM_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[FM_SEQ]);

		/** register char dev */
		register_char_dev(char_dev, chardev_class,
				  MODULE_NAME, fm_dev->name);

		/** chmod for FM char device */
		mbtchar_chmod(dev_file, 0660);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[FM_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[FM_SEQ].name), fm_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[FM_SEQ]), FM_SEQ);
	}

	if ((drv_mode & DRV_MODE_NFC) &&
	    (!(priv->bt_dev.devType == DEV_TYPE_AMP)) &&
	    (priv->bt_dev.devFeature & DEV_FEATURE_NFC)) {

		/** alloc nfc_dev */
		nfc_dev = alloc_nfc_dev();
		if (!nfc_dev) {
			PRINTM(FATAL, "Can not allocate nfc dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[NFC_SEQ]));
		priv->bt_dev.m_dev[NFC_SEQ].dev_type = NFC_TYPE;
		priv->bt_dev.m_dev[NFC_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[NFC_SEQ].dev_pointer = (void *)nfc_dev;
		priv->bt_dev.m_dev[NFC_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[NFC_SEQ].read_continue_flag = 0;

		/** create char device for NFC */
		char_dev = alloc_char_dev();
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = NFCCHAR_MINOR_BASE + nfcchar_minor;
		char_dev->dev_type = NFC_TYPE;
		if (nfc_name)
			snprintf(nfc_dev->name, sizeof(nfc_dev->name), "%s%d",
				 nfc_name, nfcchar_minor);
		else
			snprintf(nfc_dev->name, sizeof(nfc_dev->name),
				 "mnfcchar%d", nfcchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", nfc_dev->name);
		PRINTM(MSG, "BT: Create %s\n", dev_file);
		nfcchar_minor++;

		/** register m_dev to NFC char device */
		priv->bt_dev.m_dev[NFC_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[NFC_SEQ]);

		/** register char dev */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  nfc_dev->name);

		/** chmod for NFC char device */
		mbtchar_chmod(dev_file, 0666);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[NFC_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[NFC_SEQ].name),
			 nfc_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[NFC_SEQ]), NFC_SEQ);
	}

	if ((debug_intf) &&
	    ((drv_mode & DRV_MODE_BT) ||
	     (drv_mode & DRV_MODE_FM) || (drv_mode & DRV_MODE_NFC))) {
		/** alloc debug_dev */
		debug_dev = alloc_debug_dev();
		if (!debug_dev) {
			PRINTM(FATAL, "Can not allocate debug dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[DEBUG_SEQ]));
		priv->bt_dev.m_dev[DEBUG_SEQ].dev_type = DEBUG_TYPE;
		priv->bt_dev.m_dev[DEBUG_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer = (void *)debug_dev;
		priv->bt_dev.m_dev[DEBUG_SEQ].driver_data = priv;

		/** create char device for Debug */
		char_dev = alloc_char_dev();
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = DEBUGCHAR_MINOR_BASE + debugchar_minor;
		char_dev->dev_type = DEBUG_TYPE;
		if (debug_name)
			snprintf(debug_dev->name, sizeof(debug_dev->name),
				 "%s%d", debug_name, debugchar_minor);
		else
			snprintf(debug_dev->name, sizeof(debug_dev->name),
				 "mdebugchar%d", debugchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s",
			 debug_dev->name);
		PRINTM(MSG, "BT: Create %s\n", dev_file);
		debugchar_minor++;

		/** register char dev */
		priv->bt_dev.m_dev[DEBUG_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[DEBUG_SEQ]);
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  debug_dev->name);

		/** chmod for debug char device */
		mbtchar_chmod(dev_file, 0666);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[DEBUG_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[DEBUG_SEQ].name),
			 debug_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[DEBUG_SEQ]), DEBUG_SEQ);
	}

	/* Get FW version */
	bt_get_fw_version(priv);
	snprintf((char *)priv->adapter->drv_ver, MAX_VER_STR_LEN,
		 mbt_driver_version, fw_version);

done:
	LEAVE();
	return ret;
err_kmalloc:
	LEAVE();
	return ret;
}

/**
 *  @brief This function adds the card. it will probe the
 *  card, allocate the bt_priv and initialize the device.
 *
 *  @param card    A pointer to card
 *  @return        A pointer to bt_private structure
 */

bt_private *
bt_add_card(void *card)
{
	bt_private *priv = NULL;

	ENTER();

	priv = bt_alloc_priv();
	if (!priv) {
		PRINTM(FATAL, "Can not allocate priv\n");
		LEAVE();
		return NULL;
	}

	/* allocate buffer for bt_adapter */
	priv->adapter = kzalloc(sizeof(bt_adapter), GFP_KERNEL);
	if (!priv->adapter) {
		PRINTM(FATAL, "Allocate buffer for bt_adapter failed!\n");
		goto err_kmalloc;
	}
	priv->adapter->tx_buffer = kzalloc(MAX_TX_BUF_SIZE, GFP_KERNEL);
	if (!priv->adapter->tx_buffer) {
		PRINTM(FATAL, "Allocate buffer for transmit\n");
		goto err_kmalloc;
	}
	priv->adapter->tx_buf =
		(u8 *)ALIGN_ADDR(priv->adapter->tx_buffer, DMA_ALIGNMENT);
	priv->adapter->hw_regs_buf =
		kzalloc(SD_BLOCK_SIZE + DMA_ALIGNMENT, GFP_KERNEL);
	if (!priv->adapter->hw_regs_buf) {
		PRINTM(FATAL, "Allocate buffer for INT read buf failed!\n");
		goto err_kmalloc;
	}
	priv->adapter->hw_regs =
		(u8 *)ALIGN_ADDR(priv->adapter->hw_regs_buf, DMA_ALIGNMENT);
	bt_init_adapter(priv);

	PRINTM(INFO, "Starting kthread...\n");
	priv->MainThread.priv = priv;
	spin_lock_init(&priv->driver_lock);

	bt_create_thread(bt_service_main_thread, &priv->MainThread,
			 "bt_main_service");

	/* wait for mainthread to up */
	while (!priv->MainThread.pid)
		os_sched_timeout(1);

	sdio_update_card_type(priv, card);
	/* Update driver version */
	if (priv->card_type == CARD_TYPE_SD8787)
		memcpy(mbt_driver_version, CARD_SD8787, strlen(CARD_SD8787));
	else if (priv->card_type == CARD_TYPE_SD8777)
		memcpy(mbt_driver_version, CARD_SD8777, strlen(CARD_SD8777));
	else if (priv->card_type == CARD_TYPE_SD8887)
		memcpy(mbt_driver_version, CARD_SD8887, strlen(CARD_SD8887));
	else if (priv->card_type == CARD_TYPE_SD8897)
		memcpy(mbt_driver_version, CARD_SD8897, strlen(CARD_SD8897));
	else if (priv->card_type == CARD_TYPE_SD8797)
		memcpy(mbt_driver_version, CARD_SD8797, strlen(CARD_SD8797));
	else if (priv->card_type == CARD_TYPE_SD8977)
		memcpy(mbt_driver_version, CARD_SD8977, strlen(CARD_SD8977));

	if (BT_STATUS_SUCCESS != sdio_get_sdio_device(priv))
		goto err_kmalloc;

	/** user config file */
	init_waitqueue_head(&priv->init_user_conf_wait_q);

	priv->bt_dev.card = card;

	((struct sdio_mmc_card *)card)->priv = priv;
	priv->adapter->sd_ireg = 0;
	/*
	 * Register the device. Fillup the private data structure with
	 * relevant information from the card and request for the required
	 * IRQ.
	 */
	if (sbi_register_dev(priv) < 0) {
		PRINTM(FATAL, "Failed to register bt device!\n");
		goto err_registerdev;
	}
	if (bt_init_fw(priv)) {
		PRINTM(FATAL, "BT Firmware Init Failed\n");
		goto err_init_fw;
	}
	LEAVE();
	return priv;

err_init_fw:
	clean_up_m_devs(priv);
	bt_proc_remove(priv);
	PRINTM(INFO, "Unregister device\n");
	sbi_unregister_dev(priv);
err_registerdev:
	((struct sdio_mmc_card *)card)->priv = NULL;
	/* Stop the thread servicing the interrupts */
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid)
		os_sched_timeout(1);
err_kmalloc:
	if (priv->adapter)
		bt_free_adapter(priv);
	bt_priv_put(priv);
	LEAVE();
	return NULL;
}

/**
 *  @brief This function send hardware remove event
 *
 *  @param priv    A pointer to bt_private
 *  @return        N/A
 */
void
bt_send_hw_remove_event(bt_private *priv)
{
	struct sk_buff *skb = NULL;
	struct mbt_dev *mbt_dev = NULL;
	struct m_dev *mdev_bt = &(priv->bt_dev.m_dev[BT_SEQ]);
	ENTER();
	if (!priv->bt_dev.m_dev[BT_SEQ].dev_pointer) {
		LEAVE();
		return;
	}
	if (priv->bt_dev.m_dev[BT_SEQ].spec_type != BLUEZ_SPEC)
		mbt_dev =
			(struct mbt_dev *)priv->bt_dev.m_dev[BT_SEQ].
			dev_pointer;
#define HCI_HARDWARE_ERROR_EVT  0x10
#define HCI_HARDWARE_REMOVE     0x24
	skb = bt_skb_alloc(3, GFP_ATOMIC);
	skb->data[0] = HCI_HARDWARE_ERROR_EVT;
	skb->data[1] = 1;
	skb->data[2] = HCI_HARDWARE_REMOVE;
	bt_cb(skb)->pkt_type = HCI_EVENT_PKT;
	skb_put(skb, 3);
	if (mbt_dev) {
		skb->dev = (void *)mdev_bt;
		PRINTM(MSG, "Send HW ERROR event\n");
		if (!mdev_recv_frame(skb)) {
#define RX_WAIT_TIMEOUT				300
			mdev_bt->wait_rx_complete = TRUE;
			mdev_bt->rx_complete_flag = FALSE;
			if (os_wait_interruptible_timeout
			    (mdev_bt->rx_wait_q, mdev_bt->rx_complete_flag,
			     RX_WAIT_TIMEOUT))
				PRINTM(MSG, "BT stack received the event\n");
			mdev_bt->stat.byte_rx += 3;
		}
	}
	LEAVE();
	return;
}

/**
 *  @brief This function removes the card.
 *
 *  @param card    A pointer to card
 *  @return        BT_STATUS_SUCCESS
 */
int
bt_remove_card(void *card)
{
	bt_private *priv = (bt_private *)card;
	ENTER();
	if (!priv) {
		LEAVE();
		return BT_STATUS_SUCCESS;
	}
	if (!priv->adapter->SurpriseRemoved) {
		if (BT_STATUS_SUCCESS == bt_send_reset_command(priv))
			bt_send_module_cfg_cmd(priv, MODULE_SHUTDOWN_REQ);
		/* Disable interrupts on the card */
		sd_disable_host_int(priv);
		priv->adapter->SurpriseRemoved = TRUE;
	}
	bt_send_hw_remove_event(priv);
	wake_up_interruptible(&priv->adapter->cmd_wait_q);
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid) {
		os_sched_timeout(1);
		wake_up_interruptible(&priv->MainThread.waitQ);
	}

	bt_proc_remove(priv);
	PRINTM(INFO, "Unregister device\n");
	sbi_unregister_dev(priv);
	clean_up_m_devs(priv);
	PRINTM(INFO, "Free Adapter\n");
	bt_free_adapter(priv);
	bt_priv_put(priv);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function initializes module.
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_init_module(void)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	PRINTM(MSG, "BT: Loading driver\n");

	bt_root_proc_init();

	/** create char device class */
	chardev_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(chardev_class)) {
		PRINTM(ERROR, "Unable to allocate class\n");
		bt_root_proc_remove();
		ret = PTR_ERR(chardev_class);
		goto done;
	}

	if (sbi_register() == NULL) {
		bt_root_proc_remove();
		ret = BT_STATUS_FAILURE;
		goto done;
	}
done:
	if (ret)
		PRINTM(MSG, "BT: Driver loading failed\n");
	else
		PRINTM(MSG, "BT: Driver loaded successfully\n");

	LEAVE();
	return ret;
}

/**
 *  @brief This function cleans module
 *
 *  @return        N/A
 */
static void
bt_exit_module(void)
{
	ENTER();
	PRINTM(MSG, "BT: Unloading driver\n");
	sbi_unregister();

	bt_root_proc_remove();
	class_destroy(chardev_class);
	PRINTM(MSG, "BT: Driver unloaded\n");
	LEAVE();
}

module_init(bt_init_module);
module_exit(bt_exit_module);

MODULE_AUTHOR("Marvell International Ltd.");
MODULE_DESCRIPTION("Marvell Bluetooth Driver Ver. " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
module_param(fw, int, 0);
MODULE_PARM_DESC(fw, "0: Skip firmware download; otherwise: Download firmware");
module_param(fw_crc_check, int, 0);
MODULE_PARM_DESC(fw_crc_check,
		 "1: Enable FW download CRC check (default); 0: Disable FW download CRC check");
module_param(psmode, int, 0);
MODULE_PARM_DESC(psmode, "1: Enable powermode; 0: Disable powermode");
#ifdef CONFIG_OF
module_param(dts_enable, int, 0);
MODULE_PARM_DESC(dts_enable, "0: Disable DTS; 1: Enable DTS");
#endif
#ifdef	DEBUG_LEVEL1
module_param(mbt_drvdbg, uint, 0);
MODULE_PARM_DESC(mbt_drvdbg, "BIT3:DBG_DATA BIT4:DBG_CMD 0xFF:DBG_ALL");
#endif
#ifdef SDIO_SUSPEND_RESUME
module_param(mbt_pm_keep_power, int, 0);
MODULE_PARM_DESC(mbt_pm_keep_power, "1: PM keep power; 0: PM no power");
#endif
module_param(init_cfg, charp, 0);
MODULE_PARM_DESC(init_cfg, "BT init config file name");
module_param(cal_cfg, charp, 0);
MODULE_PARM_DESC(cal_cfg, "BT calibrate file name");
module_param(cal_cfg_ext, charp, 0);
MODULE_PARM_DESC(cal_cfg_ext, "BT calibrate ext file name");
module_param(bt_mac, charp, 0660);
MODULE_PARM_DESC(bt_mac, "BT init mac address");
module_param(drv_mode, int, 0);
MODULE_PARM_DESC(drv_mode, "Bit 0: BT/AMP/BLE; Bit 1: FM; Bit 2: NFC");
module_param(bt_name, charp, 0);
MODULE_PARM_DESC(bt_name, "BT interface name");
module_param(fm_name, charp, 0);
MODULE_PARM_DESC(fm_name, "FM interface name");
module_param(nfc_name, charp, 0);
MODULE_PARM_DESC(nfc_name, "NFC interface name");
module_param(debug_intf, int, 0);
MODULE_PARM_DESC(debug_intf,
		 "1: Enable debug interface; 0: Disable debug interface ");
module_param(debug_name, charp, 0);
MODULE_PARM_DESC(debug_name, "Debug interface name");
module_param(mbt_gpio_pin, int, 0);
MODULE_PARM_DESC(mbt_gpio_pin,
		 "GPIO pin to interrupt host. 0xFF: disable GPIO interrupt mode; Others: GPIO pin assigned to generate pulse to host.");
