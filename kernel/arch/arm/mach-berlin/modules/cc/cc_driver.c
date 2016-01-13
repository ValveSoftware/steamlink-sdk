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

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */

#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>

/*******************************************************************************
  Local head files
 */

#include <mach/galois_generic.h>
#include "cc_msgq.h"
#include "cc_dss.h"
#include "cc_udp.h"
#include "cc_cbuf.h"
#include "cc_driver.h"
#include "cc_error.h"


/*******************************************************************************
  Module Descrption
 */

MODULE_AUTHOR("Fang Bao");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Common Comunication Driver");

/*******************************************************************************
  Module API defined
 */

static int cc_driver_open (struct inode *inode, struct file *filp);
static int cc_driver_release(struct inode *inode, struct file *filp);
static long cc_driver_ioctl_unlocked(struct file *filp,
		unsigned int cmd, unsigned long arg);

/*******************************************************************************
  Macro Defined
 */

#ifdef ENABLE_DEBUG
#define cc_debug(...)   printk(KERN_INFO CC_DEVICE_TAG __VA_ARGS__)
#else
#define cc_debug(...)
#endif

#define cc_trace(...)      printk(KERN_ALERT CC_DEVICE_TAG __VA_ARGS__)
#define cc_error(...)      printk(KERN_ERR CC_DEVICE_TAG __VA_ARGS__)

/*******************************************************************************
  Module Variable
 */

//static cc_device_t *cc_device = NULL;

static struct cdev cc_dev;
static struct class *cc_dev_class;

static struct file_operations cc_ops = {
	.open    = cc_driver_open,
	.release = cc_driver_release,
	.unlocked_ioctl = cc_driver_ioctl_unlocked,
	.owner   = THIS_MODULE,
};

static struct proc_dir_entry *cc_driver_procdir;

char * tag_service[MV_CC_SrvType_Max + 1] = {
	"MsgQ",
	"RPC",
	"CBuf",
	"MsgQEx",
	"RPCClnt",
	"Unknown"
};

/*******************************************************************************
  Module API
 */

static int cc_driver_open (struct inode *inode, struct file *filp)
{
	struct task_struct *grptask = NULL;
	MV_CC_Task *cc_task;
	cc_task = kzalloc(sizeof(MV_CC_Task), GFP_KERNEL);
	if (cc_task == NULL) {
		cc_error("cc_driver_open kzalloc failed\n");
		return -1;
	}

	cc_task->cc_taskid = task_tgid_vnr(current);
	grptask = pid_task(task_tgid(current),PIDTYPE_PID);
	if (NULL != grptask) {
		strncpy(cc_task->cc_taskname,grptask->comm,16);
	}

	singlenode_init(&cc_task->serverid_head);
	singlenode_init(&cc_task->cbuf_head);

	filp->private_data = cc_task;
	cc_debug("cc_driver_open ok\n");

	return 0;
}

static int cc_driver_release(struct inode *inode, struct file *filp)
{
	MV_CC_Task *cc_task = (MV_CC_Task*)filp->private_data;
	if (NULL == cc_task) {
		cc_error("cc_driver_release private data is  NULL\n");
		return 0;
	}

	MV_CC_CBufSrv_Release_By_Taskid(cc_task);
	MV_CC_DSS_Release_By_Taskid(cc_task);

	kfree(cc_task);
	filp->private_data = NULL;
	cc_debug("cc_driver_release ok\n");

	return 0;
}

