/*
 * READ RETRY feature Driver for Marvell BERLIN SoC NAND controller
 *
*/

#include "pxa3xx_nand_debu.h"
#include "pxa3xx_nand_read_retry_debu.h"
#include <linux/delay.h>

static int berlin_nand_pio_setrr_micron(struct pxa3xx_nand *nand, int retry_c);
static int berlin_nand_pio_verifyrr_micron(struct pxa3xx_nand *nand);

static inline int do_wait_status_bit_set(struct pxa3xx_nand *nand, unsigned int bit,
				const char *func, int line_no)
{
	unsigned int read, i = NAND_TIME_OUT;

	do {
		read = nand_readl(nand, NDSR) & bit;
	} while (!read && --i);

	if (!i) {
		printk("wait bit %08X time out! SR %x, %s:%d\n", bit,
			nand_readl(nand, NDSR),
			func, line_no);
		return -1;
	}
	nand_writel(nand, NDSR, bit);
	return 0;
}
#define wait_status_bit_set(nand, bit) \
	do_wait_status_bit_set(nand, bit,\
			__func__, __LINE__)

static void berlin_nand_wait_stop(struct pxa3xx_nand *nand)
{
	uint32_t ndcr;
	int timeout = NAND_STOP_DELAY*10;

	/* wait RUN bit in NDCR become 0 */
	ndcr = nand_readl(nand, NDCR);
	while ((ndcr & NDCR_ND_RUN) && (timeout-- > 0)) {
		ndcr = nand_readl(nand, NDCR);
		udelay(1);
	}

	if (timeout <= 0) {
		printk("berline_nand_stop timeout\n");
		dump_stack();
	}
}


static void handle_data_pio_rr(struct pxa3xx_nand *nand, void *buf,
		int data_size, int state)
{
	uint16_t real_data_size = DIV_ROUND_UP(data_size, 4);

	switch (state) {
	case STATE_PIO_WRITING:
		if (data_size > 0)
			__raw_writesl(nand->mmio_base + NDDB,
					buf,
					real_data_size);

		break;
	case STATE_PIO_READING:
		if (data_size > 0)
			__raw_readsl(nand->mmio_base + NDDB,
					buf,
					real_data_size);

		break;
	default:
		printk(KERN_ERR "%s: invalid state %d\n", __func__,
				state);
		BUG();
	}

}

static void handle_data_dummy_rd(struct pxa3xx_nand *nand, int data_size)
{
	uint16_t real_data_size = DIV_ROUND_UP(data_size, 4);

	while (real_data_size--) {
		nand_readl(nand, NDDB);
	}
}


static void berlin_nand_pio_start(struct pxa3xx_nand *nand,int size)
{
	uint32_t ndcr;

	nand_writel(nand, NDECCCTRL, 0);
	ndcr = nand_readl(nand, NDCR);

	if (size == 1)
		ndcr &= ~(NDCR_SPARE_EN | NDCR_ECC_EN | NDCR_PAGE_SZ);//set PAGE_SZ to 512
	else
		ndcr &= ~(NDCR_SPARE_EN | NDCR_ECC_EN);

	/* clear status bits and run */
	nand_writel(nand, NDSR, NDSR_MASK);
	nand_writel(nand, NDCR, ndcr & (~NDCR_ND_RUN));
	nand_writel(nand, NDSR, NDSR_MASK);
	nand_writel(nand, NDCR, ndcr | NDCR_ND_RUN);
}

/*	berlin_nand_rr_tbl_verify_hynix
*
*	description: There're 8 sets retry parameter in otp area of hynix nand.
* 	original and inverse parameter constitute one set.
*	XOR check should be taken for these parameter for ensuring integrity
*/
static int berlin_nand_rr_tbl_verify_hynix(int rr_total, unsigned char *read_buf)
{
	int i, offset, err, set_nr = 0;

	for (set_nr = 0; set_nr < 8; set_nr++) {
		offset = set_nr * rr_total * 2;
		err = 0;

		for (i = 0; i < rr_total; i++) {
			if ((read_buf[i + offset] ^ read_buf[i + offset + rr_total]) != 0xff) {
				err = 1;
				break;
			}
		}

		if (err == 0)
			return offset;
	}
	return -EINVAL;
}

static void berlin_nand_pio_write_cmd_4(struct pxa3xx_nand *nand)
{
	nand_writel(nand, NDCB0, nand->ndcb0[0]);
	nand_writel(nand, NDCB0, nand->ndcb0[1]);
	nand_writel(nand, NDCB0, nand->ndcb0[2]);
	nand_writel(nand, NDCB0, nand->ndcb0[3]);
}

static void berlin_nand_pio_write_cmd_3(struct pxa3xx_nand *nand)
{
	nand_writel(nand, NDCB0, nand->ndcb0[0]);
	nand_writel(nand, NDCB0, nand->ndcb0[1]);
	nand_writel(nand, NDCB0, nand->ndcb0[2]);
}

