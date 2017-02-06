// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "net/base/io_buffer.h"

namespace device {
namespace usb {

namespace {

using MojoTransferInCallback =
    base::Callback<void(TransferStatus, mojo::Array<uint8_t>)>;

using MojoTransferOutCallback = base::Callback<void(TransferStatus)>;

scoped_refptr<net::IOBuffer> CreateTransferBuffer(size_t size) {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(
      std::max(static_cast<size_t>(1u), static_cast<size_t>(size)));
  return buffer;
}

void OnTransferIn(std::unique_ptr<MojoTransferInCallback> callback,
                  UsbTransferStatus status,
                  scoped_refptr<net::IOBuffer> buffer,
                  size_t buffer_size) {
  mojo::Array<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): We should change UsbDeviceHandle to use a
    // std::vector<uint8_t> instead of net::IOBuffer. Then we could move
    // instead of copy.
    std::vector<uint8_t> bytes(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, bytes.begin());
    data.Swap(&bytes);
  }
  callback->Run(mojo::ConvertTo<TransferStatus>(status), std::move(data));
}

void OnTransferOut(std::unique_ptr<MojoTransferOutCallback> callback,
                   UsbTransferStatus status,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t buffer_size) {
  callback->Run(mojo::ConvertTo<TransferStatus>(status));
}

mojo::Array<IsochronousPacketPtr> BuildIsochronousPacketArray(
    mojo::Array<uint32_t> packet_lengths,
    TransferStatus status) {
  mojo::Array<IsochronousPacketPtr> packets(packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i] = IsochronousPacket::New();
    packets[i]->length = packet_lengths[i];
    packets[i]->status = status;
  }
  return packets;
}

void OnIsochronousTransferIn(
    std::unique_ptr<Device::IsochronousTransferInCallback> callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  mojo::Array<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): We should change UsbDeviceHandle to use a
    // std::vector<uint8_t> instead of net::IOBuffer. Then we could move
    // instead of copy.
    uint32_t buffer_size =
        std::accumulate(packets.begin(), packets.end(), 0u,
                        [](const uint32_t& a,
                           const UsbDeviceHandle::IsochronousPacket& packet) {
                          return a + packet.length;
                        });
    std::vector<uint8_t> bytes(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, bytes.begin());
    data.Swap(&bytes);
  }
  callback->Run(std::move(data),
                mojo::Array<IsochronousPacketPtr>::From(packets));
}

void OnIsochronousTransferOut(
    std::unique_ptr<Device::IsochronousTransferOutCallback> callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  callback->Run(mojo::Array<IsochronousPacketPtr>::From(packets));
}

}  // namespace

DeviceImpl::DeviceImpl(scoped_refptr<UsbDevice> device,
                       DeviceInfoPtr device_info,
                       base::WeakPtr<PermissionProvider> permission_provider,
                       mojo::InterfaceRequest<Device> request)
    : device_(device),
      device_info_(std::move(device_info)),
      permission_provider_(permission_provider),
      observer_(this),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  DCHECK(device_);
  // This object owns itself and will be destroyed if,
  //  * the device is disconnected or
  //  * the message pipe it is bound to is closed or the message loop is
  //  * destructed.
  observer_.Add(device_.get());
}

DeviceImpl::~DeviceImpl() {
  CloseHandle();
}

void DeviceImpl::CloseHandle() {
  if (device_handle_) {
    device_handle_->Close();
    if (permission_provider_)
      permission_provider_->DecrementConnectionCount();
  }
  device_handle_ = nullptr;
}

