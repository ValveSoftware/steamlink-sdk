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

// fastlogo options:
//#define LOGO_TIME_PROFILE 1 // use definition in Makefile
//#define LOGO_PROC_FS      1 // use definition in Makefile

#define _FASTLOGO_DRIVER_C_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h> // for msleep()
#include <asm/cacheflush.h> // for __cpuc_flush_dcache_area(logoBuf, length);
#include <asm/outercache.h>
#include <asm/page.h>
#include <asm/io.h>

#if LOGO_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#include "galois_io.h"
#include "thinvpp_api.h"
#include "thinvpp_isr.h"
#include "api_dhub.h"
#include "api_avio_dhub.h"
#include "Galois_memmap.h"
#include "bcm_cmds.h"
#include "logo_data.h"

#if LOGO_USE_SHM
#include "shm_api.h"
#include "shm_type.h"
extern shm_device_t *shm_api_device_noncache;
#endif
#include <mach/irqs.h>

/*******************************************************************************
  Module API defined
  */

#define bTST(x, b) (((x) >> (b)) & 1)

#define FASTLOGO_DEVICE_TAG                       "[Galois][fastlogo_driver] "
#define DEVICE_NAME "galois_fastlogo"

void VPP_dhub_sem_clear(void);

/*******************************************************************************
  Macro Defined
  */

#ifdef ENABLE_DEBUG
#define gs_debug(...)   printk(KERN_DEBUG FASTLOGO_DEVICE_TAG __VA_ARGS__)
#else
#define gs_debug(...)
#endif

#define gs_info(...)    printk(KERN_INFO FASTLOGO_DEVICE_TAG __VA_ARGS__)
#define gs_notice(...)  printk(KERN_NOTICE FASTLOGO_DEVICE_TAG __VA_ARGS__)

#define gs_trace(...)   printk(KERN_WARNING FASTLOGO_DEVICE_TAG __VA_ARGS__)
#define gs_error(...)   printk(KERN_ERR FASTLOGO_DEVICE_TAG __VA_ARGS__)

/*******************************************************************************
  Module Variable
  */

logo_device_t fastlogo_ctx;
int save_console_loglevel; // to restore console printk 91  static VBUF_INFO vbuf;
#define LIMIT_CONSOLE_LOGLEVEL 4 // limit printk to KERN_EMERG, KERN_ALERT, KERN_CRIT and KERN_ERR
static VBUF_INFO vbuf;

#if LOGO_TIME_PROFILE
#define LOGO_TP_MAX_COUNT 384 //0x10000
static u64 cpcb0_isr_time_previous = (u64) 0;
u64 lat[LOGO_TP_MAX_COUNT];
u64 lat2[LOGO_TP_MAX_COUNT];
volatile int logo_tp_count = 0;
#endif

#if LOGO_PROC_FS
static int logo_device_show_stat(logo_device_t *Ctx, struct seq_file *file)
{
    int i, n, len;

	if ((NULL == Ctx) || (NULL == file))
		return -EINVAL;

    if (Ctx->planes & 1)
        len += seq_printf(file, "Plane MAIN is used\n");
    if (Ctx->planes & 2)
        len += seq_printf(file, "Plane PIP is used\n");
    if (Ctx->planes & 4)
        len += seq_printf(file, "Plane AUX is used\n");

    len += seq_printf(file, "Vres : %d\nWidth x Height : %d x %d\n", Ctx->vres, Ctx->win.width, Ctx->win.height);
	len += seq_printf(file, "logoBuf : 0x%p (0x%p)\n", Ctx->logoBuf_2, Ctx->mapaddr);
	len += seq_printf(file, "    display @(%d,%d) %dx%d\n",
                vbuf.m_active_left, vbuf.m_active_top, vbuf.m_active_width, vbuf.m_active_height);

	len += seq_printf(file, "Total count : %d\n", Ctx->count);
#if LOGO_TIME_PROFILE
    // show profile
    len += seq_printf(file,"\n[profile] %d", logo_tp_count);
    for (i = 0; i < logo_tp_count; i++)
    {
        if (i%4)
            len += seq_printf(file, "%08llu:%08llu ", lat[i], lat2[i]);
        else
            len += seq_printf(file, "\n%4d: %08llu:%08llu ", i, lat[i], lat2[i]);
    }
#endif //LOGO_TIME_PROFILE

    len += seq_printf(file,"\n");

	return len;
}

