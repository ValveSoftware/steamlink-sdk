/*
 * Driver for Marvell 88DE3100 SoC NAND controller
 * derived from drivers/mtd/nand/pxa3xx_nand.c
 * Copyright (C) 2010-2011 Zheng Shi <zhengshi@marvell.com>
 *
 * Copyright © 2005 Intel Corporation
 * Copyright © 2006 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <asm/highmem.h>
#include <asm/cacheflush.h>
#include "pxa3xx_nand_debu.h"

#ifdef CONFIG_BERLIN_NAND_READ_RETRY
#include "pxa3xx_nand_read_retry_debu.h"
#endif

/* use reserved dma coherent mem for NDSR DMA WR */
#define NDSR_STR_OFFSET		128
#define NDSR_MASK_OFFSET		256

/* macros for pbridge registers read/write */
#define pb_writel(nand, off, val)	\
	__raw_writel((val), nand->pb_base + (off))
#define pb_readl(nand, off)		\
	__raw_readl(nand->pb_base + (off))

struct dp_erase_desc {
	SIE_BCMSEMA		sema_cmd1;
	struct cmd_3_desc	plane_1_cmd;
	SIE_BCMSEMA		sema_addr1;
	struct cmd_3_desc	plane_1_addr;
	SIE_BCMSEMA		sema_cmd2;
	struct cmd_3_desc	plane_2_cmd;
	SIE_BCMSEMA		sema_addr2;
	struct cmd_3_desc	plane_2_addr;
	SIE_BCMSEMA		sema_conf_cmd;
	struct cmd_3_desc	confirm_cmd;
};

struct write_1_desc {
	SIE_BCMSEMA             sema_cmd1;
	struct cmd_3_desc       cmd1;
	SIE_BCMSEMA             sema_addr;
	struct cmd_3_desc       addr;
	/* chunks * (sema + cmd4 + sema + data) + oob */
	unsigned char data_cmd[0];
};

struct write_2_desc {
	SIE_BCMSEMA             sema_cmd2;
	struct cmd_3_desc       cmd2;
};

static char *plane_cmdline;
module_param(plane_cmdline,  charp, 0000);
MODULE_PARM_DESC(plane_cmdline, "command line for plane numbers of different partitions");

static bool use_dma = 1;
module_param(use_dma, bool, 0444);
MODULE_PARM_DESC(use_dma, "enable DMA for data transferring to/from NAND HW");

static char *usr_ndtr0 = "84840A12";
module_param(usr_ndtr0,  charp, 0000);
MODULE_PARM_DESC(usr_ndtr0, "NDTR0CS0");

static char *usr_ndtr1 = "00208662";
module_param(usr_ndtr1,  charp, 0000);
MODULE_PARM_DESC(usr_ndtr1, "NDTR1CS0");

/*
 * Default NAND flash controller configuration setup by the
 * bootloader. This configuration is used only when pdata->keep_config is set
 */
static struct pxa3xx_nand_cmdset default_cmdset = {
	.read1		= 0x3000,
	.read2		= 0x0050,
	.program	= 0x1080,
	.read_status	= 0x0070,
	.read_id	= 0x0090,
	.erase		= 0xD060,
	.reset		= 0x00FF,
	.lock		= 0x002A,
	.unlock		= 0x2423,
	.lock_status	= 0x007A,
	.dual_plane_read = 0x306060,
	.dual_plane_write = 0x10811180,
	.dual_plane_erase = 0xD06060,
	.dual_plane_randout = 0xE00500,
};

static struct pxa3xx_nand_timing timing[] = {
	{ 40, 80, 60, 100, 80, 100,  90000, 400, 40, },
	{ 10,  0, 20,  40, 30,  40,  11123, 110, 10, },
	{ 10, 25, 15,  25, 15,  30,  25000,  60, 10, },
	{ 10, 35, 15,  25, 15,  25,  25000,  60, 10, },
	{  5, 20, 10,  12, 10,  12,  60000,  60, 10, },
	{  5, 20, 10,  12, 10,  12, 200000, 120, 10, },
	{  5, 15, 10,  15, 10,  15,  60000,  60, 10, },
};

/* TODO: K9LCG08U0A (chip id = 0xdeec, 0x7ad5) is a 8GiB NAND, but we can
 *       only support the low 4GiB space at present, for the whole 8GiB space
 *       isn't continuous, and we can't support non-continous addressing.
 *       Here is the addressing space of K9LCG08U0A.
 *        - 0-4GiB Addressing: 0x000000000 ~ 0x1037fffff
 *                            (0x100000000 ~ 0x1037fffff is for spare area)
 *        - 4-8GiB Addressing: 0x200000000 ~ 0x3037fffff
 *                            (0x300000000 ~ 0x3037fffff is for spare area)
 */
static struct pxa3xx_nand_flash builtin_flash_types[] = {
{ "DEFAULT FLASH",      0,      0,   0, 2048,  8,  8, 0,    0, &timing[0], 0 },
{ "64MiB 16-bit",  0x46ec, 0xffff,  32,  512, 16, 16, 1, 4096, &timing[1], 0 },
{ "256MiB 8-bit",  0xdaec, 0xffff,  64, 2048,  8,  8, 1, 2048, &timing[1], 0 },
{ "4GiB 8-bit",    0xd7ec, 0xb655, 128, 4096,  8,  8, 4, 8192, &timing[1], 0 },
{ "4GiB 8-bit",    0xd7ec, 0x29d5, 128, 4096,  8,  8, 8, 8192, &timing[1], 0 },
{ "128MiB 8-bit",  0xa12c, 0xffff,  64, 2048,  8,  8, 1, 1024, &timing[2], 0 },
{ "128MiB 16-bit", 0xb12c, 0xffff,  64, 2048, 16, 16, 1, 1024, &timing[2], 0 },
{ "512MiB 8-bit",  0xdc2c, 0xffff,  64, 2048,  8,  8, 1, 4096, &timing[2], 0 },
{ "512MiB 16-bit", 0xcc2c, 0xffff,  64, 2048, 16, 16, 1, 4096, &timing[2], 0 },
{ "256MiB 16-bit", 0xba20, 0xffff,  64, 2048, 16, 16, 1, 2048, &timing[3], 0 },
{ "2GiB 8-bit",    0xd5ec, 0xb614, 128, 4096,  8,  8, BCH_STRENGTH, 4096, &timing[4], 1 },
{ "2GiB 8-bit",    0xd5ec, 0x7284, 128, 8192,  8,  8, 48, 2048, &timing[4], 0 },
{ "2GiB 8-bit",    0xd598, 0x3284, 128, 8192,  8,  8, 80, 2048, &timing[5], 1 },
{ "2GiB 8-bit",    0x482c, 0x4a04, 256, 4096,  8,  8, 48, 2048, &timing[6], 1 },
{ "2GiB 8-bit",    0xd5ad, 0xda94, 256, 8192,  8,  8, 80, 1024, &timing[6], 1 },
{ "4GiB 8-bit",    0xd7ec, 0x7A94, 128, 8192,  8,  8, 48, 4096, &timing[5], 1 },
{ "4GiB 8-bit",    0xd7ec, 0x7e94, 128, 8192,  8,  8, 80, 4096, &timing[5], 1 },
{ "8GiB 8-bit",    0xdeec, 0x7ad5, 128, 8192,  8,  8, 48, 4096, &timing[5], 1 },
{ "4GiB 8-bit",    0xd7ad, 0x9A94, 256, 8192,  8,  8, 48, 2048, &timing[5], 1 },
{ "4GiB 8-bit",    0x682c, 0x4a04, 256, 4096,  8,  8, 48, 4096, &timing[6], 1 },
{ "4GiB 8-bit",    0x682c, 0x4604, 256, 4096,  8,  8, 48, 4096, &timing[6], 1 },
{ "8GiB 8-bit",    0x882c, 0x4b04, 256, 8192,  8,  8, 48, 4096, &timing[6], 1 },
{ "8GiB 8-bit",    0xdead, 0xda94, 256, 8192,  8,  8, 80, 4096, &timing[6], 1 },
};

static const unsigned char g_byte_zero_bits[256] = {
	8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
	7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
};

/* Define a default flash type setting serve as flash detecting only */
#define DEFAULT_FLASH_TYPE (&builtin_flash_types[0])

const char *mtd_names[] = {"mv_nand", "mv_nand", NULL};

#define NDTR0_tCH(c)	(min((c), 7) << 19)
#define NDTR0_tCS(c)	(min((c), 7) << 16)
#define NDTR0_tWH(c)	(min((c), 7) << 11)
#define NDTR0_tWP(c)	(min((c), 7) << 8)
#define NDTR0_tRH(c)	(min((c), 7) << 3)
#define NDTR0_tRP(c)	(min((c), 7) << 0)

#define NDTR1_tR(c)	(min((c), 65535) << 16)
#define NDTR1_tWHR(c)	(min((c), 15) << 4)
#define NDTR1_tAR(c)	(min((c), 15) << 0)

int parse_mtd_partitions(struct mtd_info *master, const char **types,
			 struct mtd_partition **pparts,
			 struct mtd_part_parser_data *data);



#define PXA3XX_NAND_DMA_DEBUG 1
#if (PXA3XX_NAND_DMA_DEBUG == 1)
#define TEST_NUM	100
static void pxa3xx_nand_dump_registers(struct pxa3xx_nand_info *info);
static void pxa3xx_nand_start(struct pxa3xx_nand_info *info);
void pb_init(struct pxa3xx_nand *nand);

static struct pxa3xx_dma_debug_info {
	unsigned int cmd;
	int page_addr;
	int planes;
	unsigned int buff;
	unsigned int mode;
	unsigned int step;
	ktime_t       exec_time;
	ktime_t       finish_time;
} nand_debug_info[TEST_NUM];
static int current_num = 0;

#define loop_watcher(loop, limit)  					\
{									\
	loop++; 							\
	if ( loop > limit ) {						\
		printk("%s time out, retrying !!!!!!\n", __FUNCTION__);	\
		debug_info_print(nand);					\
		return -1;						\
	}								\
}

static void debug_push_info(struct mtd_info *mtd, unsigned command,
		int page_addr)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int num;

	current_num++;
	if (current_num == TEST_NUM) {
		current_num = 0;
	}

	num = current_num;
	memset(&nand_debug_info[num], 0, sizeof(nand_debug_info[num]));
	nand_debug_info[num].cmd = command;
	nand_debug_info[num].page_addr = page_addr;
	nand_debug_info[num].planes = info->n_planes;
	nand_debug_info[num].mode = info->eslc_mode;
	nand_debug_info[num].exec_time =  ktime_get();
	if (command == NAND_CMD_RNDOUT) {
		nand_debug_info[num].buff = (int)nand->data_buff;
	} else if (command == NAND_CMD_PAGEPROG) {
		nand_debug_info[num].buff = (int)nand->data_buff_phys;
	} else {
		nand_debug_info[num].buff = (int)NULL;
	}

}

static void debug_info_print(struct pxa3xx_nand *nand)
{
	int i, num = current_num;
	int time_ent, time_ext;

	pxa3xx_nand_dump_registers(nand->info[nand->chip_select]);
	for (i = 0; i < TEST_NUM; i++) {
		time_ent = (unsigned long) ktime_to_ns(nand_debug_info[num].exec_time) >> 10;
		time_ext = (unsigned long) ktime_to_ns(nand_debug_info[num].finish_time) >> 10;
		printk("Num[%02d] CMD[%04x] page_addr[%08x] planes[%d] mode [%d] buff[%08x] "
			"ent[%08x] ext[%08x]\n",
			num, nand_debug_info[num].cmd, nand_debug_info[num].page_addr,
			nand_debug_info[num].planes, nand_debug_info[num].mode, nand_debug_info[num].buff,
			time_ent, time_ext);

		if (num == 0)
			num = TEST_NUM -1;
		else
			num--;
	}
}

#endif	//(PXA3XX_NAND_DMA_DEBUG == 1)

#ifdef CONFIG_BERLIN_NAND_RANDOMIZER

#include "nand_randomizer.h"

#define NAND_ID_SIZE			(6)	/* full 6 bytes HW support up to 7
						 * most of the chip should be 6
						 */

static void nand_read_chip_id(struct mtd_info *mtd, unsigned char *id_data, int id_len)
{
	struct nand_chip *chip = mtd->priv;

	/* Send the command for reading device ID */
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
	/* Read entire ID string */

	chip->read_buf(mtd, id_data, id_len);

}

static void nand_randomizer_init_by_chip(struct mtd_info *mtd, int chip)
{
	static unsigned char randomizer_buffer[MV_NAND_RANDOMIZER_BUFFER_SIZE_MAX];
	static struct mtd_info *pre_mtd = NULL;
	static int pre_chip = -1;
	unsigned int block_size = 0;
	unsigned int page_size = 0;
	unsigned int oob_size = 32; /* should be mtd->oobsize,
				     * we fix 32 to make sure it can work on marvell NFC
				     */
	unsigned char id_data[NAND_ID_SIZE];
	int randomized;

	if ((chip < 0) || (/*mtd == pre_mtd &&*/ chip == pre_chip))
		return;

	pre_mtd = mtd;
	pre_chip = chip;

	nand_read_chip_id(mtd, id_data, NAND_ID_SIZE);
	printk(KERN_INFO "NAND ID 0x%02X%02X%02X%02X%02X%02X \n",id_data[0],id_data[1],id_data[2],id_data[3], id_data[4],id_data[5]);
	randomized = mv_nand_randomizer_init(id_data,
						NAND_ID_SIZE,
						block_size,
						page_size,
						oob_size,
						randomizer_buffer,
						MV_NAND_RANDOMIZER_BUFFER_SIZE_MAX);

#if 0
	/*
	 * TODO: move this print to a more reasonable place
	 */
	if (randomized)
		printk("[%s,%d] mtd=%p, chip=%d, ID=0x%02X%02X%02X%02X: "
				"!!! use sw randomizer !!!\n",
				__func__, __LINE__, mtd, chip,
				id_data[0], id_data[1], id_data[2], id_data[3]);
#endif
}

static void nand_randomizer_release(struct platform_device *pdev)
{
}

static int nand_read_page_post_process(struct mtd_info *mtd, unsigned int page_addr,
	const unsigned char *data_src, const unsigned char *oob_src,
	unsigned char *data_dst, unsigned char *oob_dst)
{
	unsigned int randomized_length = 0;

	randomized_length = mv_nand_randomizer_randomize_page(page_addr,
		data_src, oob_src, data_dst, oob_dst);

	return randomized_length > 0 ? 1 : 0;
}

/*
 * return 1, mtd needs sw randomization, and data_src is randomized into data_dst
 * return 0, mtd doesn't need sw randomization, data_src is not randomized
 */
static int nand_write_page_pre_process(struct mtd_info *mtd, unsigned int page_addr,
	const unsigned char *data_src, const unsigned char *oob_src,
	unsigned char *data_dst, unsigned char *oob_dst)
{
	unsigned int randomized_length = 0;

	randomized_length = mv_nand_randomizer_randomize_page(page_addr,
		data_src, oob_src, data_dst, oob_dst) ? 1 : 0;

	return randomized_length > 0 ? 1 : 0;
}

#endif /* CONFIG_BERLIN_NAND_RANDOMIZER */

static dma_addr_t map_addr(struct pxa3xx_nand *nand, void *buf,
			   size_t sz, int dir)
{
	struct device *dev = &nand->pdev->dev;
	/* if not cache aligned, don't use dma */
	if (((size_t)buf & 0x1f) || (sz & 0x1f))
		return ~0;
#ifdef CONFIG_HIGHMEM
	if ((size_t)buf >= PKMAP_ADDR(0) && (size_t)buf < PKMAP_ADDR(LAST_PKMAP)) {
		struct page *page = pte_page(pkmap_page_table[PKMAP_NR((size_t)buf)]);
		return dma_map_page(dev, page, (size_t)buf & (PAGE_SIZE - 1), sz, dir);
	}
#endif
	if (buf >= high_memory) {
		struct page *page;

		if (((size_t) buf & PAGE_MASK) !=
		    ((size_t) (buf + sz - 1) & PAGE_MASK))
			return ~0;

		page = vmalloc_to_page(buf);
		if (!page)
			return ~0;

		return dma_map_page(dev, page, (size_t)buf & (PAGE_SIZE - 1), sz, dir);
	}
	return dma_map_single(dev, buf, sz, dir);
}

static void unmap_addr(struct device *dev, dma_addr_t buf, void *orig_buf,
		       size_t sz, int dir)
{
	if (!buf)
		return;
#ifdef CONFIG_HIGHMEM
	if (orig_buf >= high_memory) {
		dma_unmap_page(dev, buf, sz, dir);
		return;
	} else if ((size_t)orig_buf >= PKMAP_ADDR(0) && (size_t)orig_buf < PKMAP_ADDR(LAST_PKMAP)) {
		dma_unmap_page(dev, buf, sz, dir);
		return;
	}
#endif
	dma_unmap_single(dev, buf, sz, dir);
}

static void pxa3xx_nand_dump_registers(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	printk(KERN_INFO "================================\n");
	printk(KERN_INFO "NDTR0       = %08X, expect %08X\n",
			nand_readl(nand, NDTR0CS0), info->ndtr0cs0);
	printk(KERN_INFO "NDTR1       = %08X, expect %08X\n",
			nand_readl(nand, NDTR1CS0), info->ndtr1cs0);
	printk(KERN_INFO "NDCR        = %08X\n", nand_readl(nand, NDCR));
	printk(KERN_INFO "NDECCCTRL   = %08X\n", nand_readl(nand, NDECCCTRL));
	printk(KERN_INFO "NDSR        = %08X\n", nand_readl(nand, NDSR));
	printk(KERN_INFO "NDCB        = %08x %08x %08x %08x\n",
			nand_readl(nand, NDCB0),
			nand_readl(nand, NDCB1),
			nand_readl(nand, NDCB2),
			nand_readl(nand, NDCB3));
	printk(KERN_INFO "================================\n");
}

static void pxa3xx_nand_set_timing(struct pxa3xx_nand_info *info,
				   const struct pxa3xx_nand_timing *t)
{
	struct pxa3xx_nand *nand = info->nand_data;
	uint32_t ndtr0, ndtr1;

	sscanf(usr_ndtr0, "%x", &ndtr0);
	sscanf(usr_ndtr1, "%x", &ndtr1);

	printk("[%s,%d] ndtr0=%08X, ndtr1=%08X\n",
			__func__, __LINE__, ndtr0, ndtr1);
	info->ndtr0cs0 = ndtr0;
	info->ndtr1cs0 = ndtr1;
	nand_writel(nand, NDTR0CS0, ndtr0);
	nand_writel(nand, NDTR1CS0, ndtr1);
}

