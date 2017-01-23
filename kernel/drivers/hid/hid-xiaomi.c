/*
 *  HID driver for XiaoMi game controller
 *  Based on hid-microsoft
 *
 *  Copyright (c) 2016 Juha Kuikka
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

struct xiaomi_data
{
	struct hid_device *hdev;
	struct work_struct ff_worker;
	__u8 strong;
	__u8 weak;
	__u8 *output_report_dmabuf;
};

struct xiaomi_ff_report
{
	__u8	report_id;
	__u8	weak;
	__u8	strong;
};

#define FF_REPORT		0x20

static void xiaomi_ff_worker(struct work_struct *work)
{
	struct xiaomi_data *xiaomi = container_of(work, struct xiaomi_data, ff_worker);
	struct hid_device *hdev = xiaomi->hdev;
	struct xiaomi_ff_report *r = (struct xiaomi_ff_report*) xiaomi->output_report_dmabuf;

	memset(r, 0, sizeof(*r));

	r->report_id = FF_REPORT;

	r->weak = xiaomi->weak;
	r->strong = xiaomi->strong;

	hdev->hid_output_raw_report(hdev, (__u8*) r, sizeof(*r),
				    HID_FEATURE_REPORT);
}

static int xiaomi_play_effect(struct input_dev *dev, void *data,
			    struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct xiaomi_data *xiaomi = hid_get_drvdata(hid);

	if (effect->type != FF_RUMBLE)
		return 0;

	xiaomi->strong = effect->u.rumble.strong_magnitude / 256;
	xiaomi->weak = effect->u.rumble.weak_magnitude / 256;

	schedule_work(&xiaomi->ff_worker);
	return 0;
}

static int xiaomi_initialize_ff(struct hid_device *hdev, struct xiaomi_data *xiaomi)
{
	struct hid_input *hidinput = list_entry(hdev->inputs.next,
						struct hid_input, list);
	struct input_dev *input_dev = hidinput->input;

	xiaomi->hdev = hdev;
	INIT_WORK(&xiaomi->ff_worker, xiaomi_ff_worker);

	xiaomi->output_report_dmabuf =
		kmalloc(sizeof(struct xiaomi_ff_report),
			GFP_KERNEL);

	if (xiaomi->output_report_dmabuf == NULL)
		return -ENOMEM;

	input_set_capability(input_dev, EV_FF, FF_RUMBLE);
	return input_ff_create_memless(input_dev, NULL, xiaomi_play_effect);
}

static int xiaomi_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct xiaomi_data *xiaomi;

	xiaomi = devm_kzalloc(&hdev->dev, sizeof(*xiaomi), GFP_KERNEL);
	if (xiaomi == NULL) {
		hid_err(hdev, "can't alloc data\n");
		return -ENOMEM;
	}

	hid_set_drvdata(hdev, xiaomi);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT );
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	ret = xiaomi_initialize_ff(hdev, xiaomi);
	if (ret) {
		hid_err(hdev, "could not initialize ff, continuing anyway");
	}

	return 0;

err_free:
	return ret;
}

static void xiaomi_remove(struct hid_device *hdev)
{
	struct xiaomi_data *xiaomi = hid_get_drvdata(hdev);

	hid_hw_stop(hdev);

	cancel_work_sync(&xiaomi->ff_worker);
	if (xiaomi->output_report_dmabuf)
		kfree(xiaomi->output_report_dmabuf);

	devm_kfree(&hdev->dev, xiaomi);
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id xiaomi_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_XIAOMI, USB_DEVICE_ID_XIAOMI_CONTROLLER),
		.driver_data = 0 },

	{ }
};
MODULE_DEVICE_TABLE(hid, xiaomi_devices);

static struct hid_driver xiaomi_driver = {
	.name = "xiaomi",
	.id_table = xiaomi_devices,
	.probe = xiaomi_probe,
	.remove = xiaomi_remove,
};

static int __init xiaomi_init(void)
{
	return hid_register_driver(&xiaomi_driver);
}

static void __exit xiaomi_exit(void)
{
	hid_unregister_driver(&xiaomi_driver);
}

module_init(xiaomi_init);
module_exit(xiaomi_exit);
MODULE_LICENSE("GPL");
