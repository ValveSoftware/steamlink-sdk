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
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>	/* for seq_file */
#include <linux/sched.h>	/* for tasklist_lock */

#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
/*******************************************************************************
  Local head files
*/

#include <mach/galois_generic.h>
#include "shm_type.h"
#include "shm_device.h"
#include "shm_api_kernel.h"

/*******************************************************************************
  Module Descrption
*/

MODULE_AUTHOR("Fang Bao");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Shared Memory Driver");

/*******************************************************************************
  Module API defined
*/
/*******************************************************************************
  Separate cache and non-cache driver interface
*/
static int shm_driver_open(struct inode *inode, struct file *filp);
static int shm_driver_open_noncache(struct inode *inode, struct file *filp);
static int shm_driver_release(struct inode *inode, struct file *filp);
static int shm_driver_mmap_noncache(struct file *filp,
				    struct vm_area_struct *vma);
static int shm_driver_mmap_cache(struct file *filp, struct vm_area_struct *vma);
static long shm_driver_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg);

/*******************************************************************************
  Module Parameter
*/
uint shm_base_cache ;//= 0x12000000;//CONFIG_MV88DE3100_SHMMEM_START;
static uint shm_size_cache ;//= 0x0C000000; //CONFIG_MV88DE3100_SHMMEM_CACHE_SIZE;
static uint shm_size_noncache; //= 0x2000000;
static uint shm_base_noncache;//;= 0x1A000000;

uint shm_lowmem_debug_level = 2;
module_param_named(debug_level, shm_lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

static uint shm_lowmem_shrink_threshold = 90;
module_param_named(shrink_threshold, shm_lowmem_shrink_threshold, uint, S_IRUGO | S_IWUSR);

static int shm_lowmem_start_oom_adj = 1;
module_param_named(start_oom_adj, shm_lowmem_start_oom_adj, int, S_IRUGO | S_IWUSR);

static int shm_lowmem_min_adj = 1;
module_param_named(min_adj, shm_lowmem_min_adj, int, S_IRUGO | S_IWUSR);

static int shm_lowmem_min_release_kbytes = 4096;
module_param_named(min_release_kbytes, shm_lowmem_min_release_kbytes, int, S_IRUGO | S_IWUSR);
/*******************************************************************************
  Module Variable
*/

static shm_device_t *shm_device = NULL;
static shm_device_t *shm_device_noncache = NULL;

static struct cdev shm_dev_noncache;
static struct cdev shm_dev_cache;
static struct class *shm_dev_class;

static struct file_operations shm_ops_noncache = {
	.open = shm_driver_open_noncache,
	.release = shm_driver_release,
	.mmap = shm_driver_mmap_noncache,
	.unlocked_ioctl = shm_driver_ioctl,
	.owner = THIS_MODULE,
};

static struct file_operations shm_ops_cache = {
	.open = shm_driver_open,
	.release = shm_driver_release,
	.mmap = shm_driver_mmap_cache,
	.unlocked_ioctl = shm_driver_ioctl,
	.owner = THIS_MODULE,
};

static shm_driver_cdev_t shm_driver_dev_list[] = {
	{GALOIS_SHM0_MINOR, SHM_DEVICE_NAME_NONCACHE, &shm_dev_noncache,
	 &shm_ops_noncache},
	{GALOIS_SHM1_MINOR, SHM_DEVICE_NAME_CACHE, &shm_dev_cache,
	 &shm_ops_cache},
};

static struct proc_dir_entry *shm_driver_procdir;

static struct task_struct* shm_lowmem_deathpending;
static unsigned long shm_lowmem_deathpending_timeout;
static int shm_lowmem_going;
static struct shm_stat_info shm_stat;
static DEFINE_SEMAPHORE(shm_lowmem_mutex);
/*********************** shm lowmem killer *****************************/

#define shm_lowmem_print(level, x...)   \
	do {                                \
		if (shm_lowmem_debug_level >= (level)) \
			printk(x);                  \
	}while(0)

static int shm_task_notify_func(struct notifier_block* self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == shm_lowmem_deathpending)
		shm_lowmem_deathpending = NULL;

	return NOTIFY_OK;
}

