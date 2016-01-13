/** @file bt_sdiommc.c
 *  @brief This file contains SDIO IF (interface) module
 *  related functions.
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

#include <linux/firmware.h>
#include <linux/mmc/sdio_func.h>

#include "bt_drv.h"
#include "bt_sdio.h"

/** define marvell vendor id */
#define MARVELL_VENDOR_ID 0x02df

/** Max retry number of CMD53 write */
#define MAX_WRITE_IOMEM_RETRY	2
/** Firmware name */
static char *fw_name;
/** fw serial download flag */
static int bt_fw_serial = 1;

int bt_intmode = INT_MODE_SDIO;
/** request firmware nowait */
int bt_req_fw_nowait;
static int multi_fn = BIT(2);

#define DEFAULT_FW_NAME ""

/** FW header length for CRC check disable */
#define FW_CRC_HEADER_RB2   28
/** FW header for CRC check disable */
static u8 fw_crc_header_rb_2[FW_CRC_HEADER_RB2] = {
	0x05, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
	0x9d, 0x32, 0xbb, 0x11, 0x01, 0x00, 0x00, 0x7f,
	0x00, 0x00, 0x00, 0x00, 0x67, 0xd6, 0xfc, 0x25
};

/** FW header length for CRC check disable */
#define FW_CRC_HEADER_RB   24
/** FW header for CRC check disable */
static u8 fw_crc_header_rb_1[FW_CRC_HEADER_RB] = {
	0x01, 0x00, 0x00, 0x00, 0x04, 0xfd, 0x00, 0x04,
	0x08, 0x00, 0x00, 0x00, 0x26, 0x52, 0x2a, 0x7b,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/** Default firmware name */
#define DEFAULT_FW_NAME_8777 "mrvl/sd8777_uapsta.bin"
#define DEFAULT_FW_NAME_8787 "mrvl/sd8787_uapsta.bin"
#define DEFAULT_FW_NAME_8797 "mrvl/sd8797_uapsta.bin"
#define DEFAULT_FW_NAME_8887 "mrvl/sd8887_uapsta.bin"
#define DEFAULT_FW_NAME_8897 "mrvl/sd8897_uapsta.bin"
#define DEFAULT_FW_NAME_8977 "mrvl/sdsd8977_combo.bin"

/** SD8787 chip revision ID */
#define SD8787_W0      0x30
#define SD8787_W1      0x31
#define SD8787_A0_A1   0x40
/** SD8797 chip revision ID */
#define SD8797_A0       0x00
#define SD8797_B0       0x10
/** SD8897 chip revision ID */
#define SD8897_A0       0x10
#define SD8897_B0       0x20

/** SD8887 chip revision ID */
#define SD8887_A0       0x0
#define SD8887_A2       0x2
#define SD8887_A0_FW_NAME "mrvl/sd8887_uapsta.bin"
#define SD8887_A2_FW_NAME "mrvl/sd8887_uapsta_a2.bin"
#define SD8887_A2_BT_FW_NAME "mrvl/sd8887_bt_a2.bin"

#define SD8897_A0_FW_NAME "mrvl/sd8897_uapsta_a0.bin"
#define SD8897_B0_FW_NAME "mrvl/sd8897_uapsta.bin"

#define SD8787_W1_FW_NAME "mrvl/sd8787_uapsta_w1.bin"
#define SD8787_AX_FW_NAME "mrvl/sd8787_uapsta.bin"
#define SD8797_A0_FW_NAME "mrvl/sd8797_uapsta_a0.bin"
#define SD8797_B0_FW_NAME "mrvl/sd8797_uapsta.bin"

/** SD8977 chip revision ID */
#define SD8977_V0       0x0
#define SD8977_V1       0x8
#define SD8977_V0_FW_NAME "mrvl/sdsd8977_combo.bin"
#define SD8977_V0_BT_FW_NAME "mrvl/sd8977_bt.bin"
#define SD8977_V1_FW_NAME "mrvl/sdsd8977_combo_v1.bin"
#define SD8977_V1_BT_FW_NAME "mrvl/sd8977_bt_v1.bin"

/** Function number 2 */
#define FN2			2
/** Device ID for SD8787 FN2 */
#define SD_DEVICE_ID_8787_BT_FN2    0x911A
/** Device ID for SD8787 FN3 */
#define SD_DEVICE_ID_8787_BT_FN3    0x911B
/** Device ID for SD8777 FN2 */
#define SD_DEVICE_ID_8777_BT_FN2    0x9132
/** Device ID for SD8777 FN3 */
#define SD_DEVICE_ID_8777_BT_FN3    0x9133
/** Device ID for SD8887 FN2 */
#define SD_DEVICE_ID_8887_BT_FN2    0x9136
/** Device ID for SD8887 FN3 */
#define SD_DEVICE_ID_8887_BT_FN3    0x9137
/** Device ID for SD8897 FN2 */
#define SD_DEVICE_ID_8897_BT_FN2    0x912E
/** Device ID for SD8897 FN3 */
#define SD_DEVICE_ID_8897_BT_FN3    0x912F
/** Device ID for SD8797 FN2 */
#define SD_DEVICE_ID_8797_BT_FN2    0x912A
/** Device ID for SD8797 FN3 */
#define SD_DEVICE_ID_8797_BT_FN3    0x912B
/** Device ID for SD8977 FN2 */
#define SD_DEVICE_ID_8977_BT_FN2    0x9146

/** Array of SDIO device ids when multi_fn=0x12 */
static const struct sdio_device_id bt_ids[] = {
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8787_BT_FN2)},
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8777_BT_FN2)},
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8887_BT_FN2)},
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8897_BT_FN2)},
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8797_BT_FN2)},
	{SDIO_DEVICE(MARVELL_VENDOR_ID, SD_DEVICE_ID_8977_BT_FN2)},
	{}
};

MODULE_DEVICE_TABLE(sdio, bt_ids);

/********************************************************
		Global Variables
********************************************************/
/** unregiser bus driver flag */
static u8 unregister;
#ifdef SDIO_SUSPEND_RESUME
/** PM keep power */
extern int mbt_pm_keep_power;
#endif

/********************************************************
		Local Functions
********************************************************/

/**
 *  @brief This function gets rx_unit value
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_get_rx_unit(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 card_rx_unit_reg = priv->psdio_device->reg->card_rx_unit;

	ENTER();

	reg = sdio_readb(card->func, card_rx_unit_reg, &ret);
	if (ret == BT_STATUS_SUCCESS)
		priv->bt_dev.rx_unit = reg;

	LEAVE();
	return ret;
}

/**
 *  @brief This function reads fwstatus registers
 *
 *  @param priv    A pointer to bt_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_read_firmware_status(bt_private *priv, u16 * dat)
{
	int ret = BT_STATUS_SUCCESS;
	u8 fws0;
	u8 fws1;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 card_fw_status0_reg = priv->psdio_device->reg->card_fw_status0;
	u8 card_fw_status1_reg = priv->psdio_device->reg->card_fw_status1;

	ENTER();

	fws0 = sdio_readb(card->func, card_fw_status0_reg, &ret);
	if (ret < 0) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	fws1 = sdio_readb(card->func, card_fw_status1_reg, &ret);
	if (ret < 0) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	*dat = (((u16) fws1) << 8) | fws0;

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function reads rx length
 *
 *  @param priv    A pointer to bt_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return        BT_STATUS_SUCCESS or other error no.
 */
static int
sd_read_rx_len(bt_private *priv, u16 * dat)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 card_rx_len_reg = priv->psdio_device->reg->card_rx_len;

	ENTER();

	reg = sdio_readb(card->func, card_rx_len_reg, &ret);
	if (ret == BT_STATUS_SUCCESS)
		*dat = (u16) reg << priv->bt_dev.rx_unit;

	LEAVE();
	return ret;
}