static int berlin_nand_pio_readrr_hynix(struct pxa3xx_nand *nand, unsigned char *read_buf)
{
	int i;
	const uint32_t cmd_tbl[RR_CMD_LEN] = {0x16, 0x17, 0x4, 0x19, 0x00};
	const uint32_t addr_tbl[RR_ADDR_LEN] = {0x0, 0x0, 0x0, 0x2, 0x0};

	//CMD 0x36 + Addr 0xFF + WData 0x40
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM)
			| NDCB0_ADDR_CYC(1)
			| NDCB0_LEN_OVRD
			| 0x36;
	nand->ndcb0[1] = 0xFF;
	nand->ndcb0[2] = 1<<8;
	nand->ndcb0[3] = 8;
	berlin_nand_pio_start(nand,0);
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_4(nand);
	if (wait_status_bit_set(nand, NDSR_WRDREQ) == -1)
		return -EBUSY;
	memset(read_buf, 0x40, 8);
	handle_data_pio_rr(nand, read_buf, 8, STATE_PIO_WRITING);

	//Addr 0xCC
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
			| NDCB0_ADDR_CYC(1)
			| NDCB0_NC;
	nand->ndcb0[1] = 0xcc;
	nand->ndcb0[2] = 1 << 8;
	berlin_nand_pio_start(nand,0);
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_3(nand);

	//WData 0x4d
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM)
			| NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE)
			| NDCB0_LEN_OVRD;
	nand->ndcb0[2] = 1<<8;
	nand->ndcb0[3] = 8;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_4(nand);
	if (wait_status_bit_set(nand, NDSR_WRDREQ) == -1)
		return -EBUSY;
	memset(read_buf, 0x4d, 8);
	handle_data_pio_rr(nand, read_buf, 8, STATE_PIO_WRITING);

	//5 CMD
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[2] = 1<<8;
	berlin_nand_pio_start(nand,0);
	for (i=0; i<RR_CMD_LEN; i++) {
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
				| NDCB0_NC
				| cmd_tbl[i];
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
			return -EBUSY;
		berlin_nand_pio_write_cmd_3(nand);
	}

	//5 sets of Addr
	nand->ndcb0[2] = 1<<8;
	for (i=0; i<RR_ADDR_LEN; i++) {
		memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
				| NDCB0_NC
				| NDCB0_ADDR_CYC(1);
		nand->ndcb0[1] = addr_tbl[i];
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
			return -EBUSY;
		berlin_nand_pio_write_cmd_3(nand);
	}

	//CMD 0x30 + RData
	berlin_nand_pio_start(nand,1);
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_READ)
			| NDCB0_CMD_XTYPE(NDCB0_CMDX_MREAD)
			| NDCB0_LEN_OVRD
			| 0x30;
	nand->ndcb0[2] = 0<<8;
	nand->ndcb0[3] = RR_READ_LEN;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_4(nand);

	if (wait_status_bit_set(nand, NDSR_RDDREQ) == -1)
		return -EBUSY;
	handle_data_pio_rr(nand, read_buf, RR_READ_LEN, STATE_PIO_READING);

	//CMD 0xFF
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_NC
			| 0xFF;
	nand->ndcb0[2] = 1<<8;
	berlin_nand_pio_start(nand,0);
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_3(nand);

	//CMD 0x38
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_RESET)
			| 0x38;
	nand->ndcb0[2] = 1<<8;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_3(nand);
	pxa3xx_read_page(get_mtd_by_info(nand->info[0]), NULL, 0x3800);

	return 0;
}


static int berlin_nand_pio_setrr_hynix(struct pxa3xx_nand *nand, int rr_current_cycle)
{
	unsigned int addr_nr = 0;
	unsigned char set_buf[8];
	struct pxa3xx_nand_read_retry *retry = nand->retry;
	struct pxa3xx_nand_info *info = nand->info[nand->chip_select];
	struct mtd_info *mtd = get_mtd_by_info(info);
	struct nand_chip *chip = mtd->priv;
	const unsigned int addr_tbl[8] = {0xcc, 0xbf, 0xaa, 0xab, 0xcd, 0xad, 0xae, 0xaf};

	memset(set_buf, 0, 8);
	//CMD 0x36
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_ADDR_CYC(0)
			| NDCB0_NC
			| 0x36;
	berlin_nand_pio_start(nand,0);
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_3(nand);

	//8 sets of Addr and WData
	for (addr_nr = 0; addr_nr < retry->rr_reg_nr; addr_nr++) {
		memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
				| NDCB0_ADDR_CYC(1)
				| NDCB0_NC;
		nand->ndcb0[1] = addr_tbl[addr_nr];
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
			return -EBUSY;
		berlin_nand_pio_write_cmd_3(nand);

		//W-data
		memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM)
				| NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE)
				| NDCB0_NC
				| NDCB0_LEN_OVRD;
		nand->ndcb0[3] = 8;
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
			return -EBUSY;
		berlin_nand_pio_write_cmd_4(nand);
		memset(set_buf, retry->rr_table[retry->rr_current_cycle * retry->rr_reg_nr + addr_nr], 8);
		if (wait_status_bit_set(nand, NDSR_WRDREQ) == -1)
			return -EBUSY;

		handle_data_pio_rr(nand, set_buf, 8, STATE_PIO_WRITING);
	}

	// CMD 0x16
	berlin_nand_pio_start(nand,0);
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
				| 0x16;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_3(nand);
	berlin_nand_wait_stop(nand);

	chip->cmdfunc(mtd, NAND_CMD_RESET, 0, 0);

	udelay(100);
	pxa3xx_dummy_read(mtd, 0x80);
	return 0;
}


