// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_linux.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/base/device_monitor_linux.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_device_linux.h"
#include "device/usb/webusb_descriptors.h"

namespace device {

namespace {

// Standard USB requests and descriptor types:
const uint16_t kUsbVersion2_1 = 0x0210;

const uint8_t kDeviceClassHub = 0x09;

void OnReadDescriptors(const base::Callback<void(bool)>& callback,
                       scoped_refptr<UsbDeviceHandle> device_handle,
                       std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
                       const GURL& landing_page) {
  UsbDeviceLinux* device =
      static_cast<UsbDeviceLinux*>(device_handle->GetDevice().get());

  if (allowed_origins)
    device->set_webusb_allowed_origins(std::move(allowed_origins));
  if (landing_page.is_valid())
    device->set_webusb_landing_page(landing_page);

  device_handle->Close();
  callback.Run(true /* success */);
}

void OnDeviceOpenedToReadDescriptors(
    const base::Callback<void(bool)>& callback,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    ReadWebUsbDescriptors(
        device_handle, base::Bind(&OnReadDescriptors, callback, device_handle));
  } else {
    callback.Run(false /* failure */);
  }
}

}  // namespace

class UsbServiceLinux::FileThreadHelper : public DeviceMonitorLinux::Observer {
 public:
  FileThreadHelper(base::WeakPtr<UsbServiceLinux> service,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~FileThreadHelper() override;

  void Start();

 private:
  // DeviceMonitorLinux::Observer:
  void OnDeviceAdded(udev_device* udev_device) override;
  void OnDeviceRemoved(udev_device* device) override;

  base::ThreadChecker thread_checker_;
  ScopedObserver<DeviceMonitorLinux, DeviceMonitorLinux::Observer> observer_;

  // |service_| can only be checked for validity on |task_runner_|'s thread.
  base::WeakPtr<UsbServiceLinux> service_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileThreadHelper);
};

UsbServiceLinux::FileThreadHelper::FileThreadHelper(
    base::WeakPtr<UsbServiceLinux> service,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : observer_(this), service_(service), task_runner_(std::move(task_runner)) {
  thread_checker_.DetachFromThread();
}

UsbServiceLinux::FileThreadHelper::~FileThreadHelper() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
void UsbServiceLinux::FileThreadHelper::Start() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());

  DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
  observer_.Add(monitor);
  monitor->Enumerate(
      base::Bind(&FileThreadHelper::OnDeviceAdded, base::Unretained(this)));
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&UsbServiceLinux::HelperStarted, service_));
}

void UsbServiceLinux::FileThreadHelper::OnDeviceAdded(
    udev_device* udev_device) {
  const char* subsystem = udev_device_get_subsystem(udev_device);
  if (!subsystem || strcmp(subsystem, "usb") != 0)
    return;

  const char* value = udev_device_get_devnode(udev_device);
  if (!value)
    return;
  std::string device_path = value;

  const char* sysfs_path = udev_device_get_syspath(udev_device);
  if (!sysfs_path)
    return;

  base::FilePath descriptors_path =
      base::FilePath(sysfs_path).Append("descriptors");
  std::string descriptors_str;
  if (!base::ReadFileToString(descriptors_path, &descriptors_str))
    return;

  UsbDeviceDescriptor descriptor;
  if (!descriptor.Parse(std::vector<uint8_t>(descriptors_str.begin(),
                                             descriptors_str.end()))) {
    return;
  }

  if (descriptor.device_class == kDeviceClassHub) {
    // Don't try to enumerate hubs. We never want to connect to a hub.
    return;
  }

  std::string manufacturer;
  value = udev_device_get_sysattr_value(udev_device, "manufacturer");
  if (value)
    manufacturer = value;

  std::string product;
  value = udev_device_get_sysattr_value(udev_device, "product");
  if (value)
    product = value;

  std::string serial_number;
  value = udev_device_get_sysattr_value(udev_device, "serial");
  if (value)
    serial_number = value;

  unsigned active_configuration = 0;
  value = udev_device_get_sysattr_value(udev_device, "bConfigurationValue");
  if (value)
    base::StringToUint(value, &active_configuration);

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbServiceLinux::OnDeviceAdded, service_,
                            device_path, descriptor, manufacturer, product,
                            serial_number, active_configuration));
}

void UsbServiceLinux::FileThreadHelper::OnDeviceRemoved(udev_device* device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const char* device_path = udev_device_get_devnode(device);
  if (device_path) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbServiceLinux::OnDeviceRemoved, service_,
                              std::string(device_path)));
  }
}