static void shm_shrink_dump_usage(struct shm_stat_info *stat_info,int print_level)
{
	int i = 0 ;

	if (NULL == stat_info)
		return;

	shm_lowmem_print(print_level,"total size : %d \nused : %d \ntask count : %d\n",
			stat_info->m_size,stat_info->m_used,stat_info->m_count);

	shm_lowmem_print(print_level,"  No |  task id(      name      ) | oom_adj |    alloc   |     use    |\n");
	shm_lowmem_print(print_level,"-----------------------------------------------------------------------\n");

	for (i = 0 ; i < stat_info->m_count ; i++)
			shm_lowmem_print(print_level," %3d | %8d(%16s) |   %3d   | %10d | %10d |\n",
					i, stat_info->m_nodes[i].m_taskid,
					stat_info->m_nodes[i].m_taskname, stat_info->m_nodes[i].m_oom_adj,
					stat_info->m_nodes[i].m_size_alloc, stat_info->m_nodes[i].m_size_use);
}

static size_t shm_shrink_find_task_usage(struct shm_stat_info *stat_info, pid_t taskid)
{
	int i = 0 ;

	if (NULL == stat_info)
		return 0;

	for (i = 0 ; i < stat_info->m_count ; i++){
		struct shm_usr_node *usr_node = &(stat_info->m_nodes[i]);

		if (taskid == usr_node->m_taskid)
			return usr_node->m_size_use;
	}
	return 0;
}
/**
 * find the candidate task and release it
 * a)the oom-adj of task is more than shm_lowmem_start_oom_adj
 * b)the shm used size is more than shm_lowmem_min_release_kbyte * 1024
 * c)when meet the above two cond, then select the largest oom-adj task
 */