static int berlin_nand_pio_verifyrr_hynix(struct pxa3xx_nand *nand)
{
	int offset = 0;
	int i;
	unsigned char read_buf[8];
	const uint32_t addr_tbl[8] = {0xcc, 0xbf, 0xaa, 0xab, 0xcd, 0xad, 0xae, 0xaf};

	memset(read_buf, 0, 8);
	for (i=0;i<8;i++) {
		offset = nand->data_column;
		memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_READ)
				| NDCB0_ADDR_CYC(1)
				| NDCB0_LEN_OVRD
				| 0x37;
		nand->ndcb0[1] = addr_tbl[i];
		nand->ndcb0[2] = 1<<8;
		nand->ndcb0[3] = 8;

		berlin_nand_pio_start(nand,0);
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
			return -EBUSY;
		berlin_nand_pio_write_cmd_4(nand);
		handle_data_pio_rr(nand, read_buf, 8, STATE_PIO_READING);
		printk("%02x ",*read_buf);
	}
	return 0;
}

int get_saved_rr_table(struct pxa3xx_nand *nand, struct nand_flash_dev *flash)
{
	struct pxa3xx_nand_read_retry *retry = nand->retry;
	int rr_page, rr_page_idx, rr_block_idx, ret = -1;
	u32 magic_id;

	rr_page = (flash->chipsize << 10)  / (flash->pagesize >>  10);

	{
		struct mtd_info *mtd = get_mtd_by_info(nand->info[0]);
		struct nand_chip *chip = mtd->priv;
		if ((chip->oob_poi == NULL) && chip->buffers) {
			chip->oob_poi = chip->buffers->databuf + mtd->writesize;
		}
	}

	for (rr_block_idx = 0; rr_block_idx < 8; rr_block_idx++) {
		for (rr_page_idx = 0; rr_page_idx < 8; rr_page_idx++) {
			uint rr_c, rr_rn, rr_total, offset;
			u8 *p_rr_buf;
			pxa3xx_nand_intern_read(nand, nand->bounce_buffer, rr_page + rr_page_idx);
			if (nand->retcode == ERR_DBERR
				|| nand->retcode == ERR_BBERR) {
				continue;
			}
			memcpy(&magic_id, nand->bounce_buffer, sizeof(u32));
			if (magic_id != 0x5a5a3412)
				continue;

			rr_c = nand->bounce_buffer[4];
			rr_rn = nand->bounce_buffer[5];

			if (rr_c >= 16 || !rr_c || rr_rn >= 16 || !rr_rn) {
				continue;
			}
			rr_total = rr_c * rr_rn;
			p_rr_buf = nand->bounce_buffer + sizeof(u32) + 2;
			offset = berlin_nand_rr_tbl_verify_hynix(rr_total, p_rr_buf);
			if (offset >= 0) {
				int i;
				retry->rr_table = (uint8_t *)kmalloc(sizeof(uint8_t) * rr_total, GFP_KERNEL);
				memcpy(retry->rr_table, &p_rr_buf[offset], rr_total);

				printk(KERN_INFO "got saved rr table @page %x, (%d,%d)\n", rr_page + rr_page_idx, rr_c, rr_rn);

				for (i = 0; i < rr_total; i++) {
					if ((i%rr_rn) == 0)
						printk(KERN_INFO "\n  ");
					printk(KERN_INFO "%02x ", retry->rr_table[i]);
				}
				printk(KERN_INFO "\n");
				retry->rr_cycles = rr_c;
				retry->rr_reg_nr = rr_rn;
				retry->set_rr = berlin_nand_pio_setrr_hynix;
				retry->verify_rr = berlin_nand_pio_verifyrr_hynix;
				retry->rr_current_cycle = 0;
				retry->rr_table_ready = 1;

				ret = 0;
				goto done;
			}
		}
		rr_page += flash->erasesize / flash->pagesize;
	}
done:

	return ret;
}