static long cc_driver_ioctl_unlocked(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	MV_CC_DSS_ServiceInfo_t SrvInfo;
	MV_CC_DSS_ServiceInfo_DataList_t    SrvInfoList;
	pMV_CC_DSS_ServiceInfo_DataList_t   pSrvInfoList;
	int res = 0;
	MV_CC_Task *cc_task = (MV_CC_Task*)filp->private_data;

	cc_debug("cc_driver_ioctl cmd = 0x%08x\n", cmd);

	switch(cmd) {

		case CC_DEVICE_CMD_REG:

			// DSS Reg

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_DSS_Reg(&SrvInfo,cc_task);

			if (copy_to_user((void __user *)arg, &SrvInfo, sizeof(SrvInfo)))
				return -EFAULT;

			break;

		case CC_DEVICE_CMD_FREE:

			// DSS Free

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_DSS_Free(&SrvInfo,cc_task);

			break;

		case CC_DEVICE_CMD_INQUIRY:

			// DSS Inquiry

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_DSS_Inquiry(&SrvInfo);

			if (copy_to_user((void __user *)arg, &SrvInfo, sizeof(SrvInfo)))
				return -EFAULT;

			break;

		case CC_DEVICE_CMD_GET_LIST:

			// DSS GetList

			if (copy_from_user(&SrvInfoList, (int __user *)arg, sizeof(MV_CC_DSS_ServiceInfo_DataList_t)))
				return -EFAULT;

			if (SrvInfoList.m_ListNum != 0)
			{
				pSrvInfoList = (pMV_CC_DSS_ServiceInfo_DataList_t)MV_OSAL_Malloc(SrvInfoList.m_ListNum * sizeof(MV_CC_DSS_ServiceInfo_t) + sizeof(MV_CC_DSS_ServiceInfo_DataList_t));
				if (pSrvInfoList == NULL)
					return -EFAULT;

				pSrvInfoList->m_ListNum = SrvInfoList.m_ListNum;
				pSrvInfoList->m_DataNum = 0;

				res = MV_CC_DSS_GetList(pSrvInfoList);

				if (copy_to_user((void __user *)arg, pSrvInfoList, (pSrvInfoList->m_DataNum * sizeof(MV_CC_DSS_ServiceInfo_t) + sizeof(MV_CC_DSS_ServiceInfo_DataList_t))))
				{
					MV_OSAL_Free(pSrvInfoList);
					return -EFAULT;
				}

				MV_OSAL_Free(pSrvInfoList);
			}
			else
				res = E_BADVALUE;

			break;

		case CC_DEVICE_CMD_TEST_MSG:
			// TEST MSG in Linux Kernel
			{
				MV_CC_MSG_t MSG;

				if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
					return -EFAULT;

				MSG.m_MsgID = SrvInfo.m_ServiceID;
				MSG.m_Param1 = SrvInfo.m_Data[0];
				MSG.m_Param2 = SrvInfo.m_Data[1];

				res = MV_CC_MsgQ_PostMsgByID(SrvInfo.m_ServiceID, &MSG);

				break;
			}

		case CC_DEVICE_CMD_UPDATE:

			// DSS Update

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_DSS_Update(&SrvInfo);

			break;

		case CC_DEVICE_CMD_CREATE_CBUF:

			// create CBuf

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_CBufSrv_Create((pMV_CC_DSS_ServiceInfo_CBuf_t)&SrvInfo, cc_task);

			if (copy_to_user((void __user *)arg, &SrvInfo, sizeof(SrvInfo)))
				return -EFAULT;

			break;

		case CC_DEVICE_CMD_DESTROY_CBUF:

			// destroy CBuf

			if (copy_from_user(&SrvInfo, (int __user *)arg, sizeof(SrvInfo)))
				return -EFAULT;

			res = MV_CC_CBufSrv_Destroy((pMV_CC_DSS_ServiceInfo_CBuf_t)&SrvInfo,cc_task);

			if (copy_to_user((void __user *)arg, &SrvInfo, sizeof(SrvInfo)))
				return -EFAULT;

			break;

		default:

			res = -ENOTTY;
	}

	cc_debug("cc_driver_ioctl res = 0x%08X\n", res);

	return res;
}

static int status_proc_show(struct seq_file *m, void *v)
{
	int res;
	MV_CC_DSS_Status_t status;

	res = MV_CC_DSS_GetStatus(&status);
	if (res != 0) {
		cc_error("read_proc_status failed. (0x%08X)\n", res);
		return 0;
	}

	seq_printf(m, "%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : %10u\n"
		"%-25s : 0x%08X\n"
		"%-25s : 0x%08X\n",
		"DSS Reg Count", status.m_RegCount,
		"DSS Reg Err Count", status.m_RegErrCount,
		"DSS Free Count", status.m_FreeCount,
		"DSS Free Err Count", status.m_FreeErrCount,
		"DSS Inquiry Count", status.m_InquiryCount,
		"DSS Inquiry Err Count", status.m_InquiryErrCount,
		"DSS Update Count", status.m_UpdateCount,
		"DSS Update Err Count", status.m_UpdateErrCount,
		"DSS Service Count", status.m_ServiceCount,
		"DSS Last Service ID", status.m_LastServiceID,
		"DSS Sequence ID", status.m_SeqID);

	return 0;
}

