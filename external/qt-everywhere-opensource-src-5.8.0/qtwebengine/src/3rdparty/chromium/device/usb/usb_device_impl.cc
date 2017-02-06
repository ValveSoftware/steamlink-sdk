// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_impl.h"

#include <fcntl.h>
#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_impl.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

namespace {

void ConvertConfigDescriptor(const libusb_config_descriptor* platform_config,
                             UsbConfigDescriptor* configuration) {
  for (size_t i = 0; i < platform_config->bNumInterfaces; ++i) {
    const struct libusb_interface* platform_interface =
        &platform_config->interface[i];
    for (int j = 0; j < platform_interface->num_altsetting; ++j) {
      const struct libusb_interface_descriptor* platform_alt_setting =
          &platform_interface->altsetting[j];
      UsbInterfaceDescriptor interface(
          platform_alt_setting->bInterfaceNumber,
          platform_alt_setting->bAlternateSetting,
          platform_alt_setting->bInterfaceClass,
          platform_alt_setting->bInterfaceSubClass,
          platform_alt_setting->bInterfaceProtocol);

      interface.endpoints.reserve(platform_alt_setting->bNumEndpoints);
      for (size_t k = 0; k < platform_alt_setting->bNumEndpoints; ++k) {
        const struct libusb_endpoint_descriptor* platform_endpoint =
            &platform_alt_setting->endpoint[k];
        UsbEndpointDescriptor endpoint(platform_endpoint->bEndpointAddress,
                                       platform_endpoint->bmAttributes,
                                       platform_endpoint->wMaxPacketSize,
                                       platform_endpoint->bInterval);
        endpoint.extra_data.assign(
            platform_endpoint->extra,
            platform_endpoint->extra + platform_endpoint->extra_length);

        interface.endpoints.push_back(endpoint);
      }

      interface.extra_data.assign(
          platform_alt_setting->extra,
          platform_alt_setting->extra + platform_alt_setting->extra_length);

      configuration->interfaces.push_back(interface);
    }
  }

  configuration->extra_data.assign(
      platform_config->extra,
      platform_config->extra + platform_config->extra_length);

  configuration->AssignFirstInterfaceNumbers();
}

}  // namespace

UsbDeviceImpl::UsbDeviceImpl(
    scoped_refptr<UsbContext> context,
    PlatformUsbDevice platform_device,
    const libusb_device_descriptor& descriptor,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbDevice(descriptor.bcdUSB,
                descriptor.bDeviceClass,
                descriptor.bDeviceSubClass,
                descriptor.bDeviceProtocol,
                descriptor.idVendor,
                descriptor.idProduct,
                descriptor.bcdDevice,
                base::string16(),
                base::string16(),
                base::string16()),
      platform_device_(platform_device),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);
  ReadAllConfigurations();
  RefreshActiveConfiguration();
}

UsbDeviceImpl::~UsbDeviceImpl() {
  // The destructor must be safe to call from any thread.
  libusb_unref_device(platform_device_);
}

void UsbDeviceImpl::Open(const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceImpl::OpenOnBlockingThread, this, callback));
}

void UsbDeviceImpl::ReadAllConfigurations() {
  libusb_device_descriptor device_descriptor;
  int rv = libusb_get_device_descriptor(platform_device_, &device_descriptor);
  if (rv == LIBUSB_SUCCESS) {
    uint8_t num_configurations = device_descriptor.bNumConfigurations;
    configurations_.reserve(num_configurations);
    for (uint8_t i = 0; i < num_configurations; ++i) {
      libusb_config_descriptor* platform_config;
      rv = libusb_get_config_descriptor(platform_device_, i, &platform_config);
      if (rv != LIBUSB_SUCCESS) {
        USB_LOG(EVENT) << "Failed to get config descriptor: "
                       << ConvertPlatformUsbErrorToString(rv);
        continue;
      }

      UsbConfigDescriptor config_descriptor(
          platform_config->bConfigurationValue,
          (platform_config->bmAttributes & 0x40) != 0,
          (platform_config->bmAttributes & 0x20) != 0,
          platform_config->MaxPower * 2);
      ConvertConfigDescriptor(platform_config, &config_descriptor);
      configurations_.push_back(config_descriptor);
      libusb_free_config_descriptor(platform_config);
    }
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
}

void UsbDeviceImpl::RefreshActiveConfiguration() {
  libusb_config_descriptor* platform_config;
  int rv =
      libusb_get_active_config_descriptor(platform_device_, &platform_config);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to get config descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }

  ActiveConfigurationChanged(platform_config->bConfigurationValue);
  libusb_free_config_descriptor(platform_config);
}

void UsbDeviceImpl::OpenOnBlockingThread(const OpenCallback& callback) {
  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceImpl::Opened, this, handle, callback));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

void UsbDeviceImpl::Opened(PlatformUsbDeviceHandle platform_handle,
                           const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<UsbDeviceHandle> device_handle = new UsbDeviceHandleImpl(
      context_, this, platform_handle, blocking_task_runner_);
  handles().push_back(device_handle.get());
  callback.Run(device_handle);
}

}  // namespace device
