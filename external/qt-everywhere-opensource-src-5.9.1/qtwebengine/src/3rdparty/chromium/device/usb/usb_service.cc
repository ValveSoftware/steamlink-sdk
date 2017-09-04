// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device.h"

#if defined(OS_ANDROID)
#include "device/usb/usb_service_android.h"
#elif defined(USE_UDEV)
#include "device/usb/usb_service_linux.h"
#else
#include "device/usb/usb_service_impl.h"
#endif

namespace device {

UsbService::Observer::~Observer() {}

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::WillDestroyUsbService() {}

// static
std::unique_ptr<UsbService> UsbService::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
#if defined(OS_ANDROID)
  return base::WrapUnique(new UsbServiceAndroid(blocking_task_runner));
#elif defined(USE_UDEV)
  return base::WrapUnique(new UsbServiceLinux(blocking_task_runner));
#elif defined(OS_WIN) || defined(OS_MACOSX)
  return base::WrapUnique(new UsbServiceImpl(blocking_task_runner));
#else
  return nullptr;
#endif
}

UsbService::~UsbService() {
#if DCHECK_IS_ON()
  DCHECK(did_shutdown_);
#endif
  for (const auto& map_entry : devices_)
    map_entry.second->OnDisconnect();
  for (auto& observer : observer_list_)
    observer.WillDestroyUsbService();
}

UsbService::UsbService(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : task_runner_(task_runner), blocking_task_runner_(blocking_task_runner) {}

scoped_refptr<UsbDevice> UsbService::GetDevice(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return nullptr;
  return it->second;
}

void UsbService::Shutdown() {
#if DCHECK_IS_ON()
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
#endif
}

void UsbService::GetDevices(const GetDevicesCallback& callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  devices.reserve(devices_.size());
  for (const auto& map_entry : devices_)
    devices.push_back(map_entry.second);
  if (task_runner_)
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, devices));
  else
    callback.Run(devices);
}

void UsbService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void UsbService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void UsbService::AddDeviceForTesting(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(devices_, device->guid()));
  devices_[device->guid()] = device;
  testing_devices_.insert(device->guid());
  NotifyDeviceAdded(device);
}

void UsbService::RemoveDeviceForTesting(const std::string& device_guid) {
  DCHECK(CalledOnValidThread());
  // Allow only devices added with AddDeviceForTesting to be removed with this
  // method.
  auto testing_devices_it = testing_devices_.find(device_guid);
  if (testing_devices_it != testing_devices_.end()) {
    auto devices_it = devices_.find(device_guid);
    DCHECK(devices_it != devices_.end());
    scoped_refptr<UsbDevice> device = devices_it->second;
    devices_.erase(devices_it);
    testing_devices_.erase(testing_devices_it);
    UsbService::NotifyDeviceRemoved(device);
  }
}

void UsbService::GetTestDevices(
    std::vector<scoped_refptr<UsbDevice>>* devices) {
  devices->clear();
  devices->reserve(testing_devices_.size());
  for (const std::string& guid : testing_devices_) {
    auto it = devices_.find(guid);
    DCHECK(it != devices_.end());
    devices->push_back(it->second);
  }
}

void UsbService::NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
  device->NotifyDeviceRemoved();
  for (auto& observer : observer_list_)
    observer.OnDeviceRemovedCleanup(device);
}

}  // namespace device