static int status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, status_proc_show, NULL);
}

static const struct file_operations status_proc_fops = {
	.open		= status_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void *detail_seq_start(struct seq_file *s, loff_t *pos)
{
	pMV_CC_DSS_ServiceInfo_DataList_t p;
	int res, list_num = CC_DEVICE_LIST_NUM;

	p = (pMV_CC_DSS_ServiceInfo_DataList_t)kmalloc(list_num * sizeof(MV_CC_DSS_ServiceInfo_t) + sizeof(MV_CC_DSS_ServiceInfo_DataList_t), GFP_KERNEL);
	if (!p)
		return NULL;

	p->m_ListNum = list_num;
	p->m_DataNum = 0;
	res = MV_CC_DSS_GetList(p);
	if (res != S_OK) {
		kfree(p);
		return NULL;
	}

	s->private = p;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	if ((unsigned long)(*pos - 1) >= p->m_DataNum)
		return NULL;
	return &(p->m_SrvInfo[*pos - 1]);
}

static void detail_seq_stop(struct seq_file *s, void *v)
{
	kfree(s->private);
}

static void *detail_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	pMV_CC_DSS_ServiceInfo_DataList_t p = s->private;

	if ((unsigned long)*pos >= p->m_DataNum)
		return NULL;
	return &(p->m_SrvInfo[(*pos)++]);
}

