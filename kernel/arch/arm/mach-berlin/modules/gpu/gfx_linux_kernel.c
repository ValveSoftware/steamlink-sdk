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

#include <linux/device.h>
#include <linux/platform_device.h>

#include "PreComp.h"
#include "gfx_linux_common.h"
#include "gfx_linux_kernel.h"

MODULE_DESCRIPTION("Marvell Graphics Driver");
MODULE_LICENSE("GPL");

static struct class *gpuClass;

static gcoGALDEVICE galDevice;

int galPrint = 0; // 0:1:2:3=none:fatal:warn:info
module_param(galPrint, int, 0644);

static int major = 201;
module_param(major, int, 0644);

//int irqLine = 0x06;//copy from ECOS, maybe 0x0f?
static int irqLine = 38; //BG2 GFX irqLine is 38
module_param(irqLine, int, 0644);

static ulong registerMemBase = 0xf7ef0000;// According to Berlin ECOS
module_param(registerMemBase, long, 0644);

ulong registerMemSize = 64 << 10;// Need change?
module_param(registerMemSize, ulong, 0644);

int signal = 48;
module_param(signal, int, 0644);

ulong baseAddress = 0;
module_param(baseAddress, ulong, 0644);

static int drv_open(struct inode *inode, struct file *filp);
static int drv_release(struct inode *inode, struct file *filp);
static long drv_ioctl(struct file* filp, unsigned int ioctlCode, unsigned long arg);

struct file_operations driver_fops =
{
    .open           = drv_open,
    .release        = drv_release,
    .unlocked_ioctl = drv_ioctl,
};

int drv_open(struct inode *inode, struct file* filp)
{
    gcsHAL_PRIVATE_DATA_PTR	private;

    gcmTRACE_ZONE("Entering drv_open\n");

    private = kmalloc(sizeof(gcsHAL_PRIVATE_DATA), GFP_KERNEL);

    if (private == gcvNULL)
    {
        return -ENOTTY;
    }

    private->device				= galDevice;

    filp->private_data = private;

    return 0;
}

int drv_release(struct inode* inode, struct file* filp)
{
    gcsHAL_PRIVATE_DATA_PTR	private;
    gcoGALDEVICE			device;

    gcmTRACE_ZONE("Entering drv_close\n");

    private = filp->private_data;
    if (private == gcvNULL)
        return	gcvSTATUS_INVALID_ARGUMENT;
    device = private->device;

    kfree(private);
    filp->private_data = NULL;

    return 0;
}

long drv_ioctl(struct file *filp, unsigned int ioctlCode, unsigned long arg)
{
    gcsHAL_INTERFACE interface;
    gctUINT32 copyLen;
    DRIVER_ARGS drvArgs;
    gcoGALDEVICE device;
    gceSTATUS status;
    gcsHAL_PRIVATE_DATA_PTR private;

    private = filp->private_data;

    if (private == gcvNULL)
    {
        gcmTRACE_ZONE("[galcore] drv_ioctl: private_data is NULL\n");

        return -ENOTTY;
    }

    device = private->device;

    if (device == gcvNULL)
    {
        gcmTRACE_ZONE("[galcore] drv_ioctl: device is NULL\n");

        return -ENOTTY;
    }

    if (ioctlCode != IOCTL_GCHAL_INTERFACE
            && ioctlCode != IOCTL_GCHAL_KERNEL_INTERFACE)
    {
        /* Unknown command. Fail the I/O. */
        return -ENOTTY;
    }

    /* Get the drvArgs to begin with. */
    copyLen = copy_from_user(&drvArgs,
                             (void *) arg,
                             sizeof(DRIVER_ARGS));

    if (copyLen != 0)
    {
        /* The input buffer is not big enough. So fail the I/O. */
        return -ENOTTY;
    }

    /* Now bring in the AQHAL_INTERFACE structure. */
    if ((drvArgs.InputBufferSize  != sizeof(gcsHAL_INTERFACE))
            ||  (drvArgs.OutputBufferSize != sizeof(gcsHAL_INTERFACE))
       )
    {
        return -ENOTTY;
    }

    copyLen = copy_from_user(&interface,
                             drvArgs.InputBuffer,
                             sizeof(gcsHAL_INTERFACE));

    if (copyLen != 0)
    {
        /* The input buffer is not big enough. So fail the I/O. */
        return -ENOTTY;
    }

    status = gcoKERNEL_Dispatch(device,
                                (ioctlCode == IOCTL_GCHAL_INTERFACE) , &interface);

    if (gcmIS_ERROR(status))
    {
        gcmTRACE_ZONE("[galcore] gcoKERNEL_Dispatch returned %d.\n", status);
    }

    else if (gcmIS_ERROR(interface.status))
    {
        gcmTRACE_ZONE("[galcore] IOCTL %d returned %d.\n",
                      interface.command,
                      interface.status);
    }

    /* Copy data back to the user. */
    copyLen = copy_to_user(drvArgs.OutputBuffer,
                           &interface,
                           sizeof(gcsHAL_INTERFACE));

    if (copyLen != 0)
    {
        /* The output buffer is not big enough. So fail the I/O. */
        return -ENOTTY;
    }

    return 0;
}

