// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"

#include <fcntl.h>
#include <linux/joystick.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_linux.h"

namespace {

const char kInputSubsystem[] = "input";
const char kUsbSubsystem[] = "usb";
const char kUsbDeviceType[] = "usb_device";
const float kMaxLinuxAxisValue = 32767.0;

void CloseFileDescriptorIfValid(int fd) {
  if (fd >= 0)
    close(fd);
}

bool IsGamepad(udev_device* dev, int* index, std::string* path) {
  if (!device::udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK"))
    return false;

  const char* node_path = device::udev_device_get_devnode(dev);
  if (!node_path)
    return false;

  static const char kJoystickRoot[] = "/dev/input/js";
  bool is_gamepad =
      base::StartsWith(node_path, kJoystickRoot, base::CompareCase::SENSITIVE);
  if (!is_gamepad)
    return false;

  int tmp_idx = -1;
  const int base_len = sizeof(kJoystickRoot) - 1;
  base::StringPiece str(&node_path[base_len], strlen(node_path) - base_len);
  if (!base::StringToInt(str, &tmp_idx))
    return false;
  if (tmp_idx < 0 ||
      tmp_idx >= static_cast<int>(blink::WebGamepads::itemsLengthCap)) {
    return false;
  }
  *index = tmp_idx;
  *path = node_path;
  return true;
}

}  // namespace

namespace device {

using blink::WebGamepad;
using blink::WebGamepads;

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux() {
  for (size_t i = 0; i < arraysize(device_fd_); ++i) {
    device_fd_[i] = -1;
  }
}

GamepadPlatformDataFetcherLinux::~GamepadPlatformDataFetcherLinux() {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i)
    CloseFileDescriptorIfValid(device_fd_[i]);
}

GamepadSource GamepadPlatformDataFetcherLinux::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherLinux::OnAddedToProvider() {
  std::vector<UdevLinux::UdevMonitorFilter> filters;
  filters.push_back(UdevLinux::UdevMonitorFilter(kInputSubsystem, NULL));
  udev_.reset(new UdevLinux(
      filters, base::Bind(&GamepadPlatformDataFetcherLinux::RefreshDevice,
                          base::Unretained(this))));

  EnumerateDevices();
}

void GamepadPlatformDataFetcherLinux::GetGamepadData(bool) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  // Update our internal state.
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (device_fd_[i] >= 0) {
      ReadDeviceData(i);
    }
  }
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  int index;
  std::string node_path;
  if (IsGamepad(dev, &index, &node_path)) {
    int& device_fd = device_fd_[index];

    CloseFileDescriptorIfValid(device_fd);

    // The device pointed to by dev contains information about the logical
    // joystick device. In order to get the information about the physical
    // hardware, get the parent device that is also in the "input" subsystem.
    // This function should just walk up the tree one level.
    dev = udev_device_get_parent_with_subsystem_devtype(dev, kInputSubsystem,
                                                        NULL);
    if (!dev) {
      // Unable to get device information, don't use this device.
      device_fd = -1;
      return;
    }

    device_fd = HANDLE_EINTR(open(node_path.c_str(), O_RDONLY | O_NONBLOCK));
    if (device_fd < 0) {
      // Unable to open device, don't use.
      return;
    }

    PadState* state = GetPadState(index);
    if (!state) {
      // No slot available for device, don't use.
      CloseFileDescriptorIfValid(device_fd);
      device_fd = -1;
      return;
    }

    WebGamepad& pad = state->data;
    GamepadStandardMappingFunction& mapper = state->mapper;

    const char* vendor_id = udev_device_get_sysattr_value(dev, "id/vendor");
    const char* product_id = udev_device_get_sysattr_value(dev, "id/product");
    mapper = GetGamepadStandardMappingFunction(vendor_id, product_id);

    // Driver returns utf-8 strings here, so combine in utf-8 first and
    // convert to WebUChar later once we've picked an id string.
    const char* name = udev_device_get_sysattr_value(dev, "name");
    std::string name_string(name);

    // In many cases the information the input subsystem contains isn't
    // as good as the information that the device bus has, walk up further
    // to the subsystem/device type "usb"/"usb_device" and if this device
    // has the same vendor/product id, prefer the description from that.
    struct udev_device* usb_dev = udev_device_get_parent_with_subsystem_devtype(
        dev, kUsbSubsystem, kUsbDeviceType);
    if (usb_dev) {
      const char* usb_vendor_id =
          udev_device_get_sysattr_value(usb_dev, "idVendor");
      const char* usb_product_id =
          udev_device_get_sysattr_value(usb_dev, "idProduct");

      if (strcmp(vendor_id, usb_vendor_id) == 0 &&
          strcmp(product_id, usb_product_id) == 0) {
        const char* manufacturer =
            udev_device_get_sysattr_value(usb_dev, "manufacturer");
        const char* product = udev_device_get_sysattr_value(usb_dev, "product");

        // Replace the previous name string with one containing the better
        // information, again driver returns utf-8 strings here so combine
        // in utf-8 for conversion to WebUChar below.
        name_string = base::StringPrintf("%s %s", manufacturer, product);
      }
    }

    // Append the vendor and product information then convert the utf-8
    // id string to WebUChar.
    std::string id =
        name_string + base::StringPrintf(" (%sVendor: %s Product: %s)",
                                         mapper ? "STANDARD GAMEPAD " : "",
                                         vendor_id, product_id);
    base::TruncateUTF8ToByteSize(id, WebGamepad::idLengthCap - 1, &id);
    base::string16 tmp16 = base::UTF8ToUTF16(id);
    memset(pad.id, 0, sizeof(pad.id));
    tmp16.copy(pad.id, arraysize(pad.id) - 1);

    if (mapper) {
      std::string mapping = "standard";
      base::TruncateUTF8ToByteSize(mapping, WebGamepad::mappingLengthCap - 1,
                                   &mapping);
      tmp16 = base::UTF8ToUTF16(mapping);
      memset(pad.mapping, 0, sizeof(pad.mapping));
      tmp16.copy(pad.mapping, arraysize(pad.mapping) - 1);
    } else {
      pad.mapping[0] = 0;
    }

    pad.connected = true;
  }
}

