// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator_mac.h"

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace device {

namespace {

// Searches a service and all ancestor services for a property with the
// specified key, returning NULL if no such key was found.
CFTypeRef GetCFProperty(io_service_t service, const CFStringRef key) {
  // We search for the specified property not only on the specified service, but
  // all ancestors of that service. This is important because if a device is
  // both serial and USB, in the registry tree it appears as a serial service
  // with a USB service as its ancestor. Without searching ancestors services
  // for the specified property, we'd miss all USB properties.
  return IORegistryEntrySearchCFProperty(
      service, kIOServicePlane, key, NULL,
      kIORegistryIterateRecursively | kIORegistryIterateParents);
}

// Searches a service and all ancestor services for a string property with the
// specified key, returning NULL if no such key was found.
CFStringRef GetCFStringProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFStringGetTypeID()))
    return static_cast<CFStringRef>(value);

  return NULL;
}

// Searches a service and all ancestor services for a number property with the
// specified key, returning NULL if no such key was found.
CFNumberRef GetCFNumberProperty(io_service_t service, const CFStringRef key) {
  CFTypeRef value = GetCFProperty(service, key);
  if (value && (CFGetTypeID(value) == CFNumberGetTypeID()))
    return static_cast<CFNumberRef>(value);

  return NULL;
}

// Searches the specified service for a string property with the specified key,
// sets value to that property's value, and returns whether the operation was
// successful.
bool GetStringProperty(io_service_t service,
                       const CFStringRef key,
                       mojo::String* value) {
  CFStringRef propValue = GetCFStringProperty(service, key);
  if (propValue) {
    *value = base::SysCFStringRefToUTF8(propValue);
    return true;
  }

  return false;
}

// Searches the specified service for a uint16_t property with the specified
// key, sets value to that property's value, and returns whether the operation
// was successful.
bool GetUInt16Property(io_service_t service,
                       const CFStringRef key,
                       uint16_t* value) {
  CFNumberRef propValue = GetCFNumberProperty(service, key);
  if (propValue) {
    int intValue;
    if (CFNumberGetValue(propValue, kCFNumberIntType, &intValue)) {
      *value = static_cast<uint16_t>(intValue);
      return true;
    }
  }

  return false;
}

// Returns value clamped to the range of [min, max].
int Clamp(int value, int min, int max) {
  return std::min(std::max(value, min), max);
}

// Returns an array of devices as retrieved through the new method of
// enumerating serial devices (IOKit).  This new method gives more information
// about the devices than the old method.
mojo::Array<serial::DeviceInfoPtr> GetDevicesNew() {
  mojo::Array<serial::DeviceInfoPtr> devices;

  // Make a service query to find all serial devices.
  CFMutableDictionaryRef matchingDict =
      IOServiceMatching(kIOSerialBSDServiceValue);
  if (!matchingDict)
    return devices;

  io_iterator_t it;
  kern_return_t kr =
      IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &it);
  if (kr != KERN_SUCCESS)
    return devices;

  base::mac::ScopedIOObject<io_iterator_t> scoped_it(it);
  base::mac::ScopedIOObject<io_service_t> scoped_device;
  while (scoped_device.reset(IOIteratorNext(scoped_it.get())), scoped_device) {
    serial::DeviceInfoPtr callout_info(serial::DeviceInfo::New());

    uint16_t vendorId;
    if (GetUInt16Property(scoped_device.get(), CFSTR(kUSBVendorID),
                          &vendorId)) {
      callout_info->has_vendor_id = true;
      callout_info->vendor_id = vendorId;
    }

    uint16_t productId;
    if (GetUInt16Property(scoped_device.get(), CFSTR(kUSBProductID),
                          &productId)) {
      callout_info->has_product_id = true;
      callout_info->product_id = productId;
    }

    mojo::String displayName;
    if (GetStringProperty(scoped_device.get(), CFSTR(kUSBProductString),
                          &displayName)) {
      callout_info->display_name = displayName;
    }

    // Each serial device has two "paths" in /dev/ associated with it: a
    // "dialin" path starting with "tty" and a "callout" path starting with
    // "cu". Each of these is considered a different device from Chrome's
    // standpoint, but both should share the device's USB properties.
    mojo::String dialinDevice;
    if (GetStringProperty(scoped_device.get(), CFSTR(kIODialinDeviceKey),
                          &dialinDevice)) {
      serial::DeviceInfoPtr dialin_info = callout_info.Clone();
      dialin_info->path = dialinDevice;
      devices.push_back(std::move(dialin_info));
    }

    mojo::String calloutDevice;
    if (GetStringProperty(scoped_device.get(), CFSTR(kIOCalloutDeviceKey),
                          &calloutDevice)) {
      callout_info->path = calloutDevice;
      devices.push_back(std::move(callout_info));
    }
  }

  return devices;
}