static void pxa3xx_set_datasize(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;

	nand->data_size = (info->page_size < PAGE_CHUNK_SIZE)
				? 512 : PAGE_CHUNK_SIZE;

	/* TODO:
	 * even when ndcr.SPARE_EN is disabled, we can read oob area,
	 * is confirming with ASIC team
	 */

	if (info->page_size < PAGE_CHUNK_SIZE) {
		switch (nand->ecc_strength) {
		case 0:
			nand->oob_size = 16;
			break;
		case HAMMING_STRENGTH:
			nand->oob_size = 8;
			break;
		default:
			printk("Don't support BCH on small page device!!!\n");
			BUG();
		}
		return;
	}

	switch (nand->ecc_strength) {
	case 0:
		nand->oob_size = 64;
		break;
	case HAMMING_STRENGTH:
		nand->oob_size = 40;
		break;
	default:
		nand->oob_size = 32;
	}
}

/**
 * NOTE: it is a must to set ND_RUN firstly, then write
 * command buffer, otherwise, it does not work.
 *
 * For APSE SoCs:
 * We enable all the interrupt at the same time, and
 * let pxa3xx_nand_irq to handle all logic.
 * But for DEBU SoCs:
 * We don't enable NFC interrupts, if DMA is enabled,
 * pBridge will handle all logic.
 */
static void pxa3xx_nand_start(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	uint32_t ndcr, ndeccctrl = 0;

	ndcr = info->reg_ndcr;
	ndcr |= NDCR_ND_RUN;

	switch (nand->ecc_strength) {
	default:
		ndeccctrl |= NDECCCTRL_BCH_EN;
		ndeccctrl |= NDECCCTRL_ECC_THRESH(BCH_THRESHOLD);
	case HAMMING_STRENGTH:
		ndcr |= NDCR_ECC_EN;
	case 0:
		break;
	}
	ndeccctrl |= NDECCCTRL_ECC_STRENGTH(nand->ecc_strength);

	nand->ndeccctrl = ndeccctrl;
	nand->ndcr = ndcr;

	/* clear status bits and run */
	nand_writel(nand, NDCR, 0);
	nand_readl(nand, NDECCCTRL);
	nand_writel(nand, NDECCCTRL, ndeccctrl);
	nand_writel(nand, NDSR, NDSR_MASK);

	nand_writel(nand, NDCR, ndcr);
}

static void pxa3xx_nand_stop(struct pxa3xx_nand *nand)
{
	uint32_t ndcr;
	int timeout = NAND_STOP_DELAY;

	/* wait RUN bit in NDCR become 0 */
	ndcr = nand_readl(nand, NDCR);
	while ((ndcr & NDCR_ND_RUN) && (timeout-- > 0)) {
		ndcr = nand_readl(nand, NDCR);
		udelay(1);
	}

	if (timeout <= 0) {
		ndcr &= ~NDCR_ND_RUN;
		nand_writel(nand, NDCR, ndcr);
	}
	/* clear status bits */
	nand_writel(nand, NDSR, NDSR_MASK);
}

static void handle_data_pio(struct pxa3xx_nand *nand,
		int data_size, int oob_size)
{
	uint16_t real_data_size = DIV_ROUND_UP(data_size, 4);
	uint16_t real_oob_size = DIV_ROUND_UP(oob_size, 4);

	switch (nand->state) {
	case STATE_PIO_WRITING:
		if (data_size > 0)
			__raw_writesl(nand->mmio_base + NDDB,
					nand->data_buff + nand->data_column,
					real_data_size);
		if (real_oob_size > 0)
			__raw_writesl(nand->mmio_base + NDDB,
					nand->oob_buff + nand->oob_column,
					real_oob_size);
		break;
	case STATE_PIO_READING:
		if (data_size > 0)
			__raw_readsl(nand->mmio_base + NDDB,
					nand->data_buff + nand->data_column,
					real_data_size);
		if (real_oob_size > 0)
			__raw_readsl(nand->mmio_base + NDDB,
					nand->oob_buff + nand->oob_column,
					real_oob_size);
		break;
	default:
		printk(KERN_ERR "%s: invalid state %d\n", __func__,
				nand->state);
		BUG();
	}

	nand->data_column += data_size;
	nand->oob_column += oob_size;
}

static irqreturn_t mv88dexx_nand_dma_intr(int irq, void *devid)
{
	struct pxa3xx_nand *nand = devid;
	unsigned int status;

	/* clear interrupt */
	status = pb_readl(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub
			+ RA_dHubReg_SemaHub + RA_SemaHub_Query
			+ (0x80 | PBSemaMap_dHubSemID_NFC_DATA_CP << 2));
	if ((status & 0xFFFF) == 1)
		pb_writel(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub
				+ RA_dHubReg_SemaHub + RA_SemaHub_POP,
				0x100 | PBSemaMap_dHubSemID_NFC_DATA_CP);
	pb_writel(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub
			+ RA_dHubReg_SemaHub + RA_SemaHub_full,
			(1 << PBSemaMap_dHubSemID_NFC_DATA_CP));

	status = nand_readl(nand, NDSR);
	if (status & NDSR_SBERR) {
		u32 *status_array;
		int chunks, i;
		struct pxa3xx_nand_info *info;

		nand->retcode = ERR_SBERR;
		info = nand->info[nand->chip_select];
		chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
			1 : (info->page_size / PAGE_CHUNK_SIZE);
		status_array = (u32 *)(nand->data_desc + DMA_H_SIZE - NDSR_STR_OFFSET);
		for (i = 0; i < chunks; i++) {
			nand->bad_count += (status_array[i<<1]>>16)&0xff;
		}
	}

	if (status & NDSR_DBERR)
		nand->retcode = ERR_DBERR;
	if (status & NDSR_CS0_BBD) {
		nand->retcode = ERR_BBERR;
		printk(KERN_INFO "BBERR: status %x, page %x, cmd %x\n",
			status, nand->page_addr, nand->command);
	}

	nand_writel(nand, NDSR, status);

	complete(&nand->cmd_complete);

	return IRQ_HANDLED;
}

static int is_trunk_blank(int ecc, uint8_t *data_buf, uint8_t *oob_buf,
		size_t data_len, size_t oob_len)
{
	int i, zero_bits_num = 0;

	for (i = 0, zero_bits_num = 0; i < data_len; i++) {
		zero_bits_num += g_byte_zero_bits[data_buf[i]];
		if (zero_bits_num >= ecc) {
			printk(KERN_DEBUG "[%s], data %d %d\n", __func__,
				zero_bits_num, i);
			return 0;
		}
	}

	if (oob_buf) {
		for (i = 0; i < oob_len; i++) {
			zero_bits_num += g_byte_zero_bits[oob_buf[i]];
			if (zero_bits_num >= ecc) {
				printk(KERN_DEBUG "[%s], oob %d %d\n", __func__,
					zero_bits_num, i);
				return 0;
			}
		}
	}

	return 1;
}

static inline int is_read_page_blank(int ecc, uint8_t *data_buf,
		uint8_t *oob_buf, size_t data_len, size_t oob_len, int plane_num)
{
	int plane, i, chunks, len;

	if (plane_num == 2) {
		data_len >>= 1;
		oob_len >>= 1;
	} else {
		plane_num = 1;
	}
	chunks = data_len / PAGE_CHUNK_SIZE;

	for (plane = 0; plane < plane_num; plane++) {
		for (i = 0, len = data_len; i < chunks-1; i++) {
			if (!is_trunk_blank(ecc, data_buf, NULL,
				PAGE_CHUNK_SIZE, 0)) {
				printk(KERN_INFO "[%s], planes [%d of %d], chunk %d\n",
					__func__, plane, plane_num, i);
				return 0;
			}
			data_buf += PAGE_CHUNK_SIZE;
			len -= PAGE_CHUNK_SIZE;
		}

		if (!is_trunk_blank(ecc, data_buf, oob_buf + plane*oob_len,
			len, oob_len)) {
			printk(KERN_INFO "[%s], planes [%d of %d], chunk %d\n",
				__func__, plane, plane_num, chunks-1);
			return 0;
		}
		data_buf += len;
	}
	return 1;
}

static int prepare_command_pool(struct pxa3xx_nand_info *info, int command,
		uint16_t column, int page_addr)
{
	uint16_t cmd;
	int addr_cycle, exec_cmd, i, chunks = 0;
	struct mtd_info *mtd;
	struct pxa3xx_nand *nand = info->nand_data;
	struct nand_chip *chip = &info->nand_chip;
	struct platform_device *pdev = nand->pdev;
	struct pxa3xx_nand_platform_data *pdata = pdev->dev.platform_data;
	uint32_t ndcb0;

	mtd = get_mtd_by_info(info);
	ndcb0 = (nand->chip_select) ? NDCB0_CSEL : 0;
	addr_cycle = 0;
	exec_cmd = 1;

	switch (command) {
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_RNDOUT:
		pxa3xx_set_datasize(info);
		chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
			 1 : (info->page_size / PAGE_CHUNK_SIZE);
		/* force to use LEN_OVRD which could make code more generic */
		ndcb0 |= NDCB0_LEN_OVRD;
		break;
	default:
		i = (uint32_t)(&nand->buf_start) - (uint32_t)nand;
		memset(&nand->buf_start, 0, sizeof(struct pxa3xx_nand) - i);
		break;
	}

	for (i = 0; i < CMD_POOL_SIZE; i ++)
		nand->ndcb0[i] = ndcb0;
	addr_cycle = NDCB0_ADDR_CYC(info->row_addr_cycles
				    + info->col_addr_cycles);

	nand->total_cmds = 1;
	nand->buf_start = column;
	switch (command) {
	case NAND_CMD_SEQIN:
	case NAND_CMD_READ0:
	case NAND_CMD_READOOB:
		nand->page_addr = page_addr;
		nand->column = column;
		nand->ecc_strength = info->ecc_strength;
		/* small page addr setting */
		if (unlikely(info->page_size < PAGE_CHUNK_SIZE)) {
			nand->ndcb1 = ((page_addr & 0xFFFFFF) << 8)
					| (column & 0xFF);

			nand->ndcb2 = 0;
		} else {
			nand->ndcb1 = ((page_addr & 0xFFFF) << 16)
					| (column & 0xFFFF);

			if (page_addr & 0xFF0000)
				nand->ndcb2 = (page_addr & 0xFF0000) >> 16;
			else
				nand->ndcb2 = 0;
		}

		nand->buf_count = mtd->writesize + mtd->oobsize;
		exec_cmd = 0;
		break;

	case NAND_CMD_RNDOUT:
		nand->page_addr = page_addr;
		cmd = info->cmdset->read1;
		if (command == NAND_CMD_READOOB)
			nand->buf_start = mtd->writesize + column;
		else
			nand->buf_start = column;

		if (unlikely(info->page_size < PAGE_CHUNK_SIZE)
		    || !(pdata->controller_attrs & PXA3XX_NAKED_CMD_EN)) {
			if (unlikely(info->page_size < PAGE_CHUNK_SIZE))
				nand->ndcb0[0] |= NDCB0_CMD_TYPE(0)
						| addr_cycle
						| (cmd & NDCB0_CMD1_MASK);
			else
				nand->ndcb0[0] |= NDCB0_CMD_TYPE(0)
						| NDCB0_DBC
						| addr_cycle
						| cmd;
			break;
		}

		i = 0;
		nand->ndcb0[0] &= ~NDCB0_LEN_OVRD;
		nand->ndcb0[i ++] |= NDCB0_CMD_XTYPE(NDCB0_CMDX_CMDP)
				| NDCB0_CMD_TYPE(0)
				| NDCB0_DBC
				| NDCB0_NC
				| addr_cycle
				| cmd;

		ndcb0 = nand->ndcb0[i] | NDCB0_CMD_XTYPE(NDCB0_CMDX_NREAD) | NDCB0_NC;
		nand->total_cmds = chunks + i;
		for (; i <= nand->total_cmds - 1; i ++)
			nand->ndcb0[i] = ndcb0;
		nand->ndcb0[nand->total_cmds - 1] &= ~NDCB0_NC;
		/* last cmd must contain oob data
		 * both in SINGLE_SPARE and MULTI_SPARE mode */
		nand->chunk_oob_size[chunks] = nand->oob_size;

		/* we should wait RnB go high again
		 * before read out data*/
		nand->wait_ready[1] = 1;

		break;

	case NAND_CMD_PAGEPROG:
		if (nand->command == NAND_CMD_NONE) {
			exec_cmd = 0;
			break;
		}

		cmd = info->cmdset->program;
		if (unlikely(info->page_size < PAGE_CHUNK_SIZE)
		    || !(pdata->controller_attrs & PXA3XX_NAKED_CMD_EN)) {
			nand->ndcb0[0] |= NDCB0_CMD_TYPE(0x1)
					| NDCB0_ST_ROW_EN
					| NDCB0_DBC
					| cmd
					| addr_cycle;
			break;
		}

		nand->total_cmds = chunks + 1;
		nand->ndcb0[0] |= NDCB0_CMD_XTYPE(0x4)
				| NDCB0_CMD_TYPE(0x1)
				| NDCB0_NC
				| (cmd & NDCB0_CMD1_MASK)
				| addr_cycle;

		for (i = 1; i < chunks; i ++)
			nand->ndcb0[i] |= NDCB0_CMD_XTYPE(0x5)
					| NDCB0_NC
					| (cmd & NDCB0_CMD1_MASK)
					| NDCB0_CMD_TYPE(0x1);

		nand->ndcb0[chunks] |= NDCB0_CMD_XTYPE(0x3)
					| NDCB0_CMD_TYPE(0x1)
					| NDCB0_ST_ROW_EN
					| NDCB0_DBC
					| (cmd & NDCB0_CMD2_MASK)
					| NDCB0_CMD1_MASK;
		nand->ndcb0[chunks] &= ~NDCB0_LEN_OVRD;
		/* last cmd must contain oob data
		 * both in SINGLE_SPARE and MULTI_SPARE mode */
		nand->chunk_oob_size[chunks - 1] = nand->oob_size;

		/*
		 * we should wait for RnB goes high which
		 * indicate the data has been written succesfully
		 */
		nand->wait_ready[nand->total_cmds] = 1;
		break;

	case NAND_CMD_READID:
		cmd = info->cmdset->read_id;
		nand->buf_count = info->read_id_bytes;
		nand->data_buff = (unsigned char *)chip->buffers;
		nand->ndcb0[0] |= NDCB0_CMD_TYPE(3)
				| NDCB0_ADDR_CYC(1)
				| cmd;
		nand->data_size = 8;
		break;
	case NAND_CMD_STATUS:
		cmd = info->cmdset->read_status;
		nand->buf_count = 1;
		nand->data_buff = (unsigned char *)chip->buffers;
		nand->ndcb0[0] |= NDCB0_CMD_TYPE(4)
				| NDCB0_ADDR_CYC(1)
				| cmd;

		nand->data_size = 8;
		break;

	case NAND_CMD_ERASE1:
		nand->page_addr = page_addr;
		nand->column = column;
		cmd = info->cmdset->erase;
		nand->ndcb0[0] |= NDCB0_CMD_TYPE(2)
				| NDCB0_AUTO_RS
				| NDCB0_ADDR_CYC(3)
				| NDCB0_DBC
				| cmd;
		nand->ndcb1 = page_addr;
		nand->ndcb2 = 0;

		break;
	case NAND_CMD_RESET:
		cmd = info->cmdset->reset;
		nand->ndcb0[0] |= NDCB0_CMD_TYPE(5)
				| cmd;

		break;

	case NAND_CMD_ERASE2:
		exec_cmd = 0;
		break;

	default:
		exec_cmd = 0;
		printk(KERN_ERR "pxa3xx-nand: non-supported"
			" command %x\n", command);
		break;
	}

	/* dma write needs this to unmap addr in waitfunc */
	nand->command = command;

	return exec_cmd;
}

static int do_wait_and_clear_status_bit(struct pxa3xx_nand *nand,
		int status, unsigned long delay,
		int print, const char *func, int line_no)
{
	volatile uint32_t ndsr;
	int i = 0;
	unsigned long timeo = jiffies + delay;

	ndsr = nand_readl(nand, NDSR);
	while (!(ndsr & status)) {
		i++;
		if (print && i > 10 && i <= 20) {
			printk("[%s,%d] status %08X, %dth waiting for %08X\n",
					func, line_no, ndsr, i, status);
		}
		ndsr = nand_readl(nand, NDSR);
		if (time_before(jiffies, timeo)) {
			continue;
		} else {
			printk("[%s,%d] status %08X, timeout waiting for %08X\n",
					func, line_no, ndsr, status);
			goto timeout;
		}
	}
	if (print) {
		printk("[%s,%d] status %08X, %d waits and hit request %08X\n",
				func, line_no, ndsr, i, status);
	}
	/* clear status bits */
	nand_writel(nand, NDSR, status);
timeout:
	return ndsr;
}
#define wait_and_clear_status_bit(nand, status, print) \
	do_wait_and_clear_status_bit(nand, status, CHIP_DELAY_TIMEOUT, print, \
			__func__, __LINE__)

static unsigned int do_wait_for_status(
		struct pxa3xx_nand *nand,
		unsigned status,
		unsigned long delay,
		int print,
		const char * func,
		int line_no)
{
	volatile uint32_t ndsr;
	int i = 0;
	unsigned long timeo = jiffies + delay;

	ndsr = nand_readl(nand, NDSR);
	while(!(ndsr & status)) {
		i++;
		if (print && i > 10 && i <= 20) {
			printk("[%s,%d] status %08X, %dth waiting for %08X\n",
					func, line_no, ndsr, i, status);
		}
		ndsr = nand_readl(nand, NDSR);
		if (time_before(jiffies, timeo)) {
			continue;
		} else {
			printk("[%s,%d] status %08X, timeout waiting for %08X\n",
					func, line_no, ndsr, status);
			goto timeout;
		}
	}
	if (print) {
		printk("[%s,%d] status %08X, %d waits and hit request %08X\n",
				func, line_no, ndsr, i, status);
	}
timeout:
	return ndsr;
}
#define wait_for_status(nand, status, delay, print) \
	do_wait_for_status(nand, status, delay, print, __func__, __LINE__)

