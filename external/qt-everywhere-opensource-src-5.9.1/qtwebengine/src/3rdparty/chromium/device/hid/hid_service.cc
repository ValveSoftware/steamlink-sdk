// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "device/hid/hid_service_mac.h"
#elif defined(OS_WIN)
#include "device/hid/hid_service_win.h"
#endif

namespace device {

void HidService::Observer::OnDeviceAdded(
    scoped_refptr<HidDeviceInfo> device_info) {
}

void HidService::Observer::OnDeviceRemoved(
    scoped_refptr<HidDeviceInfo> device_info) {
}

void HidService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<HidDeviceInfo> device_info) {
}

std::unique_ptr<HidService> HidService::Create(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
#if defined(OS_LINUX) && defined(USE_UDEV)
  return base::WrapUnique(new HidServiceLinux(file_task_runner));
#elif defined(OS_MACOSX)
  return base::WrapUnique(new HidServiceMac(file_task_runner));
#elif defined(OS_WIN)
  return base::WrapUnique(new HidServiceWin(file_task_runner));
#else
  return nullptr;
#endif
}

void HidService::Shutdown() {
#if DCHECK_IS_ON()
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
#endif
}

void HidService::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enumeration_ready_) {
    std::vector<scoped_refptr<HidDeviceInfo>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, devices));
  } else {
    pending_enumerations_.push_back(callback);
  }
}

void HidService::AddObserver(HidService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HidService::RemoveObserver(HidService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// Fills in the device info struct of the given device_id.
scoped_refptr<HidDeviceInfo> HidService::GetDeviceInfo(
    const HidDeviceId& device_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::const_iterator it = devices_.find(device_id);
  if (it == devices_.end()) {
    return nullptr;
  }
  return it->second;
}

HidService::HidService() = default;

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
#if DCHECK_IS_ON()
  DCHECK(did_shutdown_);
#endif
}

void HidService::AddDevice(scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::ContainsKey(devices_, device_info->device_id())) {
    devices_[device_info->device_id()] = device_info;

    HID_LOG(USER) << "HID device "
                  << (enumeration_ready_ ? "added" : "detected")
                  << ": vendorId=" << device_info->vendor_id()
                  << ", productId=" << device_info->product_id() << ", name='"
                  << device_info->product_name() << "', serial='"
                  << device_info->serial_number() << "', deviceId='"
                  << device_info->device_id() << "'";

    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceAdded(device_info);
    }
  }
}

void HidService::RemoveDevice(const HidDeviceId& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceMap::iterator it = devices_.find(device_id);
  if (it != devices_.end()) {
    HID_LOG(USER) << "HID device removed: deviceId='" << device_id << "'";

    scoped_refptr<HidDeviceInfo> device = it->second;
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemoved(device);
    }
    devices_.erase(it);
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemovedCleanup(device);
    }
  }
}

void HidService::FirstEnumerationComplete() {
  enumeration_ready_ = true;

  if (!pending_enumerations_.empty()) {
    std::vector<scoped_refptr<HidDeviceInfo>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }

    for (const GetDevicesCallback& callback : pending_enumerations_) {
      callback.Run(devices);
    }
    pending_enumerations_.clear();
  }
}

}  // namespace device
