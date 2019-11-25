/*
 *  HID driver for some microsoft "special" devices
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2006-2007 Jiri Kosina
 *  Copyright (c) 2008 Jiri Slaby
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

#define MS_HIDINPUT		0x01
#define MS_ERGONOMY		0x02
#define MS_PRESENTER		0x04
#define MS_RDESC		0x08
#define MS_NOGET		0x10
#define MS_DUPLICATE_USAGES	0x20
#define MS_RDESC_3K		0x40

struct ms_data
{
	unsigned long quirks;
#ifdef CONFIG_MICROSOFT_FF
	struct hid_device *hdev;
	struct work_struct ff_worker;
	__u8 strong;
	__u8 weak;
	__u8 *output_report_dmabuf;
#endif
};

#ifdef CONFIG_MICROSOFT_FF
struct xb1s_ff_report
{
	__u8	report_id;
	__u8	enable;
	__u8	magnitude[4];
	__u8	duration_10ms;
	__u8	start_delay_10ms;
	__u8	loop_count;
};
#endif

static __u8 *ms_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	struct ms_data *ms = hid_get_drvdata(hdev);
	unsigned long quirks = ms->quirks;

	/*
	 * Microsoft Wireless Desktop Receiver (Model 1028) has
	 * 'Usage Min/Max' where it ought to have 'Physical Min/Max'
	 */
	if ((quirks & MS_RDESC) && *rsize == 571 && rdesc[557] == 0x19 &&
			rdesc[559] == 0x29) {
		hid_info(hdev, "fixing up Microsoft Wireless Receiver Model 1028 report descriptor\n");
		rdesc[557] = 0x35;
		rdesc[559] = 0x45;
	}
	/* the same as above (s/usage/physical/) */
	if ((quirks & MS_RDESC_3K) && *rsize == 106 && rdesc[94] == 0x19 &&
			rdesc[95] == 0x00 && rdesc[96] == 0x29 &&
			rdesc[97] == 0xff) {
		rdesc[94] = 0x35;
		rdesc[96] = 0x45;
	}
	return rdesc;
}

#define ms_map_key_clear(c)	hid_map_usage_clear(hi, usage, bit, max, \
					EV_KEY, (c))
static int ms_ergonomy_kb_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct input_dev *input = hi->input;

	switch (usage->hid & HID_USAGE) {
	case 0xfd06: ms_map_key_clear(KEY_CHAT);	break;
	case 0xfd07: ms_map_key_clear(KEY_PHONE);	break;
	case 0xff05:
		set_bit(EV_REP, input->evbit);
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F14, input->keybit);
		set_bit(KEY_F15, input->keybit);
		set_bit(KEY_F16, input->keybit);
		set_bit(KEY_F17, input->keybit);
		set_bit(KEY_F18, input->keybit);
	default:
		return 0;
	}
	return 1;
}

static int ms_presenter_8k_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	set_bit(EV_REP, hi->input->evbit);
	switch (usage->hid & HID_USAGE) {
	case 0xfd08: ms_map_key_clear(KEY_FORWARD);	break;
	case 0xfd09: ms_map_key_clear(KEY_BACK);	break;
	case 0xfd0b: ms_map_key_clear(KEY_PLAYPAUSE);	break;
	case 0xfd0e: ms_map_key_clear(KEY_CLOSE);	break;
	case 0xfd0f: ms_map_key_clear(KEY_PLAY);	break;
	default:
		return 0;
	}
	return 1;
}

static int ms_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct ms_data *ms = hid_get_drvdata(hdev);
	unsigned long quirks = ms->quirks;

	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_MSVENDOR)
		return 0;

	if (quirks & MS_ERGONOMY) {
		int ret = ms_ergonomy_kb_quirk(hi, usage, bit, max);
		if (ret)
			return ret;
	}

	if ((quirks & MS_PRESENTER) &&
			ms_presenter_8k_quirk(hi, usage, bit, max))
		return 1;

	return 0;
}

static int ms_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct ms_data *ms = hid_get_drvdata(hdev);
	unsigned long quirks = ms->quirks;

	if (quirks & MS_DUPLICATE_USAGES)
		clear_bit(usage->code, *bit);

	return 0;
}

static int ms_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	struct ms_data *ms = hid_get_drvdata(hdev);
	unsigned long quirks = ms->quirks;

	if (!(hdev->claimed & HID_CLAIMED_INPUT) || !field->hidinput ||
			!usage->type)
		return 0;

	/* Handling MS keyboards special buttons */
	if (quirks & MS_ERGONOMY && usage->hid == (HID_UP_MSVENDOR | 0xff05)) {
		struct input_dev *input = field->hidinput->input;
		static unsigned int last_key = 0;
		unsigned int key = 0;
		switch (value) {
		case 0x01: key = KEY_F14; break;
		case 0x02: key = KEY_F15; break;
		case 0x04: key = KEY_F16; break;
		case 0x08: key = KEY_F17; break;
		case 0x10: key = KEY_F18; break;
		}
		if (key) {
			input_event(input, usage->type, key, 1);
			last_key = key;
		} else
			input_event(input, usage->type, last_key, 0);

		return 1;
	}

	return 0;
}

#ifdef CONFIG_MICROSOFT_FF

#define XB1S_FF_REPORT		3
#define ENABLE_WEAK		(1 << 0)
#define ENABLE_STRONG		(1 << 1)
#define MAGNITUDE_WEAK		3
#define MAGNITUDE_STRONG	2

