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
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>

#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>

#include <asm-generic/current.h>
#include <linux/sched.h>

/*******************************************************************************
  Local head files
  */

#include <mach/galois_generic.h>
#include"pe_agent_driver.h"
#include "cc_msgq.h"

/*******************************************************************************
  Module API defined
  */
#define PE_AGENT_DEVICE_NAME                      "galois_pe_agent"
#define PE_AGENT_DEVICE_TAG                       "[Galois][pe_agent_driver] "
#define PE_AGENT_DEVICE_PATH                      ("/dev/" PE_AGENT_DEVICE_NAME)

#define PE_AGENT_DEVICE_PROCFILE_STATUS           "status"

#define MAX_NUM_PE_AGENT_INSTANCES                (64)

#define PE_MODULE_MSG_TYPE_AGENT_TERM             (0x80000001)

#define PE_MODULE_MSG_ID_AGENT                    (0x00060011)	/*MV_GS_AGENT_Serv */

#define AGENT_IOCTL_SETDEVICETOKEN                (0xbeef5001)
#define AGENT_IOCTL_RECORD_HANDLE_MSG             (0xbeef5002)

static int pe_agent_driver_open(struct inode *inode, struct file *filp);
static int pe_agent_driver_release(struct inode *inode, struct file *filp);
static long pe_agent_driver_ioctl_unlocked(struct file *filp, unsigned int cmd,
					   unsigned long arg);

/*******************************************************************************
  Macro Defined
  */

#if 1
#define pe_agent_debug(...)   printk(KERN_DEBUG PE_AGENT_DEVICE_TAG __VA_ARGS__)
#else
#define pe_agent_debug(...)
#endif

#define pe_agent_trace(...)   printk(KERN_WARNING PE_AGENT_DEVICE_TAG __VA_ARGS__)
#define pe_agent_error(...)   printk(KERN_ERR PE_AGENT_DEVICE_TAG __VA_ARGS__)

/*******************************************************************************
  Module Variable
  */

static struct cdev pe_agent_dev;
static struct class *pe_agent_dev_class;

static struct file_operations pe_agent_ops = {
	.open = pe_agent_driver_open,
	.release = pe_agent_driver_release,
	.unlocked_ioctl = pe_agent_driver_ioctl_unlocked,
	.owner = THIS_MODULE,
};

typedef struct _InstanceInfo {
	HANDLE handle;		/*pe instance handle */
	long tgid;		/*tgid */
	struct file *filp;
	long token;		/*token betwen agent and server */
} InstanceInfo;

static InstanceInfo instance_list[MAX_NUM_PE_AGENT_INSTANCES];
static int pe_agent_instance_cnt;

static struct proc_dir_entry *pe_agent_driver_procdir;

static DEFINE_SPINLOCK(pe_agent_spinlock);

/*******************************************************************************
  Module API
  */

static int pe_agent_driver_open(struct inode *inode, struct file *filp)
{
	int i = 0;
	MV_CC_MSG_t msg = { 0, };

	spin_lock(&pe_agent_spinlock);

	if (pe_agent_instance_cnt < MAX_NUM_PE_AGENT_INSTANCES) {

		for (i = 0; i < MAX_NUM_PE_AGENT_INSTANCES; i++) {
			if (!instance_list[i].filp) {
				instance_list[i].filp = filp;	/*allocate this instance; */
				instance_list[i].tgid = current->tgid;
				instance_list[i].handle = 0;
				pe_agent_instance_cnt++;

				spin_unlock(&pe_agent_spinlock);
				pe_agent_debug("open filp:%x tgid:%d\n",
					       filp, instance_list[i].tgid);
				return 0;
			}
		}
	}

	spin_unlock(&pe_agent_spinlock);

	pe_agent_debug("pe agent instances:%d > %d, open failed\n",
		       pe_agent_instance_cnt, MAX_NUM_PE_AGENT_INSTANCES);
	return -1;
}

static int pe_agent_driver_release(struct inode *inode, struct file *filp)
{
	int i;
	MV_CC_MSG_t msg = { 0, };
	int res = 0;
	HANDLE pe_handle = 0;

	spin_lock(&pe_agent_spinlock);

	for (i = 0; i < MAX_NUM_PE_AGENT_INSTANCES; i++) {
		if (instance_list[i].filp == filp) {
			msg.m_MsgID = PE_MODULE_MSG_TYPE_AGENT_TERM;
			msg.m_Param1 = instance_list[i].handle;
			msg.m_Param2 = instance_list[i].tgid;

			pe_handle = instance_list[i].handle;

			instance_list[i].filp = 0;
			instance_list[i].handle = 0;
			instance_list[i].tgid = 0;
			instance_list[i].token = 0;
			pe_agent_instance_cnt--;

			spin_unlock(&pe_agent_spinlock);

			res =
			    MV_CC_MsgQ_PostMsgByID(PE_MODULE_MSG_ID_AGENT,
						   &msg);
			pe_agent_debug("close file:%x pid:%d handle:%x\n",
				       filp, current->tgid, (UINT) pe_handle);

			return 0;
		}
	}

	spin_unlock(&pe_agent_spinlock);

	return -1;
}