/*
 * no NFC interrupt is enabled for DEBU chips
 * so we need to use while to simulate NFC state machine
 */
static void pxa3xx_nand_run_state_machine(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int status, is_completed = 0, cs, ndcb1, ndcb2;
	unsigned int ready, cmd_done, page_done, badblock_detect;

	cs		= nand->chip_select;
	ready           = (cs) ? NDSR_RDY : NDSR_FLASH_RDY;
	cmd_done        = (cs) ? NDSR_CS1_CMDD : NDSR_CS0_CMDD;
	page_done       = (cs) ? NDSR_CS1_PAGED : NDSR_CS0_PAGED;
	badblock_detect = (cs) ? NDSR_CS1_BBD : NDSR_CS0_BBD;

	while (1) {
		status = wait_for_status(nand, NDSR_MASK, CHIP_DELAY_TIMEOUT, 0);

		if (status & NDSR_DBERR)
			nand->retcode = ERR_DBERR;
		if (status & NDSR_SBERR)
			nand->retcode = ERR_SBERR;
		if (status & NDSR_RDDREQ) {
			nand->state = STATE_PIO_READING;
			nand_writel(nand, NDSR, NDSR_RDDREQ);
			/* only read data */
			handle_data_pio(nand, 32, 0);
			if (nand->data_column < (nand->curr_chunk + 1) * nand->data_size)
				continue;
			nand->curr_chunk++;
			/* only read oob after final data chunk */
			if (nand->chunk_oob_size[nand->cmd_seqs - 1] &&
					nand->curr_chunk == nand->total_cmds - 1) {
				wait_and_clear_status_bit(nand, NDSR_RDDREQ, 0);
				handle_data_pio(nand, 0, nand->chunk_oob_size[nand->cmd_seqs-1]);
			}
		}
		if (status & NDSR_WRDREQ) {
			nand->state = STATE_PIO_WRITING;
			nand_writel(nand, NDSR, NDSR_WRDREQ);
			handle_data_pio(nand, nand->data_size, 0);
			nand->curr_chunk++;
			/* only write oob after final data chunk */
			if (nand->chunk_oob_size[nand->cmd_seqs - 1] &&
					nand->curr_chunk == nand->total_cmds - 1) {
				handle_data_pio(nand, 0, nand->chunk_oob_size[nand->cmd_seqs-1]);
			}
		}
		if (status & ready) {
			nand->is_ready = 1;
			nand->state = STATE_READY;
			if (nand->wait_ready[nand->cmd_seqs]) {
				if (nand->cmd_seqs == nand->total_cmds)
					is_completed = 1;
			}
		}
		if (nand->wait_ready[nand->cmd_seqs] && (nand->state != STATE_READY))
			continue;

		if (status & cmd_done) {
			nand->state = STATE_CMD_DONE;
			if (nand->cmd_seqs == nand->total_cmds
					&& !nand->wait_ready[nand->cmd_seqs])
				is_completed = 1;
		}

		if (status & NDSR_WRCMDREQ) {
			nand_writel(nand, NDSR, NDSR_WRCMDREQ);
			status &= ~NDSR_WRCMDREQ;
			nand->state = STATE_CMD_HANDLE;
			if (nand->cmd_seqs < nand->total_cmds) {
				if (nand->cmd_seqs == 0) {
					ndcb1 = nand->ndcb1;
					ndcb2 = nand->ndcb2;
				} else {
					ndcb1 = 0;
					ndcb2 = 0;
				}
				nand_writel(nand, NDCB0, nand->ndcb0[nand->cmd_seqs]);
				nand_writel(nand, NDCB0, ndcb1);
				nand_writel(nand, NDCB0, ndcb2);

				if (nand->ndcb0[nand->cmd_seqs] & NDCB0_LEN_OVRD) {
					nand_writel(nand, NDCB0, nand->data_size
							+ nand->chunk_oob_size[nand->cmd_seqs]);
				}
			} else
				is_completed = 1;

			nand->cmd_seqs ++;
		}

		/* clear NDSR to let the controller exit the IRQ */
		nand_writel(nand, NDSR, status);
		if (is_completed)
			break;
	}

	return;
}

/*******************************
 * dma related functions
 *******************************/

void init_cmd_3(struct pxa3xx_nand *nand, struct cmd_3_desc *cmd)
{
	SIE_BCMCFGW *cfgw = (SIE_BCMCFGW *)cmd;

	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
}
void init_cmd_4(struct pxa3xx_nand *nand, struct cmd_4_desc *cmd)
{
	SIE_BCMCFGW *cfgw = (SIE_BCMCFGW *)cmd;

	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	cfgw++;
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCB0);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
}
void init_wcmd(struct pxa3xx_nand *nand, SIE_BCMWCMD *wcmd)
{
	wcmd->u_ddrAdr = 0;
	wcmd->u_size = 8;
	wcmd->u_chkSemId = 0;
	wcmd->u_updSemId = 0;
	wcmd->u_hdr = BCMINSFMT_hdr_WCMD;
}
void init_wdat(struct pxa3xx_nand *nand, SIE_BCMWDAT *wdat)
{
	wdat->u_size = 8;
	wdat->u_mode = 2;
	wdat->u_endian = 0;
	wdat->u_last = 1;
	wdat->u_cUpdId = 0;
	wdat->u_devAdr = pb_get_phys_offset(nand, NDDB);
	wdat->u_hdr = BCMINSFMT_hdr_WDAT;
}
void init_rcmd(struct pxa3xx_nand *nand, SIE_BCMRCMD *rcmd)
{
	rcmd->u_ddrAdr = 0;
	rcmd->u_size = 0;
	rcmd->u_chkSemId = 0;
	rcmd->u_updSemId = 0;
	rcmd->u_rsvd = 0;
	rcmd->u_hdr = BCMINSFMT_hdr_RCMD;
}
void init_rdat(struct pxa3xx_nand *nand, SIE_BCMRDAT *rdat)
{
	rdat->u_size = 0;
	rdat->u_mode = 2;
	rdat->u_endian = 0;
	rdat->u_last = 1;
	rdat->u_cUpdId = 0;
	rdat->u_pUpdId = 0;
	rdat->u_rsvd0 = 0;
	rdat->u_devAdr = pb_get_phys_offset(nand, NDDB);
	rdat->u_hdr = BCMINSFMT_hdr_RDAT;
}
void init_sema_nfccmd(struct pxa3xx_nand *nand,
		SIE_BCMSEMA *sema)
{
	sema->u_pUpdId = 0;
	sema->u_pChkId = 0;
	sema->u_cUpdId = 0;
	sema->u_cChkId = PBSemaMap_dHubSemID_dHub_NFCCmd;
	sema->u_hdr = BCMINSFMT_hdr_SEMA;
}
void init_sema_nfcdat(struct pxa3xx_nand *nand,
		SIE_BCMSEMA *sema)
{
	sema->u_pUpdId = 0;
	sema->u_pChkId = 0;
	sema->u_cUpdId = 0;
	sema->u_cChkId = PBSemaMap_dHubSemID_dHub_NFCDat;
	sema->u_hdr = BCMINSFMT_hdr_SEMA;
}

void wait_dhub_ready(struct pxa3xx_nand *nand)
{
	uint32_t read;
	int timeout = 300000;
	/* check if NFC_DEV_CTL_CHANNEL_ID bit FULL */
	do {
		read = pb_readl(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub +
				RA_dHubReg_SemaHub+ RA_SemaHub_full);
		timeout--;
		if(!timeout) {
			pxa3xx_nand_dump_registers(nand->info[nand->chip_select]);
			printk("wait_dhub_ready time out %x\n", read);

			dump_stack();
#if (PXA3XX_NAND_DMA_DEBUG == 1)
			debug_info_print(nand);
#endif
			timeout = 0xffffffff;
		}
	} while (!(read & (1 << NFC_DEV_CTL_CHANNEL_ID)));

	/* pop NFC_DEV_CTL_CHANNEL_ID semaphore */
	pb_writel(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub +
			RA_dHubReg_SemaHub + RA_SemaHub_POP,
			0x100 | NFC_DEV_CTL_CHANNEL_ID);
	/* clear NFC_DEV_CTL_CHANNEL_ID FULL bit */
	pb_writel(nand, RA_pBridge_dHub + RA_dHubReg2D_dHub +
			RA_dHubReg_SemaHub + RA_SemaHub_full,
			1 << NFC_DEV_CTL_CHANNEL_ID);
}

static void mv88dexx_nand_dma_reset(struct pxa3xx_nand_info *info)
{
	SIE_BCMCFGW *cfgw;
	unsigned int i = 0;
	struct pxa3xx_nand *nand = info->nand_data;

	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);

	/* CFGW reset  CMD 0 */
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = (nand->chip_select) ? NDCB0_CSEL : 0 |
		NDCB0_CMD_TYPE(NDCB0_CMD_RESET) |
		(info->cmdset->reset & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);
	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,	/*  Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,	/*  CMD: buffer address */
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0 	/* Pass in current cmdQ pointer (in 64b word) */
			);
	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_read_status(struct pxa3xx_nand_info *info)
{
	SIE_BCMCFGW *cfgw;
	SIE_BCMWDAT *wdat;
	SIE_BCMWCMD *wcmd;
	unsigned int i = 0;
	struct pxa3xx_nand *nand = info->nand_data;

	/* CFGW READ Status CMD 0 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = (nand->chip_select) ? NDCB0_CSEL : 0 |
		NDCB0_CMD_TYPE(NDCB0_CMD_STATUSREAD) |
		(info->cmdset->read_status & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* DMA writes status value to ddr */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(nand->data_desc + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(nand->data_desc + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,	/*  Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,	/*  CMD: buffer address */
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0 	/* Pass in current cmdQ pointer (in 64b word) */
			);

	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_erase(struct pxa3xx_nand_info *info)
{
	SIE_BCMCFGW *cfgw;
	SIE_BCMWCMD *wcmd;
	SIE_BCMWDAT *wdat;
	unsigned int i = 0;
	uint16_t cmd;
	int cs;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, addr_cycle;
	unsigned char *desc_buf = (unsigned char *)nand->data_desc;

	cs		= nand->chip_select;
	ndcb0_cs	= cs ? NDCB0_CSEL : 0;
	cmd		= (uint16_t)info->cmdset->erase;
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles);

	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);

	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_ERASE) |
		NDCB0_NC |
		NDCB0_DBC |
		addr_cycle |
		cmd;
	cfgw++;
	cfgw->u_dat = nand->ndcb1;
	i += sizeof(struct cmd_3_desc);

	/************* require dHub_NFCCmd sema ****************/
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);

	/************* disable BCH ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl & (~NDECCCTRL_BCH_EN);
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	/************* read status ****************/
	/* cmd */
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_STATUSREAD) |
		(info->cmdset->read_status & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* DMA writes status value to ddr */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(desc_buf + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(desc_buf + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	/************* restore NDECCCTRL ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,	/*  Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,	/*  CMD: buffer address */
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0 	/* Pass in current cmdQ pointer (in 64b word) */
			);
	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_readid(struct pxa3xx_nand_info *info)
{
	SIE_BCMCFGW *cfgw;
	SIE_BCMWDAT *wdat;
	SIE_BCMWCMD *wcmd;
	unsigned int i = 0;
	struct pxa3xx_nand *nand = info->nand_data;

	/* CFGW READ ID CMD 0 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = (nand->chip_select) ? NDCB0_CSEL : 0 |
		NDCB0_CMD_TYPE(NDCB0_CMD_READID) |
		NDCB0_ADDR_CYC(1) |
		(info->cmdset->read_id & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* Read Status ID from dhub local to DDR */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(nand->data_desc + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(nand->data_desc + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle ,  /* Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,	/* CMD: buffer address */
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0 	/* Pass in current cmdQ pointer (in 64b word) */
			);
	wait_dhub_ready(nand);
}

void mv88dexx_nand_dma_start(struct pxa3xx_nand_info *info, int rw)
{
	unsigned int i = 0;
	SIE_BCMCFGW *cfgw;
	struct pxa3xx_nand *nand = info->nand_data;
	uint32_t ndcr, ndeccctrl = 0;

	ndcr = info->reg_ndcr;
	ndcr |= NDCR_ND_RUN;

	switch (nand->ecc_strength) {
	default:
		ndeccctrl |= NDECCCTRL_BCH_EN;
		ndeccctrl |= NDECCCTRL_ECC_THRESH(BCH_THRESHOLD);
	case HAMMING_STRENGTH:
		ndcr |= NDCR_ECC_EN;
	case 0:
		break;
	}
	ndeccctrl |= NDECCCTRL_ECC_STRENGTH(nand->ecc_strength);

	if (rw == 0) {
		ndcr &= ~(NDCR_SPARE_EN | NDCR_ECC_EN);
		ndeccctrl &= ~NDECCCTRL_BCH_EN;
	}

	nand->ndeccctrl = ndeccctrl;
	nand->ndcr = ndcr;

	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = 0;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = NDSR_MASK;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDSR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = ndcr;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDCR);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,	/* Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or*/
			0	/* Pass in current cmdQ pointer (in 64b word) */
			);

	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_init_read_desc(struct pxa3xx_nand_info *info)
{
	unsigned int i = 0, j = 0, k = 0, addr_cycle;
	SIE_BCMCFGW *cfgw;
	SIE_BCMWDAT *wdat;
	SIE_BCMWCMD *wcmd;
	SIE_BCMRDAT *rdat;
	SIE_BCMRCMD *rcmd;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, ndcb0_phys_addr;
	int chunks;
	struct mtd_info *mtd;
	int cs = nand->chip_select;
	u32 *p_sr;

	struct read_desc *desc =
		(struct read_desc *)(nand->read_desc + cs * DMA_H_SIZE);

	mtd		= get_mtd_by_info(info);
	ndcb0_cs	= (cs ? NDCB0_CSEL : 0);
	ndcb0_phys_addr	= pb_get_phys_offset(nand, NDCB0);
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles
			+ info->col_addr_cycles);

	nand->ecc_strength = info->ecc_strength;
	pxa3xx_set_datasize(info);
	chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
		1 : (info->page_size / PAGE_CHUNK_SIZE);

	memset(desc, 0, DMA_H_SIZE);
	p_sr = (u32 *)(nand->data_desc + DMA_H_SIZE - NDSR_MASK_OFFSET);
	p_sr[0] = 0xff0000;

	/* cmd1 */
	init_sema_nfccmd(nand, &desc->sema_cmd1);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->cmd1);
	desc->cmd1.cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		NDCB0_CMD2_MASK |
		(info->cmdset->read1 & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* address */
	init_sema_nfccmd(nand, &desc->sema_addr);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->addr); /* need instantiation */
	desc->addr.cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle;
	i += sizeof(struct cmd_3_desc);

	/* cmd2 */
	init_sema_nfccmd(nand, &desc->sema_cmd2);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->cmd2);
	desc->cmd2.cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		NDCB0_CMD2_MASK |
		((info->cmdset->read1 >> 8) & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	for (j = 0; j < chunks; j++) {
		int t_units;
		int len_ovrd;

		if (j == chunks - 1) {
			len_ovrd = nand->data_size + nand->oob_size;
			nand->transfer_units_last_trunk[cs] =
				len_ovrd / nand->mtu_size;
		} else {
			len_ovrd = nand->data_size;
			nand->transfer_units[cs] = len_ovrd / nand->mtu_size;
		}
		t_units = len_ovrd / nand->mtu_size;

		/* SEMA CHECK NFC COMMAND */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->read_desc + i));
		i += sizeof(SIE_BCMSEMA);

		/* naked read cmd */
		init_cmd_4(nand, (struct cmd_4_desc *)(nand->read_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->read_desc + i);
		cfgw->u_dat = NDCB0_LEN_OVRD |
			NDCB0_CMD_XTYPE(NDCB0_CMDX_NREAD) |
			ndcb0_cs |
			NDCB0_CMD_TYPE(NDCB0_CMD_READ) |
			NDCB0_NC |
			addr_cycle;
		cfgw += 3;
		cfgw->u_dat = NDCB3_NDLENCNT(len_ovrd);
		i += sizeof(struct cmd_4_desc);

		for (k = 0; k < t_units; k++) {
			init_sema_nfcdat(nand, (SIE_BCMSEMA *)(nand->read_desc + i));
			i += sizeof(SIE_BCMSEMA);
			wcmd = (SIE_BCMWCMD *)(nand->read_desc + i);
			init_wcmd(nand, wcmd); /* need instantiation */
			if (unlikely((j == chunks - 1) && (k == t_units - 1))) {
				wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
			} else {
				wcmd->u_updSemId = 0;
			}
			wcmd->u_size = nand->mtu_size;
			i += sizeof(SIE_BCMWCMD);
			wdat = (SIE_BCMWDAT *)(nand->read_desc + i);
			init_wdat(nand, wdat);
			wdat->u_size = nand->mtu_size;
			i += sizeof(SIE_BCMWDAT);
		}

		/*Read NDSR to DDR*/
		wcmd = (SIE_BCMWCMD *)(nand->read_desc + i);
		init_wcmd(nand, wcmd); /* need instantiation */
		wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE - NDSR_STR_OFFSET + 8*j;

		i += sizeof(SIE_BCMWCMD);
		wdat = (SIE_BCMWDAT *)(nand->read_desc + i);
		init_wdat(nand, wdat);
		wdat->u_devAdr = pb_get_phys_offset(nand, NDSR);
		i += sizeof(SIE_BCMWDAT);

		/*Clear NDSR ERR_CNT*/
		rcmd = (SIE_BCMRCMD *)(nand->read_desc + i);
		init_rcmd(nand, rcmd); /* need instantiation */
		rcmd->u_size = 8;
		rcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
			+ sizeof(struct nand_buffers) * nand->chip_select - NDSR_MASK_OFFSET;
		i += sizeof(SIE_BCMRCMD);

		rdat = (SIE_BCMRDAT *)(nand->read_desc + i);
		init_rdat(nand, rdat);
		rdat->u_size = 8;
		rdat->u_devAdr = pb_get_phys_offset(nand, NDSR);
		i += sizeof(SIE_BCMRDAT);

	}
	nand->read_desc_len[cs] = i;
	printk("DMA read descriptor length = 0x%x\n", i);
}