static void ms_ff_worker(struct work_struct *work)
{
	struct ms_data *ms = container_of(work, struct ms_data, ff_worker);
	struct hid_device *hdev = ms->hdev;
	struct xb1s_ff_report *r = (struct xb1s_ff_report*) ms->output_report_dmabuf;

	memset(r, 0, sizeof(*r));

	r->report_id = XB1S_FF_REPORT;

	r->enable = ENABLE_WEAK | ENABLE_STRONG;
	r->duration_10ms = 0xFF; /* Always max duration */
	r->magnitude[MAGNITUDE_STRONG] = ms->strong;
	r->magnitude[MAGNITUDE_WEAK] = ms->weak;

	hdev->hid_output_raw_report(hdev, (__u8*) r, sizeof(*r),
				    HID_OUTPUT_REPORT);
}

static int ms_play_effect(struct input_dev *dev, void *data,
			    struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct ms_data *ms = hid_get_drvdata(hid);

	if (effect->type != FF_RUMBLE)
		return 0;

	/* Magnitude is 0..100 so scale the 16-bit input here */
	ms->strong = effect->u.rumble.strong_magnitude / 655;
	ms->weak = effect->u.rumble.weak_magnitude / 655;

	schedule_work(&ms->ff_worker);
	return 0;
}

static int ms_initialize_ff(struct hid_device *hdev, struct ms_data *ms)
{
	struct hid_input *hidinput = list_entry(hdev->inputs.next,
						struct hid_input, list);
	struct input_dev *input_dev = hidinput->input;

	ms->hdev = hdev;
	INIT_WORK(&ms->ff_worker, ms_ff_worker);

	ms->output_report_dmabuf =
		kmalloc(sizeof(struct xb1s_ff_report),
			GFP_KERNEL);

	if (ms->output_report_dmabuf == NULL)
		return -ENOMEM;

	input_set_capability(input_dev, EV_FF, FF_RUMBLE);
	return input_ff_create_memless(input_dev, NULL, ms_play_effect);
}
#endif

static int ms_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	unsigned long quirks = id->driver_data;
	int ret;
	struct ms_data *ms;

	ms = devm_kzalloc(&hdev->dev, sizeof(*ms), GFP_KERNEL);
	if (ms == NULL) {
		hid_err(hdev, "can't alloc ms data\n");
		return -ENOMEM;
	}
	ms->quirks = quirks;

	hid_set_drvdata(hdev, ms);

	if (quirks & MS_NOGET)
		hdev->quirks |= HID_QUIRK_NOGET;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT | ((quirks & MS_HIDINPUT) ?
				HID_CONNECT_HIDINPUT_FORCE : 0));
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

#ifdef CONFIG_MICROSOFT_FF
	if (hdev->product == USB_DEVICE_ID_MS_XBOX_ONE_S_CONTROLLER ||
	    hdev->product == USB_DEVICE_ID_MS_XBOX_ONE_ELITE_SERIES_2_CONTROLLER) {
		ret = ms_initialize_ff(hdev, ms);
		if (ret) {
			hid_err(hdev, "could not initialize ff, continuing anyway");
		}
	}
#endif

	return 0;
err_free:
	return ret;
}

static void ms_remove(struct hid_device *hdev)
{
	struct ms_data *ms = hid_get_drvdata(hdev);

	hid_hw_stop(hdev);

#ifdef CONFIG_MICROSOFT_FF
	if (hdev->product == USB_DEVICE_ID_MS_XBOX_ONE_S_CONTROLLER ||
	    hdev->product == USB_DEVICE_ID_MS_XBOX_ONE_ELITE_SERIES_2_CONTROLLER) {
		cancel_work_sync(&ms->ff_worker);
		if (ms->output_report_dmabuf)
			kfree(ms->output_report_dmabuf);
	}
#endif

	devm_kfree(&hdev->dev, ms);
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id ms_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_SIDEWINDER_GV),
		.driver_data = MS_HIDINPUT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_NE4K),
		.driver_data = MS_ERGONOMY },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_LK6K),
		.driver_data = MS_ERGONOMY | MS_RDESC },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_PRESENTER_8K_USB),
		.driver_data = MS_PRESENTER },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_DIGITAL_MEDIA_3K),
		.driver_data = MS_ERGONOMY | MS_RDESC_3K },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_WIRELESS_OPTICAL_DESKTOP_3_0),
		.driver_data = MS_NOGET },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_COMFORT_MOUSE_4500),
		.driver_data = MS_DUPLICATE_USAGES },

	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_PRESENTER_8K_BT),
		.driver_data = MS_PRESENTER },
#ifdef CONFIG_MICROSOFT_FF
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_XBOX_ONE_S_CONTROLLER),
		.driver_data = 0 },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_XBOX_ONE_ELITE_SERIES_2_CONTROLLER),
		.driver_data = 0 },
#endif

	{ }
};
MODULE_DEVICE_TABLE(hid, ms_devices);

static struct hid_driver ms_driver = {
	.name = "microsoft",
	.id_table = ms_devices,
	.report_fixup = ms_report_fixup,
	.input_mapping = ms_input_mapping,
	.input_mapped = ms_input_mapped,
	.event = ms_event,
	.probe = ms_probe,
	.remove = ms_remove,
};

static int __init ms_init(void)
{
	return hid_register_driver(&ms_driver);
}

static void __exit ms_exit(void)
{
	hid_unregister_driver(&ms_driver);
}

module_init(ms_init);
module_exit(ms_exit);
MODULE_LICENSE("GPL");