static long pe_agent_driver_ioctl_unlocked(struct file *filp, unsigned int cmd,
					   unsigned long arg)
{

	int i = 0;
	MV_CC_MSG_t msg = { 0, };
	int res = 0;

	switch (cmd) {
	case AGENT_IOCTL_SETDEVICETOKEN:
		spin_lock(&pe_agent_spinlock);

		for (i = 1; i < MAX_NUM_PE_AGENT_INSTANCES; i++) {
			if (instance_list[i].filp == filp) {
				instance_list[i].token = arg;

				spin_unlock(&pe_agent_spinlock);

				pe_agent_debug
				    ("filp:%x ioctl:set token:%08x tgid:%d\n",
				     filp, arg, instance_list[i].tgid);
				return 0;
			}
		}

		spin_unlock(&pe_agent_spinlock);
		res = -ENODEV;
		break;
	case AGENT_IOCTL_RECORD_HANDLE_MSG:
		if (copy_from_user
		    (&msg, (int __user *)arg, sizeof(MV_CC_MSG_t)))

			return -EFAULT;

		spin_lock(&pe_agent_spinlock);
		for (i = 0; i < MAX_NUM_PE_AGENT_INSTANCES; i++) {
			if (instance_list[i].filp
			    && instance_list[i].token == msg.m_MsgID) {
				instance_list[i].handle = msg.m_Param1;

				spin_unlock(&pe_agent_spinlock);
				pe_agent_debug
				    ("filp:%x ioctl:set pe inst:%08x tgid:%d token:%08x\n",
				     filp, instance_list[i].handle,
				     instance_list[i].tgid,
				     instance_list[i].token);
				return 0;
			}
		}

		spin_unlock(&pe_agent_spinlock);
		pe_agent_debug("not found token holder\n");
		res = -ENODEV;
		break;

	default:
		break;
	}

	return res;
}

static int read_proc_status(char *page, char **start, off_t offset,
			    int count, int *eof, void *data)
{
	int len = 0;
	int i = 0;

	len +=
	    sprintf(page + len, "pe_instance_cnt:%d\n", pe_agent_instance_cnt);

	len +=
	    sprintf(page + len,
		    "----------------------------------------------------------------\n");
	len +=
	    sprintf(page + len,
		    "  No  |    filp     |     tgid      |  PE handle   |  token\n");
	len +=
	    sprintf(page + len,
		    "----------------------------------------------------------------\n");

	for (i = 0; (i < MAX_NUM_PE_AGENT_INSTANCES) && (len < count - 200);
	     i++) {
		if (instance_list[i].filp) {
			len +=
			    sprintf(page + len,
				    " %2d  |  0x%08x  |     %08d  |  0x%08x  | 0x%08x\n",
				    i, instance_list[i].filp,
				    instance_list[i].tgid,
				    instance_list[i].handle,
				    instance_list[i].token);
		}

	}

	pe_agent_debug("%s OK. (%d / %d)\n", __func__, len, count);

	*eof = 1;

	return ((count < len) ? count : len);

}

/*******************************************************************************
  Module Register API
  */
static int pe_agent_driver_setup_cdev(struct cdev *dev, int major, int minor,
				      struct file_operations *fops)
{
	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	return cdev_add(dev, MKDEV(major, minor), 1);
}

int __init pe_agent_driver_init(void)
{
	int res;

	/* Now setup cdevs. */
	res =
	    pe_agent_driver_setup_cdev(&pe_agent_dev, GALOIS_PE_MAJOR,
				       GALOIS_PE_AGENT_MINOR, &pe_agent_ops);
	if (res) {
		pe_agent_error("pe_agent_driver_setup_cdev failed.\n");
		goto err_add_device;
	}
	pe_agent_debug("setup cdevs device minor [%d]\n",
		       GALOIS_PE_AGENT_MINOR);

	/* add PE agent devices to sysfs */
	pe_agent_dev_class = class_create(THIS_MODULE, PE_AGENT_DEVICE_NAME);
	if (IS_ERR(pe_agent_dev_class)) {
		pe_agent_error("class_create failed.\n");
		res = -ENODEV;
		goto err_add_device;
	}

	device_create(pe_agent_dev_class, NULL,
		      MKDEV(GALOIS_PE_MAJOR, GALOIS_PE_AGENT_MINOR),
		      NULL, PE_AGENT_DEVICE_NAME);
	pe_agent_debug("create device sysfs [%s]\n", PE_AGENT_DEVICE_NAME);

	/* create PE agent device proc file */
	pe_agent_driver_procdir = proc_mkdir(PE_AGENT_DEVICE_NAME, NULL);
	/*pe_driver_procdir->owner = THIS_MODULE; */
	create_proc_read_entry(PE_AGENT_DEVICE_PROCFILE_STATUS, 0,
			       pe_agent_driver_procdir, read_proc_status, NULL);

	pe_agent_instance_cnt = 0;

	pe_agent_trace("pe_agent_driver_init OK\n");

	return 0;

 err_add_device:

	cdev_del(&pe_agent_dev);

 err_reg_device:

	pe_agent_trace("pe_agent_driver_init failed !!! (%d)\n", res);

	return res;
}

void __exit pe_agent_driver_exit(void)
{
	int res;

	/* remove PE agent device proc file */
	remove_proc_entry(PE_AGENT_DEVICE_PROCFILE_STATUS,
			  pe_agent_driver_procdir);
	remove_proc_entry(PE_AGENT_DEVICE_NAME, NULL);

	/* del sysfs entries */
	device_destroy(pe_agent_dev_class,
		       MKDEV(GALOIS_PE_MAJOR, GALOIS_PE_AGENT_MINOR));
	pe_agent_debug("delete device sysfs [%s]\n", PE_AGENT_DEVICE_NAME);

	class_destroy(pe_agent_dev_class);

	/* del cdev */
	cdev_del(&pe_agent_dev);

	pe_agent_trace("pe_agent_driver_exit OK\n");
}