static void mv88dexx_nand_dma_fill_read_desc(struct pxa3xx_nand_info *info,
		int page_addr, dma_addr_t phys_data_addr,
		dma_addr_t phys_oob_addr)
{
	unsigned int i = 0, j = 0, k = 0, addr_cycle;
	SIE_BCMCFGW *cfgw;
	SIE_BCMWCMD *wcmd;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, ndcb0_phys_addr;
	int chunks, cs;
	struct mtd_info *mtd;
	struct read_desc *desc;

	cs		= nand->chip_select;
	mtd		= get_mtd_by_info(info);
	ndcb0_cs	= (cs ? NDCB0_CSEL : 0);
	ndcb0_phys_addr	= pb_get_phys_offset(nand, NDCB0);
	chunks		= nand->total_cmds - 1;
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles
			+ info->col_addr_cycles);

	desc = (struct read_desc *)(nand->read_desc + cs * DMA_H_SIZE);
	chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
		1 : (info->page_size / PAGE_CHUNK_SIZE);

	/* cmd1 */
	i += sizeof(SIE_BCMSEMA);
	i += sizeof(struct cmd_3_desc);

	/* address */
	i += sizeof(SIE_BCMSEMA);
	cfgw = (SIE_BCMCFGW *)&(desc->addr);
	cfgw++;
	cfgw->u_dat = (page_addr & 0xffff) << 16;
	cfgw++;
	cfgw->u_dat = (page_addr & 0xff0000) >> 16;
	i += sizeof(struct cmd_3_desc);

	/* cmd2 */
	i += sizeof(SIE_BCMSEMA);
	i += sizeof(struct cmd_3_desc);

	for (j = 0; j < chunks; j++) {
		int t_units;

		if (j == chunks - 1) {
			t_units = nand->transfer_units_last_trunk[cs];
		} else {
			t_units = nand->transfer_units[cs];
		}

		/* naked read cmd */
		i += sizeof(SIE_BCMSEMA);
		i += sizeof(struct cmd_4_desc);

		for (k = 0; k < t_units; k++) {
			i += sizeof(SIE_BCMSEMA);
			wcmd = (SIE_BCMWCMD *)(nand->read_desc + i);
			if (unlikely((j == chunks - 1) && (k == t_units - 1))) {
				wcmd->u_ddrAdr = phys_oob_addr;
			} else {
				wcmd->u_ddrAdr = phys_data_addr;
				phys_data_addr += nand->mtu_size;
			}
			i += sizeof(SIE_BCMWCMD);

			i += sizeof(SIE_BCMWDAT);
		}
		i += sizeof(SIE_BCMWCMD) + sizeof(SIE_BCMWDAT)
			+ sizeof(SIE_BCMRCMD) + sizeof(SIE_BCMRDAT);
	}
}

static void mv88dexx_nand_dma_read_go(struct pxa3xx_nand *nand)
{
	int cs = nand->chip_select;

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,		/* Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,		/* Channel ID in $dHubReg */
			nand->read_desc_addr + cs * DMA_H_SIZE,
				/* CMD: buffer address */
			nand->read_desc_len[cs],
				/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0	/* Pass in current cmdQ pointer (in 64b word), */
			);
	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_init_write_desc(struct pxa3xx_nand_info *info)
{
	unsigned int i = 0, j = 0, addr_cycle;
	SIE_BCMCFGW *cfgw;
	SIE_BCMRDAT *rdat;
	SIE_BCMRCMD *rcmd;
	SIE_BCMSEMA *sema;
	SIE_BCMWDAT *wdat;
	SIE_BCMWCMD *wcmd;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, ndcb0_phys_addr;
	int chunks, cs;
	struct mtd_info *mtd;
	unsigned char *desc_buf;
	struct write_1_desc *desc;
	struct write_2_desc *desc2;
	int cmd;

	cs		= nand->chip_select;
	desc_buf	= nand->write_desc + cs * DMA_H_SIZE;
	desc		= (struct write_1_desc *)desc_buf;
	mtd		= get_mtd_by_info(info);
	ndcb0_cs	= (cs ? NDCB0_CSEL : 0);
	ndcb0_phys_addr	= pb_get_phys_offset(nand, NDCB0);
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles
			+ info->col_addr_cycles);
	cmd		= info->cmdset->program;

	nand->ecc_strength = info->ecc_strength;
	pxa3xx_set_datasize(info);
	chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
		1 : (info->page_size / PAGE_CHUNK_SIZE);

	memset(desc_buf, 0, DMA_H_SIZE);

	/* cmd1 */
	init_sema_nfccmd(nand, &desc->sema_cmd1);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->cmd1);
	cfgw = (SIE_BCMCFGW *)&(desc->cmd1);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		NDCB0_CMD2_MASK |
		(cmd & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);


	/* address */
	init_sema_nfccmd(nand, &desc->sema_addr);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->addr); /* need instantiation */
	cfgw = (SIE_BCMCFGW *)&(desc->addr);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle;
	i += sizeof(struct cmd_3_desc);

	for (j = 0; j < chunks; j++) {
		int len_ovrd;

		if (j == chunks - 1) {
			len_ovrd = nand->data_size + nand->oob_size;
		} else {
			len_ovrd = nand->data_size;
		}

		/* naked write cmd */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_4(nand, (struct cmd_4_desc *)(desc_buf + i));
		cfgw = (SIE_BCMCFGW *)(desc_buf + i);
		cfgw->u_dat = NDCB0_LEN_OVRD |
			NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE) |
			ndcb0_cs |
			NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
			NDCB0_NC |
			addr_cycle;
		cfgw += 3;
		cfgw->u_dat = NDCB3_NDLENCNT(len_ovrd);
		i += sizeof(struct cmd_4_desc);

		/* SEMA CHECK NFC DATA */
		init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);

		/* write NDDB
		 * 1. RCMD WRITE from DDR to Dhub local */
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		init_rcmd(nand, rcmd); /* need instantiation */
		rcmd->u_size = nand->data_size;
		i += sizeof(SIE_BCMRCMD);

		/* write NDDB
		 * 2. forward data from dhub to NFC NDDB */
		rdat = (SIE_BCMRDAT *)(desc_buf + i);
		init_rdat(nand, rdat);
		rdat->u_size = nand->data_size;
		i += sizeof(SIE_BCMRDAT);

		if (j == chunks - 1) {
			rcmd = (SIE_BCMRCMD *)(desc_buf + i);
			init_rcmd(nand, rcmd); /* need instantiation*/
			rcmd->u_size = nand->oob_size;
			i += sizeof(SIE_BCMRCMD);

			rdat = (SIE_BCMRDAT *)(desc_buf + i);
			init_rdat(nand, rdat);
			rdat->u_size = nand->oob_size;
			i += sizeof(SIE_BCMRDAT);
		}
	}

	desc2 = (struct write_2_desc *)(desc_buf + i);

	/* cmd2 */
	init_sema_nfccmd(nand, &desc2->sema_cmd2);
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc2->cmd2);
	cfgw = (SIE_BCMCFGW *)&(desc2->cmd2);
	cfgw->u_dat = NDCB0_CMD_XTYPE(NDCB0_CMDX_FINAL_CMD) |
		ndcb0_cs |
		NDCB0_NC |
		NDCB0_DBC |
		NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
		addr_cycle |
		(cmd & NDCB0_CMD2_MASK);
	i += sizeof(struct cmd_3_desc);

	/************* require dHub_NFCCmd sema ****************/
	sema = (SIE_BCMSEMA *)(desc_buf + i);
	init_sema_nfccmd(nand, sema);
	i += sizeof(SIE_BCMSEMA);

	/************* disable BCH ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl & (~NDECCCTRL_BCH_EN);
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	/************* read status ****************/
	/* cmd */
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_STATUSREAD) |
		(info->cmdset->read_status & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* DMA writes status value to ddr */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(desc_buf + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(desc_buf + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	/************* restore NDECCCTRL ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	nand->write_desc_len[cs] = i;

	printk("DMA write descriptor length = 0x%x\n", i);
}

static void mv88dexx_nand_dma_fill_write_desc(struct pxa3xx_nand_info *info)
{
	unsigned int i = 0, j = 0, addr_cycle;
	SIE_BCMCFGW *cfgw;
	SIE_BCMRCMD *rcmd;
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, ndcb0_phys_addr;
	int chunks, cs;
	struct mtd_info *mtd;
	unsigned char *desc_buf;
	struct write_1_desc *desc;

	cs		= nand->chip_select;
	desc_buf	= nand->write_desc + cs * DMA_H_SIZE;
	desc		= (struct write_1_desc *)desc_buf;
	mtd		= get_mtd_by_info(info);
	ndcb0_cs	= (cs ? NDCB0_CSEL : 0);
	ndcb0_phys_addr	= pb_get_phys_offset(nand, NDCB0);
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles
			+ info->col_addr_cycles);
	chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
		1 : (info->page_size / PAGE_CHUNK_SIZE);

	/* sema for cmd1 */
	i += sizeof(SIE_BCMSEMA);

	/* cmd1 */
	i += sizeof(struct cmd_3_desc);

	/* sema for addr */
	i += sizeof(SIE_BCMSEMA);

	/* addr */
	cfgw = (SIE_BCMCFGW *)&(desc->addr);
	cfgw++;
	cfgw->u_dat = nand->ndcb1;
	cfgw++;
	cfgw->u_dat = nand->ndcb2;
	i += sizeof(struct cmd_3_desc);

	for (j = 0; j < chunks; j++) {
		int len_ovrd;

		if (j == chunks - 1) {
			len_ovrd = nand->data_size + nand->oob_size;
		} else {
			len_ovrd = nand->data_size;
		}

		/* sema for nfc cmd */
		i += sizeof(SIE_BCMSEMA);

		/* naked write cmd */
		i += sizeof(struct cmd_4_desc);

		/* SEMA CHECK NFC DATA */
		i += sizeof(SIE_BCMSEMA);

		/* write NDDB
		 * 1. RCMD WRITE from DDR to Dhub local */
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		rcmd->u_ddrAdr = nand->data_buff_phys + j * nand->data_size;
		i += sizeof(SIE_BCMRCMD);

		/* write NDDB
		 * 2. forward data from dhub to NFC NDDB */
		i += sizeof(SIE_BCMRDAT);
	}
	/*
	 * command for writing oob area
	 */
	rcmd = (SIE_BCMRCMD *)(desc_buf + i);
	rcmd->u_ddrAdr = nand->oob_buff_phys;

	/*
	 * command for committing write
	 * is already initialized
	 */
}

static void mv88dexx_nand_dma_write_go(struct pxa3xx_nand *nand)
{
	int cs, len;

	cs = nand->chip_select;
	len = nand->write_desc_len[cs];

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,		/* Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,		/* Channel ID in $dHubReg */
			nand->write_desc_addr,		/* CMD: buffer address */
			len,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub */
			0	/* Pass in current cmdQ pointer (in 64b word) */
			);

	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_dp_erase(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ndcb0_cs, addr_cycle;
	struct dp_erase_desc *desc;
	SIE_BCMCFGW *cfgw;
	SIE_BCMWCMD *wcmd;
	SIE_BCMWDAT *wdat;
	unsigned int i = 0;
	int page_addr, cs, cmd;
	int pages_per_blk;
	unsigned char *desc_buf = nand->data_desc;

	cs		= nand->chip_select;
	ndcb0_cs	= cs ? NDCB0_CSEL : 0;
	cmd		= info->cmdset->dual_plane_erase;
	addr_cycle	= NDCB0_ADDR_CYC(info->row_addr_cycles);

	/* nand->page_addr is always erase block addr aligned */
	page_addr	= nand->page_addr * info->n_planes;

	pages_per_blk	= 1 << (info->erase_shift - info->page_shift);
	BUG_ON(page_addr & (pages_per_blk - 1));

	desc = (struct dp_erase_desc *)desc_buf;

	/* plane 1 cmd */
	init_sema_nfccmd(nand, &(desc->sema_cmd1));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs
		| NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
		| NDCB0_NC
		| addr_cycle
		| (cmd & 0xff);
	i += sizeof(struct cmd_3_desc);

	/* plane 1 addr */
	init_sema_nfccmd(nand, &(desc->sema_addr1));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle;
	cfgw++;
	cfgw->u_dat = page_addr;
	i += sizeof(struct cmd_3_desc);

	/* plane 2 cmd */
	init_sema_nfccmd(nand, &(desc->sema_cmd2));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		((cmd & 0xff00) >> 8);
	i += sizeof(struct cmd_3_desc);

	/* plane 2 addr */
	init_sema_nfccmd(nand, &(desc->sema_addr2));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->plane_2_addr);
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle;
	cfgw++;
	cfgw->u_dat = (page_addr + pages_per_blk) & 0xFFFFFF;
	i += sizeof(struct cmd_3_desc);

	/* confirm dp erase */
	init_sema_nfccmd(nand, &(desc->sema_conf_cmd));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, &desc->confirm_cmd);
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs
		| NDCB0_CMD_TYPE(NDCB0_CMD_ERASE)
		| addr_cycle
		| NDCB0_NC
		| ((cmd & 0xff0000) >> 16);
	i += sizeof(struct cmd_3_desc);

	/************* require dHub_NFCCmd sema ****************/
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);

	/************* disable BCH ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl & (~NDECCCTRL_BCH_EN);
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	/************* read status ****************/
	/* cmd */
	init_cmd_3(nand, (struct cmd_3_desc *)(desc_buf + i));
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_STATUSREAD) |
		(info->cmdset->read_status & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* DMA writes status value to ddr */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(desc_buf + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(desc_buf + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	/************* restore NDECCCTRL ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,	/*  Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,	/* Channel ID in $dHubReg */
			nand->data_desc_addr,	/*  CMD: buffer address */
			i,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub, or */
			0 	/* Pass in current cmdQ pointer (in 64b word) */
			);
	wait_dhub_ready(nand);
}

static void mv88dexx_nand_dma_init_dp_write_desc(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	unsigned int ready, cmd_done, page_done, badblock_detect;
	int addr_cycle, i = 0, c;
	uint32_t cmd;
	uint32_t cs = nand->chip_select, ndcb0_cs;
	int chunks;
	SIE_BCMRCMD *rcmd;
	SIE_BCMRDAT *rdat;
	SIE_BCMWCMD *wcmd;
	SIE_BCMWDAT *wdat;
	SIE_BCMCFGW *cfgw;
	struct cmd_3_desc *cmd_3;
	struct cmd_4_desc *cmd_4;
	unsigned char *desc_buf =
		(unsigned char *)(nand->dp_write_desc + cs * DMA_H_SIZE);
	struct mtd_info *mtd;

	ready           = (cs) ? NDSR_RDY : NDSR_FLASH_RDY;
	cmd_done        = (cs) ? NDSR_CS1_CMDD : NDSR_CS0_CMDD;
	page_done       = (cs) ? NDSR_CS1_PAGED : NDSR_CS0_PAGED;
	badblock_detect = (cs) ? NDSR_CS1_BBD : NDSR_CS0_BBD;
	cmd		= info->cmdset->dual_plane_write;
	ndcb0_cs	= (cs ? NDCB0_CSEL : 0);
	mtd		= get_mtd_by_info(info);

	addr_cycle = NDCB0_ADDR_CYC(info->row_addr_cycles
			+ info->col_addr_cycles);

	nand->ecc_strength = info->ecc_strength;
	pxa3xx_set_datasize(info);
	chunks = (info->page_size < PAGE_CHUNK_SIZE) ?
		1 : (info->page_size / PAGE_CHUNK_SIZE);

	memset(desc_buf, 0, DMA_H_SIZE);

	/****************** plane 1 ****************/
	/* cmd 1 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		NDCB0_CMD2_MASK |
		(cmd & 0xff);
	i += sizeof(struct cmd_3_desc);

	/* addr */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle |
		0xFFFF;
	i += sizeof(struct cmd_3_desc);

	/* data */
	for (c = 0; c < chunks; c++) {
		int len_ovrd;

		if (c == chunks - 1) {
			len_ovrd = nand->data_size + nand->oob_size;
		} else {
			len_ovrd = nand->data_size;
		}

		/* naked write */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);
		cmd_4 = (struct cmd_4_desc *)(desc_buf + i);
		init_cmd_4(nand, cmd_4);
		cmd_4->cfgw_ndcb0[0].u_dat = NDCB0_LEN_OVRD |
			NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE) |
			ndcb0_cs |
			NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
			NDCB0_NC |
			addr_cycle;
		cmd_4->cfgw_ndcb0[3].u_dat = len_ovrd;
		i += sizeof(struct cmd_4_desc);

		/* data copy from DDR to NFC */
		/* data area*/
		init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		init_rcmd(nand, rcmd);
		rcmd->u_size = nand->data_size;
		i += sizeof(SIE_BCMRCMD);
		rdat = (SIE_BCMRDAT *)(desc_buf + i);
		init_rdat(nand, rdat);
		rdat->u_size = nand->data_size;
		i += sizeof(SIE_BCMRDAT);
	}
	/* oob area with last chunk */
	rcmd = (SIE_BCMRCMD *)(desc_buf + i);
	init_rcmd(nand, rcmd);
	rcmd->u_ddrAdr = 0; /* need instantiation*/
	rcmd->u_size = nand->oob_size;
	i += sizeof(SIE_BCMRCMD);
	rdat = (SIE_BCMRDAT *)(desc_buf + i);
	init_rdat(nand, rdat);
	rdat->u_size = nand->oob_size;
	i += sizeof(SIE_BCMRDAT);

	/* cmd 2: commit write */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_XTYPE(NDCB0_CMDX_FINAL_CMD) |
		NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
		NDCB0_NC |
		NDCB0_DBC |
		addr_cycle |
		NDCB0_CMD1_MASK |
		(cmd & 0xff00);
	i += sizeof(struct cmd_3_desc);

	/****************** plane 2 ****************/
	/* cmd 1 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD) |
		NDCB0_NC |
		addr_cycle |
		NDCB0_CMD2_MASK |
		((cmd & 0xff0000) >> 16);
	i += sizeof(struct cmd_3_desc);

	/* addr */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR) |
		NDCB0_NC |
		addr_cycle |
		0xFFFF;
	i += sizeof(struct cmd_3_desc);

	/* data */
	for (c = 0; c < chunks; c++) {
		int len_ovrd;

		if (c == chunks - 1) {
			len_ovrd = nand->data_size + nand->oob_size;
		} else {
			len_ovrd = nand->data_size;
		}

		/* naked write */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);
		cmd_4 = (struct cmd_4_desc *)(desc_buf + i);
		init_cmd_4(nand, cmd_4);
		cmd_4->cfgw_ndcb0[0].u_dat = NDCB0_LEN_OVRD |
			NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE) |
			ndcb0_cs |
			NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
			NDCB0_NC |
			addr_cycle;
		cmd_4->cfgw_ndcb0[3].u_dat = len_ovrd;
		i += sizeof(struct cmd_4_desc);

		/* data copy from DDR to NFC */
		/* data area*/
		init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
		i += sizeof(SIE_BCMSEMA);
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		init_rcmd(nand, rcmd);
		rcmd->u_size = nand->data_size;
		i += sizeof(SIE_BCMRCMD);
		rdat = (SIE_BCMRDAT *)(desc_buf + i);
		init_rdat(nand, rdat);
		rdat->u_size = nand->data_size;
		i += sizeof(SIE_BCMRDAT);
	}
	/* oob area with last chunk */
	rcmd = (SIE_BCMRCMD *)(desc_buf + i);
	init_rcmd(nand, rcmd);
	rcmd->u_ddrAdr = 0; /* need instantiation*/
	rcmd->u_size = nand->oob_size;
	i += sizeof(SIE_BCMRCMD);
	rdat = (SIE_BCMRDAT *)(desc_buf + i);
	init_rdat(nand, rdat);
	rdat->u_size = nand->oob_size;
	i += sizeof(SIE_BCMRDAT);

	/* cmd 2: commit write */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cmd_3->cfgw_ndcb0[0].u_dat = ndcb0_cs |
		NDCB0_CMD_XTYPE(NDCB0_CMDX_FINAL_CMD) |
		NDCB0_NC |
		NDCB0_DBC |
		NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM) |
		addr_cycle |
		NDCB0_CMD1_MASK | ((cmd & 0xff000000) >> 16);
	i += sizeof(struct cmd_3_desc);

	/************* require dHub_NFCCmd sema ****************/
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);

	/************* disable BCH ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl & (~NDECCCTRL_BCH_EN);
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	/************* read status ****************/
	/* cmd */
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	init_cmd_3(nand, cmd_3);
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = ndcb0_cs |
		NDCB0_CMD_TYPE(NDCB0_CMD_STATUSREAD) |
		(info->cmdset->read_status & NDCB0_CMD1_MASK);
	i += sizeof(struct cmd_3_desc);

	/* DMA writes status value to ddr */
	init_sema_nfcdat(nand, (SIE_BCMSEMA *)(desc_buf + i));
	i += sizeof(SIE_BCMSEMA);
	wcmd = (SIE_BCMWCMD *)(desc_buf + i);
	init_wcmd(nand, wcmd);
	wcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	wcmd->u_updSemId = PBSemaMap_dHubSemID_NFC_DATA_CP;
	i += sizeof(SIE_BCMWCMD);
	wdat = (SIE_BCMWDAT *)(desc_buf + i);
	init_wdat(nand, wdat);
	i += sizeof(SIE_BCMWDAT);

	/************* restore NDECCCTRL ****************/
	cfgw = (SIE_BCMCFGW *)(desc_buf + i);
	cfgw->u_dat = nand->ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
	cfgw->u_hdr = BCMINSFMT_hdr_CFGW;
	i += sizeof(SIE_BCMCFGW);

	nand->dp_write_desc_len[cs] = i;

	printk("DMA dp write descriptor length = 0x%x\n", i);
}

