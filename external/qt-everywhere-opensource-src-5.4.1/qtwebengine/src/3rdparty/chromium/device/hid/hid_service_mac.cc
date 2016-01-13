// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection_mac.h"
#include "device/hid/hid_utils_mac.h"

namespace device {

class HidServiceMac;

namespace {

typedef std::vector<IOHIDDeviceRef> HidDeviceList;

HidServiceMac* HidServiceFromContext(void* context) {
  return static_cast<HidServiceMac*>(context);
}

// Callback for CFSetApplyFunction as used by EnumerateHidDevices.
void HidEnumerationBackInserter(const void* value, void* context) {
  HidDeviceList* devices = static_cast<HidDeviceList*>(context);
  const IOHIDDeviceRef device =
      static_cast<IOHIDDeviceRef>(const_cast<void*>(value));
  devices->push_back(device);
}

void EnumerateHidDevices(IOHIDManagerRef hid_manager,
                         HidDeviceList* device_list) {
  DCHECK(device_list->size() == 0);
  // Note that our ownership of each copied device is implied.
  base::ScopedCFTypeRef<CFSetRef> devices(IOHIDManagerCopyDevices(hid_manager));
  if (devices)
    CFSetApplyFunction(devices, HidEnumerationBackInserter, device_list);
}

}  // namespace

HidServiceMac::HidServiceMac() {
  DCHECK(thread_checker_.CalledOnValidThread());
  message_loop_ = base::MessageLoopProxy::current();
  DCHECK(message_loop_);
  hid_manager_.reset(IOHIDManagerCreate(NULL, 0));
  if (!hid_manager_) {
    LOG(ERROR) << "Failed to initialize HidManager";
    return;
  }
  DCHECK(CFGetTypeID(hid_manager_) == IOHIDManagerGetTypeID());
  IOHIDManagerOpen(hid_manager_, kIOHIDOptionsTypeNone);
  IOHIDManagerSetDeviceMatching(hid_manager_, NULL);

  // Enumerate all the currently known devices.
  Enumerate();

  // Register for plug/unplug notifications.
  StartWatchingDevices();
}

HidServiceMac::~HidServiceMac() {
  StopWatchingDevices();
}

void HidServiceMac::StartWatchingDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  IOHIDManagerRegisterDeviceMatchingCallback(
      hid_manager_, &AddDeviceCallback, this);
  IOHIDManagerRegisterDeviceRemovalCallback(
      hid_manager_, &RemoveDeviceCallback, this);
  IOHIDManagerScheduleWithRunLoop(
      hid_manager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
}

void HidServiceMac::StopWatchingDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!hid_manager_)
    return;
  IOHIDManagerUnscheduleFromRunLoop(
      hid_manager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_, kIOHIDOptionsTypeNone);
}

void HidServiceMac::AddDeviceCallback(void* context,
                                      IOReturn result,
                                      void* sender,
                                      IOHIDDeviceRef hid_device) {
  DCHECK(CFRunLoopGetMain() == CFRunLoopGetCurrent());
  // Claim ownership of the device.
  CFRetain(hid_device);
  HidServiceMac* service = HidServiceFromContext(context);
  service->message_loop_->PostTask(FROM_HERE,
                                   base::Bind(&HidServiceMac::PlatformAddDevice,
                                              base::Unretained(service),
                                              base::Unretained(hid_device)));
}

void HidServiceMac::RemoveDeviceCallback(void* context,
                                         IOReturn result,
                                         void* sender,
                                         IOHIDDeviceRef hid_device) {
  DCHECK(CFRunLoopGetMain() == CFRunLoopGetCurrent());
  HidServiceMac* service = HidServiceFromContext(context);
  service->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceMac::PlatformRemoveDevice,
                 base::Unretained(service),
                 base::Unretained(hid_device)));
}

void HidServiceMac::Enumerate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  HidDeviceList devices;
  EnumerateHidDevices(hid_manager_, &devices);
  for (HidDeviceList::const_iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    IOHIDDeviceRef hid_device = *iter;
    PlatformAddDevice(hid_device);
  }
}

void HidServiceMac::PlatformAddDevice(IOHIDDeviceRef hid_device) {
  // Note that our ownership of hid_device is implied if calling this method.
  // It is balanced in PlatformRemoveDevice.
  DCHECK(thread_checker_.CalledOnValidThread());

  HidDeviceInfo device_info;
  device_info.device_id = hid_device;
  device_info.vendor_id =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDVendorIDKey));
  device_info.product_id =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDProductIDKey));
  device_info.input_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxInputReportSizeKey));
  device_info.output_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxOutputReportSizeKey));
  device_info.feature_report_size =
      GetHidIntProperty(hid_device, CFSTR(kIOHIDMaxFeatureReportSizeKey));
  CFTypeRef deviceUsagePairsRaw =
      IOHIDDeviceGetProperty(hid_device, CFSTR(kIOHIDDeviceUsagePairsKey));
  CFArrayRef deviceUsagePairs =
      base::mac::CFCast<CFArrayRef>(deviceUsagePairsRaw);
  CFIndex deviceUsagePairsCount = CFArrayGetCount(deviceUsagePairs);
  for (CFIndex i = 0; i < deviceUsagePairsCount; i++) {
    CFDictionaryRef deviceUsagePair = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(deviceUsagePairs, i));
    CFNumberRef usage_raw = base::mac::CFCast<CFNumberRef>(
        CFDictionaryGetValue(deviceUsagePair, CFSTR(kIOHIDDeviceUsageKey)));
    uint16_t usage;
    CFNumberGetValue(usage_raw, kCFNumberSInt32Type, &usage);
    CFNumberRef page_raw = base::mac::CFCast<CFNumberRef>(
        CFDictionaryGetValue(deviceUsagePair, CFSTR(kIOHIDDeviceUsagePageKey)));
    HidUsageAndPage::Page page;
    CFNumberGetValue(page_raw, kCFNumberSInt32Type, &page);
    device_info.usages.push_back(HidUsageAndPage(usage, page));
  }
  device_info.product_name =
      GetHidStringProperty(hid_device, CFSTR(kIOHIDProductKey));
  device_info.serial_number =
      GetHidStringProperty(hid_device, CFSTR(kIOHIDSerialNumberKey));
  AddDevice(device_info);
}

void HidServiceMac::PlatformRemoveDevice(IOHIDDeviceRef hid_device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RemoveDevice(hid_device);
  CFRelease(hid_device);
}

scoped_refptr<HidConnection> HidServiceMac::Connect(
    const HidDeviceId& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HidDeviceInfo device_info;
  if (!GetDeviceInfo(device_id, &device_info))
    return NULL;
  return scoped_refptr<HidConnection>(new HidConnectionMac(device_info));
}

}  // namespace device