void GamepadPlatformDataFetcherLinux::EnumerateDevices() {
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_->udev_handle()));
  if (!enumerate)
    return;
  int ret =
      udev_enumerate_add_match_subsystem(enumerate.get(), kInputSubsystem);
  if (ret != 0)
    return;
  ret = udev_enumerate_scan_devices(enumerate.get());
  if (ret != 0)
    return;

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* dev_list_entry = devices; dev_list_entry != NULL;
       dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {
    // Get the filename of the /sys entry for the device and create a
    // udev_device object (dev) representing it
    const char* path = udev_list_entry_get_name(dev_list_entry);
    ScopedUdevDevicePtr dev(
        udev_device_new_from_syspath(udev_->udev_handle(), path));
    if (!dev)
      continue;
    RefreshDevice(dev.get());
  }
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
  // Linker does not like CHECK_LT(index, WebGamepads::itemsLengthCap). =/
  if (index >= WebGamepads::itemsLengthCap) {
    CHECK(false);
    return;
  }

  PadState* state = GetPadState(index);
  if (!state)
    return;

  int fd = device_fd_[index];
  DCHECK_GE(fd, 0);

  WebGamepad& pad = state->data;

  js_event event;
  while (HANDLE_EINTR(read(fd, &event, sizeof(struct js_event))) > 0) {
    size_t item = event.number;
    if (event.type & JS_EVENT_AXIS) {
      if (item >= WebGamepad::axesLengthCap)
        continue;

      pad.axes[item] = event.value / kMaxLinuxAxisValue;

      if (item >= pad.axesLength)
        pad.axesLength = item + 1;
    } else if (event.type & JS_EVENT_BUTTON) {
      if (item >= WebGamepad::buttonsLengthCap)
        continue;

      pad.buttons[item].pressed = event.value;
      pad.buttons[item].value = event.value ? 1.0 : 0.0;

      if (item >= pad.buttonsLength)
        pad.buttonsLength = item + 1;
    }
    pad.timestamp = event.time;
  }
}

}  // namespace device