static int shm_lowmem_shrink_killer(int allocsize)
{
	int res = 0;
	struct task_struct *p;
	struct task_struct *selected = NULL;
	int selected_shmsize = 0;
	int selected_oom_adj = 0;

	/* check whether in shrinking or exits pending task */
	down(&shm_lowmem_mutex);
	if (shm_lowmem_going) {
		/* another shrinking is executing */
		up(&shm_lowmem_mutex);
		shm_lowmem_print(2,"shm_lowmem shrink is under going\n");
		return 0;
	} else {
		if (shm_lowmem_deathpending &&
				time_before_eq(jiffies,shm_lowmem_deathpending_timeout)) {
			/* no shrinking is executing, but exists pending task*/
			up(&shm_lowmem_mutex);
			shm_lowmem_print(2,"a pending task exists pid(%d)\n",shm_lowmem_deathpending->pid);
			return 0;
		}
		shm_lowmem_going = 1;
		up(&shm_lowmem_mutex);
	}

	/* the following is only access by one thread at one time */
	/* query current shm stat*/
	memset(&shm_stat, 0, sizeof(struct shm_stat_info));
	if (res != shm_device_get_stat(shm_device,&shm_stat)) {
		shm_lowmem_print(2,"shm_lowmemkiller fail to get shm stat info\n");
		goto _exit_entry;
	}

	/* find task whose oom-adj larger than
	 * and use largest shm memory size than allocsize
	 */
	read_lock(&tasklist_lock);
	for_each_process(p) {
		struct mm_struct *mm;
		struct signal_struct *sig;
		int oom_adj;
		size_t shmsize = 0;
		int tasksize;

		task_lock(p);
		mm = p->mm;
		sig = p->signal;
		if (!mm || !sig) {
			task_unlock(p);
			continue;
		}
		/* ignore task whose pid less than shm_lowmem_start_oom_adj */
		oom_adj = sig->oom_score_adj;
		if (oom_adj < shm_lowmem_start_oom_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(mm);
		task_unlock(p);

		shmsize = shm_shrink_find_task_usage(&shm_stat, p->pid);

		/* ignore precess has no tasksize */
		if (tasksize <= 0)
			continue;

		/* ignore process has little shmsize */
		if (shmsize <= (shm_lowmem_min_release_kbytes * 1024))
			continue;

		/* if shmsize larger than shm_lowmem_min_free_kbytes
		* select large oom-adj to release*/
		if (selected && (oom_adj <= selected_oom_adj))
				continue;

		selected = p;
		selected_shmsize = shmsize;
		selected_oom_adj = oom_adj;
		shm_lowmem_print(3, "shm_lowmem_killer select %d (%s),adj %d, shmsize %d to kill\n",
					p->pid,p->comm,oom_adj,shmsize);

	}
	if (selected) {
		shm_lowmem_print(2,"shm_lowmem_killer send sigkill to %d (%s), adj %d, shmsize %d\n",
				selected->pid, selected->comm,
				selected_oom_adj, selected_shmsize);

		force_sig(SIGTERM, selected);
	} else {
		shm_lowmem_print(2,"shm_lowmem_killer alert!!!! no task selected to release\n");
		shm_shrink_dump_usage(&shm_stat, 2);
	}

	read_unlock(&tasklist_lock);
	res = 0;

_exit_entry:

	down(&shm_lowmem_mutex);

	shm_lowmem_going = 0;
	if (selected) {
		shm_lowmem_deathpending = selected;
		shm_lowmem_deathpending_timeout = jiffies + HZ;
	}else
		shm_lowmem_deathpending = NULL;

	up(&shm_lowmem_mutex);

	return res;
}

static struct notifier_block shm_task_nb = {
	.notifier_call = shm_task_notify_func,
};

/*******************************************************************************
  Module API
*/

static int shm_driver_open(struct inode *inode, struct file *filp)
{
	/*save the device and opening taskid to private_data,
	 * then from user space we will depend on the specified device
	 * since there are cache and non-cache device */
	struct shm_device_priv_data *priv_data
				= kzalloc(sizeof(struct shm_device_priv_data), GFP_KERNEL);
	if (NULL == priv_data) {
		shm_error("shm_driver_open fail to allocate private data\n");
		return -ENOMEM;
	}

	priv_data->m_taskid = task_tgid_vnr(current);
	priv_data->m_device = shm_device;
	filp->private_data = priv_data;

	shm_debug("shm_driver_open ok\n");

	return 0;
}

static int shm_driver_open_noncache(struct inode *inode, struct file *filp)
{
	/*save the device and opening task id to private_data
	 * then from user space we will depend on the specified device
	 * since there are cache and non-cache device */
	struct shm_device_priv_data *priv_data
				= kzalloc(sizeof(struct shm_device_priv_data), GFP_KERNEL);
	if (NULL == priv_data) {
		shm_error("shm_driver_open_noncache fail to allocate memory\n");
		return -ENOMEM;
	}

	priv_data->m_taskid = task_tgid_vnr(current);
	priv_data->m_device = shm_device_noncache;
	filp->private_data = priv_data;

	shm_debug("shm_driver_open_noncache ok\n");

	return 0;
}

static int shm_driver_release(struct inode *inode, struct file *filp)
{
	shm_device_t *pDevice;
	pid_t taskid;
	struct shm_device_priv_data *priv_data
			= (struct shm_device_priv_data*)filp->private_data;

	if (NULL == priv_data) {
		shm_error("shm_driver_release private data is  NULL\n");
		return 0;
	}

	pDevice = priv_data->m_device;
	taskid = priv_data->m_taskid;
	/* free private data*/
	kfree(priv_data);
	filp->private_data = NULL;

	shm_debug("shm_driver_release for pid:%d\n",taskid);

	if (NULL == pDevice) {
		shm_error("shm_driver_release device NULL\n");
		return 0;
	}

	/* once the fd released, we should release allocated cache and noncache
	 * memory device accordingly to avoid shm leak */
	if ((pDevice == shm_device) || (pDevice == shm_device_noncache))
	{
		shm_device_release_by_taskid(pDevice, taskid);
	}
	shm_debug("after shm_driver_release OK for pid:%d\n",taskid);

	return 0;
}

static int shm_driver_mmap_noncache(struct file *filp, struct vm_area_struct *vma)
{
	shm_device_t *pDevice;
	unsigned long pfn, vsize;
	struct shm_device_priv_data *priv_data
				= (struct shm_device_priv_data*)filp->private_data;
	if (NULL == priv_data) {
		shm_error("shm_driver_mmap_noncache NULL private data\n");
		return -ENOTTY;
	}

	pDevice = priv_data->m_device;
	if (NULL == pDevice) {
		shm_error("shm_driver_mmap_noncache NULL shm device\n");
		return -ENOTTY;
	}

	pfn = pDevice->m_base >> PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;

	shm_debug("shm_driver_mmap_nocache size = 0x%08lX(0x%x, 0x%x), base:0x%x\n",
				vsize, shm_size_noncache, pDevice->m_size, pDevice->m_base);

	if (vsize > shm_size_noncache)
		return -EINVAL;

	vma->vm_pgoff = 0;	// skip offset
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);	// set memory non-cache

	if (remap_pfn_range(vma, vma->vm_start, pfn, vsize, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int shm_driver_mmap_cache(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long pfn, vsize;
	shm_device_t *pDevice;
	struct shm_device_priv_data *priv_data
			= (struct shm_device_priv_data*)filp->private_data;
	if (NULL == priv_data) {
		shm_error("shm_driver_mmap_cache NULL private data\n");
		return -ENOTTY;
	}

	pDevice = (shm_device_t*)priv_data->m_device;
	if (NULL == pDevice) {
		shm_error("shm_driver_mmap_cache NULL shm device\n");
		return -ENOTTY;
	}

	pfn = pDevice->m_base >> PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;

	shm_debug("shm_driver_mmap_nocache size = 0x%08lX(0x%x, 0x%x), base:0x%x\n",
				vsize, shm_size_cache, pDevice->m_size, pDevice->m_base);

	if (vsize > shm_size_cache)
		return -EINVAL;

	vma->vm_pgoff = 0;	// skip offset

	if (remap_pfn_range(vma, vma->vm_start, pfn, vsize, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static long shm_driver_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	int res = 0;
	shm_driver_operation_t op;
	shm_device_t *shm_dev;
	MV_SHM_MemInfo_t meminfo;
	MV_SHM_BaseInfo_t baseinfo;

	struct shm_device_priv_data *priv_data
				= (struct shm_device_priv_data*)filp->private_data;
	if (NULL == priv_data) {
		shm_error("shm_driver_ioctl NULL private data\n");
		return -ENOTTY;
	}

	shm_dev = (shm_device_t*)priv_data->m_device;
	if (NULL == shm_dev) {
		shm_error("shm_driver_ioctl NULL shm device\n");
		return -ENOTTY;
	}

	shm_debug("shm_driver_ioctl cmd = 0x%08x\n, base:0x%08X size:0x%08X\n",
				cmd, shm_dev->m_base, shm_dev->m_size);

	switch (cmd) {
	case SHM_DEVICE_CMD_GET_MEMINFO:
		{
			res = shm_device_get_meminfo(shm_dev, &meminfo);
			if (res == 0)
				res =
				    copy_to_user((void __user *)arg, &meminfo,
						 sizeof(meminfo));
			break;
		}

	case SHM_DEVICE_CMD_GET_DEVINFO:
		{
			res = shm_device_get_baseinfo(shm_dev, &baseinfo);
			if (res == 0)
				res =
				    copy_to_user((void __user *)arg, &baseinfo,
						 sizeof(baseinfo));
			break;
		}

	case SHM_DEVICE_CMD_ALLOCATE:
		{
			res =
			    copy_from_user(&op, (int __user *)arg, sizeof(op));
			if (res != 0)
				break;
			op.m_param1 =
			    shm_device_allocate(shm_dev, op.m_param1,
						op.m_param2);
			res = copy_to_user((void __user *)arg, &op, sizeof(op));

			break;
		}

	case SHM_DEVICE_CMD_FREE:
		{
			res =
			    copy_from_user(&op, (int __user *)arg, sizeof(op));
			if (res != 0)
				break;

			op.m_param1 =
				shm_device_free(shm_dev, op.m_param1);
			res = copy_to_user((void __user *)arg, &op, sizeof(op));

			break;
		}

	case SHM_DEVICE_CMD_INVALIDATE:
	case SHM_DEVICE_CMD_CLEAN:
	case SHM_DEVICE_CMD_CLEANANDINVALIDATE:
		{
			res =
			    copy_from_user(&op, (int __user *)arg, sizeof(op));
			if (res == 0) {
				res = shm_device_cache(shm_dev, cmd, op);
			}
			break;
		}

	default:

		res = -ENOTTY;
	}

	shm_debug("shm_driver_ioctl res = %d\n", res);

	return res;
}

static int meminfo_proc_show(struct seq_file *m, void *v)
{
	int res;
	MV_SHM_MemInfo_t meminfo;

	res = shm_device_get_meminfo(shm_device, &meminfo);
	if (res != 0) {
		shm_error("shm_driver_read_proc_meminfo failed. (%d)\n", res);
		return 0;
	}
	seq_printf(m, "cache memory information:\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Blocks\n"
		"%20s : %10u Blocks\n",
		"total mem", meminfo.m_totalmem,
		"free mem", meminfo.m_freemem,
		"used mem", meminfo.m_usedmem,
		"peak used mem", meminfo.m_peak_usedmem,
		"num free block", meminfo.m_num_freeblock,
		"num used block", meminfo.m_num_usedblock);
	res = shm_device_get_meminfo(shm_device_noncache, &meminfo);
	if (res != 0) {
		shm_error("shm_driver_read_proc_meminfo failed. (%d)\n", res);
		return 0;
	}
	seq_printf(m, "non-cache memory information:\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Blocks\n"
		"%20s : %10u Blocks\n",
		"total mem", meminfo.m_totalmem,
		"free mem", meminfo.m_freemem,
		"used mem", meminfo.m_usedmem,
		"peak used mem", meminfo.m_peak_usedmem,
		"num free block", meminfo.m_num_freeblock,
		"num used block", meminfo.m_num_usedblock);

	return 0;
}

static int meminfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, meminfo_proc_show, NULL);
}

static const struct file_operations meminfo_proc_fops = {
	.open		= meminfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int baseinfo_proc_show(struct seq_file *m, void *v)
{
	int res;
	MV_SHM_BaseInfo_t baseinfo;

	res = shm_device_get_baseinfo(shm_device, &baseinfo);
	if (res != 0) {
		shm_error("shm_device_get_baseinfo failed. (%d)\n", res);
		return 0;
	}

	seq_printf(m, "cache device base information:\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"------------ physical address:0x%08X\n",
		"memory size", baseinfo.m_size,
		"threshold", baseinfo.m_threshold,
		baseinfo.m_base_physaddr);

	res = shm_device_get_baseinfo(shm_device_noncache, &baseinfo);
	if (res != 0) {
		shm_error("shm_device_get_baseinfo failed. (%d)\n", res);
		return 0;
	}

	seq_printf(m, "non-cache device base information:\n"
		"%20s : %10u Bytes\n"
		"%20s : %10u Bytes\n"
		"------------ physical address:0x%08X\n",
		"memory size", baseinfo.m_size,
		"threshold", baseinfo.m_threshold,
		baseinfo.m_base_physaddr);

	return 0;
}

static int baseinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, baseinfo_proc_show, NULL);
}

static const struct file_operations baseinfo_proc_fops = {
	.open		= baseinfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int detail_seq_show(struct seq_file *file, void *data)
{
	shm_device_t *pShmdev = (shm_device_t *) data;
	if (pShmdev)
		shm_device_show_detail(pShmdev, file);

	return 0;
}

static void detail_seq_stop(struct seq_file *file, void *data)
{
}

static void *detail_seq_start(struct seq_file *file, loff_t * pos)
{
	if (*pos == 0)
		return (void *)shm_device;
	/* return non cache device for the second time,
	 * first start will return cache device.
	 */
	else if (*pos == 1)
		return (void *)shm_device_noncache;
	return NULL;
}

static void *detail_seq_next(struct seq_file *file, void *data, loff_t * pos)
{
	(*pos)++;
	/* return non cache device for the second time,
	 * first start will return cache device.
	 */
	if (*pos == 1)
		return (void *)shm_device_noncache;
	return NULL;
}

static struct seq_operations detail_seq_ops = {
	.start		= detail_seq_start,
	.next		= detail_seq_next,
	.stop		= detail_seq_stop,
	.show		= detail_seq_show
};

static int detail_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &detail_seq_ops);
}

static struct file_operations detail_proc_ops = {
	.open		= detail_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};

static int shm_stat_seq_show(struct seq_file *file, void *data)
{
	shm_device_t *pShmdev = (shm_device_t *) data;
	if (pShmdev)
		shm_device_show_stat(pShmdev, file);

	return 0;
}

static void shm_stat_seq_stop(struct seq_file *file, void *data)
{
}

static void *shm_stat_seq_start(struct seq_file *file, loff_t * pos)
{
	if (*pos == 0)
		return (void *)shm_device;
	return NULL;
}

static void *shm_stat_seq_next(struct seq_file *file, void *data, loff_t * pos)
{
	(*pos)++;
	/* return non cache device for the second time,
	 * first start will return cache device.
	 */
	if (*pos == 0)
		return (void *)shm_device;
	return NULL;
}

static struct seq_operations shm_stat_seq_ops = {
	.start		= shm_stat_seq_start,
	.next		= shm_stat_seq_next,
	.stop		= shm_stat_seq_stop,
	.show		= shm_stat_seq_show
};

static int shm_stat_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &shm_stat_seq_ops);
}