bool DeviceImpl::HasControlTransferPermission(
    ControlTransferRecipient recipient,
    uint16_t index) {
  DCHECK(device_handle_);
  const UsbConfigDescriptor* config = device_->active_configuration();

  if (!permission_provider_)
    return false;

  if (recipient == ControlTransferRecipient::INTERFACE ||
      recipient == ControlTransferRecipient::ENDPOINT) {
    if (!config)
      return false;

    const UsbInterfaceDescriptor* interface = nullptr;
    if (recipient == ControlTransferRecipient::ENDPOINT) {
      interface = device_handle_->FindInterfaceByEndpoint(index & 0xff);
    } else {
      auto interface_it =
          std::find_if(config->interfaces.begin(), config->interfaces.end(),
                       [index](const UsbInterfaceDescriptor& this_iface) {
                         return this_iface.interface_number == (index & 0xff);
                       });
      if (interface_it != config->interfaces.end())
        interface = &*interface_it;
    }
    if (interface == nullptr)
      return false;

    return permission_provider_->HasFunctionPermission(
        interface->first_interface, config->configuration_value, device_);
  } else if (config) {
    return permission_provider_->HasConfigurationPermission(
        config->configuration_value, device_);
  } else {
    // Client must already have device permission to have gotten this far.
    return true;
  }
}

void DeviceImpl::OnOpen(const OpenCallback& callback,
                        scoped_refptr<UsbDeviceHandle> handle) {
  device_handle_ = handle;
  if (device_handle_ && permission_provider_)
    permission_provider_->IncrementConnectionCount();
  callback.Run(handle ? OpenDeviceError::OK : OpenDeviceError::ACCESS_DENIED);
}

void DeviceImpl::OnPermissionGrantedForOpen(const OpenCallback& callback,
                                            bool granted) {
  if (granted && permission_provider_ &&
      permission_provider_->HasDevicePermission(device_)) {
    device_->Open(
        base::Bind(&DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(), callback));
  } else {
    callback.Run(OpenDeviceError::ACCESS_DENIED);
  }
}

void DeviceImpl::GetDeviceInfo(const GetDeviceInfoCallback& callback) {
  const UsbConfigDescriptor* config = device_->active_configuration();
  device_info_->active_configuration = config ? config->configuration_value : 0;
  callback.Run(device_info_->Clone());
}

void DeviceImpl::Open(const OpenCallback& callback) {
  if (device_handle_) {
    callback.Run(OpenDeviceError::ALREADY_OPEN);
    return;
  }

  if (!device_->permission_granted()) {
    device_->RequestPermission(
        base::Bind(&DeviceImpl::OnPermissionGrantedForOpen,
                   weak_factory_.GetWeakPtr(), callback));
    return;
  }

  if (permission_provider_ &&
      permission_provider_->HasDevicePermission(device_)) {
    device_->Open(
        base::Bind(&DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(), callback));
  } else {
    callback.Run(OpenDeviceError::ACCESS_DENIED);
  }
}

void DeviceImpl::Close(const CloseCallback& callback) {
  CloseHandle();
  callback.Run();
}

void DeviceImpl::SetConfiguration(uint8_t value,
                                  const SetConfigurationCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  if (permission_provider_ &&
      permission_provider_->HasConfigurationPermission(value, device_)) {
    device_handle_->SetConfiguration(value, callback);
  } else {
    callback.Run(false);
  }
}

void DeviceImpl::ClaimInterface(uint8_t interface_number,
                                const ClaimInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  const UsbConfigDescriptor* config = device_->active_configuration();
  if (!config) {
    callback.Run(false);
    return;
  }

  auto interface_it =
      std::find_if(config->interfaces.begin(), config->interfaces.end(),
                   [interface_number](const UsbInterfaceDescriptor& interface) {
                     return interface.interface_number == interface_number;
                   });
  if (interface_it == config->interfaces.end()) {
    callback.Run(false);
    return;
  }

  if (permission_provider_ &&
      permission_provider_->HasFunctionPermission(interface_it->first_interface,
                                                  config->configuration_value,
                                                  device_)) {
    device_handle_->ClaimInterface(interface_number, callback);
  } else {
    callback.Run(false);
  }
}

void DeviceImpl::ReleaseInterface(uint8_t interface_number,
                                  const ReleaseInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ReleaseInterface(interface_number, callback);
}