static void mv88dexx_nand_dma_fill_dp_write_desc(struct pxa3xx_nand_info *info,
		int page_addr, dma_addr_t phys_data_addr)
{
	int i = 0, c;
	struct pxa3xx_nand *nand = info->nand_data;
	uint32_t cs = nand->chip_select;
	int chunks = nand->total_cmds - 1;
	SIE_BCMRCMD *rcmd;
	struct cmd_3_desc *cmd_3;
	unsigned char *desc_buf =
		(unsigned char *)(nand->dp_write_desc + cs * DMA_H_SIZE);
	int pages_per_blk = 1 << (info->erase_shift - info->page_shift);

	/****************** plane 1 ****************/
	/* cmd 1 */
	i += sizeof(SIE_BCMSEMA);
	i += sizeof(struct cmd_3_desc);

	/* addr */
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	cmd_3->cfgw_ndcb0[1].u_dat = (page_addr & 0xFFFF) << 16;
	cmd_3->cfgw_ndcb0[2].u_dat = (page_addr & 0xFF0000) >> 16;
	i += sizeof(struct cmd_3_desc);

	/* data */
	for (c = 0; c < chunks; c++) {
		/* naked write */
		i += sizeof(SIE_BCMSEMA);
		i += sizeof(struct cmd_4_desc);

		/* data copy from DDR to NFC */
		/* data area*/
		i += sizeof(SIE_BCMSEMA);
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		rcmd->u_ddrAdr = phys_data_addr;
		i += sizeof(SIE_BCMRCMD);
		i += sizeof(SIE_BCMRDAT);

		phys_data_addr += nand->data_size;
	}
	/* oob area with last chunk */
	rcmd = (SIE_BCMRCMD *)(desc_buf + i);
	rcmd->u_ddrAdr = nand->oob_buff_phys;
	i += sizeof(SIE_BCMRCMD);
	i += sizeof(SIE_BCMRDAT);

	/* cmd 2: commit write */
	i += sizeof(SIE_BCMSEMA);
	i += sizeof(struct cmd_3_desc);

	/****************** plane 2 ****************/
	/* cmd 1 */
	i += sizeof(SIE_BCMSEMA);
	i += sizeof(struct cmd_3_desc);

	/* addr */
	i += sizeof(SIE_BCMSEMA);
	cmd_3 = (struct cmd_3_desc *)(desc_buf + i);
	cmd_3->cfgw_ndcb0[1].u_dat = ((page_addr + pages_per_blk) & 0xFFFF) << 16;
	cmd_3->cfgw_ndcb0[2].u_dat = ((page_addr + pages_per_blk) & 0xFF0000) >> 16;
	i += sizeof(struct cmd_3_desc);

	/* data */
	for (c = 0; c < chunks; c++) {
		/* naked write */
		i += sizeof(SIE_BCMSEMA);
		i += sizeof(struct cmd_4_desc);

		/* data copy from DDR to NFC */
		/* data area*/
		i += sizeof(SIE_BCMSEMA);
		rcmd = (SIE_BCMRCMD *)(desc_buf + i);
		rcmd->u_ddrAdr = phys_data_addr;
		i += sizeof(SIE_BCMRCMD);
		i += sizeof(SIE_BCMRDAT);

		phys_data_addr += nand->data_size;
	}
	/* oob area with last chunk */
	rcmd = (SIE_BCMRCMD *)(desc_buf + i);
	rcmd->u_ddrAdr = nand->oob_buff_phys;

	/* cmd 2: commit write */

	/************* require dHub_NFCCmd sema ****************/

	/************* disable BCH ****************/

	/************* read status ****************/
	/* cmd */

	/* DMA writes status value to ddr */

	/************* restore NDECCCTRL ****************/
}

static void mv88dexx_nand_dma_dp_write_go(struct pxa3xx_nand *nand)
{
	int cs, len;

	cs = nand->chip_select;
	len = nand->dp_write_desc_len[cs];

	wmb();
	dhub_channel_write_cmd(
			&nand->PB_dhubHandle,		/* Handle to HDL_dhub */
			NFC_DEV_CTL_CHANNEL_ID,		/* Channel ID in $dHubReg */
			nand->dp_write_desc_addr + cs * DMA_H_SIZE,	/* CMD: buffer address */
			len,	/* CMD: number of bytes to transfer */
			0,	/* CMD: semaphore operation at CMD/MTU (0/1) */
			0,	/* CMD: non-zero to check semaphore */
			0,	/* CMD: non-zero to update semaphore */
			1,	/* CMD: raise interrupt at CMD finish */
			0,	/* Pass NULL to directly update dHub */
			0	/* Pass in current cmdQ pointer (in 64b word) */
			);
	wait_dhub_ready(nand);
}
/*******************************
 * dma related functions ends
 *******************************/

static inline int dp_page_addr_to_sp(struct pxa3xx_nand_info *info, int page)
{
	uint32_t mask = (1 << (info->erase_shift - info->page_shift)) - 1;
	return ((page & (~mask)) << 1) + (page & mask);
}

static void pxa3xx_nand_dma_cmdfunc(struct pxa3xx_nand_info *info,
		unsigned command)
{
	int ret;
	unsigned long i, t;
	struct pxa3xx_nand *nand = info->nand_data;

	if (((command == NAND_CMD_PAGEPROG) || (command == NAND_CMD_RNDOUT)))
		mv88dexx_nand_dma_start(info, 1);
	if ((command == NAND_CMD_RESET)
			|| (command == NAND_CMD_READID)
			|| (command == NAND_CMD_STATUS)
			|| (command == NAND_CMD_ERASE1))
		mv88dexx_nand_dma_start(info, 0);

	if(command == NAND_CMD_RESET) {
		mv88dexx_nand_dma_reset(info);
		/*
		 * TODO
		 * need print as proper delay for reset OP
		 * 111019
		 *  need to polling both ready and cmdd for reset OK
		 *  waiting for more stress test to get rid of this TODO
		 */
		wait_and_clear_status_bit(nand, NDSR_FLASH_RDY, 0);
		wait_and_clear_status_bit(nand, NDSR_CS0_CMDD, 0);

		nand->is_ready = 1;
		goto finish;
	}

	init_completion(&nand->cmd_complete);

	switch (command) {
	case NAND_CMD_ERASE1:
		if (info->n_planes == 2)
			mv88dexx_nand_dma_dp_erase(info);
		else
			mv88dexx_nand_dma_erase(info);
		break;
	case NAND_CMD_RNDOUT:
		if (info->n_planes == 2) {
			/* TODO
			 * use 2 DMA sp reads to simulate 1 DMA dp read
			 * haven't figured out why DMA dp read can't work
			 *
			 * 110907
			 *   DMA dp read can receive interrupt, but data read-out
			 *   are in a mess.
			 */
			int page_addr;
			dma_addr_t oob_buff_phys;
			int pages_per_blk = 1 << (info->erase_shift - info->page_shift);

			page_addr = dp_page_addr_to_sp(info, nand->page_addr);
			oob_buff_phys	= nand->data_desc_addr + DMA_H_SIZE
				+ nand->chip_select * sizeof(struct nand_buffers)
				+ (NAND_MAX_OOBSIZE * 2) + info->page_size * 2;

			mv88dexx_nand_dma_fill_read_desc(info, page_addr,
					nand->data_buff_phys,
					oob_buff_phys);
			mv88dexx_nand_dma_read_go(nand);
			ret = wait_for_completion_timeout(&nand->cmd_complete,
					CHIP_DELAY_TIMEOUT);
			if (!ret) {
				printk(KERN_ERR "Wait time out for DMA dp read!!!\n");
				/* Stop State Machine for next command cycle */
				pxa3xx_nand_stop(nand);
				goto finish;
			}

			mv88dexx_nand_dma_start(info, 1);
			init_completion(&nand->cmd_complete);
			mv88dexx_nand_dma_fill_read_desc(info,
					page_addr + pages_per_blk,
					nand->data_buff_phys + info->page_size,
					oob_buff_phys + 32);
			mv88dexx_nand_dma_read_go(nand);
		} else {
			dma_addr_t oob_buff_phys;

			oob_buff_phys	= nand->data_desc_addr + DMA_H_SIZE
				+ nand->chip_select * sizeof(struct nand_buffers)
				+ (NAND_MAX_OOBSIZE * 2) + info->page_size;

			mv88dexx_nand_dma_fill_read_desc(info, nand->page_addr,
					nand->data_buff_phys,
					oob_buff_phys);
			mv88dexx_nand_dma_read_go(nand);
		}
		break;
	case NAND_CMD_PAGEPROG:
		if (info->n_planes == 2) {
			int page_addr = dp_page_addr_to_sp(info, nand->page_addr);

			mv88dexx_nand_dma_fill_dp_write_desc(info,
					page_addr, nand->data_buff_phys);
			mv88dexx_nand_dma_dp_write_go(nand);
		} else {
			mv88dexx_nand_dma_fill_write_desc(info);
			mv88dexx_nand_dma_write_go(nand);
		}
		break;
	case NAND_CMD_READID:
		mv88dexx_nand_dma_readid(info);
		break;
	case NAND_CMD_STATUS:
		mv88dexx_nand_dma_read_status(info);
		break;
	default:
		printk("%s: unexpected command 0x%x\n", __func__, command);
		goto finish;
	}

	t = CHIP_DELAY_TIMEOUT;
	for (i = 0; i < 10; i++) {
		ret = wait_for_completion_timeout(&nand->cmd_complete,
				t);
		if (!ret) {
			printk(KERN_ERR "%s pid %d cmd 0x%02x "
					"Wait %lums time out!!! "
					"trying %lums\n",
					__func__, current->pid, nand->command,
					t*1000/HZ, t*2000/HZ);
		} else {
			if (i)
				printk(KERN_INFO "%s pid %d "
						"survived, retry %lu times\n",
						__func__, current->pid, i);
			break;
		}
	}
	if (!ret) {
		printk(KERN_ERR "BLOCK NAND DRVIER, page [%x]\n", nand->page_addr);
		pxa3xx_nand_dump_registers(info);
		wait_for_completion(&nand->cmd_complete);
	}

finish:
	return;
}

static void pxa3xx_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
				int column, int page_addr)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	int exec_cmd;

#if (PXA3XX_NAND_DMA_DEBUG == 1)
	debug_push_info(mtd, command, page_addr);
#endif

	/*
	 * if this is a x16 device ,then convert the input
	 * "byte" address into a "word" address appropriate
	 * for indexing a word-oriented device
	 */
	if (info->reg_ndcr & NDCR_DWIDTH_M)
		column /= 2;

	/*
	 * There may be different NAND chip hooked to
	 * different chip select, so check whether
	 * chip select has been changed, if yes, reset the timing
	 */
	if (nand->chip_select != info->chip_select) {
		nand->chip_select = info->chip_select;
		nand_writel(nand, NDTR0CS0, info->ndtr0cs0);
		nand_writel(nand, NDTR1CS0, info->ndtr1cs0);
	}

	if (info->eslc_mode && nand->p_slc) {
		struct pxa3xx_nand_slc *slc = nand->p_slc;
		if(info->eslc_mode == 2) {
			page_addr = slc->slc_start(slc->priv,
				command, page_addr);
		} else {
			page_addr = slc->eslc_start(slc->priv,
				command, page_addr);
		}
	}

	nand->state = STATE_PREPARED;
	exec_cmd = prepare_command_pool(info, command, column, page_addr);
	if (exec_cmd) {
		if (likely(use_dma)) {
			pxa3xx_nand_dma_cmdfunc(info, command);
		} else {
			pxa3xx_nand_start(info);
			pxa3xx_nand_run_state_machine(info);
		}
	}

	if (info->eslc_mode && nand->p_slc) {
		struct pxa3xx_nand_slc *slc = nand->p_slc;
		if(info->eslc_mode == 2) {
			slc->slc_stop(slc->priv, command);
		} else {
			slc->eslc_stop(slc->priv, command);
		}
	}
	nand->state = STATE_IDLE;
}

#ifdef CONFIG_BERLIN_NAND_RANDOMIZER
static int pxa3xx_nand_write_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	dma_addr_t mapped_addr = 0;
	uint8_t *oob = chip->oob_poi;
	int randomized = 0;
	int pages_per_blk = 1 << (info->erase_shift - info->page_shift);

	if (info->n_planes == 2) {
		int page_addr = dp_page_addr_to_sp(info, nand->page_addr);

		if (nand_write_page_pre_process(mtd, page_addr,
				buf, oob, nand->bounce_buffer,
				nand->bounce_buffer + mtd->writesize)) {
			nand_write_page_pre_process(mtd, page_addr + pages_per_blk,
					buf + info->page_size, NULL,
					nand->bounce_buffer + info->page_size, NULL);
			buf = nand->bounce_buffer;
			oob = nand->bounce_buffer + mtd->writesize;
			randomized = 1;
		}
	} else {
		if (nand_write_page_pre_process(mtd, nand->page_addr,
					buf, oob,
					nand->bounce_buffer,
					nand->bounce_buffer + mtd->writesize)) {
			/* chip needs sw randomization */
			buf = nand->bounce_buffer;
			oob = nand->bounce_buffer + mtd->writesize;
			randomized = 1;
		} /* else chip doesn't need sw randomization */
	}

	if (use_dma) {
		/*
		 * case 1: chip doesn't need randomization
		 * and mtd uses chip->buffers directly.
		 *   buf == chip->buffers->databuf &&
		 *   oob == chip->oob_poi
		 * chip->buffers->databuf is a coherent buffer,
		 * no-need-to and should-not do DMA mapping again.
		 */
		if (unlikely(buf == chip->buffers->databuf)) {
			nand->data_buff_phys = nand->data_desc_addr +
				DMA_H_SIZE +
				sizeof(*chip->buffers) * nand->chip_select +
				NAND_MAX_OOBSIZE * 2;
			nand->oob_buff_phys = nand->data_buff_phys +
				mtd->writesize;
			goto skip_map;
		}

		/*
		 * case 2: chip doesn't need randomization,
		 * but mtd passes a different buffer.
		 *   buf == any addr except chip->buffers->databuf
		 *   oob == chip->oob_poi
		 * If mapping fails, it may be caused by addr unalignment
		 * (some drivers don't pass aligned addr to mtd, such as fts),
		 * so try to use bounce_buffer, goto case 3.
		 *
		 * case 3: chip needs randomization
		 *   buf == nand->bounce_buffer
		 *   oob == chip->bounce_buffer + mtd->writesize
		 * or an error handling for case 3.
		 *   buf == nand->bounce_buffer
		 *   oob == chip->oob_poi
		 * Since bounce_buffer is kmalloc'ed, most likely mapping
		 * should succeed. If mapping fails, should abort write
		 * and return with error.
		 * We can't use coherent chip->buffers which are managed by mtd layer,
		 * and should not be touched by specific NAND driver.
		 */
again:
		mapped_addr = map_addr(nand, (void *)buf,
				(oob == chip->oob_poi) ?
				mtd->writesize : (mtd->writesize + mtd->oobsize),
				DMA_TO_DEVICE);

		if (dma_mapping_error(&nand->pdev->dev, mapped_addr)) {
			if (buf == nand->bounce_buffer) {
				printk(KERN_ERR "%s: DMA mapping error for bounce_buffer\n",
						__func__);
				nand->retcode = ERR_DMAMAPERR;
				nand->command = NAND_CMD_NONE;
				return -EIO;
			}
			/* failure in case 2 */
			printk(KERN_NOTICE "%s: copy buf to bounce buffer\n", __func__);
			memcpy(nand->bounce_buffer, buf, mtd->writesize);
			buf = nand->bounce_buffer;
			goto again;
		} else {
			nand->use_mapped_buf = 1;
			nand->data_buff_phys = mapped_addr;
			if (oob == chip->oob_poi)
				nand->oob_buff_phys = nand->data_desc_addr
					+ DMA_H_SIZE
					+ sizeof(*chip->buffers) * nand->chip_select
					+ NAND_MAX_OOBSIZE * 2
					+ mtd->writesize;
			else
				nand->oob_buff_phys = mapped_addr + mtd->writesize;
		}
	}

