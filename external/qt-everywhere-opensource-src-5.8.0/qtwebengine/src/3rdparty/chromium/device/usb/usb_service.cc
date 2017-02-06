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
  for (const auto& map_entry : devices_)
    map_entry.second->OnDisconnect();
  FOR_EACH_OBSERVER(Observer, observer_list_, WillDestroyUsbService());
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

void UsbService::NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(device));
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(device));
  device->NotifyDeviceRemoved();
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemovedCleanup(device));
}

}  // namespace device