/**
 *  @brief This function enables the host interrupts mask
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mask	   the interrupt mask
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_enable_host_int_mask(bt_private *priv, u8 mask)
{
	int ret = BT_STATUS_SUCCESS;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 host_int_mask_reg = priv->psdio_device->reg->host_int_mask;

	ENTER();

	sdio_writeb(card->func, mask, host_int_mask_reg, &ret);
	if (ret) {
		PRINTM(WARN, "BT: Unable to enable the host interrupt!\n");
		ret = BT_STATUS_FAILURE;
	}

	LEAVE();
	return ret;
}

/** @brief This function disables the host interrupts mask.
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mask	   the interrupt mask
 *  @return        BT_STATUS_SUCCESS or other error no.
 */
static int
sd_disable_host_int_mask(bt_private *priv, u8 mask)
{
	int ret = BT_STATUS_FAILURE;
	u8 host_int_mask;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 host_int_mask_reg = priv->psdio_device->reg->host_int_mask;

	ENTER();

	/* Read back the host_int_mask register */
	host_int_mask = sdio_readb(card->func, host_int_mask_reg, &ret);
	if (ret)
		goto done;

	/* Update with the mask and write back to the register */
	host_int_mask &= ~mask;
	sdio_writeb(card->func, host_int_mask, host_int_mask_reg, &ret);
	if (ret < 0) {
		PRINTM(WARN, "BT: Unable to diable the host interrupt!\n");
		goto done;
	}
	ret = BT_STATUS_SUCCESS;
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function polls the card status register
 *
 *  @param priv     A pointer to bt_private structure
 *  @param bits     the bit mask
 *  @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_poll_card_status(bt_private *priv, u8 bits)
{
	int tries;
	int rval;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 cs;
	u8 card_status_reg = priv->psdio_device->reg->card_status;

	ENTER();

	for (tries = 0; tries < MAX_POLL_TRIES * 1000; tries++) {
		cs = sdio_readb(card->func, card_status_reg, &rval);
		if (rval != 0)
			break;
		if (rval == 0 && (cs & bits) == bits) {
			LEAVE();
			return BT_STATUS_SUCCESS;
		}
		udelay(1);
	}
	PRINTM(ERROR,
	       "BT: sdio_poll_card_status failed (%d), tries = %d, cs = 0x%x\n",
	       rval, tries, cs);

	LEAVE();
	return BT_STATUS_FAILURE;
}

/**
 *  @brief This function reads updates the Cmd52 value in dev structure
 *
 *  @param priv     A pointer to bt_private structure
 *  @return         BT_STATUS_SUCCESS or other error no.
 */
int
sd_read_cmd52_val(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 func, reg, val;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	func = priv->bt_dev.cmd52_func;
	reg = priv->bt_dev.cmd52_reg;
	sdio_claim_host(card->func);
	if (func)
		val = sdio_readb(card->func, reg, &ret);
	else
		val = sdio_f0_readb(card->func, reg, &ret);
	sdio_release_host(card->func);
	if (ret) {
		PRINTM(ERROR, "BT: Cannot read value from func %d reg %d\n",
		       func, reg);
	} else {
		priv->bt_dev.cmd52_val = val;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function updates card reg based on the Cmd52 value in dev structure
 *
 *  @param priv     A pointer to bt_private structure
 *  @param func     Stores func variable
 *  @param reg      Stores reg variable
 *  @param val      Stores val variable
 *  @return         BT_STATUS_SUCCESS or other error no.
 */
int
sd_write_cmd52_val(bt_private *priv, int func, int reg, int val)
{
	int ret = BT_STATUS_SUCCESS;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;

	ENTER();

	if (val >= 0) {
		/* Perform actual write only if val is provided */
		sdio_claim_host(card->func);
		if (func)
			sdio_writeb(card->func, val, reg, &ret);
		else
			sdio_f0_writeb(card->func, val, reg, &ret);
		sdio_release_host(card->func);
		if (ret) {
			PRINTM(ERROR,
			       "BT: Cannot write value (0x%x) to func %d reg %d\n",
			       val, func, reg);
			goto done;
		}
		priv->bt_dev.cmd52_val = val;
	}

	/* Save current func and reg for future read */
	priv->bt_dev.cmd52_func = func;
	priv->bt_dev.cmd52_reg = reg;

done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function updates card reg based on the Cmd52 value in dev structure
 *
 *  @param priv     A pointer to bt_private structure
 *  @param reg      register to write
 *  @param val      value
 *  @return         BT_STATUS_SUCCESS or other error no.
 */
int
sd_write_reg(bt_private *priv, int reg, u8 val)
{
	int ret = BT_STATUS_SUCCESS;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	ENTER();
	sdio_claim_host(card->func);
	sdio_writeb(card->func, val, reg, &ret);
	sdio_release_host(card->func);
	LEAVE();
	return ret;
}

/**
 *  @brief This function reads updates the Cmd52 value in dev structure
 *
 *  @param priv     A pointer to bt_private structure
 *  @param reg      register to read
 *  @return         BT_STATUS_SUCCESS or other error no.
 */
int
sd_read_reg(bt_private *priv, int reg, u8 *data)
{
	int ret = BT_STATUS_SUCCESS;
	u8 val;
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	ENTER();
	sdio_claim_host(card->func);
	val = sdio_readb(card->func, reg, &ret);
	sdio_release_host(card->func);
	*data = val;
	LEAVE();
	return ret;
}

/**
 *  @brief This function probes the card
 *
 *  @param func    A pointer to sdio_func structure.
 *  @param id      A pointer to structure sdio_device_id
 *  @return        BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
static int
sd_probe_card(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = BT_STATUS_SUCCESS;
	bt_private *priv = NULL;
	struct sdio_mmc_card *card = NULL;

	ENTER();

	PRINTM(INFO, "BT: vendor=0x%x,device=0x%x,class=%d,fn=%d\n", id->vendor,
	       id->device, id->class, func->num);
	card = kzalloc(sizeof(struct sdio_mmc_card), GFP_KERNEL);
	if (!card) {
		ret = -ENOMEM;
		goto done;
	}
	card->func = func;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
	/* wait for chip fully wake up */
	if (!func->enable_timeout)
		func->enable_timeout = 200;
#endif
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	if (ret) {
		sdio_disable_func(func);
		sdio_release_host(func);
		PRINTM(FATAL, "BT: sdio_enable_func() failed: ret=%d\n", ret);
		kfree(card);
		LEAVE();
		return -EIO;
	}
	sdio_release_host(func);
	priv = bt_add_card(card);
	if (!priv) {
		sdio_claim_host(func);
		sdio_disable_func(func);
		sdio_release_host(func);
		ret = BT_STATUS_FAILURE;
		kfree(card);
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function checks if the firmware is ready to accept
 *  command or not.
 *
 *  @param priv     A pointer to bt_private structure
 *  @param pollnum  Number of times to poll fw status
 *  @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_verify_fw_download(bt_private *priv, int pollnum)
{
	int ret = BT_STATUS_FAILURE;
	u16 firmwarestat = 0;
	int tries;

	ENTER();

	/* Wait for firmware initialization event */
	for (tries = 0; tries < pollnum; tries++) {
		if (sd_read_firmware_status(priv, &firmwarestat) < 0)
			continue;
		if (firmwarestat == FIRMWARE_READY) {
			PRINTM(MSG, "BT FW is active(%d)\n", tries);
			ret = BT_STATUS_SUCCESS;
			break;
		}
		mdelay(100);
	}
	if ((pollnum > 1) && (ret != BT_STATUS_SUCCESS))
		PRINTM(ERROR,
		       "Fail to poll firmware status: firmwarestat=0x%x\n",
		       firmwarestat);
	LEAVE();
	return ret;
}

/**
 *  @brief Transfers firmware to card
 *
 *  @param priv      A Pointer to bt_private structure
 *  @param fw        A Pointer to fw image
 *  @param fw_len    fw image len
 *  @return          BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
static int
sd_init_fw_dpc(bt_private *priv, u8 *fw, int fw_len)
{
	struct sdio_mmc_card *card = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 *firmware = fw;
	int firmwarelen = fw_len;
	u8 base0;
	u8 base1;
	int ret = BT_STATUS_SUCCESS;
	int offset;
	void *tmpfwbuf = NULL;
	int tmpfwbufsz;
	u8 *fwbuf;
	u16 len;
	int txlen = 0;
	int tx_blocks = 0;
	int i = 0;
	int tries = 0;
	u8 sq_read_base_address_a0_reg =
		priv->psdio_device->reg->sq_read_base_addr_a0;
	u8 sq_read_base_address_a1_reg =
		priv->psdio_device->reg->sq_read_base_addr_a1;
	u8 crc_buffer = 0;
	u8 *header_crc_fw = NULL;
	u8 header_crc_fw_len = 0;

	if (priv->card_type == CARD_TYPE_SD8787) {
		header_crc_fw = fw_crc_header_rb_1;
		header_crc_fw_len = FW_CRC_HEADER_RB;
	} else if (priv->card_type == CARD_TYPE_SD8777) {
		header_crc_fw = fw_crc_header_rb_2;
		header_crc_fw_len = FW_CRC_HEADER_RB2;
	}

	ENTER();

	PRINTM(INFO, "BT: Downloading FW image (%d bytes)\n", firmwarelen);

	tmpfwbufsz = BT_UPLD_SIZE + DMA_ALIGNMENT;
	tmpfwbuf = kzalloc(tmpfwbufsz, GFP_KERNEL);
	if (!tmpfwbuf) {
		PRINTM(ERROR,
		       "BT: Unable to allocate buffer for firmware. Terminating download\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	/* Ensure aligned firmware buffer */
	fwbuf = (u8 *)ALIGN_ADDR(tmpfwbuf, DMA_ALIGNMENT);

	if (!(priv->fw_crc_check)
	    && ((priv->card_type == CARD_TYPE_SD8787) ||
		(priv->card_type == CARD_TYPE_SD8777))
		) {
		/* CRC check not required, use custom header first */
		firmware = header_crc_fw;
		firmwarelen = header_crc_fw_len;
		crc_buffer = 1;
	}

	/* Perform firmware data transfer */
	offset = 0;
	do {
		/* The host polls for the DN_LD_CARD_RDY and CARD_IO_READY bits
		 */
		ret = sd_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
		if (ret < 0) {
			PRINTM(FATAL,
			       "BT: FW download with helper poll status timeout @ %d\n",
			       offset);
			goto done;
		}
		if (!crc_buffer)
			/* More data? */
			if (offset >= firmwarelen)
				break;

		for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
			base0 = sdio_readb(card->func,
					   sq_read_base_address_a0_reg, &ret);
			if (ret) {
				PRINTM(WARN, "Dev BASE0 register read failed:"
				       " base0=0x%04X(%d). Terminating download\n",
				       base0, base0);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			base1 = sdio_readb(card->func,
					   sq_read_base_address_a1_reg, &ret);
			if (ret) {
				PRINTM(WARN, "Dev BASE1 register read failed:"
				       " base1=0x%04X(%d). Terminating download\n",
				       base1, base1);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			len = (((u16) base1) << 8) | base0;

			if (len != 0)
				break;
			udelay(10);
		}

		if (len == 0)
			break;
		else if (len > BT_UPLD_SIZE) {
			PRINTM(FATAL,
			       "BT: FW download failure @ %d, invalid length %d\n",
			       offset, len);
			ret = BT_STATUS_FAILURE;
			goto done;
		}

		txlen = len;

		if (len & BIT(0)) {
			i++;
			if (i > MAX_WRITE_IOMEM_RETRY) {
				PRINTM(FATAL,
				       "BT: FW download failure @ %d, over max retry count\n",
				       offset);
				ret = BT_STATUS_FAILURE;
				goto done;
			}
			PRINTM(ERROR,
			       "BT: FW CRC error indicated by the helper:"
			       " len = 0x%04X, txlen = %d\n", len, txlen);
			len &= ~BIT(0);

			PRINTM(ERROR, "BT: retry: %d, offset %d\n", i, offset);
			/* Setting this to 0 to resend from same offset */
			txlen = 0;
		} else {
			i = 0;

			/* Set blocksize to transfer - checking for last block */
			if (firmwarelen - offset < txlen)
				txlen = firmwarelen - offset;

			PRINTM(INFO, ".");

			tx_blocks =
				(txlen + SD_BLOCK_SIZE_FW_DL -
				 1) / SD_BLOCK_SIZE_FW_DL;

			/* Copy payload to buffer */
			memcpy(fwbuf, &firmware[offset], txlen);
		}

		/* Send data */
		ret = sdio_writesb(card->func, priv->bt_dev.ioport, fwbuf,
				   tx_blocks * SD_BLOCK_SIZE_FW_DL);

		if (ret < 0) {
			PRINTM(ERROR,
			       "BT: FW download, write iomem (%d) failed @ %d\n",
			       i, offset);
			sdio_writeb(card->func, 0x04, CONFIGURATION_REG, &ret);
			if (ret)
				PRINTM(ERROR, "write ioreg failed (CFG)\n");
		}

		offset += txlen;
		if (crc_buffer
		    && ((priv->card_type == CARD_TYPE_SD8787) ||
			(priv->card_type == CARD_TYPE_SD8777))
			) {
			if (offset >= header_crc_fw_len) {
				/* Custom header download complete, restore
				   original FW */
				offset = 0;
				firmware = fw;
				firmwarelen = fw_len;
				crc_buffer = 0;
			}
		}
	} while (TRUE);

	PRINTM(MSG, "BT: FW download over, size %d bytes\n", offset);

	ret = BT_STATUS_SUCCESS;
done:
	kfree(tmpfwbuf);
	LEAVE();
	return ret;
}

/**
 * @brief request_firmware callback
 *
 * @param fw_firmware  A pointer to firmware structure
 * @param context      A Pointer to bt_private structure
 * @return             BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_request_fw_dpc(const struct firmware *fw_firmware, void *context)
{
	int ret = BT_STATUS_SUCCESS;
	bt_private *priv = (bt_private *)context;
	struct sdio_mmc_card *card = NULL;
	struct m_dev *m_dev_bt = NULL;
	struct m_dev *m_dev_fm = NULL;
	struct m_dev *m_dev_nfc = NULL;
	struct timeval tstamp;

	ENTER();

	m_dev_bt = &priv->bt_dev.m_dev[BT_SEQ];
	m_dev_fm = &priv->bt_dev.m_dev[FM_SEQ];
	m_dev_nfc = &priv->bt_dev.m_dev[NFC_SEQ];

	if ((priv == NULL) || (priv->adapter == NULL) ||
	    (priv->bt_dev.card == NULL) || (m_dev_bt == NULL) ||
	    (m_dev_fm == NULL) || (m_dev_nfc == NULL)) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	card = (struct sdio_mmc_card *)priv->bt_dev.card;

	if (!fw_firmware) {
		do_gettimeofday(&tstamp);
		if (tstamp.tv_sec >
		    (priv->req_fw_time.tv_sec + REQUEST_FW_TIMEOUT)) {
			PRINTM(ERROR,
			       "BT: No firmware image found. Skipping download\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
		PRINTM(ERROR,
		       "BT: No firmware image found! Retrying download\n");
		/* Wait a second here before calling the callback again */
		os_sched_timeout(1000);
		sd_download_firmware_w_helper(priv);
		LEAVE();
		return ret;
	}

	priv->firmware = fw_firmware;

	if (BT_STATUS_FAILURE ==
	    sd_init_fw_dpc(priv, (u8 *)priv->firmware->data,
			   priv->firmware->size)) {
		PRINTM(ERROR,
		       "BT: sd_init_fw_dpc failed (download fw with nowait: %d). Terminating download\n",
		       bt_req_fw_nowait);
		sdio_release_host(card->func);
		ret = BT_STATUS_FAILURE;
		goto done;
	}

	/* check if the fimware is downloaded successfully or not */
	if (sd_verify_fw_download(priv, MAX_FIRMWARE_POLL_TRIES)) {
		PRINTM(ERROR, "BT: FW failed to be active in time!\n");
		ret = BT_STATUS_FAILURE;
		sdio_release_host(card->func);
		goto done;
	}
	sdio_release_host(card->func);
	sd_enable_host_int(priv);
	if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
		PRINTM(ERROR,
		       "BT: sbi_register_conf_dpc failed. Terminating download\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	if (fw_firmware) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
		if (!bt_req_fw_nowait)
#endif
			release_firmware(fw_firmware);
	}
	LEAVE();
	return ret;

done:
	if (fw_firmware) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
		if (!bt_req_fw_nowait)
#endif
			release_firmware(fw_firmware);
	}
	/* For synchronous download cleanup will be done in add_card */
	if (!bt_req_fw_nowait)
		return ret;
	PRINTM(INFO, "unregister device\n");
	sbi_unregister_dev(priv);
	((struct sdio_mmc_card *)card)->priv = NULL;
	/* Stop the thread servicing the interrupts */
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid)
		os_sched_timeout(1);
	bt_proc_remove(priv);
	clean_up_m_devs(priv);
	bt_free_adapter(priv);
	bt_priv_put(priv);
	LEAVE();
	return ret;
}

/**
 * @brief request_firmware callback
 *        This function is invoked by request_firmware_nowait system call
 *
 * @param firmware     A pointer to firmware structure
 * @param context      A Pointer to bt_private structure
 * @return             None
 **/
static void
sd_request_fw_callback(const struct firmware *firmware, void *context)
{
	ENTER();
	sd_request_fw_dpc(firmware, context);
	LEAVE();
	return;
}

/**
 *  @brief This function downloads firmware image to the card.
 *
 *  @param priv     A pointer to bt_private structure
 *  @return         BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sd_download_firmware_w_helper(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	int err;
	char *cur_fw_name = NULL;

	ENTER();

	cur_fw_name = fw_name;
	if (fw_name == NULL) {
		if (priv->card_type == CARD_TYPE_SD8787)
			cur_fw_name = DEFAULT_FW_NAME_8787;
		else if (priv->card_type == CARD_TYPE_SD8777)
			cur_fw_name = DEFAULT_FW_NAME_8777;
		else if (priv->card_type == CARD_TYPE_SD8887) {
			/* Check revision ID */
			switch (priv->adapter->chip_rev) {
			case SD8887_A0:
				cur_fw_name = SD8887_A0_FW_NAME;
				break;
			case SD8887_A2:
				if (bt_fw_serial == 1)
					cur_fw_name = SD8887_A2_FW_NAME;
				else
					cur_fw_name = SD8887_A2_BT_FW_NAME;
				break;
			default:
				cur_fw_name = DEFAULT_FW_NAME_8887;
				break;
			}
		} else if (priv->card_type == CARD_TYPE_SD8897)
			cur_fw_name = DEFAULT_FW_NAME_8897;
		else if (priv->card_type == CARD_TYPE_SD8797)
			cur_fw_name = DEFAULT_FW_NAME_8797;
		else if (priv->card_type == CARD_TYPE_SD8977) {
			switch (priv->adapter->chip_rev) {
			case SD8977_V0:
				if (bt_fw_serial == 1)
					cur_fw_name = SD8977_V0_FW_NAME;
				else
					cur_fw_name = SD8977_V0_BT_FW_NAME;
				break;
			case SD8977_V1:
				if (bt_fw_serial == 1)
					cur_fw_name = SD8977_V1_FW_NAME;
				else
					cur_fw_name = SD8977_V1_BT_FW_NAME;
				break;
			default:
				cur_fw_name = DEFAULT_FW_NAME_8977;
				break;
			}
		}
	}

	if (bt_req_fw_nowait) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					      cur_fw_name, priv->hotplug_device,
					      GFP_KERNEL, priv,
					      sd_request_fw_callback);