static int detail_seq_show(struct seq_file *s, void *v)
{
	char *ptr_tag;
	pMV_CC_DSS_ServiceInfo_t pi = v;
	pMV_CC_DSS_ServiceInfo_DataList_t p = s->private;

	if (v == SEQ_START_TOKEN) {
		seq_printf(s, "%-10s : %10u\n%-10s : %10u\n",
			"Data Num", p->m_DataNum,
			"Max Num", p->m_MaxNum);

		seq_printf(s, "\nGalois CC Service List\n"
"-----------------------------------------------------------------------------\n"
" Service ID |   Type  | Data[0]  Data[4]  Data[8]  Data[12] Data[16] Data[20]\n"
"-----------------------------------------------------------------------------\n");
		return 0;
	}


	if (pi->m_SrvType < MV_CC_SrvType_Max)
		ptr_tag = tag_service[pi->m_SrvType];
	else
		ptr_tag = tag_service[MV_CC_SrvType_Max];
	seq_printf(s, " 0x%08X | %7s | %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X \n",
		pi->m_ServiceID, ptr_tag,
		pi->m_Data[0], pi->m_Data[1], pi->m_Data[2], pi->m_Data[3],
		pi->m_Data[4], pi->m_Data[5], pi->m_Data[6], pi->m_Data[7],
		pi->m_Data[8], pi->m_Data[9], pi->m_Data[10], pi->m_Data[11],
		pi->m_Data[12], pi->m_Data[13], pi->m_Data[14], pi->m_Data[15],
		pi->m_Data[16], pi->m_Data[17], pi->m_Data[18], pi->m_Data[19],
		pi->m_Data[20], pi->m_Data[21], pi->m_Data[22], pi->m_Data[23]);

	return 0;
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

static const struct file_operations detail_proc_fops = {
	.open		= detail_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

/*******************************************************************************
  Module Register API
 */

static int cc_driver_setup_cdev(struct cdev *dev, int major, int minor,
		struct file_operations *fops)
{
	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	return cdev_add (dev, MKDEV(major, minor), 1);
}

static int __init cc_driver_init(void)
{
	int res;

	/* Figure out our device number. */
	res = register_chrdev_region(MKDEV(GALOIS_CCNEW_MAJOR, 0), GALOIS_CCNEW_MINORS, CC_DEVICE_NAME);
	if (res < 0) {
		cc_error("unable to get cc device major [%d]\n", GALOIS_CCNEW_MAJOR);
		goto err_reg_device;
	}
	cc_debug("register cdev device major [%d]\n", GALOIS_CCNEW_MAJOR);

	/* Now setup cdevs. */
	res = cc_driver_setup_cdev(&cc_dev, GALOIS_CCNEW_MAJOR, GALOIS_CCNEW_MINOR, &cc_ops);
	if (res) {
		cc_error("cc_driver_setup_cdev failed.\n");
		goto err_add_device;
	}
	cc_debug("setup cdevs device minor [%d]\n", GALOIS_CCNEW_MINOR);

	/* add cc devices to sysfs */
	cc_dev_class = class_create(THIS_MODULE, CC_DEVICE_NAME);
	if (IS_ERR(cc_dev_class)) {
		cc_error("class_create failed.\n");
		res = -ENODEV;
		goto err_add_device;
	}

	device_create(cc_dev_class, NULL,
			MKDEV(GALOIS_CCNEW_MAJOR, GALOIS_CCNEW_MINOR),
			NULL, CC_DEVICE_NAME);
	cc_debug("create device sysfs [%s]\n", CC_DEVICE_NAME);

	/* create cc device*/
	res = MV_CC_DSS_Init();
	if (res != 0)
		cc_error("MV_CC_DSS_Init failed !!! res = 0x%08X\n", res);

	/* create cc kernel API */
	res = MV_CC_UDP_Init();
	if (res != 0)
		cc_error("MV_CC_UDP_Init failed !!! res = 0x%08X\n", res);

	/* create cc device proc file*/
	cc_driver_procdir = proc_mkdir(CC_DEVICE_NAME, NULL);
	if (!cc_driver_procdir) {
		cc_error("Failed to mkdir /proc/%s\n", CC_DEVICE_NAME);
		return 0;
	}

	proc_create(CC_DEVICE_PROCFILE_STATUS, 0, cc_driver_procdir, &status_proc_fops);
	proc_create(CC_DEVICE_PROCFILE_DETAIL, 0, cc_driver_procdir, &detail_proc_fops);

	cc_trace("cc_driver_init OK\n");

	return 0;

err_add_device:

	cdev_del(&cc_dev);

	unregister_chrdev_region(MKDEV(GALOIS_CCNEW_MAJOR, 0), GALOIS_CCNEW_MINORS);

err_reg_device:

	cc_trace("cc_driver_init failed !!! (%d)\n", res);

	return res;
}

static void __exit cc_driver_exit(void)
{
	int res;

	/* destroy cc kernel API */
	res = MV_CC_UDP_Exit();
	if (res != 0)
		cc_error("MV_CC_UDP_Exit failed !!! res = 0x%08X\n", res);

	/* remove cc device proc file*/
	remove_proc_entry(CC_DEVICE_PROCFILE_DETAIL, cc_driver_procdir);
	remove_proc_entry(CC_DEVICE_PROCFILE_STATUS, cc_driver_procdir);
	remove_proc_entry(CC_DEVICE_NAME, NULL);

	/* destroy cc device*/
	res = MV_CC_DSS_Exit();
	if (res != 0)
		cc_error("MV_CC_DSS_Exit failed !!! res = 0x%08X\n", res);

	/* del sysfs entries */
	device_destroy(cc_dev_class, MKDEV(GALOIS_CCNEW_MAJOR, GALOIS_CCNEW_MINOR));
	cc_debug("delete device sysfs [%s]\n", CC_DEVICE_NAME);

	class_destroy(cc_dev_class);

	/* del cdev */
	cdev_del(&cc_dev);

	unregister_chrdev_region(MKDEV(GALOIS_CCNEW_MAJOR, 0), GALOIS_CCNEW_MINORS);
	cc_debug("unregister cdev device major [%d]\n", GALOIS_CCNEW_MAJOR);

	cc_trace("cc_driver_exit OK\n");
}

module_init(cc_driver_init);
module_exit(cc_driver_exit);