UsbServiceLinux::UsbServiceLinux(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_in)
    : UsbService(base::ThreadTaskRunnerHandle::Get(),
                 std::move(blocking_task_runner_in)),
      weak_factory_(this) {
  helper_ = base::MakeUnique<FileThreadHelper>(weak_factory_.GetWeakPtr(),
                                               task_runner());
  blocking_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&FileThreadHelper::Start, base::Unretained(helper_.get())));
}

UsbServiceLinux::~UsbServiceLinux() {
  DCHECK(!helper_);
}

void UsbServiceLinux::Shutdown() {
  const bool did_post_task =
      blocking_task_runner()->DeleteSoon(FROM_HERE, helper_.release());
  DCHECK(did_post_task);
  UsbService::Shutdown();
}

void UsbServiceLinux::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (enumeration_ready())
    UsbService::GetDevices(callback);
  else
    enumeration_callbacks_.push_back(callback);
}

void UsbServiceLinux::OnDeviceAdded(const std::string& device_path,
                                    const UsbDeviceDescriptor& descriptor,
                                    const std::string& manufacturer,
                                    const std::string& product,
                                    const std::string& serial_number,
                                    uint8_t active_configuration) {
  DCHECK(CalledOnValidThread());

  if (ContainsKey(devices_by_path_, device_path)) {
    USB_LOG(ERROR) << "Got duplicate add event for path: " << device_path;
    return;
  }

  // Devices that appear during initial enumeration are gathered into the first
  // result returned by GetDevices() and prevent device add/remove notifications
  // from being sent.
  if (!enumeration_ready())
    ++first_enumeration_countdown_;

  scoped_refptr<UsbDeviceLinux> device(new UsbDeviceLinux(
      device_path, descriptor, manufacturer, product, serial_number,
      active_configuration, blocking_task_runner()));
  devices_by_path_[device->device_path()] = device;
  if (device->usb_version() >= kUsbVersion2_1) {
    device->Open(base::Bind(&OnDeviceOpenedToReadDescriptors,
                            base::Bind(&UsbServiceLinux::DeviceReady,
                                       weak_factory_.GetWeakPtr(), device)));
  } else {
    DeviceReady(device, true /* success */);
  }
}

void UsbServiceLinux::DeviceReady(scoped_refptr<UsbDeviceLinux> device,
                                  bool success) {
  DCHECK(CalledOnValidThread());

  bool enumeration_became_ready = false;
  if (!enumeration_ready()) {
    DCHECK_GT(first_enumeration_countdown_, 0u);
    first_enumeration_countdown_--;
    if (enumeration_ready())
      enumeration_became_ready = true;
  }

  // If |device| was disconnected while descriptors were being read then it
  // will have been removed from |devices_by_path_|.
  auto it = devices_by_path_.find(device->device_path());
  if (it == devices_by_path_.end()) {
    success = false;
  } else if (success) {
    DCHECK(!base::ContainsKey(devices(), device->guid()));
    devices()[device->guid()] = device;

    USB_LOG(USER) << "USB device added: path=" << device->device_path()
                  << " vendor=" << device->vendor_id() << " \""
                  << device->manufacturer_string()
                  << "\", product=" << device->product_id() << " \""
                  << device->product_string() << "\", serial=\""
                  << device->serial_number() << "\", guid=" << device->guid();
  } else {
    devices_by_path_.erase(it);
  }

  if (enumeration_became_ready) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (const auto& callback : enumeration_callbacks_)
      callback.Run(result);
    enumeration_callbacks_.clear();
  } else if (success && enumeration_ready()) {
    NotifyDeviceAdded(device);
  }
}

void UsbServiceLinux::OnDeviceRemoved(const std::string& path) {
  DCHECK(CalledOnValidThread());
  auto by_path_it = devices_by_path_.find(path);
  if (by_path_it == devices_by_path_.end())
    return;

  scoped_refptr<UsbDeviceLinux> device = by_path_it->second;
  devices_by_path_.erase(by_path_it);
  device->OnDisconnect();

  auto by_guid_it = devices().find(device->guid());
  if (by_guid_it != devices().end() && enumeration_ready()) {
    USB_LOG(USER) << "USB device removed: path=" << device->device_path()
                  << " guid=" << device->guid();

    devices().erase(by_guid_it);
    NotifyDeviceRemoved(device);
  }
}

void UsbServiceLinux::HelperStarted() {
  DCHECK(CalledOnValidThread());
  helper_started_ = true;
  if (enumeration_ready()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (const auto& callback : enumeration_callbacks_)
      callback.Run(result);
    enumeration_callbacks_.clear();
  }
}

}  // namespace device
