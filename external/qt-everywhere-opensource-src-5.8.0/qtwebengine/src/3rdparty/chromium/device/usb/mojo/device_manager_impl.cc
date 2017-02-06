// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "device/core/device_client.h"
#include "device/usb/mojo/device_impl.h"
#include "device/usb/mojo/permission_provider.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

// static
void DeviceManagerImpl::Create(
    base::WeakPtr<PermissionProvider> permission_provider,
    mojo::InterfaceRequest<DeviceManager> request) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (usb_service) {
    new DeviceManagerImpl(permission_provider, usb_service, std::move(request));
  }
}

DeviceManagerImpl::DeviceManagerImpl(
    base::WeakPtr<PermissionProvider> permission_provider,
    UsbService* usb_service,
    mojo::InterfaceRequest<DeviceManager> request)
    : permission_provider_(permission_provider),
      usb_service_(usb_service),
      observer_(this),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if the message pipe it is
  // bound to is closed, the message loop is destructed, or the UsbService is
  // shut down.
  observer_.Add(usb_service_);
}

DeviceManagerImpl::~DeviceManagerImpl() {
  if (!connection_error_handler_.is_null())
    connection_error_handler_.Run();
}

void DeviceManagerImpl::GetDevices(EnumerationOptionsPtr options,
                                   const GetDevicesCallback& callback) {
  usb_service_->GetDevices(base::Bind(&DeviceManagerImpl::OnGetDevices,
                                      weak_factory_.GetWeakPtr(),
                                      base::Passed(&options), callback));
}

void DeviceManagerImpl::GetDevice(
    const mojo::String& guid,
    mojo::InterfaceRequest<Device> device_request) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  if (permission_provider_ &&
      permission_provider_->HasDevicePermission(device)) {
    new DeviceImpl(device, DeviceInfo::From(*device), permission_provider_,
                   std::move(device_request));
  }
}

void DeviceManagerImpl::SetClient(DeviceManagerClientPtr client) {
  client_ = std::move(client);
}

void DeviceManagerImpl::OnGetDevices(
    EnumerationOptionsPtr options,
    const GetDevicesCallback& callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::vector<UsbDeviceFilter> filters;
  if (options)
    filters = options->filters.To<std::vector<UsbDeviceFilter>>();

  mojo::Array<DeviceInfoPtr> device_infos;
  for (const auto& device : devices) {
    if (filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) {
      if (permission_provider_ &&
          permission_provider_->HasDevicePermission(device)) {
        device_infos.push_back(DeviceInfo::From(*device));
      }
    }
  }

  callback.Run(std::move(device_infos));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  if (client_ && permission_provider_ &&
      permission_provider_->HasDevicePermission(device))
    client_->OnDeviceAdded(DeviceInfo::From(*device));
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  if (client_ && permission_provider_ &&
      permission_provider_->HasDevicePermission(device))
    client_->OnDeviceRemoved(DeviceInfo::From(*device));
}

void DeviceManagerImpl::WillDestroyUsbService() {
  delete this;
}

}  // namespace usb
}  // namespace device