static struct proc_dir_entry *logo_driver_procdir;
static int logo_stat_seq_show(struct seq_file *file, void *data)
{
	logo_device_t *Ctx = (logo_device_t *) data;
	if (Ctx)
		logo_device_show_stat(Ctx, file);

	return 0;
}

static void logo_stat_seq_stop(struct seq_file *file, void *data)
{
}

static void *logo_stat_seq_start(struct seq_file *file, loff_t * pos)
{
	if (*pos == 0)
		return (void *)&fastlogo_ctx;
	return NULL;
}

static void *logo_stat_seq_next(struct seq_file *file, void *data, loff_t * pos)
{
	(*pos)++;
	if (*pos == 0)
		return (void *)&fastlogo_ctx;
	return NULL;
}

static struct seq_operations logo_stat_seq_ops = {
	.start		= logo_stat_seq_start,
	.next		= logo_stat_seq_next,
	.stop		= logo_stat_seq_stop,
	.show		= logo_stat_seq_show
};

static int logo_stat_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &logo_stat_seq_ops);
}

static struct file_operations logo_stat_file_ops = {
	.owner		= THIS_MODULE,
	.open		= logo_stat_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};

#endif //LOGO_PROC_FS

u64 logo_time_0 = (u64) 0;
u64 last_isr_time = (u64) 0;
unsigned last_isr_interval = 0;
volatile int logo_isr_count = 0;
volatile int cpcb_start_flag = 0;