int get_opt_rr_table(struct pxa3xx_nand *nand)
{
	struct pxa3xx_nand_read_retry *retry = nand->retry;
	uint rr_c, rr_rn, rr_total, offset;
	unsigned char *read_buf, *p_rr_buf;
	int i, ret = -EINVAL;

	read_buf = kzalloc(RR_READ_LEN, GFP_KERNEL);
	if (!read_buf)
		goto exit;

	if (berlin_nand_pio_readrr_hynix(nand, read_buf) != 0) {
		printk("read RR table timeout\n");
		goto exit_free;
	}

	rr_c = read_buf[0];
	rr_rn = read_buf[1];
	if (rr_c >= 16 || !rr_c || rr_rn >= 16 || !rr_rn) {
		printk("invalid rr table header, set %d, rn %d\n", rr_c, rr_rn);
		goto exit_free;
	}

	rr_total = rr_c * rr_rn;
	p_rr_buf = read_buf + 2;
	offset = berlin_nand_rr_tbl_verify_hynix(rr_total, p_rr_buf);
	if (offset >= 0) {
		retry->rr_table = (uint8_t *)kmalloc(sizeof(uint8_t) * rr_total, GFP_KERNEL);
		memcpy(retry->rr_table, &p_rr_buf[offset], rr_total);
	} else {
		printk(KERN_INFO "invalid rr table data\n");
		goto exit_free;
	}

	for (i = 0; i < rr_total; i++) {
		if ((i%rr_rn) == 0)
			printk("\n  ");
		printk("%02x ", retry->rr_table[i]);
	}
	printk("\n");

	retry->rr_cycles = rr_c;
	retry->rr_reg_nr = rr_rn;
	retry->set_rr = berlin_nand_pio_setrr_hynix;
	retry->verify_rr = berlin_nand_pio_verifyrr_hynix;
	retry->rr_current_cycle = 0;
	retry->rr_table_ready = 1;
	ret = 0;
exit_free:
	kfree(read_buf);
exit:
	return ret;
}

int berlin_nand_nake_cmd(struct pxa3xx_nand *nand, u8 cmd, int last)
{
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_ADDR_CYC(0)
			| cmd;
	if (!last)
		nand->ndcb0[0] |= NDCB0_NC;

	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);
	return 0;
}

int berlin_nand_nake_addr(struct pxa3xx_nand *nand, u32 addr, int last)
{
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
			| NDCB0_ADDR_CYC(1);
	if (!last)
		nand->ndcb0[0] |= NDCB0_NC;

	nand->ndcb0[1] = addr;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);
	return 0;
}



const unsigned int eslc_reg_addr[HY_ESLC_REG_NUM] = {0xb0, 0xb1, 0xa0, 0xa1};
const unsigned char hynix_paired_tbl[128][2] =
{
	{0x00,0x04}, {0x01,0x05}, {0x02,0x08}, {0x03,0x09},
	{0x06,0x0C}, {0x07,0x0D}, {0x0A,0x10}, {0x0B,0x11},
	{0x0E,0x14}, {0x0F,0x15}, {0x12,0x18}, {0x13,0x19},
	{0x16,0x1C}, {0x17,0x1D}, {0x1A,0x20}, {0x1B,0x21},
	{0x1E,0x24}, {0x1F,0x25}, {0x22,0x28}, {0x23,0x29},
	{0x26,0x2C}, {0x27,0x2D}, {0x2A,0x30}, {0x2B,0x31},
	{0x2E,0x34}, {0x2F,0x35}, {0x32,0x38}, {0x33,0x39},
	{0x36,0x3C}, {0x37,0x3D}, {0x3A,0x40}, {0x3B,0x41},
	{0x3E,0x44}, {0x3F,0x45}, {0x42,0x48}, {0x43,0x49},
	{0x46,0x4C}, {0x47,0x4D}, {0x4A,0x50}, {0x4B,0x51},
	{0x4E,0x54}, {0x4F,0x55}, {0x52,0x58}, {0x53,0x59},
	{0x56,0x5C}, {0x57,0x5D}, {0x5A,0x60}, {0x5B,0x61},
	{0x5E,0x64}, {0x5F,0x65}, {0x62,0x68}, {0x63,0x69},
	{0x66,0x6C}, {0x67,0x6D}, {0x6A,0x70}, {0x6B,0x71},
	{0x6E,0x74}, {0x6F,0x75}, {0x72,0x78}, {0x73,0x79},
	{0x76,0x7C}, {0x77,0x7D}, {0x7A,0x80}, {0x7B,0x81},
	{0x7E,0x84}, {0x7F,0x85}, {0x82,0x88}, {0x83,0x89},
	{0x86,0x8C}, {0x87,0x8D}, {0x8A,0x90}, {0x8B,0x91},
	{0x8E,0x94}, {0x8F,0x95}, {0x92,0x98}, {0x93,0x99},
	{0x96,0x9C}, {0x97,0x9D}, {0x9A,0xA0}, {0x9B,0xA1},
	{0x9E,0xA4}, {0x9F,0xA5}, {0xA2,0xA8}, {0xA3,0xA9},
	{0xA6,0xAC}, {0xA7,0xAD}, {0xAA,0xB0}, {0xAB,0xB1},
	{0xAE,0xB4}, {0xAF,0xB5}, {0xB2,0xB8}, {0xB3,0xB9},
	{0xB6,0xBC}, {0xB7,0xBD}, {0xBA,0xC0}, {0xBB,0xC1},
	{0xBE,0xC4}, {0xBF,0xC5}, {0xC2,0xC8}, {0xC3,0xC9},
	{0xC6,0xCC}, {0xC7,0xCD}, {0xCA,0xD0}, {0xCB,0xD1},
	{0xCE,0xD4}, {0xCF,0xD5}, {0xD2,0xD8}, {0xD3,0xD9},
	{0xD6,0xDC}, {0xD7,0xDD}, {0xDA,0xE0}, {0xDB,0xE1},
	{0xDE,0xE4}, {0xDF,0xE5}, {0xE2,0xE8}, {0xE3,0xE9},
	{0xE6,0xEC}, {0xE7,0xED}, {0xEA,0xF0}, {0xEB,0xF1},
	{0xEE,0xF4}, {0xEF,0xF5}, {0xF2,0xF8}, {0xF3,0xF9},
	{0xF6,0xFC}, {0xF7,0xFD}, {0xFA,0xFE}, {0xFB,0xFF}
};