static int drv_init(void)
{
    int ret;
    gcoGALDEVICE device;

    gcmTRACE_ZONE("Entering drv_init\n");

    if (galPrint)
    {
        printk("Berlin2D: irqLine=%d, reg.base=%lxh, major=%d\n",
               irqLine, registerMemBase, major);
    }
    /* Create the GAL device. */
    gcoGALDEVICE_Construct(irqLine,
                           registerMemBase,
                           registerMemSize,
                           baseAddress,
                           signal,
                           &device);

    /* Start the GAL device. */
    if (gcmIS_ERROR(gcoGALDEVICE_Start(device)))
    {
        gcmTRACE_ZONE("[galcore] Can't start the gal device.\n");

        /* Roll back. */
        gcoGALDEVICE_Stop(device);
        gcoGALDEVICE_Destroy(device);

        return -1;
    }

    /* Register the character device. */
    ret = register_chrdev(major, DRV_NAME, &driver_fops);
    if (ret < 0)
    {
        gcmTRACE_ZONE("[galcore] Could not allocate major number for mmap.\n");

        /* Roll back. */
        gcoGALDEVICE_Stop(device);
        gcoGALDEVICE_Destroy(device);

        return -1;
    }
    else
    {
        if (major == 0)
        {
            major = ret;
        }
    }

    galDevice = device;

    gpuClass = class_create(THIS_MODULE, "graphics_class");
    if (IS_ERR(gpuClass))
    {
        gcmTRACE_ZONE("Failed to create the class.\n");
        return -1;
    }

    device_create(gpuClass, NULL, MKDEV(major, 0), NULL, "galcore");

    gcmTRACE_ZONE("[galcore] driver registered successfully.\n");

    return 0;
}

static void drv_exit(void)
{
    gcmTRACE_ZONE("[galcore] Entering drv_exit\n");

    device_destroy(gpuClass, MKDEV(major, 0));
    class_destroy(gpuClass);

    unregister_chrdev(major, DRV_NAME);

    gcoGALDEVICE_Stop(galDevice);
    gcoGALDEVICE_Destroy(galDevice);
}

#define DEVICE_NAME "galcore"


static int __devinit gpu_probe(struct platform_device *pdev)
{
    int ret = -ENODEV;
    struct resource *res;
    if (galPrint > 2)
        printk("[halcore]%s(%d)\n", __FUNCTION__, __LINE__);

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "gpu_irq");
    if (!res)
    {
        if (galPrint)
            printk("%s: No irq line supplied.\n", __FUNCTION__);
        goto gpu_probe_fail;
    }
    irqLine = res->start;
    if (galPrint > 2)
        printk("[halcore]%s(%d)\n", __FUNCTION__, __LINE__);

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_base");
    if (!res)
    {
        if (galPrint)
            printk("%s: No register base supplied.\n", __FUNCTION__);
        goto gpu_probe_fail;
    }
    registerMemBase = res->start;
    registerMemSize = res->end - res->start;
    if (galPrint > 2)
        printk("[halcore]%s(%d)\n", __FUNCTION__, __LINE__);

    ret = drv_init();
    if (!ret)
    {
        platform_set_drvdata(pdev,galDevice);
    }

    return ret;

gpu_probe_fail:
    if (galPrint)
        printk("Failed to register gpu driver.\n");
    return ret;
}

static int __devexit gpu_remove(struct platform_device *pdev)
{
    drv_exit();
    platform_set_drvdata(pdev, NULL);

    return 0;
}

static int gpu_suspend(struct platform_device *dev, pm_message_t state)
{
    gceSTATUS status;
    gcoGALDEVICE device;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    status = gcoGALDEVICE_Stop(device);

    if (galPrint)
        printk("gpu_suspend return %d\n", status);
    return status;
}

static int gpu_resume(struct platform_device *dev)
{
    gceSTATUS status;
    gcoGALDEVICE device;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    status = gcoGALDEVICE_Start(device);

    if (galPrint)
        printk("gpu_resume return %d\n", status);

    return status;
}

static struct platform_driver gpu_driver =
{
    .probe    = gpu_probe,
    .remove   = __devexit_p(gpu_remove),
    .suspend  = gpu_suspend,
    .resume   = gpu_resume,
    .driver   =
    {
        .name	= DEVICE_NAME,
    }
};

static struct resource gpu_resources[] =
{
    {
        .name   = "gpu_irq",
        .flags  = IORESOURCE_IRQ,
                        },
    {
        .name   = "gpu_base",
        .flags  = IORESOURCE_MEM,
    },
};

static struct platform_device * gpu_device;

static int __init gpu_init(void)
{
    int ret = 0;

    gpu_resources[0].start = gpu_resources[0].end = irqLine;

    gpu_resources[1].start = registerMemBase;
    gpu_resources[1].end   = registerMemBase + registerMemSize - 1;

    /* Allocate device */
    gpu_device = platform_device_alloc(DEVICE_NAME, -1);
    if (!gpu_device)
    {
        if (galPrint)
            printk("galcore: platform_device_alloc failed.\n");
        ret = -ENOMEM;
        goto out;
    }

    /* Insert resource */
    ret = platform_device_add_resources(gpu_device, gpu_resources, 2);
    if (ret)
    {
        if (galPrint)
            printk("galcore: platform_device_add_resources failed.\n");
        goto put_dev;
    }

    /* Add device */
    ret = platform_device_add(gpu_device);
    if (ret)
    {
        if (galPrint)
            printk(KERN_ERR "galcore: platform_device_add failed. ret 0x%x\n", ret);
        goto del_dev;
    }

    ret = platform_driver_register(&gpu_driver);
    if (!ret)
    {
        goto out;
    }

del_dev:
    platform_device_del(gpu_device);
put_dev:
    platform_device_put(gpu_device);

out:
    return ret;

}

static void __exit gpu_exit(void)
{
    platform_driver_unregister(&gpu_driver);
    platform_device_unregister(gpu_device);
}

module_init(gpu_init);
module_exit(gpu_exit);