skip_map:
	nand->data_buff = (unsigned char *)buf;
	nand->oob_buff = oob;
	return 0;
}
#else
static int pxa3xx_nand_write_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	dma_addr_t mapped_addr = 0;
	uint8_t *oob = chip->oob_poi;

	if (use_dma) {
		mapped_addr = map_addr(nand, (void *)buf,
				mtd->writesize, DMA_TO_DEVICE);

		if (dma_mapping_error(&nand->pdev->dev, mapped_addr)) {
			memcpy(chip->buffers->databuf, buf, mtd->writesize);
			buf = chip->buffers->databuf;
			nand->data_buff_phys = nand->data_desc_addr
				+ DMA_H_SIZE
				+ sizeof(struct nand_buffers)
				* nand->chip_select
				+ (NAND_MAX_OOBSIZE * 2);
		} else {
			nand->data_buff_phys = mapped_addr;
			nand->use_mapped_buf = 1;
		}
		nand->oob_buff_phys = nand->data_desc_addr + DMA_H_SIZE
				+ sizeof(struct nand_buffers) * nand->chip_select
				+ (NAND_MAX_OOBSIZE * 2) + mtd->writesize;
	}

	nand->data_buff = (unsigned char *)buf;
	nand->oob_buff = oob;
	return 0;
}
#endif /* CONFIG_BERLIN_NAND_RANDOMIZER */

int pxa3xx_read_page(struct mtd_info *mtd, uint8_t *buf, int page)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct nand_chip *chip = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	dma_addr_t mapped_addr = 0;
	uint8_t *orig_buf = buf;
	int need_copy_back = 0;
	int buf_blank = 0;

#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	struct pxa3xx_nand_read_retry *retry = nand->retry;
	int retry_c = 0;

read_retry:
#endif
	if (use_dma) {
again:
		if (buf) {
			mapped_addr = map_addr(nand, (void *)buf,
					mtd->writesize, DMA_FROM_DEVICE);
			if (dma_mapping_error(&nand->pdev->dev, mapped_addr)) {
				if (buf == nand->bounce_buffer) {
					printk(KERN_ERR "%s: can't map DMA buffer\n",
							__func__);
					return -ENOMEM;
				}
				printk(KERN_DEBUG "[%s,%d] nand page %08X "
						"direct map buf %p error, "
						"try bounce buffer\n",
						__func__, __LINE__,
						nand->page_addr, buf);
				buf = nand->bounce_buffer;
				need_copy_back = 1;
				goto again;
			} else {
				nand->use_mapped_buf = 1;
				nand->data_buff_phys = mapped_addr;
			}
		} else {
			buf = nand->bounce_buffer;
			goto again;
		}
	}

	nand->data_buff = buf;
	nand->oob_buff = chip->oob_poi;
	nand->bad_count = 0;

	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_RNDOUT, 0, page);

	if (nand->use_mapped_buf)
		unmap_addr(&nand->pdev->dev, mapped_addr,
			   buf, mtd->writesize, DMA_FROM_DEVICE);

	if (nand->retcode == ERR_SBERR) {
		switch (nand->ecc_strength) {
		default:
			mtd->ecc_stats.corrected += nand->bad_count;
			break;
		case HAMMING_STRENGTH:
			mtd->ecc_stats.corrected++;
		case 0:
			break;
		}
	} else if (nand->retcode == ERR_DBERR) {
		/*
		 * for blank page (all 0xff), HW will calculate its ECC as
		 * 0, which is different from the ECC information within
		 * OOB, ignore such double bit errors
		 */
		int oob_len;
		oob_len = info->n_planes == 2 ? 64 : 32;
		if (is_read_page_blank(nand->ecc_strength, nand->data_buff,
			nand->oob_buff, mtd->writesize, oob_len, info->n_planes)) {
			nand->retcode = ERR_NONE;
			memset(nand->data_buff, 0xff, mtd->writesize);
			memset(nand->oob_buff, 0xff, oob_len);
			buf_blank = 1;
		}
		else {
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
			if ((retry->rr_table_ready == 1)
				|| (retry->rr_type == MICRON_MT29F32G08CBADA_RR)) {
				retry_c++;
				if (retry_c <= retry->rr_cycles) {
					retry->rr_current_cycle++;
					retry->rr_current_cycle %= retry->rr_cycles;
					printk(KERN_DEBUG "page %x rd error, retrying %d/%d\n",
						page, retry->rr_current_cycle, retry->rr_cycles);
					retry->set_rr(nand, retry->rr_current_cycle);
					buf = orig_buf;
					need_copy_back = buf_blank = mapped_addr = 0;
					pxa3xx_nand_cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
					goto read_retry;
				}
			}
#endif
			mtd->ecc_stats.failed++;
			printk(KERN_ERR "uncorrectable ECC error @ page 0x%x\n", page);
		}
	}
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	if (retry_c) {
		if(retry->rr_type == MICRON_MT29F32G08CBADA_RR) {
			//GetFeature to check
			int get_retry;
			get_retry = retry->verify_rr(nand);
			printk(KERN_INFO "Micron RR SetFeature[%d] GetFeature[%d]!\n",
				retry_c, get_retry);

			//Clear variable and setting
			retry->rr_current_cycle = 0;
			retry->set_rr(nand, -1);
		}

		printk(KERN_INFO "read %s with %d retry @ page %x\n",
			retry_c > retry->rr_cycles ? "failed" : "done",
			retry_c > retry->rr_cycles ? (retry_c - 1) : retry_c, page);
	}
#endif

#ifdef CONFIG_BERLIN_NAND_RANDOMIZER
	if (buf_blank)
		goto skip_derandomize;
	if (info->n_planes == 2) {
		int pages_per_blk = 1 << (info->erase_shift - info->page_shift);
		page = dp_page_addr_to_sp(info, page);
		nand_read_page_post_process(mtd, page,
				nand->data_buff, nand->oob_buff,
				nand->data_buff, nand->oob_buff);
		nand_read_page_post_process(mtd, page + pages_per_blk,
				nand->data_buff + info->page_size,
				nand->oob_buff + 32,
				nand->data_buff + info->page_size,
				nand->oob_buff + 32);
	} else {
		nand_read_page_post_process(mtd, nand->page_addr, nand->data_buff,
				nand->oob_buff, nand->data_buff, nand->oob_buff);
	}
skip_derandomize:
#endif /* CONFIG_BERLIN_NAND_RANDOMIZER */

	if (info->n_planes == 2) {
		uint64_t *p1, *p2;
		p1 = (uint64_t *)nand->oob_buff;
		p2 = (uint64_t *)(nand->oob_buff + 32);
		*p1 = *p1 & *p2; p1++; p2++;
		*p1 = *p1 & *p2; p1++; p2++;
		*p1 = *p1 & *p2; p1++; p2++;
		*p1 = *p1 & *p2; p1++; p2++;
	}
	if (need_copy_back)
		memcpy(orig_buf, nand->data_buff, mtd->writesize);

	return nand->bad_count;
}

static void free_dummy_rd_buf(struct pxa3xx_nand *nand)
{

	if (nand->drd_buf != NULL) {
		unmap_addr(&nand->pdev->dev, nand->drd_dma_addr,
			nand->drd_buf, NAND_MAX_PAGESIZE*2,
			DMA_FROM_DEVICE);
		kfree(nand->drd_buf);
		nand->drd_buf = NULL;
		nand->drd_dma_addr = (dma_addr_t)0;
	}
}

static int alloc_dummy_rd_buf(struct pxa3xx_nand *nand)
{
	if (nand->drd_buf == NULL) {
		nand->drd_buf = kmalloc(NAND_MAX_PAGESIZE*2,
					GFP_KERNEL);
		if (nand->drd_buf == NULL) {
			printk(KERN_ERR "dummy read malloc failed !!!\n");
			return -ENOMEM;
		}
		nand->drd_dma_addr = map_addr(nand,
			(void *)nand->drd_buf,
			NAND_MAX_PAGESIZE*2, DMA_FROM_DEVICE);
		if (dma_mapping_error(&nand->pdev->dev, nand->drd_dma_addr)) {
			printk(KERN_ERR "%s: can't map DMA buffer\n",
					__func__);
			kfree(nand->drd_buf);
			nand->drd_dma_addr = (dma_addr_t)0;
			nand->drd_buf = NULL;
			return -ENOMEM;
		}
		printk(KERN_INFO "dummy read map done!!! \n");
	}
	return 0;
}

int pxa3xx_dummy_read(struct mtd_info *mtd, int page)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	int ret;

	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);

	if(nand->drd_buf == NULL){
		if (alloc_dummy_rd_buf(nand) != 0) {
			return -ENOMEM;
		}
	}

#if (PXA3XX_NAND_DMA_DEBUG == 1)
	debug_push_info(mtd, NAND_CMD_READ0 | 0x200, page);
#endif
	udelay(10);
	mv88dexx_nand_dma_start(info, 1);

	init_completion(&nand->cmd_complete);

	mv88dexx_nand_dma_fill_read_desc(info, page,
			nand->drd_dma_addr,
			nand->drd_dma_addr + NAND_MAX_PAGESIZE);
	mv88dexx_nand_dma_read_go(nand);
	ret = wait_for_completion_timeout(&nand->cmd_complete,
			CHIP_DELAY_TIMEOUT);
	if (!ret) {
		printk(KERN_ERR "pxa3xx_dummy_read  time out!!!\n");
	}

	return ret;
}

static int pxa3xx_nand_read_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	return pxa3xx_read_page(mtd, buf, page);
}

static int pxa3xx_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
	pxa3xx_read_page(mtd, NULL, page);
	return 0;
}

int pxa3xx_nand_intern_read(struct pxa3xx_nand *nand, uint8_t *buf, int page)
{
	int ret;
	struct mtd_info *mtd = get_mtd_by_info(nand->info[0]);

	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
	ret = pxa3xx_read_page(mtd, buf, page);

	return ret;
}

static uint8_t pxa3xx_nand_read_byte(struct mtd_info *mtd)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	char retval = 0xFF;

	if (nand->buf_start < nand->buf_count)
		/* Has just send a new command? */
		retval = nand->data_buff[nand->buf_start++];

	return retval;
}

static u16 pxa3xx_nand_read_word(struct mtd_info *mtd)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	u16 retval = 0xFFFF;

	if (!(nand->buf_start & 0x01) && nand->buf_start < nand->buf_count) {
		retval = *((u16 *)(nand->data_buff+nand->buf_start));
		nand->buf_start += 2;
	}
	return retval;
}

static void pxa3xx_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	int real_len = min_t(size_t, len, nand->buf_count - nand->buf_start);

	memcpy(buf, nand->data_buff + nand->buf_start, real_len);
	nand->buf_start += real_len;
}

static void pxa3xx_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	int real_len = min_t(size_t, len, nand->buf_count - nand->buf_start);

	memcpy(nand->data_buff + nand->buf_start, buf, real_len);
	nand->buf_start += real_len;
}

static int pxa3xx_nand_scan_bbt(struct mtd_info *mtd)
{
	struct pxa3xx_nand_info *info = mtd->priv;

	if ((info->n_planes == MTD_DUAL_PLANE) || info->eslc_mode)
		return 0;

	return nand_default_bbt(mtd);
}

static int pxa3xx_nand_block_markbad(struct mtd_info *mtd_dp, loff_t ofs)
{
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct pxa3xx_nand_info *dp_info = mtd_dp->priv;
	struct pxa3xx_nand *nand = dp_info->nand_data;
	int ret = 0;

	mtd = get_mtd_by_info(nand->info[dp_info->chip_select]);
	chip = mtd->priv;
	if (chip->block_markbad) {
		ret = chip->block_markbad(mtd, ofs);
		if (ret)
			return ret;
		return chip->block_markbad(mtd, ofs + (mtd_dp->erasesize/2));
	}
	return ret;
}

static int pxa3xx_nand_slc_markbad(struct mtd_info *mtd_src, loff_t ofs)
{
	struct nand_chip *chip;
	//struct pxa3xx_nand_info *src_info = mtd_src->priv;
	struct mtd_info *mtd_mlc;
	struct pxa3xx_nand_info *slc_info = mtd_src->priv;
	struct pxa3xx_nand *nand = slc_info->nand_data;
	int ret = 0;

	mtd_mlc = get_mtd_by_info(nand->info[slc_info->chip_select]);
	chip = mtd_mlc->priv;
	if (slc_info->eslc_mode == 1) {
		loff_t dst;
		dst = nand->p_slc->eslc_get_phy_off(
				nand->p_slc->priv, ofs);

		printk("eslc markbad %llx -> %llx", ofs, dst);
		ofs = dst;
	} else if (slc_info->eslc_mode == 2) {
		loff_t dst;
		dst = nand->p_slc->slc_get_phy_off(
				nand->p_slc->priv, ofs);
		printk("slc markbad %llx -> %llx", ofs, dst);
		ofs = dst;
	} else {
		return -EINVAL;
	}

	if (chip->block_markbad) {
		ret = chip->block_markbad(mtd_mlc, ofs);
	}
	return ret;
}

static int pxa3xx_nand_get_mtd_info(struct mtd_info *mtd, loff_t off, loff_t *dst)
{
	struct pxa3xx_nand_info *info = mtd->priv;

	*dst = off;
	if (info->eslc_mode == 1) {
		struct pxa3xx_nand *nand = info->nand_data;
		if (nand->p_slc) {
			*dst = nand->p_slc->eslc_get_phy_off(
				nand->p_slc->priv, off);
		}
		return MTD_ESLC;
	} else if (info->eslc_mode == 2) {
		struct pxa3xx_nand *nand = info->nand_data;
		if (nand->p_slc) {
			*dst = nand->p_slc->slc_get_phy_off(
				nand->p_slc->priv, off);
		}
		return MTD_SLC;
	} else {
		return info->n_planes;
	}
}

static void pxa3xx_nand_select_chip(struct mtd_info *mtd, int chip)
{
#ifdef CONFIG_BERLIN_NAND_RANDOMIZER
	nand_randomizer_init_by_chip(mtd, chip);
#endif /* CONFIG_BERLIN_NAND_RANDOMIZER */
	return;
}

static void pxa3xx_nand_wait(struct mtd_info *mtd, struct nand_chip *this)
{
	unsigned long timeo = jiffies;
	int state = this->state;
	uint8_t status;
	int delta, i;
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;

	if (state == FL_ERASING)
		delta = (HZ * 400) / 1000;
	else
		delta = (HZ * 20) / 1000;
	timeo += delta;

	for (i = 0; i < 4; i++) {
		while (time_before(jiffies, timeo)) {
			pxa3xx_nand_cmdfunc(mtd, NAND_CMD_STATUS, 0, -1);
			status = pxa3xx_nand_read_byte(mtd);
			if (status & NAND_STATUS_READY)
				goto finish_in_time;
			cond_resched();
		}

		/* recheck status */
		if (status & NAND_STATUS_READY)
			goto finish_in_time;

		/* enlarge waiting time */
		printk(KERN_INFO "%s pid %d cmd 0x%02x Wait %dms time out!!! "
				"trying %dms\n", __func__,
				current->pid, nand->command, delta*1000/HZ,
				delta*2000/HZ);
		timeo = jiffies + delta;
	}
	printk(KERN_ERR "%s pid %d cmd 0x%02x Wait time out %s!!!\n",
			__func__, current->pid, nand->command,
			(status & NAND_STOP_DELAY) ? "READY" : "BUSY");

finish_in_time:
	if (i)
		printk(KERN_INFO "%s pid %d survived, retry %d times\n",
				__func__, current->pid, i);
	return;
}

static int pxa3xx_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;

	if (nand->retcode == ERR_DMAMAPERR)
		return 1;

	if ((nand->command == NAND_CMD_PAGEPROG) && nand->use_mapped_buf)
		unmap_addr(&nand->pdev->dev, nand->data_buff_phys,
			nand->data_buff, mtd->writesize, DMA_TO_DEVICE);

	/*
	 * for MV88DEXXXX, DMA engine can't do status check,
	 * need CPU polling involved
	 */
	if (use_dma)
		pxa3xx_nand_wait(mtd, this);

	/* pxa3xx_nand_send_command has waited for command complete */
	if (this->state == FL_WRITING || this->state == FL_ERASING) {
		if (nand->retcode == ERR_NONE)
			return 0;
		else {
			/*
			 * any error make it return 0x01 which will tell
			 * the caller the erase and write fail
			 */
			return 0x01;
		}
	}

	return 0;
}