static int hynix_eslc_get_def(struct hynix_eslc_data *priv)
{
	struct pxa3xx_nand *nand = priv->nand;
	unsigned char set_buf[8];
	int i;

	berlin_nand_pio_start(nand,0);
	/* cmd 0x37 */
	berlin_nand_nake_cmd(nand, 0x37, 0);

	for (i = 0; i < HY_ESLC_REG_NUM; i++) {
		berlin_nand_nake_addr(nand, priv->reg_addr[i], 0);

		/* R-data */
		memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
		nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_READ)
				| NDCB0_CMD_XTYPE(NDCB0_CMDX_NREAD)
				| NDCB0_NC
				| NDCB0_LEN_OVRD;
		nand->ndcb0[3] = 8;
		if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
			return -EBUSY;
		}
		berlin_nand_pio_write_cmd_4(nand);

		if (wait_status_bit_set(nand, NDSR_RDDREQ) == -1) {
			return -EBUSY;
		}

		handle_data_pio_rr(nand, set_buf, 8, STATE_PIO_READING);

		priv->eslc_def[i] = set_buf[0];
	}

	for (i = 0; i < HY_ESLC_REG_NUM; i++) {
		printk("0x%02x ", priv->eslc_def[i]);
	}
	printk("\n");

	handle_data_dummy_rd(nand, 2048);
	return 0;
}


int hynix_nand_eslc_en(struct hynix_eslc_data *eslc_d, int en)
{
	struct pxa3xx_nand *nand = eslc_d->nand;
	struct mtd_info *mtd = get_mtd_by_info(nand->info[nand->chip_select]);
	struct pxa3xx_nand_info *info = mtd->priv;
	u8 *p_data_buf;
	SIE_BCMCFGW *cfgw;
	SIE_BCMRDAT *rdat;
	SIE_BCMRCMD *rcmd;
	u8 offset = 0;
	int c, i=0;

	if (en)
		offset = 0x0a;

	p_data_buf = nand->data_desc + DMA_H_SIZE
		+ sizeof(struct nand_buffers) * nand->chip_select;
	//berlin_nand_pio_start(nand, 0);
	mv88dexx_nand_dma_start(info, 0);
	//berlin_nand_nake_cmd(nand, 0x36, 0);
	/* CMD 0x36 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

	cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_ADDR_CYC(0)
			| NDCB0_NC
			| 0x36;
	i += sizeof(struct cmd_3_desc);

	for (c = 0; c < HY_ESLC_REG_NUM; c++) {
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
				| NDCB0_ADDR_CYC(1)
				| NDCB0_NC;
		cfgw++;
		cfgw->u_dat = eslc_d->reg_addr[c] & 0xff;
		i += sizeof(struct cmd_3_desc);
		//berlin_nand_nake_addr(nand, eslc_d->reg_addr[c], 0);

		/* w-cmd */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_4(nand, (struct cmd_4_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM)
				| NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE)
				| NDCB0_NC
				| NDCB0_LEN_OVRD;
		cfgw += 3;
		cfgw->u_dat = 8;
		i += sizeof(struct cmd_4_desc);

		/* write NDDB
		 * 1. RCMD WRITE from DDR to Dhub local */
		memset(p_data_buf + i * 8, eslc_d->eslc_def[c] + offset, 8);
		rcmd = (SIE_BCMRCMD *)(nand->data_desc + i);
		init_rcmd(nand, rcmd); /* need instantiation */
		rcmd->u_ddrAdr = nand->data_desc_addr + DMA_H_SIZE
			+ sizeof(struct nand_buffers) * nand->chip_select + i * 8;
		rcmd->u_size = 8;
		i += sizeof(SIE_BCMRCMD);

		/* write NDDB
		 * 2. forward data from dhub to NFC NDDB */
		rdat = (SIE_BCMRDAT *)(nand->data_desc + i);
		init_rdat(nand, rdat);
		rdat->u_size = 8;
		i += sizeof(SIE_BCMRDAT);


	}

	/* CMD 0x16 */
	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

	cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_ADDR_CYC(0)
			| 0x16;
	i += sizeof(struct cmd_3_desc);

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
	berlin_nand_wait_stop(nand);
	if (!en) {
		/* a dummy read according to spec */
		i = 0;
		udelay(100);
		mv88dexx_nand_dma_start(info, 0);
		udelay(50);
		/* CMD 0x00 */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
				| NDCB0_ADDR_CYC(0)
				| NDCB0_NC
				| 0x00;
		i += sizeof(struct cmd_3_desc);

		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
			| NDCB0_ADDR_CYC(5)
			| NDCB0_NC;
		i += sizeof(struct cmd_3_desc);

		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
				| NDCB0_ADDR_CYC(0)
				//| NDCB0_NC
				| 0x30;
		i += sizeof(struct cmd_3_desc);
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
		berlin_nand_wait_stop(nand);
		if (wait_status_bit_set(nand, NDSR_FLASH_RDY) == -1)
			return -EBUSY;
		udelay(100);

		mv88dexx_nand_dma_start(info, 0);
		udelay(50);
		i = 0;
		/* CMD 0xff */
		init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
		i += sizeof(SIE_BCMSEMA);
		init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
		cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

		cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
				| NDCB0_ADDR_CYC(0)
				| 0xff;
		i += sizeof(struct cmd_3_desc);

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
		berlin_nand_wait_stop(nand);

		udelay(200);
		pxa3xx_dummy_read(mtd, eslc_d->page_addr);
		udelay(100);
	}

	return 0;
}