static struct file_operations shm_stat_file_ops = {
	.owner		= THIS_MODULE,
	.open		= shm_stat_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};

static int shm_driver_setup_cdev(struct cdev *dev, int major, int minor,
				 struct file_operations *fops)
{
	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	return cdev_add(dev, MKDEV(major, minor), 1);
}

static int __init shm_driver_init(void)
{
	int res, i;
	struct device_node *np;
	struct resource r;
	struct proc_dir_entry *pent = NULL;
	struct proc_dir_entry *pstat = NULL;

	np = of_find_compatible_node(NULL, NULL, "mrvl,berlin-shm");
	if (!np)
		goto err_node;
	res = of_address_to_resource(np, 0, &r);
	if (res)
		goto err_reg_device;
	shm_base_cache = r.start;
	shm_size_cache = resource_size(&r);
	res = of_address_to_resource(np, 1, &r);
	if (res)
		goto err_reg_device;
	shm_base_noncache = r.start;
	shm_size_noncache = resource_size(&r);
	of_node_put(np);
	/* Figure out our device number. */
	res =
	    register_chrdev_region(MKDEV(GALOIS_SHM_MAJOR, 0),
				   GALOIS_SHM_MINORS, SHM_DEVICE_NAME);
	if (res < 0) {
		shm_error("unable to get shm device major [%d]\n",
			  GALOIS_SHM_MAJOR);
		goto err_reg_device;
	}
	shm_debug("register cdev device major [%d]\n", GALOIS_SHM_MAJOR);

	/* Now setup cdevs. */
	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		res = shm_driver_setup_cdev(shm_driver_dev_list[i].cdev,
					    GALOIS_SHM_MAJOR,
					    shm_driver_dev_list[i].minor,
					    shm_driver_dev_list[i].fops);
		if (res) {
			shm_error("shm_driver_setup_cdev failed in [%d].\n", i);
			goto err_add_device;
		}
		shm_debug("setup cdevs device minor [%d]\n",
			  shm_driver_dev_list[i].minor);
	}

	/* add shm devices to sysfs */
	shm_dev_class = class_create(THIS_MODULE, SHM_DEVICE_NAME);
	if (IS_ERR(shm_dev_class)) {
		shm_error("class_create failed.\n");
		res = -ENODEV;
		goto err_add_device;
	}

	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		device_create(shm_dev_class, NULL,
			      MKDEV(GALOIS_SHM_MAJOR,
				    shm_driver_dev_list[i].minor), NULL,
			      shm_driver_dev_list[i].name);
		shm_debug("create device sysfs [%s]\n",
			  shm_driver_dev_list[i].name);
	}

	/* create shm cache device */
	res =
	    shm_device_create(&shm_device, shm_base_cache, shm_size_cache,
			      SHM_DEVICE_THRESHOLD);
	if (res != 0) {
		shm_error("shm_device_create failed.\n");
		goto err_add_device;
	}
	/* init shrinker */
	shm_device->m_shrinker = shm_lowmem_shrink_killer;
	/* create shm cache device */
	res = shm_device_create(&shm_device_noncache, shm_base_noncache,
			      shm_size_noncache, SHM_DEVICE_THRESHOLD);
	if (res != 0) {
		shm_error("shm_device_create failed.\n");
		goto err_add_device;
	}
	/* init shrinker */
	shm_device_noncache->m_shrinker = NULL;
	/* create shm kernel API, need map for noncache and cache device!!! */
	res = MV_SHM_Init(shm_device_noncache, shm_device);
	if (res != 0) {
		shm_error("MV_SHM_Init failed !!!\n");
		goto err_SHM_Init;
	}

	/* create shm device proc file */
	shm_driver_procdir = proc_mkdir(SHM_DEVICE_NAME, NULL);
	if (!shm_driver_procdir) {
		shm_error(KERN_WARNING "Failed to mkdir /proc/%s\n", SHM_DEVICE_NAME);
		return 0;
	}
	proc_create("meminfo", 0, shm_driver_procdir, &meminfo_proc_fops);
	proc_create("baseinfo", 0, shm_driver_procdir, &baseinfo_proc_fops);

	pent = create_proc_entry("detail", 0, shm_driver_procdir);
	if (pent)
		pent->proc_fops = &detail_proc_ops;

	pstat = create_proc_entry("stat", 0, shm_driver_procdir);
	if (pstat)
		pstat->proc_fops = &shm_stat_file_ops;

	task_free_register(&shm_task_nb);
	shm_trace("shm_driver_init OK\n");

	return 0;

 err_SHM_Init:

	shm_trace("shm_driver_init Undo ...\n");

	shm_device_destroy(&shm_device);
	shm_device_destroy(&shm_device_noncache);

	/* del sysfs entries */
	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		device_destroy(shm_dev_class,
			       MKDEV(GALOIS_SHM_MAJOR,
				     shm_driver_dev_list[i].minor));
		shm_debug("delete device sysfs [%s]\n",
			  shm_driver_dev_list[i].name);
	}
	class_destroy(shm_dev_class);

 err_add_device:

	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		cdev_del(shm_driver_dev_list[i].cdev);
	}
	unregister_chrdev_region(MKDEV(GALOIS_SHM_MAJOR, 0), GALOIS_SHM_MINORS);

