// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_report_descriptor.h"
#include "device/hid/hid_service_linux.h"
#include "device/udev_linux/udev.h"

namespace device {

namespace {

const char kHIDSubSystem[] = "hid";
const char kHidrawSubsystem[] = "hidraw";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";

}  // namespace

HidServiceLinux::HidServiceLinux() {
  DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
  monitor->AddObserver(this);
  monitor->Enumerate(
      base::Bind(&HidServiceLinux::OnDeviceAdded, base::Unretained(this)));
}

scoped_refptr<HidConnection> HidServiceLinux::Connect(
    const HidDeviceId& device_id) {
  HidDeviceInfo device_info;
  if (!GetDeviceInfo(device_id, &device_info))
    return NULL;

  ScopedUdevDevicePtr device =
      DeviceMonitorLinux::GetInstance()->GetDeviceFromPath(
          device_info.device_id);

  if (device) {
    std::string dev_node;
    if (!FindHidrawDevNode(device.get(), &dev_node)) {
      LOG(ERROR) << "Cannot open HID device as hidraw device.";
      return NULL;
    }
    return new HidConnectionLinux(device_info, dev_node);
  }

  return NULL;
}

HidServiceLinux::~HidServiceLinux() {
  if (DeviceMonitorLinux::HasInstance())
    DeviceMonitorLinux::GetInstance()->RemoveObserver(this);
}

void HidServiceLinux::OnDeviceAdded(udev_device* device) {
  if (!device)
    return;

  const char* device_path = udev_device_get_syspath(device);
  if (!device_path)
    return;
  const char* subsystem = udev_device_get_subsystem(device);
  if (!subsystem || strcmp(subsystem, kHIDSubSystem) != 0)
    return;

  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  uint32_t int_property = 0;
  const char* str_property = NULL;

  const char* hid_id = udev_device_get_property_value(device, kHIDID);
  if (!hid_id)
    return;

  std::vector<std::string> parts;
  base::SplitString(hid_id, ':', &parts);
  if (parts.size() != 3) {
    return;
  }

  if (HexStringToUInt(base::StringPiece(parts[1]), &int_property)) {
    device_info.vendor_id = int_property;
  }

  if (HexStringToUInt(base::StringPiece(parts[2]), &int_property)) {
    device_info.product_id = int_property;
  }

  str_property = udev_device_get_property_value(device, kHIDUnique);
  if (str_property != NULL)
    device_info.serial_number = str_property;

  str_property = udev_device_get_property_value(device, kHIDName);
  if (str_property != NULL)
    device_info.product_name = str_property;

  std::string dev_node;
  if (!FindHidrawDevNode(device, &dev_node)) {
    LOG(ERROR) << "Cannot find device node for HID device.";
    return;
  }

  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

  base::File device_file(base::FilePath(dev_node), flags);
  if (!device_file.IsValid()) {
    LOG(ERROR) << "Cannot open '" << dev_node << "': "
        << base::File::ErrorToString(device_file.error_details());
    return;
  }

  int desc_size = 0;
  int res = ioctl(device_file.GetPlatformFile(), HIDIOCGRDESCSIZE, &desc_size);
  if (res < 0) {
    LOG(ERROR) << "HIDIOCGRDESCSIZE failed.";
    device_file.Close();
    return;
  }

  hidraw_report_descriptor rpt_desc;
  rpt_desc.size = desc_size;

  res = ioctl(device_file.GetPlatformFile(), HIDIOCGRDESC, &rpt_desc);
  if (res < 0) {
    LOG(ERROR) << "HIDIOCGRDESC failed.";
    device_file.Close();
    return;
  }

  device_file.Close();

  HidReportDescriptor report_descriptor(rpt_desc.value, rpt_desc.size);
  report_descriptor.GetTopLevelCollections(&device_info.usages);

  AddDevice(device_info);
}

void HidServiceLinux::OnDeviceRemoved(udev_device* device) {
  const char* device_path = udev_device_get_syspath(device);;
  if (device_path)
    RemoveDevice(device_path);
}

bool HidServiceLinux::FindHidrawDevNode(udev_device* parent,
                                        std::string* result) {
  udev* udev = udev_device_get_udev(parent);
  if (!udev) {
    return false;
  }
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  if (!enumerate) {
    return false;
  }
  if (udev_enumerate_add_match_subsystem(enumerate.get(), kHidrawSubsystem)) {
    return false;
  }
  if (udev_enumerate_scan_devices(enumerate.get())) {
    return false;
  }
  std::string parent_path(udev_device_get_devpath(parent));
  if (parent_path.length() == 0 || *parent_path.rbegin() != '/')
    parent_path += '/';
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr hid_dev(
        udev_device_new_from_syspath(udev, udev_list_entry_get_name(i)));
    const char* raw_path = udev_device_get_devnode(hid_dev.get());
    std::string device_path = udev_device_get_devpath(hid_dev.get());
    if (raw_path &&
        !device_path.compare(0, parent_path.length(), parent_path)) {
      std::string sub_path = device_path.substr(parent_path.length());
      if (sub_path.substr(0, sizeof(kHidrawSubsystem) - 1) ==
          kHidrawSubsystem) {
        *result = raw_path;
        return true;
      }
    }
  }

  return false;
}

}  // namespace device