static int hynix_slc_page_trans(struct hynix_eslc_data *eslc_d, u32 page_addr)
{
	struct pxa3xx_nand *nand = eslc_d->nand;
	struct mtd_info *mtd = get_mtd_by_info(nand->info_slc[nand->chip_select]);
	struct pxa3xx_nand_info *info = mtd->priv;
	struct eslc_part *eslc_q;
	int page = -1;

	list_for_each_entry(eslc_q, &eslc_d->slc_parts_qh, list) {
		u32 start, end;

		start = eslc_q->part->offset >> info->page_shift;
		end  = start + (eslc_q->part->size >> info->page_shift);
		if ((page_addr >= start) && (page_addr < end)) {
			page = page_addr - (start>>1);
			break;
		}
	}
	return page;
}

static int hynix_nand_set_slc_mode(struct hynix_eslc_data *eslc_d, int mode)
{
	struct pxa3xx_nand *nand = eslc_d->nand;
	unsigned int i = 0;
	SIE_BCMCFGW *cfgw;
	uint32_t ndcr, ndeccctrl = 0, cmd;

	if (mode == SLC_MODE) {
		cmd = 0xBF;
	} else {
		cmd = 0xBE;
	}

	ndcr = nand_readl(nand, NDCR);//info->reg_ndcr;
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

	ndcr &= ~(NDCR_SPARE_EN | NDCR_ECC_EN);
	ndeccctrl &= ~NDECCCTRL_BCH_EN;

	//nand->ndeccctrl = ndeccctrl;
	//nand->ndcr = ndcr;

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

	init_sema_nfccmd(nand, (SIE_BCMSEMA *)(nand->data_desc + i));
	i += sizeof(SIE_BCMSEMA);
	init_cmd_3(nand, (struct cmd_3_desc *)(nand->data_desc + i));
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);

	cfgw->u_dat = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
			| NDCB0_ADDR_CYC(0)
			| cmd;
	i += sizeof(struct cmd_3_desc);

	/************* restore NDECCCTRL ****************/
	cfgw = (SIE_BCMCFGW *)(nand->data_desc + i);
	cfgw->u_dat = nand->ndeccctrl;
	cfgw->u_devAdr = pb_get_phys_offset(nand, NDECCCTRL);
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

	berlin_nand_wait_stop(nand);
	return 0;
}

static int hynix_slc_add_part(void *priv, struct mtd_partition *part)
{
	struct hynix_eslc_data *p_eslc = (struct hynix_eslc_data *) priv;
	struct eslc_part *slc_partition;

	slc_partition = kmalloc(sizeof(struct eslc_part), GFP_KERNEL);
	if (slc_partition) {
		slc_partition->part = part;
		list_add_tail(&slc_partition->list, &p_eslc->slc_parts_qh);
		return 0;
	}
	return -ENOMEM;
}

static int hynix_slc_start(void *priv, unsigned cmd, int page)
{
	struct hynix_eslc_data *eslc_d = (struct hynix_eslc_data *)priv;
	int new_page;

	new_page = hynix_slc_page_trans(eslc_d, page);

	if (cmd == NAND_CMD_PAGEPROG ||
		cmd == NAND_CMD_RNDOUT ||
		cmd == NAND_CMD_ERASE1)  {
		hynix_nand_set_slc_mode(eslc_d, SLC_MODE);
		/* at least 100ns delay according to spec */
		udelay(1);
	}

	if (new_page != -1) {
		eslc_d->page_addr = new_page;
		return new_page;
	} else {
		return page;
	}

}

static void hynix_slc_stop(void *priv, uint cmd)
{
	if (cmd == NAND_CMD_PAGEPROG ||
		cmd == NAND_CMD_RNDOUT ||
		cmd == NAND_CMD_ERASE1)  {
		hynix_nand_set_slc_mode((struct hynix_eslc_data *)priv, MLC_MODE);
		/* at least 100ns delay according to spec */
		udelay(10);
	}
}

