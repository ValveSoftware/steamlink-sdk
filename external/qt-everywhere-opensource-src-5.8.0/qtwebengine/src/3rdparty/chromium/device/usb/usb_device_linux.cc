// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_linux.h"

#include <fcntl.h>
#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_usbfs.h"
#include "device/usb/usb_error.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "dbus/file_descriptor.h"  // nogncheck
#endif                             // defined(OS_CHROMEOS)

namespace device {

UsbDeviceLinux::UsbDeviceLinux(
    const std::string& device_path,
    const UsbDeviceDescriptor& descriptor,
    const std::string& manufacturer_string,
    const std::string& product_string,
    const std::string& serial_number,
    uint8_t active_configuration,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbDevice(descriptor.usb_version,
                descriptor.device_class,
                descriptor.device_subclass,
                descriptor.device_protocol,
                descriptor.vendor_id,
                descriptor.product_id,
                descriptor.device_version,
                base::UTF8ToUTF16(manufacturer_string),
                base::UTF8ToUTF16(product_string),
                base::UTF8ToUTF16(serial_number)),
      device_path_(device_path),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  configurations_ = descriptor.configurations;
  ActiveConfigurationChanged(active_configuration);
}

UsbDeviceLinux::~UsbDeviceLinux() {}

#if defined(OS_CHROMEOS)

void UsbDeviceLinux::CheckUsbAccess(const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->CheckPathAccess(device_path_, callback);
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceLinux::Open(const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->OpenPath(
      device_path_,
      base::Bind(&UsbDeviceLinux::OnOpenRequestComplete, this, callback),
      base::Bind(&UsbDeviceLinux::OnOpenRequestError, this, callback));
#else
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceLinux::OpenOnBlockingThread, this, callback));
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)

void UsbDeviceLinux::OnOpenRequestComplete(const OpenCallback& callback,
                                           dbus::FileDescriptor fd) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceLinux::OpenOnBlockingThreadWithFd, this,
                            base::Passed(&fd), callback));
}

void UsbDeviceLinux::OnOpenRequestError(const OpenCallback& callback,
                                        const std::string& error_name,
                                        const std::string& error_message) {
  USB_LOG(EVENT) << "Permission broker failed to open the device: "
                 << error_name << ": " << error_message;
  callback.Run(nullptr);
}

void UsbDeviceLinux::OpenOnBlockingThreadWithFd(dbus::FileDescriptor fd,
                                                const OpenCallback& callback) {
  fd.CheckValidity();
  if (fd.is_valid()) {
    base::ScopedFD scoped_fd(fd.TakeValue());
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&UsbDeviceLinux::Opened, this,
                                      base::Passed(&scoped_fd), callback));
  } else {
    USB_LOG(EVENT) << "Did not get valid device handle from permission broker.";
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

#else

void UsbDeviceLinux::OpenOnBlockingThread(const OpenCallback& callback) {
  base::ScopedFD fd(HANDLE_EINTR(open(device_path_.c_str(), O_RDWR)));
  if (fd.is_valid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(&UsbDeviceLinux::Opened, this,
                                                 base::Passed(&fd), callback));
  } else {
    USB_PLOG(EVENT) << "Failed to open " << device_path_;
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceLinux::Opened(base::ScopedFD fd, const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<UsbDeviceHandle> device_handle =
      new UsbDeviceHandleUsbfs(this, std::move(fd), blocking_task_runner_);
  handles().push_back(device_handle.get());
  callback.Run(device_handle);
}

}  // namespace device