static irqreturn_t fastlogo_devices_vpp_isr(int irq, void *dev_id)
{
    int instat;
    HDL_semaphore *pSemHandle;
    u64 cpcb0_isr_time_current;

    ++fastlogo_ctx.count;
    logo_isr_count++;

    cpcb0_isr_time_current = cpu_clock(smp_processor_id());
    last_isr_interval = (unsigned) (cpcb0_isr_time_current - last_isr_time);
    last_isr_time = cpcb0_isr_time_current;

#if LOGO_TIME_PROFILE
    {
        u64 curr_interval;
        if (cpcb0_isr_time_previous) {
            curr_interval = cpcb0_isr_time_current - cpcb0_isr_time_previous;
            if (logo_tp_count < LOGO_TP_MAX_COUNT)
                lat[logo_tp_count++] = curr_interval;
        }
        cpcb0_isr_time_previous = cpcb0_isr_time_current;
    }
#endif

    /* VPP interrupt handling  */
    pSemHandle = dhub_semaphore(&VPP_dhubHandle.dhub);
    instat = semaphore_chk_full(pSemHandle, -1);

    if (bTST(instat, avioDhubSemMap_vpp_vppCPCB0_intr)) {
        /* our CPCB interrupt */
        /* clear interrupt */
        semaphore_pop(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
        semaphore_clr_full(pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr);

		if(logo_isr_count > 1)
		{
			THINVPP_CPCB_ISR_service(thinvpp_obj, CPCB_1);
		}
    }

#if LOGO_TIME_PROFILE
    if (logo_tp_count) {
        u64 curr_interval = cpu_clock(0) - cpcb0_isr_time_current;
        if ((logo_tp_count-1) < LOGO_TP_MAX_COUNT)
            lat2[logo_tp_count-1] = curr_interval;
    }
#endif

    return IRQ_HANDLED;
}

static int __init fastlogo_device_init(unsigned int cpu_id)
{
    int err;

    logo_time_0 = cpu_clock(0);

    // init context
    fastlogo_ctx.bcm_cmd_0 = bcm_cmd_0;
    fastlogo_ctx.bcm_cmd_0_len = bcm_cmd_0_len;
    fastlogo_ctx.bcm_cmd_a = bcm_cmd_a;
    fastlogo_ctx.bcm_cmd_a_len = bcm_cmd_a_len;
    fastlogo_ctx.bcm_cmd_n = bcm_cmd_n;
    fastlogo_ctx.bcm_cmd_n_len = bcm_cmd_n_len;
    fastlogo_ctx.bcm_cmd_z = bcm_cmd_z;
    fastlogo_ctx.bcm_cmd_z_len = bcm_cmd_z_len;

    fastlogo_ctx.bcmQ_len = bcmQ_len;
    fastlogo_ctx.dmaQ_len = 8*8;
    fastlogo_ctx.cfgQ_len = 8*8;

    /* set up logo frame */
    fastlogo_ctx.length = yuv_logo_stride*yuv_logo_height;
#ifndef YUV_LOGO_ALPHA
    vbuf.alpha   = 255;
#else
    vbuf.alpha   = YUV_LOGO_ALPHA;
#endif
#ifndef YUV_LOGO_BGCOLOR
    vbuf.bgcolor = yuv_logo[0];
    if ((vbuf.bgcolor & 0xff00ff) != 0x800080)
        vbuf.bgcolor = 0x00800080; // black
#else
    vbuf.bgcolor = YUV_LOGO_BGCOLOR;
#endif
    vbuf.m_disp_offset   = 0;
    vbuf.m_active_left   = ((720 - yuv_logo_width)/8)*4; // at center
    vbuf.m_active_top    = ((480 - yuv_logo_height)/8)*4; // at center
    vbuf.m_buf_stride    = yuv_logo_stride;
    vbuf.m_active_width  = yuv_logo_width;
    vbuf.m_active_height = yuv_logo_height;

#if LOGO_USE_SHM
    // use MV_SHM for logo buffer and 3 dhub queues to have contiguous memory
    fastlogo_ctx.mSHMSize = fastlogo_ctx.length +
        fastlogo_ctx.bcmQ_len + fastlogo_ctx.dmaQ_len + fastlogo_ctx.cfgQ_len;

    fastlogo_ctx.mSHMOffset = MV_SHM_NONCACHE_Malloc(fastlogo_ctx.mSHMSize, 4096);
    if (fastlogo_ctx.mSHMOffset == ERROR_SHM_MALLOC_FAILED)
    {
        return -1;
    }

    // put logo image in logo buffer
    fastlogo_ctx.logoBuf = (int *) MV_SHM_GetNonCacheVirtAddr(fastlogo_ctx.mSHMOffset);
    fastlogo_ctx.mapaddr = (unsigned int *) MV_SHM_GetNonCachePhysAddr(fastlogo_ctx.mSHMOffset);
    memcpy(fastlogo_ctx.logoBuf, yuv_logo, fastlogo_ctx.length);

    // arrange dhub queues and commands
    {
        char *shm = (char *) fastlogo_ctx.logoBuf;
        unsigned shm_phys = (unsigned) fastlogo_ctx.mapaddr;
        fastlogo_ctx.bcmQ = shm + fastlogo_ctx.length;
        fastlogo_ctx.dmaQ = fastlogo_ctx.bcmQ + fastlogo_ctx.bcmQ_len;
        fastlogo_ctx.cfgQ = fastlogo_ctx.dmaQ + fastlogo_ctx.dmaQ_len;
        fastlogo_ctx.bcmQ_phys = shm_phys + fastlogo_ctx.length;
        fastlogo_ctx.dmaQ_phys = fastlogo_ctx.bcmQ_phys + fastlogo_ctx.bcmQ_len;
        fastlogo_ctx.cfgQ_phys = fastlogo_ctx.dmaQ_phys + fastlogo_ctx.dmaQ_len;

        // pre-load vpp commands
        memcpy(fastlogo_ctx.bcmQ, bcm_cmd_0, bcm_cmd_0_len);

        // pre-load logo frame dma commands
        fastlogo_ctx.logo_dma_cmd_len = logo_dma_cmd_len;
        fastlogo_ctx.logo_frame_dma_cmd = logo_frame_dma_cmd;
        logo_frame_dma_cmd[2] = shm_phys;
        vbuf.m_pbuf_start = (void *) shm_phys;
        memcpy(fastlogo_ctx.dmaQ, logo_frame_dma_cmd, logo_dma_cmd_len);
    }
#else
    fastlogo_ctx.logoBuf = kmalloc(fastlogo_ctx.length, GFP_KERNEL);
    if (!fastlogo_ctx.logoBuf) {
        gs_trace("kmalloc error\n");
        return err;
    }
    memcpy(fastlogo_ctx.logoBuf, yuv_logo, fastlogo_ctx.length);

    fastlogo_ctx.mapaddr = (unsigned int *)dma_map_single(NULL, fastlogo_ctx.logoBuf, fastlogo_ctx.length, DMA_TO_DEVICE);
    err = dma_mapping_error(NULL, (dma_addr_t)fastlogo_ctx.logoBuf);
    if (err) {
        gs_trace("dma_mapping_error\n");
        kfree(fastlogo_ctx.logoBuf);
        fastlogo_ctx.logoBuf = NULL;
        return err;
    }
	outer_cache.flush_range(virt_to_phys(fastlogo_ctx.logoBuf), virt_to_phys(fastlogo_ctx.logoBuf)+fastlogo_ctx.length);
    logo_frame_dma_cmd[2] = virt_to_phys(fastlogo_ctx.logoBuf);
    vbuf.m_pbuf_start = (void *) logo_frame_dma_cmd[2];
#endif
    fastlogo_ctx.logoBuf_2 = fastlogo_ctx.logoBuf;

    /* initialize dhub */
    DhubInitialization(cpu_id, VPP_DHUB_BASE, VPP_HBO_SRAM_BASE, &VPP_dhubHandle, VPP_config, VPP_NUM_OF_CHANNELS);
    DhubInitialization(cpu_id, AG_DHUB_BASE, AG_HBO_SRAM_BASE, &AG_dhubHandle, AG_config, AG_NUM_OF_CHANNELS);

    MV_THINVPP_Create(MEMMAP_VPP_REG_BASE);
    MV_THINVPP_Reset();
    MV_THINVPP_Config();

	/* set output resolution */
    MV_THINVPP_SetCPCBOutputResolution(CPCB_1, RES_525P5994, OUTPUT_BIT_DEPTH_8BIT);

    // use MAIN plane
    fastlogo_ctx.planes = 1;
    fastlogo_ctx.win.x = 0;
    fastlogo_ctx.win.y = 0;
    fastlogo_ctx.win.width = 720;
    fastlogo_ctx.win.height = 480;
    MV_THINVPP_SetMainDisplayFrame(&vbuf);
    MV_THINVPP_OpenDispWindow(PLANE_MAIN, &fastlogo_ctx.win, NULL);

    /* register ISR */
    err = request_irq(IRQ_DHUBINTRAVIO0, fastlogo_devices_vpp_isr, IRQF_DISABLED, "fastlogo_module_vpp", NULL);
    if (unlikely(err < 0)) {
        gs_trace("vec_num:%5d, err:%8x\n", IRQ_DHUBINTRAVIO0, err);
        return err;
    }

	/*
	 * using 3 for debugging legacy; should change to a more reasonable
	 * number after clean-up
	 */
	cpcb_start_flag = 3;

    /* clean up and enable ISR */
    VPP_dhub_sem_clear();
    semaphore_pop(thinvpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr, 1);
    semaphore_clr_full(thinvpp_obj->pSemHandle, avioDhubSemMap_vpp_vppCPCB0_intr);
    THINVPP_Enable_ISR_Interrupt(thinvpp_obj, CPCB_1, 1);

    return 0;
}

static void fastlogo_device_exit(void)
{
    MV_THINVPP_CloseDispWindow();
    MV_THINVPP_Stop();

    msleep(100); //100 milliseconds
    MV_THINVPP_Destroy();
#if LOGO_USE_SHM
    if (fastlogo_ctx.mSHMOffset != ERROR_SHM_MALLOC_FAILED)
    {
        MV_SHM_NONCACHE_Free(fastlogo_ctx.mSHMOffset);
        fastlogo_ctx.logoBuf = NULL;
        fastlogo_ctx.mSHMOffset = ERROR_SHM_MALLOC_FAILED;
    }
#else
    if (fastlogo_ctx.logoBuf) {
        dma_unmap_single(NULL, (dma_addr_t)fastlogo_ctx.mapaddr, fastlogo_ctx.length, DMA_TO_DEVICE);
        gs_trace("will free pBuf OK\n");
        kfree(fastlogo_ctx.logoBuf);
        fastlogo_ctx.logoBuf = NULL;
    }
#endif

    /* unregister VPP interrupt */
    msleep(100); //100 milliseconds
    free_irq(IRQ_DHUBINTRAVIO0, NULL);
    console_loglevel = save_console_loglevel; // restore console printk

    gs_trace("dev exit done\n");

#if LOGO_TIME_PROFILE
    {
        int i;
        gs_notice("profile %d:\n", logo_tp_count);
        for (i = 0; i < logo_tp_count; i++) {
            if (lat[i]>17000000 || lat[i]<16000000)
                printk("%4d: %08llu:%08llu\n", i, lat[i], lat2[i]);
        }
    }
#endif
}

/*******************************************************************************
  Module API
  */
void fastlogo_stop(void)
{
    if (MV_THINVPP_IsCPCBActive(CPCB_1))
        fastlogo_device_exit();
}
EXPORT_SYMBOL(fastlogo_stop);


/*******************************************************************************
  Module Register API
  */
static int __init fastlogo_driver_init(void)
{
    int res;

    gs_info("drv init\n");

    memset(&fastlogo_ctx, 0, sizeof(fastlogo_ctx));
#if LOGO_PROC_FS
    {
        struct proc_dir_entry *pstat = NULL;
        logo_driver_procdir = proc_mkdir(DEVICE_NAME, NULL);
        pstat = create_proc_entry("stat", 0, logo_driver_procdir);
        if (pstat)
            pstat->proc_fops = &logo_stat_file_ops;
    }
#endif //LOGO_PROC_FS

    fastlogo_ctx.vres = MV_THINVPP_IsCPCBActive(CPCB_1);

    if (!fastlogo_ctx.vres)
    {
        gs_trace("fastlogo is not enabled in bootloader\n");
        return 0; // do nothing if fastlogo is not enabled in bootloader
    }
    if (fastlogo_ctx.vres != 524)
    {
        gs_trace("fastlogo does not supprt vres=%d\n", fastlogo_ctx.vres);
        return 0; // do nothing if vres is not supported
    }

    //console_silent(); // don't want printk to interfere us
    save_console_loglevel = console_loglevel; // to restore console printk
    if (console_loglevel > LIMIT_CONSOLE_LOGLEVEL)
        console_loglevel = LIMIT_CONSOLE_LOGLEVEL;

    /* create PE device */
    res = fastlogo_device_init(CPUINDEX);
    if (res != 0) {
        gs_error("drv init failed !!! res = 0x%08X\n", res);
        return res;
    } else {
        gs_trace("drv init OK\n");
    }

    return 0;
}

static void __exit fastlogo_driver_exit(void)
{
    fastlogo_stop();

    gs_trace("drv exit done\n");
}

module_init(fastlogo_driver_init);
module_exit(fastlogo_driver_exit);

MODULE_AUTHOR("marvell");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fastlogo module");