loff_t hynix_slc_get_phy_off(void *priv, loff_t off)
{
	struct hynix_eslc_data *eslc_d = (struct hynix_eslc_data *)priv;
	struct eslc_part *slc_q;

	list_for_each_entry(slc_q, &eslc_d->slc_parts_qh, list) {
		if ((off >= slc_q->part->offset) &&
		(off < (slc_q->part->offset + slc_q->part->size))) {
			return ((off - slc_q->part->offset) << 1)
				+ slc_q->part->offset;
		}
	}
	printk(KERN_ERR "eslc get phy offset failed!!!\n");
	return off;
}

static int hynix_eslc_page_trans(struct hynix_eslc_data *eslc_d, u32 page_addr)
{
	struct pxa3xx_nand *nand = eslc_d->nand;
	struct mtd_info *mtd = get_mtd_by_info(nand->info_eslc[nand->chip_select]);
	struct pxa3xx_nand_info *info = mtd->priv;
	struct eslc_part *eslc_q;
	u32 inter_off, pages_per_blk, page_shift;
	int page = -1;

	page_shift = info->erase_shift - info->page_shift;
	pages_per_blk = 1 << page_shift;
	list_for_each_entry(eslc_q, &eslc_d->eslc_parts_qh, list) {
		u32 start, end;

		start = eslc_q->part->offset >> info->page_shift;
		end  = start + (eslc_q->part->size >> info->page_shift);
		if ((page_addr >= start) && (page_addr < end)) {
			page_addr -= start;
			inter_off =  page_addr % pages_per_blk;
			inter_off = hynix_paired_tbl[inter_off][0];
			page = (page_addr >> page_shift) << (page_shift + 1);
			page += start + inter_off;
			break;
		}
	}
	return page;
}

loff_t hynix_get_phy_off(void *priv, loff_t off)
{
	struct hynix_eslc_data *eslc_d = (struct hynix_eslc_data *)priv;
	struct eslc_part *eslc_q;

	list_for_each_entry(eslc_q, &eslc_d->eslc_parts_qh, list) {
		if ((off >= eslc_q->part->offset) &&
		(off < (eslc_q->part->offset + eslc_q->part->size))) {
			return ((off - eslc_q->part->offset) << 1)
				+ eslc_q->part->offset;
		}
	}
	printk(KERN_ERR "eslc get phy offset failed!!!\n");
	return off;
}

static int hynix_eslc_add_part(void *priv, struct mtd_partition *part)
{
	struct hynix_eslc_data *p_eslc = (struct hynix_eslc_data *) priv;
	struct eslc_part *eslc_partition;

	eslc_partition = kmalloc(sizeof(struct eslc_part), GFP_KERNEL);
	if (eslc_partition) {
		eslc_partition->part = part;
		list_add_tail(&eslc_partition->list, &p_eslc->eslc_parts_qh);
		return 0;
	}
	return -ENOMEM;
}

static int hynix_eslc_start(void *priv, unsigned cmd, int page)
{
	struct hynix_eslc_data *eslc_d = (struct hynix_eslc_data *)priv;
	int new_page;

	new_page = hynix_eslc_page_trans(eslc_d, page);

	if (cmd == NAND_CMD_PAGEPROG) {
		hynix_nand_eslc_en((struct hynix_eslc_data *)priv, 1);
	}

	if (new_page != -1) {
		eslc_d->page_addr = new_page;
		return new_page;
	} else {
		return page;
	}

}

static void hynix_eslc_stop(void *priv, uint cmd)
{
	if (cmd == NAND_CMD_PAGEPROG) {
		hynix_nand_eslc_en((struct hynix_eslc_data *)priv, 0);
	}
}

struct pxa3xx_nand_slc* hynix_slc_init(struct pxa3xx_nand *nand)
{
	struct pxa3xx_nand_slc *p_slc = NULL;
	struct hynix_eslc_data *priv;
	p_slc = kmalloc(sizeof(struct pxa3xx_nand_slc), GFP_KERNEL);
	if (!p_slc)
		return NULL;

	p_slc->priv = kmalloc(sizeof(struct hynix_eslc_data), GFP_KERNEL);
	if (!p_slc->priv) {
		kfree(p_slc);
		return NULL;
	}

	priv = (struct hynix_eslc_data *)p_slc->priv;
	priv->nand = nand;
	priv->reg_addr = eslc_reg_addr;
	INIT_LIST_HEAD(&priv->eslc_parts_qh);
	hynix_eslc_get_def(priv);

	p_slc->eslc_part_add = hynix_eslc_add_part;
	p_slc->eslc_start = hynix_eslc_start;
	p_slc->eslc_stop = hynix_eslc_stop;
	p_slc->eslc_get_phy_off = hynix_get_phy_off;


	p_slc->slc_part_add = hynix_slc_add_part;
	p_slc->slc_start = hynix_slc_start;
	p_slc->slc_stop = hynix_slc_stop;
	p_slc->slc_get_phy_off = hynix_slc_get_phy_off;
	INIT_LIST_HEAD(&priv->slc_parts_qh);

	return p_slc;
}