void DeviceImpl::SetInterfaceAlternateSetting(
    uint8_t interface_number,
    uint8_t alternate_setting,
    const SetInterfaceAlternateSettingCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->SetInterfaceAlternateSetting(interface_number,
                                               alternate_setting, callback);
}

void DeviceImpl::Reset(const ResetCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ResetDevice(callback);
}

void DeviceImpl::ClearHalt(uint8_t endpoint,
                           const ClearHaltCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ClearHalt(endpoint, callback);
}

void DeviceImpl::ControlTransferIn(ControlTransferParamsPtr params,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const ControlTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR, mojo::Array<uint8_t>());
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
    auto callback_ptr =
        base::WrapUnique(new ControlTransferInCallback(callback));
    device_handle_->ControlTransfer(
        USB_DIRECTION_INBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, length, timeout,
        base::Bind(&OnTransferIn, base::Passed(&callback_ptr)));
  } else {
    mojo::Array<uint8_t> data;
    callback.Run(TransferStatus::PERMISSION_DENIED, std::move(data));
  }
}

void DeviceImpl::ControlTransferOut(
    ControlTransferParamsPtr params,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const ControlTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
    const std::vector<uint8_t>& storage = data.storage();
    std::copy(storage.begin(), storage.end(), buffer->data());
    auto callback_ptr =
        base::WrapUnique(new ControlTransferOutCallback(callback));
    device_handle_->ControlTransfer(
        USB_DIRECTION_OUTBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, data.size(),
        timeout, base::Bind(&OnTransferOut, base::Passed(&callback_ptr)));
  } else {
    callback.Run(TransferStatus::PERMISSION_DENIED);
  }
}

void DeviceImpl::GenericTransferIn(uint8_t endpoint_number,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const GenericTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR, mojo::Array<uint8_t>());
    return;
  }

  auto callback_ptr = base::WrapUnique(new GenericTransferInCallback(callback));
  uint8_t endpoint_address = endpoint_number | 0x80;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->GenericTransfer(
      USB_DIRECTION_INBOUND, endpoint_address, buffer, length, timeout,
      base::Bind(&OnTransferIn, base::Passed(&callback_ptr)));
}

void DeviceImpl::GenericTransferOut(
    uint8_t endpoint_number,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const GenericTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  auto callback_ptr =
      base::WrapUnique(new GenericTransferOutCallback(callback));
  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  const std::vector<uint8_t>& storage = data.storage();
  std::copy(storage.begin(), storage.end(), buffer->data());
  device_handle_->GenericTransfer(
      USB_DIRECTION_OUTBOUND, endpoint_address, buffer, data.size(), timeout,
      base::Bind(&OnTransferOut, base::Passed(&callback_ptr)));
}

void DeviceImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    mojo::Array<uint32_t> packet_lengths,
    uint32_t timeout,
    const IsochronousTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(mojo::Array<uint8_t>(),
                 BuildIsochronousPacketArray(std::move(packet_lengths),
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  auto callback_ptr =
      base::WrapUnique(new IsochronousTransferInCallback(callback));
  uint8_t endpoint_address = endpoint_number | 0x80;
  device_handle_->IsochronousTransferIn(
      endpoint_address, packet_lengths.storage(), timeout,
      base::Bind(&OnIsochronousTransferIn, base::Passed(&callback_ptr)));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    mojo::Array<uint8_t> data,
    mojo::Array<uint32_t> packet_lengths,
    uint32_t timeout,
    const IsochronousTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(BuildIsochronousPacketArray(std::move(packet_lengths),
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  auto callback_ptr =
      base::WrapUnique(new IsochronousTransferOutCallback(callback));
  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  {
    const std::vector<uint8_t>& storage = data.storage();
    std::copy(storage.begin(), storage.end(), buffer->data());
  }
  device_handle_->IsochronousTransferOut(
      endpoint_address, buffer, packet_lengths.storage(), timeout,
      base::Bind(&OnIsochronousTransferOut, base::Passed(&callback_ptr)));
}

void DeviceImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK_EQ(device_, device);
  delete this;
}

}  // namespace usb
}  // namespace device