void pb_init(struct pxa3xx_nand *nand)
{
	HDL_semaphore *sema_handle;

	nand->mtu_size = 32;
	nand->mtu_type = MV_MTU_32_BYTE;

	pb_writel(nand, RA_pBridge_BCM + RA_BCM_base,
		((unsigned long)nand->mmio_base & 0xF0000000));

	/* Initialize HDL_dhub with a $dHub BIU instance. */
	dhub_hdl((unsigned long)nand->pb_base + RA_pBridge_tcm,	/*	Base address of DTCM */
		(unsigned long)nand->pb_base + RA_pBridge_dHub,	/*	Base address of a BIU instance of $dHub */
		&nand->PB_dhubHandle                    /*	Handle to HDL_dhub */
		);

	/* Initialize FiFos */
	dhub_channel_cfg(&nand->PB_dhubHandle,		/* Handle to HDL_dhub */
			READ_DATA_CHANNEL_ID,		/* Channel ID in $dHubReg */
			READ_DATA_CHAN_CMD_BASE,	/* Channel FIFO base address (byte address) for cmdQ */
			READ_DATA_CHAN_DATA_BASE,	/* Channel FIFO base address (byte address) for dataQ */
			READ_DATA_CHAN_CMD_SIZE/8,	/* Channel FIFO depth for cmdQ, in 64b word */
			READ_DATA_CHAN_DATA_SIZE/8,	/* Channel FIFO depth for dataQ, in 64b word */
			nand->mtu_type,		/* See 'dHubChannel.CFG.MTU', 0/1/2 for 8/32/128 bytes */
			0,			/* See 'dHubChannel.CFG.QoS' */
			0,			/* See 'dHubChannel.CFG.selfLoop' */
			1,			/* 0 to disable, 1 to enable */
			0			/* Pass NULL to directly init dHub */
			);

	/* Write Data Channel */
	dhub_channel_cfg(&nand->PB_dhubHandle,
			WRITE_DATA_CHANNEL_ID,
			WRITE_DATA_CHAN_CMD_BASE,
			WRITE_DATA_CHAN_DATA_BASE,
			WRITE_DATA_CHAN_CMD_SIZE/8,
			WRITE_DATA_CHAN_DATA_SIZE/8,
			nand->mtu_type,
			0,
			0,
			1,
			0
			);

	/* Descriptor Channel */
	dhub_channel_cfg(&nand->PB_dhubHandle,
			DESCRIPTOR_CHANNEL_ID,
			DESCRIPTOR_CHAN_CMD_BASE,
			DESCRIPTOR_CHAN_DATA_BASE,
			DESCRIPTOR_CHAN_CMD_SIZE/8,
			DESCRIPTOR_CHAN_DATA_SIZE/8,
			nand->mtu_type,
			0,
			0,
			1,
			0
			);

	/* NFC Device Control (function) Channel */
	dhub_channel_cfg(&nand->PB_dhubHandle,
			NFC_DEV_CTL_CHANNEL_ID,
			NFC_DEV_CTL_CHAN_CMD_BASE,
			NFC_DEV_CTL_CHAN_DATA_BASE,
			NFC_DEV_CTL_CHAN_CMD_SIZE/8,
			NFC_DEV_CTL_CHAN_DATA_SIZE/8,
			nand->mtu_type,
			0,
			0,
			1,
			0
			);

	sema_handle = dhub_semaphore(&nand->PB_dhubHandle);
	/*configure the semaphore depth to be 1 */
	semaphore_cfg(sema_handle, READ_DATA_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, WRITE_DATA_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, DESCRIPTOR_CHANNEL_ID, 1, 0);
	semaphore_cfg(sema_handle, NFC_DEV_CTL_CHANNEL_ID, 1, 0);

	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_NFCCmd, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_NFCDat, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_NFC_DATA_CP, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_dHub_APBRx2, 1, 0);
	semaphore_cfg(sema_handle, PBSemaMap_dHubSemID_APB0_DATA_CP, 1, 0);

	semaphore_intr_enable (
			sema_handle,
			PBSemaMap_dHubSemID_NFC_DATA_CP,
			0,
			1,
			0,
			0,
			0);
}

static int pxa3xx_nand_config_flash(struct pxa3xx_nand_info *info,
				    const struct pxa3xx_nand_flash *f)
{
	struct pxa3xx_nand *nand = info->nand_data;
	struct platform_device *pdev = nand->pdev;
	struct pxa3xx_nand_platform_data *pdata = pdev->dev.platform_data;
	uint32_t ndcr = 0x0; /* enable all interrupts */

	if (f->page_size != 4096 && f->page_size != 2048 &&
			f->page_size != 512 && f->page_size != 8192)
		return -EINVAL;

	if (f->flash_width != 16 && f->flash_width != 8)
		return -EINVAL;

	if (f->page_size > PAGE_CHUNK_SIZE
			&& !(pdata->controller_attrs & PXA3XX_NAKED_CMD_EN)) {
		printk(KERN_ERR "Your controller don't support 4k or larger "
			       "page NAND for don't support naked command\n");
		return -EINVAL;
	}
	if (f->ecc_strength != 0 && f->ecc_strength != HAMMING_STRENGTH
	    && (f->ecc_strength % BCH_STRENGTH != 0)) {
		printk(KERN_ERR "ECC strength definition error, please recheck!!\n");
		return -EINVAL;
	}
	info->ecc_strength = f->ecc_strength;

	printk("ecc strength  %d\n", f->ecc_strength);

	/* calculate flash information */
	info->cmdset = &default_cmdset;
	info->page_size = f->page_size;
	info->read_id_bytes = (f->page_size >= 2048) ? NAND_ID_SIZE : 2;
	info->erase_size = f->page_per_block * f->page_size;

	info->page_shift = ffs(info->page_size) - 1;
	info->erase_shift = ffs(info->erase_size) - 1;

	/* calculate plane information*/
	if (use_dma) {
		info->has_dual_plane = f->has_dual_plane;
	} else {
		/*
		 * we don't add dual plane operations
		 * for PIO mode, so we need to disable it!
		 */
		printk(KERN_NOTICE "Force to disable dual plane "
				"under PIO mode\n");
		info->has_dual_plane = 0;
	}
	info->n_planes = 1; /* use single plane by default */
	info->eslc_mode = 0;

	/* calculate addressing information */
	info->col_addr_cycles = (f->page_size >= 2048) ? 2 : 1;

	if (f->num_blocks * f->page_per_block > 65536)
		info->row_addr_cycles = 3;
	else
		info->row_addr_cycles = 2;

	ndcr |= (pdata->controller_attrs & PXA3XX_ARBI_EN)
		? NDCR_ND_ARB_EN : 0;
	ndcr |= (info->col_addr_cycles == 2) ? NDCR_RA_START : 0;
	ndcr |= (f->flash_width == 16) ? NDCR_DWIDTH_M : 0;
	ndcr |= (f->dfc_width == 16) ? NDCR_DWIDTH_C : 0;

	switch (f->page_per_block) {
	case 32:
		ndcr |= NDCR_PG_PER_BLK(0x0);
		break;
	case 128:
		ndcr |= NDCR_PG_PER_BLK(0x1);
		break;
	case 256:
		ndcr |= NDCR_PG_PER_BLK(0x3);
		break;
	case 64:
	default:
		ndcr |= NDCR_PG_PER_BLK(0x2);
		break;
	}

	if (f->page_size >= 2048)
		ndcr |= NDCR_PAGE_SZ;

	ndcr |= NDCR_RD_ID_CNT(info->read_id_bytes);
	/* only enable spare area when ecc is lower than 8bits per 512 bytes */
	if (f->ecc_strength <= BCH_STRENGTH)
		ndcr |= NDCR_SPARE_EN;

	info->reg_ndcr = ndcr;

	pxa3xx_nand_set_timing(info, f->timing);
	return 0;
}

static void free_cs_resource(struct pxa3xx_nand_info *info, int cs)
{
	struct pxa3xx_nand *nand;
	struct mtd_info *mtd;

	if (!info)
		return;

	nand = info->nand_data;
	mtd = get_mtd_by_info(info);
	kfree(mtd);
	nand->info[cs] = NULL;
}

static int pxa3xx_nand_sensing(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand *nand = info->nand_data;
	struct mtd_info *mtd = get_mtd_by_info(info);
	struct nand_chip *chip = mtd->priv;

	/* use the common timing to make a try */
	if (pxa3xx_nand_config_flash(info, &builtin_flash_types[0]))
		return 0;
	chip->cmdfunc(mtd, NAND_CMD_RESET, 0, 0);
	if (nand->is_ready)
		return 1;
	else
		return 0;
}

static struct nand_ecclayout nand_oob_128 = {
	.eccbytes = 48,
	.eccpos = {
		   80, 81, 82, 83, 84, 85, 86, 87,
		   88, 89, 90, 91, 92, 93, 94, 95,
		   96, 97, 98, 99, 100, 101, 102, 103,
		   104, 105, 106, 107, 108, 109, 110, 111,
		   112, 113, 114, 115, 116, 117, 118, 119,
		   120, 121, 122, 123, 124, 125, 126, 127},
	.oobfree = {
		{.offset = 2,
		 .length = 78}}
};

#define NAND_ID_SZ	8
static uint8_t pxa3xx_nand_ids[NAND_ID_SZ];
static int pxa3xx_nand_scan(struct mtd_info *mtd, int type_plane)
{
	struct pxa3xx_nand_info *info = mtd->priv;
	struct pxa3xx_nand *nand = info->nand_data;
	struct platform_device *pdev = nand->pdev;
	struct pxa3xx_nand_platform_data *pdata = pdev->dev.platform_data;
	struct nand_flash_dev pxa3xx_flash_ids[2];
	const struct pxa3xx_nand_flash *f = NULL;
	struct nand_chip *chip = mtd->priv;
	uint16_t* id;
	uint64_t chipsize;
	int i, ret, num;
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	struct pxa3xx_nand_read_retry *retry = nand->retry;
#endif

	nand->chip_select = info->chip_select;

	ret = pxa3xx_nand_sensing(info);
	if (!ret) {
		free_cs_resource(info, nand->chip_select);
		printk(KERN_INFO "There is no nand chip on cs %d!\n",
				nand->chip_select);

		return -EINVAL;
	}

	pxa3xx_nand_dump_registers(info);

	chip->cmdfunc(mtd, NAND_CMD_READID, 0, 0);
	id = (uint16_t *)(nand->data_buff);
	if (id[0] != 0)
		printk(KERN_INFO "Detect a flash id %x, ext id %x part id %x \n", id[0], id[1], id[2]);
	else {
		printk(KERN_INFO "NAND_CMD_READID failed, id %x, ext id %x part id %x\n", id[0], id[1], id[2]);
		pxa3xx_nand_dump_registers(info);
		free_cs_resource(info, nand->chip_select);
		return -EINVAL;
	}

	num = ARRAY_SIZE(builtin_flash_types) + pdata->num_flash - 1;
	for (i = 0; i < num; i++) {
		if (i < pdata->num_flash)
			f = pdata->flash + i;
		else
			f = &builtin_flash_types[i - pdata->num_flash + 1];

		/* find the chip in default list */
		if ((f->chip_id == id[0]) && ((f->ext_id & id[1]) == id[1]))
			break;
	}

	if (i >= (ARRAY_SIZE(builtin_flash_types) + pdata->num_flash - 1)) {
		pxa3xx_nand_dump_registers(info);
		free_cs_resource(info, nand->chip_select);
		printk(KERN_ERR "ERROR!! flash not defined!!!\n");

		return -EINVAL;
	}

	if (pxa3xx_nand_config_flash(info, f)) {
		pxa3xx_nand_dump_registers(info);
		printk(KERN_ERR "ERROR! Configure failed\n");
		return -EINVAL;
	}
	pxa3xx_flash_ids[0].name = f->name;
	pxa3xx_flash_ids[0].id = (f->chip_id >> 8) & 0xffff;
	pxa3xx_flash_ids[0].pagesize = f->page_size;
	chipsize = (uint64_t)f->num_blocks * f->page_per_block * f->page_size;
	pxa3xx_flash_ids[0].erasesize =
		f->page_size * f->page_per_block * info->n_planes;

	if (type_plane == TYPE_DUAL_PLANE) {
		info->n_planes = 2;
		pxa3xx_flash_ids[0].pagesize *= 2;
		pxa3xx_flash_ids[0].erasesize *= 2;
	} else if (type_plane == TYPE_E_SLC) {
		mtd->erasesize /= 2;
		pxa3xx_flash_ids[0].erasesize /= 2;
		info->eslc_mode = 1;
		info->erase_shift -= 1;
		info->erase_size /= 2;
	} else if (type_plane == TYPE_SLC) {
		mtd->erasesize /= 2;
		pxa3xx_flash_ids[0].erasesize /= 2;
		info->eslc_mode = 2;
		info->erase_shift -= 1;
		info->erase_size /= 2;
	}

	pxa3xx_flash_ids[0].chipsize = chipsize >> 20;
	if (f->flash_width == 16)
		pxa3xx_flash_ids[0].options = NAND_BUSWIDTH_16;
	else
		pxa3xx_flash_ids[0].options = 0;
	pxa3xx_flash_ids[1].name = NULL;

	/*********** init mtd and nand_chip ***********/
	if (nand_scan_ident(mtd, 1, pxa3xx_flash_ids)) {
		pxa3xx_nand_dump_registers(info);
		return -ENODEV;
	}

	/* calculate addressing information */
	nand->oob_buff = nand->data_buff + mtd->writesize;
	info->col_addr_cycles = (mtd->writesize >= 2048) ? 2 : 1;
	if ((mtd->size >> chip->page_shift) > 65536)
		info->row_addr_cycles = 3;
	else
		info->row_addr_cycles = 2;
	mtd->name = mtd_names[nand->chip_select];
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = info->page_size;
	chip->ecc.strength = info->ecc_strength;
	mtd->bitflip_threshold = info->ecc_strength / 2;

	chip->options = (info->reg_ndcr & NDCR_DWIDTH_M) ? NAND_BUSWIDTH_16 : 0;
	chip->options |= NAND_OWN_BUFFERS;
        /*set this flag to skip nand_check_wp(), not needed for internal flash*/
	chip->options |= NAND_BROKEN_XD;
	chip->bbt_options = NAND_BBT_USE_FLASH;
	//chip->options |= NAND_USE_FLASH_BBT | NAND_USE_FLASH_BBT_WITH_BBM;

	if ((f->chip_id == 0xd7ec && f->ext_id == 0x7a94) ||
			(f->chip_id == 0xd7ec && f->ext_id == 0x7e94) ||
			(f->chip_id == 0xdeec && f->ext_id == 0x7ad5) ||
			(f->chip_id == 0xd5ec && f->ext_id == 0x7284) ||
			(f->chip_id == 0xd5ec && f->ext_id == 0xb614) ||
			(f->chip_id == 0xd598 && f->ext_id == 0x3284) ||
			(f->chip_id == 0xd7ad && f->ext_id == 0x9a94) ||
			(f->chip_id == 0x482c && f->ext_id == 0x4a04) ||
			(f->chip_id == 0x682c && f->ext_id == 0x4a04) ||
			(f->chip_id == 0x682c && f->ext_id == 0x4604) ||
			(f->chip_id == 0x882c && f->ext_id == 0x4b04) ||
			(f->chip_id == 0xdead && f->ext_id == 0xda94) ||
			(f->chip_id == 0xd5ad && f->ext_id == 0xda94))
		chip->ecc.layout = &nand_oob_128;

	printk("mtd_info: writesize=0x%X, oobsize=0x%X, erasesize=0x%X size=%llX\n",
			mtd->writesize, mtd->oobsize,
			mtd->erasesize, mtd->size);
	printk("nand_chip: options=0x%X, bbt_erase_shift =0x%X, page_shift=0x%X\n",
			chip->options, chip->bbt_erase_shift, chip->page_shift);

	if (use_dma) {
		if (type_plane == TYPE_DUAL_PLANE) {
			mv88dexx_nand_dma_init_dp_write_desc(info);
			mv88dexx_nand_dma_init_read_desc(info);
		} else if(type_plane == TYPE_SINGLE_PLANE) {
			mv88dexx_nand_dma_init_write_desc(info);
			mv88dexx_nand_dma_init_read_desc(info);
		}
	}

#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	if (type_plane == TYPE_SINGLE_PLANE) {
		switch (f->chip_id) {
		case 0xdead:
		case 0xd5ad:
			if (f->ext_id == 0xda94)
				retry->rr_type = HYNIX_H27UCG8T_RR_MODE1;
			break;
		case 0x442c:
			if (f->ext_id == 0x4b44)
				retry->rr_type = MICRON_MT29F32G08CBADA_RR;
			break;
		default:
			retry->rr_type = NO_READ_RETRY;
		}

		if (berlin_nand_rr_init(nand, pxa3xx_flash_ids) != 0) {
			printk("RR init failed!!!\n");
			retry->rr_type = NO_READ_RETRY;
		} else {
			printk("RR init done!!!\n");
		}
	}
#endif

	/*
	 * This is the second phase of the normal nand_scan() function. It
	 * fills out all the uninitialized function pointers with the defaults
	 * and scans for a bad block table if appropriate.
	 */
	return nand_scan_tail(mtd);
}

static int pxa3xx_info_fill(struct pxa3xx_nand *nand,
		struct mtd_info *mtd_dst, struct mtd_info *mtd_src,
		int cs, int type)
{
	struct pxa3xx_nand_info *info;
	int  info_size = sizeof(struct mtd_info) +
		sizeof(struct pxa3xx_nand_info);

	memcpy(mtd_dst, mtd_src, info_size);
	info = (struct pxa3xx_nand_info *)(&mtd_dst[1]);
	info->nand_data = nand;
	mtd_dst->priv = info;
	switch (type) {
		case TYPE_DUAL_PLANE:
			nand->info_dp[cs] = info;
		break;
		case TYPE_E_SLC:
			nand->info_eslc[cs] = info;
		break;
		case TYPE_SLC:
			nand->info_slc[cs] = info;
		break;
	}
	return 0;
}


static int alloc_nand_resource(struct platform_device *pdev)
{
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct nand_chip *chip;
	struct mtd_info *mtd, *mtd_dp, *mtd_eslc, *mtd_slc;
	struct pxa3xx_nand *nand;
	struct resource *r;
	int ret, irq, cs;
	struct device_node *np = pdev->dev.of_node;
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	struct pxa3xx_nand_read_retry *retry;
#endif

	pdata = pdev->dev.platform_data;
	nand = kzalloc(sizeof(struct pxa3xx_nand), GFP_KERNEL);
	if (!nand) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	nand->pdev = pdev;

#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	retry = kzalloc(sizeof(struct pxa3xx_nand_read_retry), GFP_KERNEL);
	if (!retry) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		kfree(nand);
		return -ENOMEM;
	}
	nand->retry = retry;