err_reg_device:
	 of_node_put(np);
err_node:
	shm_trace("shm_driver_init failed !!! (%d)\n", res);

	return res;
}

static void __exit shm_driver_exit(void)
{
	int res, i;

	task_free_unregister(&shm_task_nb);

	/* destroy shm kernel API */
	res = MV_SHM_Exit();
	if (res != 0)
		shm_error("MV_SHM_Exit failed !!!\n");

	/* remove shm device proc file */
	remove_proc_entry("meminfo", shm_driver_procdir);
	remove_proc_entry("baseinfo", shm_driver_procdir);
	remove_proc_entry("detail", shm_driver_procdir);
	remove_proc_entry("stat", shm_driver_procdir);
	remove_proc_entry(SHM_DEVICE_NAME, NULL);

	if (shm_device_destroy(&shm_device) != 0)
		shm_error("shm_device_destroy cache mem failed.\n");
	if (shm_device_destroy(&shm_device_noncache) != 0)
		shm_error("shm_device_destroy non-cache mem failed.\n");

	/* del sysfs entries */
	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		device_destroy(shm_dev_class,
			       MKDEV(GALOIS_SHM_MAJOR,
				     shm_driver_dev_list[i].minor));
		shm_debug("delete device sysfs [%s]\n",
			  shm_driver_dev_list[i].name);
	}
	class_destroy(shm_dev_class);

	/* del cdev */
	for (i = 0; i < ARRAY_SIZE(shm_driver_dev_list); i++) {
		cdev_del(shm_driver_dev_list[i].cdev);
		shm_debug("delete cdevs device minor [%d]\n",
			  shm_driver_dev_list[i].minor);
	}

	unregister_chrdev_region(MKDEV(GALOIS_SHM_MAJOR, 0), GALOIS_SHM_MINORS);
	shm_debug("unregister cdev device major [%d]\n", GALOIS_SHM_MAJOR);

	shm_trace("shm_driver_exit OK\n");
}

module_init(shm_driver_init);
module_exit(shm_driver_exit);
