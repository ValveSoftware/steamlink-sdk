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
#include "mojo/common/common_type_converters.h"
#include "net/base/io_buffer.h"

namespace device {
namespace usb {

namespace {

scoped_refptr<net::IOBuffer> CreateTransferBuffer(size_t size) {
  return new net::IOBuffer(
      std::max(static_cast<size_t>(1u), static_cast<size_t>(size)));
}

void OnTransferIn(const Device::GenericTransferInCallback& callback,
                  UsbTransferStatus status,
                  scoped_refptr<net::IOBuffer> buffer,
                  size_t buffer_size) {
  std::vector<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): We should change UsbDeviceHandle to use a
    // std::vector<uint8_t> instead of net::IOBuffer. Then we could move
    // instead of copy.
    data.resize(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, data.begin());
  }

  callback.Run(mojo::ConvertTo<TransferStatus>(status), data);
}

void OnTransferOut(const Device::GenericTransferOutCallback& callback,
                   UsbTransferStatus status,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t buffer_size) {
  callback.Run(mojo::ConvertTo<TransferStatus>(status));
}

std::vector<IsochronousPacketPtr> BuildIsochronousPacketArray(
    const std::vector<uint32_t>& packet_lengths,
    TransferStatus status) {
  std::vector<IsochronousPacketPtr> packets;
  packets.reserve(packet_lengths.size());
  for (uint32_t packet_length : packet_lengths) {
    auto packet = IsochronousPacket::New();
    packet->length = packet_length;
    packet->status = status;
    packets.push_back(std::move(packet));
  }
  return packets;
}

void OnIsochronousTransferIn(
    const Device::IsochronousTransferInCallback& callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  std::vector<uint8_t> data;
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
    data.resize(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, data.begin());
  }
  callback.Run(data,
               mojo::ConvertTo<std::vector<IsochronousPacketPtr>>(packets));
}

void OnIsochronousTransferOut(
    const Device::IsochronousTransferOutCallback& callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  callback.Run(mojo::ConvertTo<std::vector<IsochronousPacketPtr>>(packets));
}

}  // namespace

DeviceImpl::DeviceImpl(scoped_refptr<UsbDevice> device,
                       DeviceInfoPtr device_info,
                       base::WeakPtr<PermissionProvider> permission_provider,
                       DeviceRequest request)
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
  binding_.set_connection_error_handler(
      base::Bind([](DeviceImpl* self) { delete self; }, this));
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
    callback.Run(TransferStatus::TRANSFER_ERROR, base::nullopt);
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
    device_handle_->ControlTransfer(
        USB_DIRECTION_INBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, length, timeout,
        base::Bind(&OnTransferIn, callback));
  } else {
    callback.Run(TransferStatus::PERMISSION_DENIED, base::nullopt);
  }
}

void DeviceImpl::ControlTransferOut(
    ControlTransferParamsPtr params,
    const std::vector<uint8_t>& data,
    uint32_t timeout,
    const ControlTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  if (HasControlTransferPermission(params->recipient, params->index)) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
    std::copy(data.begin(), data.end(), buffer->data());
    device_handle_->ControlTransfer(
        USB_DIRECTION_OUTBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, data.size(),
        timeout, base::Bind(&OnTransferOut, callback));
  } else {
    callback.Run(TransferStatus::PERMISSION_DENIED);
  }
}

void DeviceImpl::GenericTransferIn(uint8_t endpoint_number,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const GenericTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR, base::nullopt);
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->GenericTransfer(USB_DIRECTION_INBOUND, endpoint_address,
                                  buffer, length, timeout,
                                  base::Bind(&OnTransferIn, callback));
}

void DeviceImpl::GenericTransferOut(
    uint8_t endpoint_number,
    const std::vector<uint8_t>& data,
    uint32_t timeout,
    const GenericTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  std::copy(data.begin(), data.end(), buffer->data());
  device_handle_->GenericTransfer(USB_DIRECTION_OUTBOUND, endpoint_address,
                                  buffer, data.size(), timeout,
                                  base::Bind(&OnTransferOut, callback));
}

void DeviceImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    const IsochronousTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(base::nullopt,
                 BuildIsochronousPacketArray(packet_lengths,
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  uint8_t endpoint_address = endpoint_number | 0x80;
  device_handle_->IsochronousTransferIn(
      endpoint_address, packet_lengths, timeout,
      base::Bind(&OnIsochronousTransferIn, callback));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    const std::vector<uint8_t>& data,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    const IsochronousTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(BuildIsochronousPacketArray(packet_lengths,
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  std::copy(data.begin(), data.end(), buffer->data());
  device_handle_->IsochronousTransferOut(
      endpoint_address, buffer, packet_lengths, timeout,
      base::Bind(&OnIsochronousTransferOut, callback));
}

void DeviceImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK_EQ(device_, device);
  delete this;
}

}  // namespace usb
}  // namespace device