#endif

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		ret = -ENODEV;
		goto fail_alloc;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto fail_alloc;
	}

	r = request_mem_region(r->start, resource_size(r), pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto fail_alloc;
	}

	nand->mmio_base = (void __iomem *)r->start;
	nand->mmio_phys = r->start;

	nand->pb_base = of_iomap(np, 1);
	if (!nand->pb_base) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = request_irq(irq, mv88dexx_nand_dma_intr, IRQF_DISABLED,
			  pdev->name, nand);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		ret = -ENXIO;
		goto fail_free_io;
	}

	platform_set_drvdata(pdev, nand);

	spin_lock_init(&nand->controller.lock);
	init_waitqueue_head(&nand->controller.wq);

	if (use_dma) {
		nand->data_desc = dma_alloc_coherent(&pdev->dev, DMA_H_SIZE
				+ sizeof(*chip->buffers) * pdata->cs_num,
				&nand->data_desc_addr, GFP_KERNEL);
		if (nand->data_desc == NULL) {
			dev_err(&pdev->dev, "failed to allocate"
					" dma data buffer\n");
			ret = -ENOMEM;
			goto fail_free_irq;
		}
		nand->read_desc = dma_alloc_coherent(&pdev->dev,
				4 * DMA_H_SIZE * pdata->cs_num,
				&nand->read_desc_addr, GFP_KERNEL);
		if (nand->read_desc == NULL) {
			dev_err(&pdev->dev, "failed to allocate"
					" dma read desc buffer\n");
			ret = -ENOMEM;
			goto fail_free_data_desc;
		}
		nand->write_desc		= nand->read_desc
			+ DMA_H_SIZE * pdata->cs_num;
		nand->dp_read_desc		= nand->write_desc
			+ DMA_H_SIZE * pdata->cs_num;
		nand->dp_write_desc		= nand->dp_read_desc
			+ DMA_H_SIZE * pdata->cs_num;

		nand->write_desc_addr		= nand->read_desc_addr
			+ DMA_H_SIZE * pdata->cs_num;
		nand->dp_read_desc_addr 	= nand->write_desc_addr
			+ DMA_H_SIZE * pdata->cs_num;
		nand->dp_write_desc_addr	= nand->dp_read_desc_addr
			+ DMA_H_SIZE * pdata->cs_num;
	}

	nand->bounce_buffer = kmalloc(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE, GFP_KERNEL);
	if (nand->bounce_buffer == NULL) {
		dev_err(&pdev->dev, "failed to allocate"
				" randomized buffer\n");
		ret = -ENOMEM;
		goto fail_free_write_desc;
	}

	for (cs = 0; cs < pdata->cs_num; cs++) {
		int info_size = sizeof(struct mtd_info) +
			sizeof(struct pxa3xx_nand_info);

		/*
		 * TODO:
		 *  dual plane mtd_master should be freed
		 *  if chip doesn't support dual plane,
		 *  so it's better if it has a dedicated allocation action
		 */
		mtd = kzalloc(info_size * 4, GFP_KERNEL);
		if (!mtd) {
			dev_err(&pdev->dev, "failed to allocate memory\n");
			ret = -ENOMEM;
			goto fail_free_buf;
		}

		mtd_dp = (struct mtd_info *)((char *)mtd + info_size);
		mtd_eslc = (struct mtd_info *)((char *)mtd_dp + info_size);
		mtd_slc = (struct mtd_info *)((char *)mtd_eslc + info_size);

		info = (struct pxa3xx_nand_info *)(&mtd[1]);
		info->nand_data = nand;
		info->chip_select = cs;
		mtd->priv = info;
		mtd->owner = THIS_MODULE;
		nand->info[cs] = info;

		chip = (struct nand_chip *)(&mtd[1]);

		if (use_dma)
			chip->buffers =
				(struct nand_buffers *)((void *)nand->data_desc
				+ DMA_H_SIZE + sizeof(*chip->buffers) * cs);
		else
			chip->buffers = kmalloc(sizeof(*chip->buffers),
						GFP_KERNEL);
		chip->controller        = &nand->controller;
		chip->ecc.read_page	= pxa3xx_nand_read_page_hwecc;
		chip->ecc.read_page_raw = pxa3xx_nand_read_page_hwecc;
		chip->ecc.read_oob	= pxa3xx_nand_read_oob;
		chip->ecc.write_page	= pxa3xx_nand_write_page_hwecc;
		chip->waitfunc		= pxa3xx_nand_waitfunc;
		chip->select_chip	= pxa3xx_nand_select_chip;
		chip->cmdfunc		= pxa3xx_nand_cmdfunc;
		chip->read_word		= pxa3xx_nand_read_word;
		chip->read_byte		= pxa3xx_nand_read_byte;
		chip->read_buf		= pxa3xx_nand_read_buf;
		chip->write_buf		= pxa3xx_nand_write_buf;
		chip->get_mtd_info	= pxa3xx_nand_get_mtd_info;
		chip->scan_bbt		= pxa3xx_nand_scan_bbt;

		pxa3xx_info_fill(nand, mtd_dp, mtd, cs, TYPE_DUAL_PLANE);
		pxa3xx_info_fill(nand, mtd_eslc, mtd, cs, TYPE_E_SLC);
		pxa3xx_info_fill(nand, mtd_slc, mtd, cs, TYPE_SLC);
	}

	return 0;

fail_free_buf:
	for (cs = 0; cs < pdata->cs_num; cs++) {
		info = nand->info[cs];
		free_cs_resource(info, cs);
	}

	kfree(nand->bounce_buffer);
fail_free_write_desc:
	if (use_dma)
		dma_free_coherent(&pdev->dev,
				4 * DMA_H_SIZE * pdata->cs_num,
				nand->read_desc, nand->read_desc_addr);
fail_free_data_desc:
	if (use_dma)
		dma_free_coherent(&pdev->dev, DMA_H_SIZE
				+ sizeof(*chip->buffers) * pdata->cs_num,
				nand->data_desc, nand->data_desc_addr);
fail_free_irq:
	free_irq(irq, nand);
fail_free_io:
	release_mem_region(r->start, resource_size(r));
fail_alloc:
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	kfree(retry);
#endif
	kfree(nand);
	return ret;
}

static int pxa3xx_nand_remove(struct platform_device *pdev)
{
	struct pxa3xx_nand *nand = platform_get_drvdata(pdev);
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct resource *r;
	int irq, cs;

	platform_set_drvdata(pdev, NULL);
	pdata = pdev->dev.platform_data;

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0)
		free_irq(irq, nand);
	if (use_dma) {
		dma_free_coherent(&pdev->dev, DMA_H_SIZE
				+ sizeof(*chip->buffers) * pdata->cs_num,
				nand->data_desc, nand->data_desc_addr);
		dma_free_coherent(&pdev->dev,
				4 * DMA_H_SIZE * pdata->cs_num,
				nand->read_desc, nand->read_desc_addr);
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(r->start, resource_size(r));
	free_dummy_rd_buf(nand);

	for (cs = 0; cs < pdata->cs_num; cs++) {
		info = nand->info[cs];
		if (!info)
			continue;
		mtd = get_mtd_by_info(info);
		/*
		 * TODO: how to delete dp partitions
		 */
		mtd_device_unregister(mtd);
		free_cs_resource(info, cs);
	}

	kfree(nand->bounce_buffer);

#ifdef CONFIG_BERLIN_NAND_RANDOMIZER
	nand_randomizer_release(pdev);
#endif /* CONFIG_BERLIN_NAND_RANDOMIZER */
#ifdef CONFIG_BERLIN_NAND_READ_RETRY
	berlin_nand_rr_exit(nand);
#endif

	if (pdata->controller_attrs & PXA3XX_PDATA_MALLOC) {
		kfree(pdata);
		pdev->dev.platform_data = NULL;
	}
	return 0;
}

/*
 * Read partition plane configuration from command line
 * return the partition type by name
 * The format for the command line is as follows:
 *
 * mtdplanes=<name><planes>[,<name><planes>]
 * <name>    := '(' NAME ')'
 *		Plane configuration will be associated with the named partition
 * <planes>  := 'sp' for single plane operation, 'dp' for dual plane.
 *		Partitions will be 'sp' by default.
 *
 * Example: mtdplanes=(partA)dp,(partB)sp,(partC)sp,(partD)dp
 */
static int pxa3xx_parse_part_type(const struct mtd_partition *parts,
		char *cmd)
{
	const char *name;
	int name_len, part_name_len;
	char *p;


	if (parts && parts->name) {
		part_name_len = strlen(parts->name);
	} else {
		return TYPE_UNKNOWN;
	}
	while (cmd && (*cmd != 0)) {
		cmd = strchr(cmd, '(');
		if (!cmd) {
			return TYPE_UNKNOWN;
		}
		name = ++cmd;
		p = strchr(name, ')');
		if (!p) {
			printk("no closing ) found in partition name\n");
			return TYPE_UNKNOWN;
		}
		name_len = p - cmd;
		cmd = ++p;

		if ((part_name_len == name_len) && (strncmp(parts->name, name, name_len) == 0)) {
			if (strncmp(p, "dp", 2) == 0) {
				return TYPE_DUAL_PLANE;
			} else if (strncmp(p, "eslc", 4) == 0) {
				return TYPE_E_SLC;
			} else if(strncmp(p, "slc", 3) == 0) {
				return TYPE_SLC;
			} else if(strncmp(p, "sp", 2) == 0) {
				return TYPE_SINGLE_PLANE;
			}
			return TYPE_UNKNOWN;
		}
	}
	return TYPE_UNKNOWN;
}

static struct pxa3xx_nand_platform_data *pxa3xx_nand_probe_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct pxa3xx_nand_platform_data *pdata;

	pdata = kzalloc(sizeof(struct pxa3xx_nand_platform_data), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->controller_attrs = PXA3XX_PDATA_MALLOC;

	if (!of_get_property(np, "mrvl,nfc-dma", NULL)) {
		use_dma = 0;
	}

	if (of_get_property(np, "mrvl,nfc-naked-cmd", NULL))
		pdata->controller_attrs |= PXA3XX_NAKED_CMD_EN;

	if (of_get_property(np, "mrvl,nfc-arbi", NULL))
		pdata->controller_attrs |= PXA3XX_ARBI_EN;

	if (of_get_property(np, "mrvl,nfc-dual-chip", NULL))
		pdata->cs_num = 2;
	else
		pdata->cs_num = 1;

	return pdata;
}

static int pxa3xx_nand_probe(struct platform_device *pdev)
{
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct nand_chip *chip = NULL, *chip_dp = NULL, *chip_eslc = NULL;
	struct nand_chip *chip_slc = NULL;
	struct pxa3xx_nand *nand;
	struct mtd_info *mtd, *mtd_dp, *mtd_eslc, *mtd_slc;
	int cs, part_num, ret, nr_parts, probe_success;

	probe_success = 0;
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		pdata = pxa3xx_nand_probe_dt(pdev);
		if (pdata == NULL)
			return -ENOMEM;
		pdev->dev.platform_data = (void *)pdata;
	} else {

		if (!(pdata->controller_attrs & PXA3XX_DMA_EN))
			use_dma = 0;
	}
	ret = alloc_nand_resource(pdev);
	if (ret) {
		// todo:  free above malloc
		return -ENOMEM;
	}

	nand = platform_get_drvdata(pdev);

	if (use_dma) {
		printk(KERN_INFO "NFC DMA engine init\n");
		pb_init(nand);
	}

	for (cs = 0; cs < pdata->cs_num; cs++) {
		const char *probes[] = { "cmdlinepart", NULL };
		struct mtd_partition *parts;
		info = nand->info[cs];
		mtd = get_mtd_by_info(info);
		mtd_dp = get_mtd_by_info(nand->info_dp[cs]);
		mtd_eslc = get_mtd_by_info(nand->info_eslc[cs]);
		mtd_slc = get_mtd_by_info(nand->info_slc[cs]);
		chip = mtd->priv;
		chip_dp = mtd_dp->priv;
		chip_eslc = mtd_eslc->priv;
		chip_slc = mtd_slc->priv;

		if (pxa3xx_nand_scan(mtd, TYPE_SINGLE_PLANE)) {
			dev_err(&pdev->dev, "failed to scan nand\n");
			continue;
		}

		chip_dp->block_markbad = pxa3xx_nand_block_markbad;
		if (info->has_dual_plane &&
				pxa3xx_nand_scan(mtd_dp, TYPE_DUAL_PLANE)) {
			dev_err(&pdev->dev, "failed to scan dp nand\n");
			continue;
		}

		chip_eslc->block_markbad = pxa3xx_nand_slc_markbad;
		if (pxa3xx_nand_scan(mtd_eslc, TYPE_E_SLC)) {
			dev_err(&pdev->dev, "failed to scan eslc nand\n");
			continue;
		}

		chip_slc->block_markbad = pxa3xx_nand_slc_markbad;
		if (pxa3xx_nand_scan(mtd_slc, TYPE_SLC)) {
			dev_err(&pdev->dev, "failed to scan slc nand\n");
			continue;
		}

		chip_slc->bbt = chip_eslc->bbt = chip_dp->bbt = chip->bbt;
		ret = 0;
		nr_parts = 0;

		nr_parts = parse_mtd_partitions(mtd, probes, &parts, 0);
		if (!nr_parts) {
			printk(KERN_ERR "nr parts = 0, breaks\n");
			goto add_predefined_parts;
		}
		if (info->has_dual_plane && plane_cmdline) {
			printk(KERN_INFO "analyzing plane info\n");
			for (part_num = 0; part_num < nr_parts; part_num++) {
				int type = pxa3xx_parse_part_type(&parts[part_num],
							plane_cmdline);
				switch (type) {
				case TYPE_DUAL_PLANE:
					printk(KERN_INFO "dual plane part: %s\n",
							parts[part_num].name);
					ret = mtd_device_register(mtd_dp,
							&parts[part_num], 1);
				break;
				case TYPE_E_SLC:
					printk(KERN_INFO "e-slc part: %s, offset %llx\n",
							parts[part_num].name,
							parts[part_num].offset);
					if (nand->p_slc) {
						parts[part_num].size /= 2;
						nand->p_slc->eslc_part_add(
							nand->p_slc->priv,
							&parts[part_num]);
						ret = mtd_device_register(
							mtd_eslc,
							&parts[part_num], 1);
					}

				break;
				case TYPE_SLC:
					printk(KERN_INFO "slc part: %s, offset %llx\n",
							parts[part_num].name,
							parts[part_num].offset);
					if (nand->p_slc) {
						parts[part_num].size /= 2;
						nand->p_slc->slc_part_add(
							nand->p_slc->priv,
							&parts[part_num]);
						ret = mtd_device_register(
							mtd_slc,
							&parts[part_num], 1);
					}
				break;
				default:
					printk(KERN_INFO "sing plane part: %s\n",
							parts[part_num].name);
					ret = mtd_device_register(mtd,
							&parts[part_num], 1);
				}
			}
		} else {
			printk(KERN_INFO "create single-plane partitions directly\n");
			mtd_device_register(mtd, parts, nr_parts);
		}

add_predefined_parts:
		if (!nr_parts)
			ret = mtd_device_register(mtd, pdata->parts[cs],
					pdata->nr_parts[cs]);

		/* need a mtd device for the whole chip
		 * so that partitions can re-arranged more flexible
		 */
		if (!ret)
			ret = mtd_device_register(mtd, NULL, 0);

		if (!ret)
			probe_success = 1;
	}

	if (!probe_success) {
		pxa3xx_nand_remove(pdev);
		return -ENODEV;
	} else
		return 0;
}

#ifdef CONFIG_PM
static int pxa3xx_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pxa3xx_nand *nand = platform_get_drvdata(pdev);

	printk(KERN_DEBUG "%s\n", __func__);

	if (nand->state != STATE_IDLE) {
		dev_err(&pdev->dev, "driver busy, state = %d\n", nand->state);
		return -EAGAIN;
	}
	return 0;
}

static int pxa3xx_nand_resume(struct platform_device *pdev)
{
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct pxa3xx_nand *nand;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int cs;

	printk(KERN_DEBUG "%s\n", __func__);

	pdata = pdev->dev.platform_data;

	nand = platform_get_drvdata(pdev);

	if (use_dma) {
		printk(KERN_INFO "NFC DMA engine re-init\n");
		pb_init(nand);
	}

	for (cs = 0; cs < pdata->cs_num; cs++) {
		info = nand->info[cs];
		if (!info)
			continue;

		/* restore timing register */
		nand_writel(nand, NDTR0CS0, info->ndtr0cs0);
		nand_writel(nand, NDTR1CS0, info->ndtr1cs0);

		/* force NAND chip reset */
		mtd = get_mtd_by_info(info);
		chip = mtd->priv;
		chip->cmdfunc(mtd, NAND_CMD_RESET, 0, 0);
	}

	return 0;
}
#else
#define pxa3xx_nand_suspend	NULL
#define pxa3xx_nand_resume	NULL
#endif

static ssize_t show_nand_ids(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       	pxa3xx_nand_ids[0],
		       	pxa3xx_nand_ids[1],
		       	pxa3xx_nand_ids[2],
		       	pxa3xx_nand_ids[3],
		       	pxa3xx_nand_ids[4],
		       	pxa3xx_nand_ids[5],
		       	pxa3xx_nand_ids[6],
		       	pxa3xx_nand_ids[7]);
}

static DEVICE_ATTR(nand_ids, 0444, show_nand_ids, NULL);

static struct attribute *pxa3xx_attributes[] = {
	&dev_attr_nand_ids.attr,
	NULL
};

static struct attribute_group pxa3xx_attr_group = {
	.attrs = pxa3xx_attributes
};

static const struct attribute_group *pxa3xx_attr_groups[] = {
	&pxa3xx_attr_group,
	NULL
};

static const struct of_device_id berlin_nfc_of_match[] = {
	{.compatible = "mrvl,berlin-nfc",},
	{},
};

static struct platform_driver pxa3xx_nand_driver = {
	.driver = {
		.name	= "berlin-nfc",
		.of_match_table = berlin_nfc_of_match,
		.groups = pxa3xx_attr_groups,
	},
	.probe		= pxa3xx_nand_probe,
	.remove		= pxa3xx_nand_remove,
	.suspend	= pxa3xx_nand_suspend,
	.resume		= pxa3xx_nand_resume,
};

static int __init pxa3xx_nand_init(void)
{
	return platform_driver_register(&pxa3xx_nand_driver);
}
late_initcall(pxa3xx_nand_init);

static void __exit pxa3xx_nand_exit(void)
{
	platform_driver_unregister(&pxa3xx_nand_driver);
}
module_exit(pxa3xx_nand_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PXA3xx NAND controller driver for MV88DE3100");
MODULE_AUTHOR("Zheng Shi <zhengshi@marvell.com>");