// Returns an array of devices as retrieved through the old method of
// enumerating serial devices (pattern matching in /dev/). This old method gives
// less information about the devices than the new method.
mojo::Array<serial::DeviceInfoPtr> GetDevicesOld() {
  const base::FilePath kDevRoot("/dev");
  const int kFilesAndSymLinks =
      base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS;

  std::set<std::string> valid_patterns;
  valid_patterns.insert("/dev/*Bluetooth*");
  valid_patterns.insert("/dev/*Modem*");
  valid_patterns.insert("/dev/*bluetooth*");
  valid_patterns.insert("/dev/*modem*");
  valid_patterns.insert("/dev/*serial*");
  valid_patterns.insert("/dev/tty.*");
  valid_patterns.insert("/dev/cu.*");

  mojo::Array<serial::DeviceInfoPtr> devices;
  base::FileEnumerator enumerator(kDevRoot, false, kFilesAndSymLinks);
  do {
    const base::FilePath next_device_path(enumerator.Next());
    const std::string next_device = next_device_path.value();
    if (next_device.empty())
      break;

    std::set<std::string>::const_iterator i = valid_patterns.begin();
    for (; i != valid_patterns.end(); ++i) {
      if (base::MatchPattern(next_device, *i)) {
        serial::DeviceInfoPtr info(serial::DeviceInfo::New());
        info->path = next_device;
        devices.push_back(std::move(info));
        break;
      }
    }
  } while (true);
  return devices;
}

}  // namespace

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return std::unique_ptr<SerialDeviceEnumerator>(
      new SerialDeviceEnumeratorMac());
}

SerialDeviceEnumeratorMac::SerialDeviceEnumeratorMac() {}

SerialDeviceEnumeratorMac::~SerialDeviceEnumeratorMac() {}

mojo::Array<serial::DeviceInfoPtr> SerialDeviceEnumeratorMac::GetDevices() {
  mojo::Array<serial::DeviceInfoPtr> newDevices = GetDevicesNew();
  mojo::Array<serial::DeviceInfoPtr> oldDevices = GetDevicesOld();

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "Hardware.Serial.NewMinusOldDeviceListSize",
      Clamp(newDevices.size() - oldDevices.size(), -10, 10));

  // Add devices found from both the new and old methods of enumeration. If a
  // device is found using both the new and the old enumeration method, then we
  // take the device from the new enumeration method because it's able to
  // collect more information. We do this by inserting the new devices first,
  // because insertions are ignored if the key already exists.
  mojo::Map<mojo::String, serial::DeviceInfoPtr> deviceMap;
  for (unsigned long i = 0; i < newDevices.size(); i++) {
    deviceMap.insert(newDevices[i]->path, newDevices[i].Clone());
  }
  for (unsigned long i = 0; i < oldDevices.size(); i++) {
    deviceMap.insert(oldDevices[i]->path, oldDevices[i].Clone());
  }

  mojo::Array<mojo::String> paths;
  mojo::Array<serial::DeviceInfoPtr> devices;
  deviceMap.DecomposeMapTo(&paths, &devices);

  return devices;
}

}  // namespace device