void hynix_slc_deinit(struct pxa3xx_nand *nand)
{
	if (nand->p_slc) {
		if (nand->p_slc->priv) {
			struct eslc_part *eslc_part;
			struct hynix_eslc_data *priv;

			priv = (struct hynix_eslc_data *)nand->p_slc->priv;
			list_for_each_entry(eslc_part, &priv->eslc_parts_qh, list) {
				list_del(&eslc_part->list);
				kfree(eslc_part);
			}
			list_for_each_entry(eslc_part, &priv->slc_parts_qh, list) {
				list_del(&eslc_part->list);
				kfree(eslc_part);
			}
			kfree(nand->p_slc->priv);
		}
		kfree(nand->p_slc);
	}

	nand->p_slc = NULL;
}
int berlin_nand_rr_init(struct pxa3xx_nand *nand, struct nand_flash_dev *flash)
{
	int ret = 0;
	struct pxa3xx_nand_read_retry *retry = nand->retry;

	switch (retry->rr_type) {
		case HYNIX_H27UCG8T_RR_MODE1:
			if (retry->rr_table_ready)
				break;
			if (!nand->p_slc) {
				nand->p_slc = hynix_slc_init(nand);
			}
			ret = get_saved_rr_table(nand, flash);
			if (ret != 0) {
				ret = get_opt_rr_table(nand);
			}
			break;
		case MICRON_MT29F32G08CBADA_RR:
			retry->rr_cycles = 3;
			retry->rr_reg_nr = 0;
			retry->rr_table_ready = 0;
			retry->rr_table = NULL;
			retry->set_rr = berlin_nand_pio_setrr_micron;
			retry->verify_rr = berlin_nand_pio_verifyrr_micron;
			break;
		case NO_READ_RETRY:
			retry->rr_cycles = 0;
			retry->rr_reg_nr = 0;
			retry->rr_table_ready = 0;
			retry->rr_table = NULL;
			retry->set_rr = NULL;
			retry->verify_rr = NULL;
			printk("READ RETRY :\n    TYPE:NOT SUPPORTING\n");
			break;
	}
	return ret;
}

void berlin_nand_rr_exit(struct pxa3xx_nand *nand)
{
	struct pxa3xx_nand_read_retry *retry = nand->retry;
	if (retry->rr_table != NULL)
		kfree(retry->rr_table);
	kfree(retry);

	if (nand->p_slc) {
		hynix_slc_deinit(nand);
	}
}

//Read Retry for Micron MT29F32G08CBADA
static int berlin_nand_pio_setrr_micron(struct pxa3xx_nand *nand, int retry_c)
{
	unsigned char set_buf[8] = {0};

	//CMD 0xEF + Addr 0x89 + WData P1+P2+P3+P4
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	berlin_nand_pio_start(nand,0);

	/* cmd 0xEF */
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
		| NDCB0_ADDR_CYC(0)
		| NDCB0_NC
		| 0xEF;

	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);

	/* addr 0x89 */
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
		| NDCB0_ADDR_CYC(1)
		| NDCB0_NC;
	nand->ndcb0[1] = 0x89;

	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);

	/* 4-byte data */
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_PROGRAM)
		| NDCB0_CMD_XTYPE(NDCB0_CMDX_NWRITE)
		| NDCB0_NC
		| NDCB0_LEN_OVRD;
	nand->ndcb0[3] = 8;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_4(nand);

	if (wait_status_bit_set(nand, NDSR_WRDREQ) == -1)
		return -EBUSY;
	if((retry_c > 0) && (retry_c <= 3))
		set_buf[0] = retry_c;
	handle_data_pio_rr(nand, set_buf, 8, STATE_PIO_WRITING);

	printk(KERN_INFO "Micron read retry SetFeature %d\n", retry_c);
	return 0;
}

static int berlin_nand_pio_verifyrr_micron(struct pxa3xx_nand *nand)
{
	unsigned char read_buf[8] = {0};

	//CMD 0xEE + Addr 0x89 + RData P1+P2+P3+P4
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	berlin_nand_pio_start(nand,0);

	/* cmd 0xEE */
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_CMD)
		| NDCB0_ADDR_CYC(0)
		| NDCB0_NC
		| 0xEE;

	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);

	/* addr 0x89 */
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_NAKED_ADDR)
		| NDCB0_ADDR_CYC(1)
		| NDCB0_NC;
	nand->ndcb0[1] = 0x89;

	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1) {
		return -EBUSY;
	}
	berlin_nand_pio_write_cmd_3(nand);

	/* 4-byte data */
	memset(nand->ndcb0, 0, sizeof(uint32_t)*CMD_POOL_SIZE);
	nand->ndcb0[0] = NDCB0_CMD_TYPE(NDCB0_CMD_READ)
		| NDCB0_CMD_XTYPE(NDCB0_CMDX_NREAD)
		| NDCB0_NC
		| NDCB0_LEN_OVRD;
	nand->ndcb0[3] = 8;
	if (wait_status_bit_set(nand, NDSR_WRCMDREQ) == -1)
		return -EBUSY;
	berlin_nand_pio_write_cmd_4(nand);

	if (wait_status_bit_set(nand, NDSR_RDDREQ) == -1)
		return -EBUSY;
	handle_data_pio_rr(nand, read_buf, 8, STATE_PIO_READING);

	printk(KERN_INFO "Micron read retry GetFeature %x\n", read_buf[0]);

	return read_buf[0];
}