#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 13)
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					      cur_fw_name, priv->hotplug_device,
					      priv, sd_request_fw_callback);
#else
		ret = request_firmware_nowait(THIS_MODULE,
					      cur_fw_name, priv->hotplug_device,
					      priv, sd_request_fw_callback);
#endif
#endif
		if (ret < 0)
			PRINTM(FATAL,
			       "BT: request_firmware_nowait() failed, error code = %#x\n",
			       ret);
	} else {
		err = request_firmware(&priv->firmware, cur_fw_name,
				       priv->hotplug_device);
		if (err < 0) {
			PRINTM(FATAL,
			       "BT: request_firmware() failed, error code = %#x\n",
			       err);
			ret = BT_STATUS_FAILURE;
		} else
			ret = sd_request_fw_dpc(priv->firmware, priv);
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function reads data from the card.
 *
 *  @param priv     A pointer to bt_private structure
 *  @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
sd_card_to_host(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u16 buf_len = 0;
	int buf_block_len;
	int blksz;
	struct sk_buff *skb = NULL;
	u32 type;
	u8 *payload = NULL;
	struct hci_dev *hdev = NULL;
	struct m_dev *mdev_fm = &(priv->bt_dev.m_dev[FM_SEQ]);
	struct m_dev *mdev_nfc = &(priv->bt_dev.m_dev[NFC_SEQ]);
	struct nfc_dev *nfc_dev =
		(struct nfc_dev *)priv->bt_dev.m_dev[NFC_SEQ].dev_pointer;
	struct fm_dev *fm_dev =
		(struct fm_dev *)priv->bt_dev.m_dev[FM_SEQ].dev_pointer;
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();
	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;
	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	/* Read the length of data to be transferred */
	ret = sd_read_rx_len(priv, &buf_len);
	if (ret < 0) {
		PRINTM(ERROR, "BT: card_to_host, read scratch reg failed\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	/* Allocate buffer */
	blksz = SD_BLOCK_SIZE;
	buf_block_len = (buf_len + blksz - 1) / blksz;
	if (buf_len <= BT_HEADER_LEN ||
	    (buf_block_len * blksz) > ALLOC_BUF_SIZE) {
		PRINTM(ERROR, "BT: card_to_host, invalid packet length: %d\n",
		       buf_len);
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	skb = bt_skb_alloc(buf_block_len * blksz + DMA_ALIGNMENT, GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "BT: No free skb\n");
		goto exit;
	}
	if ((t_ptr)skb->data & (DMA_ALIGNMENT - 1)) {
		skb_put(skb,
			DMA_ALIGNMENT -
			((t_ptr)skb->data & (DMA_ALIGNMENT - 1)));
		skb_pull(skb,
			 DMA_ALIGNMENT -
			 ((t_ptr)skb->data & (DMA_ALIGNMENT - 1)));
	}

	payload = skb->data;
	ret = sdio_readsb(card->func, payload, priv->bt_dev.ioport,
			  buf_block_len * blksz);
	if (ret < 0) {
		PRINTM(ERROR, "BT: card_to_host, read iomem failed: %d\n", ret);
		kfree_skb(skb);
		skb = NULL;
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	/* This is SDIO specific header length: byte[2][1][0], * type: byte[3]
	   (HCI_COMMAND = 1, ACL_DATA = 2, SCO_DATA = 3, 0xFE = Vendor) */
	buf_len = payload[0];
	buf_len |= (u16) payload[1] << 8;
	type = payload[3];
	PRINTM(DATA, "BT: SDIO Blk Rd %s: len=%d type=%d\n", hdev->name,
	       buf_len, type);
	if (buf_len > buf_block_len * blksz) {
		PRINTM(ERROR,
		       "BT: Drop invalid rx pkt, len in hdr=%d, cmd53 length=%d\n",
		       buf_len, buf_block_len * blksz);
		ret = BT_STATUS_FAILURE;
		kfree_skb(skb);
		skb = NULL;
		goto exit;
	}
	DBG_HEXDUMP(DAT_D, "BT: SDIO Blk Rd", payload, buf_len);
	switch (type) {
	case HCI_ACLDATA_PKT:
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (hdev) {
			skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
			hci_recv_frame(skb);
#else
			hci_recv_frame(hdev, skb);
#endif
			hdev->stat.byte_rx += buf_len;
		}
		break;
	case HCI_SCODATA_PKT:
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (hdev) {
			skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
			hci_recv_frame(skb);
#else
			hci_recv_frame(hdev, skb);
#endif
			hdev->stat.byte_rx += buf_len;
		}
		break;
	case HCI_EVENT_PKT:
		/** add EVT Demux */
		bt_cb(skb)->pkt_type = type;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (BT_STATUS_SUCCESS == check_evtpkt(priv, skb))
			break;
		switch (skb->data[0]) {
		case 0x0E:
			/** cmd complete */
			if (skb->data[3] == 0x80 && skb->data[4] == 0xFE) {
				/** FM cmd complete */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else if (skb->data[3] == 0x81 && skb->data[4] == 0xFE) {
				/** NFC cmd complete */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else {
				if (hdev) {
					skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
					hci_recv_frame(skb);
#else
					hci_recv_frame(hdev, skb);
#endif
					hdev->stat.byte_rx += buf_len;
				}
			}
			break;
		case 0x0F:
			/** cmd status */
			if (skb->data[4] == 0x80 && skb->data[5] == 0xFE) {
				/** FM cmd ststus */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else if (skb->data[4] == 0x81 && skb->data[5] == 0xFE) {
				/** NFC cmd ststus */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else {
				/** BT cmd status */
				if (hdev) {
					skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
					hci_recv_frame(skb);
#else
					hci_recv_frame(hdev, skb);
#endif
					hdev->stat.byte_rx += buf_len;
				}
			}
			break;
		case 0xFF:
			/** Vendor specific pkt */
			if (skb->data[2] == 0xC0) {
				/** NFC EVT */
				if (nfc_dev) {
					skb->dev = (void *)mdev_nfc;
					mdev_recv_frame(skb);
					mdev_nfc->stat.byte_rx += buf_len;
				}
			} else if (skb->data[2] >= 0x80 && skb->data[2] <= 0xAF) {
				/** FM EVT */
				if (fm_dev) {
					skb->dev = (void *)mdev_fm;
					mdev_recv_frame(skb);
					mdev_fm->stat.byte_rx += buf_len;
				}
			} else {
				/** BT EVT */
				if (hdev) {
					skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
					hci_recv_frame(skb);
#else
					hci_recv_frame(hdev, skb);
#endif
					hdev->stat.byte_rx += buf_len;
				}
			}
			break;
		default:
			/** BT EVT */
			if (hdev) {
				skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
				hci_recv_frame(skb);
#else
				hci_recv_frame(hdev, skb);
#endif
				hdev->stat.byte_rx += buf_len;
			}
			break;
		}
		break;
	case MRVL_VENDOR_PKT:
		/* Just think here need to back compatible FM */
		bt_cb(skb)->pkt_type = HCI_VENDOR_PKT;
		skb_put(skb, buf_len);
		skb_pull(skb, BT_HEADER_LEN);
		if (hdev) {
			if (BT_STATUS_SUCCESS != bt_process_event(priv, skb)) {
				skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
				hci_recv_frame(skb);
#else
				hci_recv_frame(hdev, skb);
#endif
				hdev->stat.byte_rx += buf_len;
			}
		}

		break;
	default:
		/* Driver specified event and command resp should be handle
		   here */
		PRINTM(INFO, "BT: Unknown PKT type:%d\n", type);
		kfree_skb(skb);
		skb = NULL;
		break;
	}
exit:
	if (ret) {
		if (hdev)
			hdev->stat.err_rx++;
		PRINTM(ERROR, "error when recv pkt!\n");
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function removes the card
 *
 *  @param func    A pointer to sdio_func structure
 *  @return        N/A
 */
static void
sd_remove_card(struct sdio_func *func)
{
	struct sdio_mmc_card *card;

	ENTER();

	if (func) {
		card = sdio_get_drvdata(func);
		if (card) {
			if (!unregister && card->priv) {
				PRINTM(INFO, "BT: card removed from sd slot\n");
				((bt_private *)(card->priv))->adapter->
					SurpriseRemoved = TRUE;
			}
			bt_remove_card(card->priv);
			kfree(card);
		}
	}

	LEAVE();
}

/**
 *  @brief This function handles the interrupt.
 *
 *  @param func  A pointer to sdio_func structure
 *  @return      N/A
 */
static void
sd_interrupt(struct sdio_func *func)
{
	bt_private *priv;
	struct m_dev *m_dev = NULL;
	struct sdio_mmc_card *card;
	int ret = BT_STATUS_SUCCESS;
	u8 ireg = 0;
	u8 host_intstatus_reg = 0;

	ENTER();

	card = sdio_get_drvdata(func);
	if (!card || !card->priv) {
		PRINTM(INFO,
		       "BT: %s: sbi_interrupt(%p) card or priv is NULL, card=%p\n",
		       __func__, func, card);
		LEAVE();
		return;
	}
	priv = card->priv;
	host_intstatus_reg = priv->psdio_device->reg->host_intstatus;
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	if (priv->card_type == CARD_TYPE_SD8887 ||
	    priv->card_type == CARD_TYPE_SD8897 ||
	    priv->card_type == CARD_TYPE_SD8977) {
		ret = sdio_readsb(card->func, priv->adapter->hw_regs, 0,
				  SD_BLOCK_SIZE);
		if (ret) {
			PRINTM(ERROR,
			       "BT: sdio_read_ioreg: cmd53 read int status register failed %d\n",
			       ret);
			goto done;
		}
		ireg = priv->adapter->hw_regs[host_intstatus_reg];
	} else {
		ireg = sdio_readb(card->func, host_intstatus_reg, &ret);
	}
	if (ret) {
		PRINTM(ERROR,
		       "BT: sdio_read_ioreg: CMD52 read int status register failed %d\n",
		       ret);
		goto done;
	}
	if (ireg != 0) {
		/*
		 * DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS
		 * Clear the interrupt status register and re-enable
		 * the interrupt
		 */
		PRINTM(INTR, "BT: INT %s: sdio_ireg = 0x%x\n", m_dev->name,
		       ireg);
		priv->adapter->irq_recv = ireg;
		if (priv->card_type == CARD_TYPE_SD8777 ||
		    priv->card_type == CARD_TYPE_SD8787) {
			sdio_writeb(card->func,
				    ~(ireg) & (DN_LD_HOST_INT_STATUS |
					       UP_LD_HOST_INT_STATUS),
				    host_intstatus_reg, &ret);
			if (ret) {
				PRINTM(ERROR,
				       "BT: sdio_write_ioreg: clear int status register failed\n");
				goto done;
			}
		}
	} else {
		PRINTM(ERROR, "BT: ERR: ireg=0\n");
	}
	OS_INT_DISABLE;
	priv->adapter->sd_ireg |= ireg;
	OS_INT_RESTORE;
	bt_interrupt(m_dev);
done:
	LEAVE();
}

/**
 *  @brief This function checks if the interface is ready to download
 *  or not while other download interfaces are present
 *
 *  @param priv   A pointer to bt_private structure
 *  @param val    Winner status (0: winner)
 *  @return       BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_check_winner_status(bt_private *priv, u8 *val)
{

	int ret = BT_STATUS_SUCCESS;
	u8 winner = 0;
	struct sdio_mmc_card *cardp = (struct sdio_mmc_card *)priv->bt_dev.card;
	u8 card_fw_status0_reg = priv->psdio_device->reg->card_fw_status0;

	ENTER();
	winner = sdio_readb(cardp->func, card_fw_status0_reg, &ret);
	if (ret != BT_STATUS_SUCCESS) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	*val = winner;

	LEAVE();
	return ret;
}

#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
#ifdef MMC_PM_FUNC_SUSPENDED
/** @brief This function tells lower driver that BT is suspended
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        None
 */
void
bt_is_suspended(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	priv->adapter->is_suspended = TRUE;
	sdio_func_suspended(card->func);
}
#endif

/** @brief This function handles client driver suspend
 *
 *  @param dev	   A pointer to device structure
 *  @return        BT_STATUS_SUCCESS or other error no.
 */
int
bt_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	bt_private *priv = NULL;
	struct sdio_mmc_card *cardp;
	struct m_dev *m_dev = NULL;
	struct hci_dev *hcidev;

	ENTER();

	pm_flags = sdio_get_host_pm_caps(func);
	PRINTM(CMD, "BT: %s: suspend: PM flags = 0x%x\n", sdio_func_id(func),
	       pm_flags);
	if (!(pm_flags & MMC_PM_KEEP_POWER)) {
		PRINTM(ERROR,
		       "BT: %s: cannot remain alive while host is suspended\n",
		       sdio_func_id(func));
		return -ENOSYS;
	}
	cardp = sdio_get_drvdata(func);
	if (!cardp || !cardp->priv) {
		PRINTM(ERROR, "BT: Card or priv structure is not valid\n");
		LEAVE();
		return BT_STATUS_SUCCESS;
	}

	priv = cardp->priv;

	if ((mbt_pm_keep_power) && (priv->adapter->hs_state != HS_ACTIVATED)) {
		/* disable FM event mask */
		if ((priv->bt_dev.m_dev[FM_SEQ].dev_type == FM_TYPE) &&
		    test_bit(HCI_RUNNING, &(priv->bt_dev.m_dev[FM_SEQ].flags)))
			fm_set_intr_mask(priv, FM_DISABLE_INTR_MASK);
		if (BT_STATUS_SUCCESS != bt_enable_hs(priv)) {
			PRINTM(CMD, "BT: HS not actived, suspend fail!\n");
			if (BT_STATUS_SUCCESS != bt_enable_hs(priv)) {
				PRINTM(CMD,
				       "BT: HS not actived the second time, force to suspend!\n");
			}
		}
	}
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	PRINTM(CMD, "BT %s: SDIO suspend\n", m_dev->name);
	hcidev = (struct hci_dev *)m_dev->dev_pointer;
	hci_suspend_dev(hcidev);
	skb_queue_purge(&priv->adapter->tx_queue);

	priv->adapter->is_suspended = TRUE;

	LEAVE();
	/* We will keep the power when hs enabled successfully */
	if ((mbt_pm_keep_power) && (priv->adapter->hs_state == HS_ACTIVATED)) {
#ifdef MMC_PM_SKIP_RESUME_PROBE
		PRINTM(CMD, "BT: suspend with MMC_PM_KEEP_POWER and "
		       "MMC_PM_SKIP_RESUME_PROBE\n");
		return sdio_set_host_pm_flags(func,
					      MMC_PM_KEEP_POWER |
					      MMC_PM_SKIP_RESUME_PROBE);
#else
		PRINTM(CMD, "BT: suspend with MMC_PM_KEEP_POWER\n");
		return sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
#endif
	} else {
		PRINTM(CMD, "BT: suspend without MMC_PM_KEEP_POWER\n");
		return BT_STATUS_SUCCESS;
	}
}

void
bt_sdio_shutdown(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	bt_private *priv = NULL;
	struct sdio_mmc_card *cardp;

	ENTER();

	pm_flags = sdio_get_host_pm_caps(func);
	PRINTM(CMD, "BT: %s: shutdown: PM flags = 0x%x\n", sdio_func_id(func),
	       pm_flags);
	if (!(pm_flags & MMC_PM_KEEP_POWER)) {
		PRINTM(ERROR,
		       "BT: %s: cannot remain alive while host is shutdown\n",
		       sdio_func_id(func));
		return;
	}
	cardp = sdio_get_drvdata(func);
	if (!cardp || !cardp->priv) {
		PRINTM(ERROR, "BT: Card or priv structure is not valid\n");
		LEAVE();
		return;
	}

	priv = cardp->priv;

	if ((mbt_pm_keep_power) && (priv->adapter->hs_state != HS_ACTIVATED)) {
		/* disable FM event mask */
		if ((priv->bt_dev.m_dev[FM_SEQ].dev_type == FM_TYPE) &&
		    test_bit(HCI_RUNNING, &(priv->bt_dev.m_dev[FM_SEQ].flags)))
			fm_set_intr_mask(priv, FM_DISABLE_INTR_MASK);
		if (BT_STATUS_SUCCESS != bt_enable_hs(priv)) {
			PRINTM(CMD, "BT: HS not actived, shutdown fail!\n");
			if (BT_STATUS_SUCCESS != bt_enable_hs(priv)) {
				PRINTM(CMD,
				       "BT: HS not actived the second time, force to shutdown!\n");
			}
		}
	}
	LEAVE();
}

/** @brief This function handles client driver resume
 *
 *  @param dev	   A pointer to device structure
 *  @return        BT_STATUS_SUCCESS
 */
int
bt_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	bt_private *priv = NULL;
	struct sdio_mmc_card *cardp;
	struct m_dev *m_dev = NULL;
	struct hci_dev *hcidev;

	ENTER();
	pm_flags = sdio_get_host_pm_caps(func);
	PRINTM(CMD, "BT: %s: resume: PM flags = 0x%x\n", sdio_func_id(func),
	       pm_flags);
	cardp = sdio_get_drvdata(func);
	if (!cardp || !cardp->priv) {
		PRINTM(ERROR, "BT: Card or priv structure is not valid\n");
		LEAVE();
		return BT_STATUS_SUCCESS;
	}

	priv = cardp->priv;
	priv->adapter->is_suspended = FALSE;
	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	PRINTM(CMD, "BT %s: SDIO resume\n", m_dev->name);
	hcidev = (struct hci_dev *)m_dev->dev_pointer;
	hci_resume_dev(hcidev);
	sbi_wakeup_firmware(priv);
	/* enable FM event mask */
	if ((priv->bt_dev.m_dev[FM_SEQ].dev_type == FM_TYPE) &&
	    test_bit(HCI_RUNNING, &(priv->bt_dev.m_dev[FM_SEQ].flags)))
		fm_set_intr_mask(priv, FM_DEFAULT_INTR_MASK);
	priv->adapter->hs_state = HS_DEACTIVATED;
	PRINTM(CMD, "BT:%s: HS DEACTIVATED in Resume!\n", m_dev->name);
	LEAVE();
	return BT_STATUS_SUCCESS;
}
#endif
#endif

/********************************************************
		Global Functions
********************************************************/
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
static const struct dev_pm_ops bt_sdio_pm_ops = {
	.suspend = bt_sdio_suspend,
	.resume = bt_sdio_resume,
};
#endif
#endif
static struct sdio_driver sdio_bt = {
	.name = "sdio_bt",
	.id_table = bt_ids,
	.probe = sd_probe_card,
	.remove = sd_remove_card,
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
	.drv = {
		.pm = &bt_sdio_pm_ops,
		.shutdown = bt_sdio_shutdown,
		}
#endif
#endif
};

/**
 *  @brief This function registers the bt module in bus driver.
 *
 *  @return	   An int pointer that keeps returned value
 */
int *
sbi_register(void)
{
	int *ret;

	ENTER();

	if (sdio_register_driver(&sdio_bt) != 0) {
		PRINTM(FATAL, "BT: SD Driver Registration Failed\n");
		LEAVE();
		return NULL;
	} else
		ret = (int *)1;

	LEAVE();
	return ret;
}

/**
 *  @brief This function de-registers the bt module in bus driver.
 *
 *  @return        N/A
 */
void
sbi_unregister(void)
{
	ENTER();
	unregister = TRUE;
	sdio_unregister_driver(&sdio_bt);
	LEAVE();
}

/**
 *  @brief This function registers the device.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_register_dev(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 reg;
	u8 chiprev;
	struct sdio_mmc_card *card = priv->bt_dev.card;
	struct sdio_func *func;
	u8 host_intstatus_reg = priv->psdio_device->reg->host_intstatus;
	u8 host_int_rsr_reg = priv->psdio_device->reg->host_int_rsr_reg;
	u8 card_misc_cfg_reg = priv->psdio_device->reg->card_misc_cfg_reg;
	u8 card_revision_reg = priv->psdio_device->reg->card_revision;
	u8 io_port_0_reg = priv->psdio_device->reg->io_port_0;
	u8 io_port_1_reg = priv->psdio_device->reg->io_port_1;
	u8 io_port_2_reg = priv->psdio_device->reg->io_port_2;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: Error: card or function is NULL!\n");
		goto failed;
	}
	func = card->func;
	priv->hotplug_device = &func->dev;

	/* Initialize the private structure */
	strncpy(priv->bt_dev.name, "bt_sdio0", sizeof(priv->bt_dev.name));
	priv->bt_dev.ioport = 0;
	priv->bt_dev.fn = func->num;

	sdio_claim_host(func);
	ret = sdio_claim_irq(func, sd_interrupt);
	if (ret) {
		PRINTM(FATAL, ": sdio_claim_irq failed: ret=%d\n", ret);
		goto release_host;
	}
	ret = sdio_set_block_size(card->func, SD_BLOCK_SIZE);
	if (ret) {
		PRINTM(FATAL, ": %s: cannot set SDIO block size\n", __func__);
		goto release_irq;
	}

	/* read Revision Register to get the chip revision number */
	chiprev = sdio_readb(func, card_revision_reg, &ret);
	if (ret) {
		PRINTM(FATAL, ": cannot read CARD_REVISION_REG\n");
		goto release_irq;
	}
	priv->adapter->chip_rev = chiprev;
	PRINTM(INFO, "revision=%#x\n", chiprev);

	/*
	 * Read the HOST_INTSTATUS_REG for ACK the first interrupt got
	 * from the bootloader. If we don't do this we get a interrupt
	 * as soon as we register the irq.
	 */
	reg = sdio_readb(func, host_intstatus_reg, &ret);
	if (ret < 0)
		goto release_irq;

	/* Read the IO port */
	reg = sdio_readb(func, io_port_0_reg, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= reg;

	reg = sdio_readb(func, io_port_1_reg, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= (reg << 8);

	reg = sdio_readb(func, io_port_2_reg, &ret);
	if (ret < 0)
		goto release_irq;
	else
		priv->bt_dev.ioport |= (reg << 16);

	PRINTM(INFO, ": SDIO FUNC%d IO port: 0x%x\n", priv->bt_dev.fn,
	       priv->bt_dev.ioport);

	if (priv->card_type == CARD_TYPE_SD8977) {
		if (bt_intmode == INT_MODE_GPIO) {
			PRINTM(MSG, "Enable GPIO-1 INT\n");
			sdio_writeb(func, ENABLE_GPIO_1_INT_MODE,
				    SCRATCH_REG_32, &ret);
			if (ret < 0)
				goto release_irq;
		}
	}

#define SDIO_INT_MASK       0x3F
	if (priv->card_type == CARD_TYPE_SD8887 ||
	    priv->card_type == CARD_TYPE_SD8897 ||
	    priv->card_type == CARD_TYPE_SD8797 ||
	    priv->card_type == CARD_TYPE_SD8977) {
		/* Set Host interrupt reset to read to clear */
		reg = sdio_readb(func, host_int_rsr_reg, &ret);
		if (ret < 0)
			goto release_irq;
		sdio_writeb(func, reg | SDIO_INT_MASK, host_int_rsr_reg, &ret);
		if (ret < 0)
			goto release_irq;
		/* Set auto re-enable */
		reg = sdio_readb(func, card_misc_cfg_reg, &ret);
		if (ret < 0)
			goto release_irq;
		sdio_writeb(func, reg | AUTO_RE_ENABLE_INT, card_misc_cfg_reg,
			    &ret);
		if (ret < 0)
			goto release_irq;
	}

	sdio_set_drvdata(func, card);
	sdio_release_host(func);

	LEAVE();
	return BT_STATUS_SUCCESS;
release_irq:
	sdio_release_irq(func);
release_host:
	sdio_release_host(func);
failed:

	LEAVE();
	return BT_STATUS_FAILURE;
}

/**
 *  @brief This function de-registers the device.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS
 */
int
sbi_unregister_dev(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();

	if (card && card->func) {
		sdio_claim_host(card->func);
		sdio_release_irq(card->func);
		sdio_disable_func(card->func);
		sdio_release_host(card->func);
		sdio_set_drvdata(card->func, NULL);
	}

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function enables the host interrupts.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sd_enable_host_int(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret;

	ENTER();

	if (!card || !card->func) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	ret = sd_enable_host_int_mask(priv, HIM_ENABLE);
	sd_get_rx_unit(priv);
	sdio_release_host(card->func);

	LEAVE();
	return ret;
}

/**
 *  @brief This function disables the host interrupts.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sd_disable_host_int(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret;

	ENTER();

	if (!card || !card->func) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	ret = sd_disable_host_int_mask(priv, HIM_DISABLE);
	sdio_release_host(card->func);

	LEAVE();
	return ret;
}

/**
 *  @brief This function sends data to the card.
 *
 *  @param priv    A pointer to bt_private structure
 *  @param payload A pointer to the data/cmd buffer
 *  @param nb      Length of data/cmd
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_host_to_card(bt_private *priv, u8 *payload, u16 nb)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	struct m_dev *m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	int ret = BT_STATUS_SUCCESS;
	int buf_block_len;
	int blksz;
	int i = 0;
	u8 *buf = NULL;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	buf = payload;

	blksz = SD_BLOCK_SIZE;
	buf_block_len = (nb + blksz - 1) / blksz;
	/* Allocate buffer and copy payload */
	if ((t_ptr)payload & (DMA_ALIGNMENT - 1)) {
		if (nb > MAX_TX_BUF_SIZE) {
			PRINTM(ERROR, "BT: Invalid tx packet, size=%d\n", nb);
			LEAVE();
			return BT_STATUS_FAILURE;
		}
		/* Ensure 8-byte aligned CMD buffer */
		buf = priv->adapter->tx_buf;
		memcpy(buf, payload, nb);
	}
	sdio_claim_host(card->func);
#define MAX_WRITE_IOMEM_RETRY	2
	do {
		/* Transfer data to card */
		ret = sdio_writesb(card->func, priv->bt_dev.ioport, buf,
				   buf_block_len * blksz);
		if (ret < 0) {
			i++;
			PRINTM(ERROR,
			       "BT: host_to_card, write iomem (%d) failed: %d\n",
			       i, ret);
			if ((priv->card_type == CARD_TYPE_SD8887) ||
			    (priv->card_type == CARD_TYPE_SD8897))
				break;
			sdio_writeb(card->func, HOST_WO_CMD53_FINISH_HOST,
				    CONFIGURATION_REG, &ret);
			udelay(20);
			ret = BT_STATUS_FAILURE;
			if (i > MAX_WRITE_IOMEM_RETRY)
				goto exit;
		} else {
			PRINTM(DATA, "BT: SDIO Blk Wr %s: len=%d\n",
			       m_dev->name, nb);
			DBG_HEXDUMP(DAT_D, "BT: SDIO Blk Wr", payload, nb);
		}
	} while (ret == BT_STATUS_FAILURE);
	priv->bt_dev.tx_dnld_rdy = FALSE;
exit:
	sdio_release_host(card->func);
	LEAVE();
	return ret;
}

/**
 *  @brief This function downloads firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_download_fw(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;
	u8 winner = 0;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	sdio_claim_host(card->func);
	if (BT_STATUS_SUCCESS == sd_verify_fw_download(priv, 1)) {
		PRINTM(MSG, "BT: FW already downloaded!\n");
		sdio_release_host(card->func);
		sd_enable_host_int(priv);
		if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
			PRINTM(ERROR,
			       "BT: sbi_register_conf_dpc failed. Terminating download\n");
			ret = BT_STATUS_FAILURE;
			goto err_register;
		}
		goto exit;
	}
	/* Check if other interface is downloading */
	ret = sd_check_winner_status(priv, &winner);
	if (ret == BT_STATUS_FAILURE) {
		PRINTM(FATAL, "BT read winner status failed!\n");
		goto done;
	}
	if (winner) {
		PRINTM(MSG, "BT is not the winner (0x%x). Skip FW download\n",
		       winner);
		/* check if the fimware is downloaded successfully or not */
		if (sd_verify_fw_download(priv, MAX_MULTI_INTERFACE_POLL_TRIES)) {
			PRINTM(FATAL, "BT: FW failed to be active in time!\n");
			ret = BT_STATUS_FAILURE;
			goto done;
		}
		sdio_release_host(card->func);
		sd_enable_host_int(priv);
		if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
			PRINTM(ERROR,
			       "BT: sbi_register_conf_dpc failed. Terminating download\n");
			ret = BT_STATUS_FAILURE;
			goto err_register;
		}
		goto exit;
	}

	do_gettimeofday(&priv->req_fw_time);
	/* Download the main firmware via the helper firmware */
	if (sd_download_firmware_w_helper(priv)) {
		PRINTM(INFO, "BT: FW download failed!\n");
		ret = BT_STATUS_FAILURE;
	}
	goto exit;
done:
	sdio_release_host(card->func);
exit:
	LEAVE();
	return ret;
err_register:
	LEAVE();
	return ret;
}

/**
 *  @brief This function checks the interrupt status and handle it accordingly.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS
 */
int
sbi_get_int_status(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u8 sdio_ireg = 0;
	struct sdio_mmc_card *card = priv->bt_dev.card;

	ENTER();

	OS_INT_DISABLE;
	sdio_ireg = priv->adapter->sd_ireg;
	priv->adapter->sd_ireg = 0;
	OS_INT_RESTORE;
	sdio_claim_host(card->func);
	priv->adapter->irq_done = sdio_ireg;
	if (sdio_ireg & DN_LD_HOST_INT_STATUS) {	/* tx_done INT */
		if (priv->bt_dev.tx_dnld_rdy) {	/* tx_done already received */
			PRINTM(INFO,
			       "BT: warning: tx_done already received: tx_dnld_rdy=0x%x int status=0x%x\n",
			       priv->bt_dev.tx_dnld_rdy, sdio_ireg);
		} else {
			priv->bt_dev.tx_dnld_rdy = TRUE;
		}
	}
	if (sdio_ireg & UP_LD_HOST_INT_STATUS)
		sd_card_to_host(priv);

	ret = BT_STATUS_SUCCESS;
	sdio_release_host(card->func);
	LEAVE();
	return ret;
}

/**
 *  @brief This function wakeup firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS/BT_STATUS_FAILURE or other error no.
 */
int
sbi_wakeup_firmware(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;

	ENTER();

	if (!card || !card->func) {
		PRINTM(ERROR, "BT: card or function is NULL!\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	sdio_claim_host(card->func);
	sdio_writeb(card->func, HOST_POWER_UP, CONFIGURATION_REG, &ret);
	sdio_release_host(card->func);
	PRINTM(CMD, "BT wake up firmware\n");

	LEAVE();
	return ret;
}

/** @brief This function updates the SDIO card types
 *
 *  @param priv     A Pointer to the bt_private structure
 *  @param card     A Pointer to card
 *
 *  @return         N/A
 */
void
sdio_update_card_type(bt_private *priv, void *card)
{
	struct sdio_mmc_card *cardp = (struct sdio_mmc_card *)card;

	/* Update card type */
	if (cardp->func->device == SD_DEVICE_ID_8777_BT_FN2 ||
	    cardp->func->device == SD_DEVICE_ID_8777_BT_FN3)
		priv->card_type = CARD_TYPE_SD8777;
	else if (cardp->func->device == SD_DEVICE_ID_8787_BT_FN2 ||
		 cardp->func->device == SD_DEVICE_ID_8787_BT_FN3)
		priv->card_type = CARD_TYPE_SD8787;
	else if (cardp->func->device == SD_DEVICE_ID_8887_BT_FN2 ||
		 cardp->func->device == SD_DEVICE_ID_8887_BT_FN3)
		priv->card_type = CARD_TYPE_SD8887;
	else if (cardp->func->device == SD_DEVICE_ID_8897_BT_FN2 ||
		 cardp->func->device == SD_DEVICE_ID_8897_BT_FN3)
		priv->card_type = CARD_TYPE_SD8897;
	else if (cardp->func->device == SD_DEVICE_ID_8797_BT_FN2 ||
		 cardp->func->device == SD_DEVICE_ID_8797_BT_FN3)
		priv->card_type = CARD_TYPE_SD8797;
	else if (cardp->func->device == SD_DEVICE_ID_8977_BT_FN2)
		priv->card_type = CARD_TYPE_SD8977;
}

/**
 *  @brief This function get sdio device from card type
 *
 *  @param pmadapter  A pointer to mlan_adapter structure
 *  @return           MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
int
sdio_get_sdio_device(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	u16 card_type = priv->card_type;

	ENTER();

	switch (card_type) {
	case CARD_TYPE_SD8777:
		priv->psdio_device = &bt_sdio_sd8777;
		break;
	case CARD_TYPE_SD8787:
		priv->psdio_device = &bt_sdio_sd8787;
		break;
	case CARD_TYPE_SD8887:
		priv->psdio_device = &bt_sdio_sd8887;
		break;
	case CARD_TYPE_SD8897:
		priv->psdio_device = &bt_sdio_sd8897;
		break;
	case CARD_TYPE_SD8797:
		priv->psdio_device = &bt_sdio_sd8797;
		break;
	case CARD_TYPE_SD8977:
		priv->psdio_device = &bt_sdio_sd8977;
		break;
	default:
		PRINTM(ERROR, "BT can't get right card type \n");
		ret = BT_STATUS_FAILURE;
		break;
	}

	LEAVE();
	return ret;
}

/** @brief This function dump the SDIO register
 *
 *  @param priv     A Pointer to the bt_private structure
 *
 *  @return         N/A
 */
void
bt_dump_sdio_regs(bt_private *priv)
{
	struct sdio_mmc_card *card = priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;
	char buf[256], *ptr;
	u8 loop, func, data;
	unsigned int reg, reg_start, reg_end;
	u8 index = 0;
	unsigned int reg_table_8887[] = { 0x58, 0x59, 0x5c, 0x60, 0x64, 0x70,
		0x71, 0x72, 0x73, 0xd8, 0xd9, 0xda
	};
	u8 loop_num = 0;
	unsigned int *reg_table = NULL;
	u8 reg_table_size = 0;
	if (priv->card_type == CARD_TYPE_SD8887) {
		loop_num = 3;
		reg_table = reg_table_8887;
		reg_table_size = sizeof(reg_table_8887) / sizeof(int);
	} else
		loop_num = 2;
	if (priv->adapter->ps_state)
		sbi_wakeup_firmware(priv);

	sdio_claim_host(card->func);
	for (loop = 0; loop < loop_num; loop++) {
		memset(buf, 0, sizeof(buf));
		ptr = buf;
		if (loop == 0) {
			/* Read the registers of SDIO function0 */
			func = loop;
			reg_start = 0;
			reg_end = 9;

		} else if (loop == 2) {
			/* Read specific registers of SDIO function1 */
			index = 0;
			func = 2;
			reg_start = reg_table[index++];
			reg_end = reg_table[reg_table_size - 1];
		} else {
			func = 2;
			reg_start = 0;
			reg_end = 0x09;
		}
		if (loop == 2)
			ptr += sprintf(ptr, "SDIO Func%d: ", func);
		else
			ptr += sprintf(ptr, "SDIO Func%d (%#x-%#x): ", func,
				       reg_start, reg_end);
		for (reg = reg_start; reg <= reg_end;) {
			if (func == 0)
				data = sdio_f0_readb(card->func, reg, &ret);
			else
				data = sdio_readb(card->func, reg, &ret);
			if (loop == 2)
				ptr += sprintf(ptr, "(%#x)", reg);
			if (!ret)
				ptr += sprintf(ptr, "%02x ", data);
			else {
				ptr += sprintf(ptr, "ERR");
				break;
			}
			if (loop == 2 && reg < reg_end)
				reg = reg_table[index++];
			else
				reg++;
		}
		PRINTM(MSG, "%s\n", buf);
	}
	sdio_release_host(card->func);
}

module_param(fw_name, charp, 0);
MODULE_PARM_DESC(fw_name, "Firmware name");
module_param(bt_req_fw_nowait, int, 0);
MODULE_PARM_DESC(bt_req_fw_nowait,
		 "0: Use request_firmware API; 1: Use request_firmware_nowait API");
module_param(multi_fn, int, 0);
MODULE_PARM_DESC(multi_fn, "Bit 2: FN2;");
module_param(bt_fw_serial, int, 0);
MODULE_PARM_DESC(bt_fw_serial,
		 "0: Support parallel download FW; 1: Support serial download FW");

module_param(bt_intmode, int, 0);
MODULE_PARM_DESC(bt_intmode, "0: INT_MODE_SDIO, 1: INT_MODE_GPIO");
