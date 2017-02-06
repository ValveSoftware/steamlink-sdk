// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_device.h"
#include "mojo/public/cpp/bindings/array.h"

namespace mojo {

// static
device::UsbDeviceFilter
TypeConverter<device::UsbDeviceFilter, device::usb::DeviceFilterPtr>::Convert(
    const device::usb::DeviceFilterPtr& mojo_filter) {
  device::UsbDeviceFilter filter;
  if (mojo_filter->has_vendor_id)
    filter.SetVendorId(mojo_filter->vendor_id);
  if (mojo_filter->has_product_id)
    filter.SetProductId(mojo_filter->product_id);
  if (mojo_filter->has_class_code)
    filter.SetInterfaceClass(mojo_filter->class_code);
  if (mojo_filter->has_subclass_code)
    filter.SetInterfaceSubclass(mojo_filter->subclass_code);
  if (mojo_filter->has_protocol_code)
    filter.SetInterfaceProtocol(mojo_filter->protocol_code);
  return filter;
}

// static
device::usb::TransferDirection
TypeConverter<device::usb::TransferDirection, device::UsbEndpointDirection>::
    Convert(const device::UsbEndpointDirection& direction) {
  if (direction == device::USB_DIRECTION_INBOUND)
    return device::usb::TransferDirection::INBOUND;
  DCHECK(direction == device::USB_DIRECTION_OUTBOUND);
  return device::usb::TransferDirection::OUTBOUND;
}

// static
device::usb::TransferStatus
TypeConverter<device::usb::TransferStatus, device::UsbTransferStatus>::Convert(
    const device::UsbTransferStatus& status) {
  switch (status) {
    case device::USB_TRANSFER_COMPLETED:
      return device::usb::TransferStatus::COMPLETED;
    case device::USB_TRANSFER_ERROR:
      return device::usb::TransferStatus::TRANSFER_ERROR;
    case device::USB_TRANSFER_TIMEOUT:
      return device::usb::TransferStatus::TIMEOUT;
    case device::USB_TRANSFER_CANCELLED:
      return device::usb::TransferStatus::CANCELLED;
    case device::USB_TRANSFER_STALLED:
      return device::usb::TransferStatus::STALLED;
    case device::USB_TRANSFER_DISCONNECT:
      return device::usb::TransferStatus::DISCONNECT;
    case device::USB_TRANSFER_OVERFLOW:
      return device::usb::TransferStatus::BABBLE;
    case device::USB_TRANSFER_LENGTH_SHORT:
      return device::usb::TransferStatus::SHORT_PACKET;
    default:
      NOTREACHED();
      return device::usb::TransferStatus::TRANSFER_ERROR;
  }
}

// static
device::UsbDeviceHandle::TransferRequestType
TypeConverter<device::UsbDeviceHandle::TransferRequestType,
              device::usb::ControlTransferType>::
    Convert(const device::usb::ControlTransferType& type) {
  switch (type) {
    case device::usb::ControlTransferType::STANDARD:
      return device::UsbDeviceHandle::STANDARD;
    case device::usb::ControlTransferType::CLASS:
      return device::UsbDeviceHandle::CLASS;
    case device::usb::ControlTransferType::VENDOR:
      return device::UsbDeviceHandle::VENDOR;
    case device::usb::ControlTransferType::RESERVED:
      return device::UsbDeviceHandle::RESERVED;
    default:
      NOTREACHED();
      return device::UsbDeviceHandle::RESERVED;
  }
}

// static
device::UsbDeviceHandle::TransferRecipient
TypeConverter<device::UsbDeviceHandle::TransferRecipient,
              device::usb::ControlTransferRecipient>::
    Convert(const device::usb::ControlTransferRecipient& recipient) {
  switch (recipient) {
    case device::usb::ControlTransferRecipient::DEVICE:
      return device::UsbDeviceHandle::DEVICE;
    case device::usb::ControlTransferRecipient::INTERFACE:
      return device::UsbDeviceHandle::INTERFACE;
    case device::usb::ControlTransferRecipient::ENDPOINT:
      return device::UsbDeviceHandle::ENDPOINT;
    case device::usb::ControlTransferRecipient::OTHER:
      return device::UsbDeviceHandle::OTHER;
    default:
      NOTREACHED();
      return device::UsbDeviceHandle::OTHER;
  }
}

// static
device::usb::EndpointType
TypeConverter<device::usb::EndpointType, device::UsbTransferType>::Convert(
    const device::UsbTransferType& type) {
  switch (type) {
    case device::USB_TRANSFER_ISOCHRONOUS:
      return device::usb::EndpointType::ISOCHRONOUS;
    case device::USB_TRANSFER_BULK:
      return device::usb::EndpointType::BULK;
    case device::USB_TRANSFER_INTERRUPT:
      return device::usb::EndpointType::INTERRUPT;
    // Note that we do not expose control transfer in the public interface
    // because control endpoints are implied rather than explicitly enumerated
    // there.
    default:
      NOTREACHED();
      return device::usb::EndpointType::BULK;
  }
}

// static
device::usb::EndpointInfoPtr
TypeConverter<device::usb::EndpointInfoPtr, device::UsbEndpointDescriptor>::
    Convert(const device::UsbEndpointDescriptor& endpoint) {
  device::usb::EndpointInfoPtr info = device::usb::EndpointInfo::New();
  info->endpoint_number = endpoint.address & 0xf;
  info->direction =
      ConvertTo<device::usb::TransferDirection>(endpoint.direction);
  info->type = ConvertTo<device::usb::EndpointType>(endpoint.transfer_type);
  info->packet_size = static_cast<uint32_t>(endpoint.maximum_packet_size);
  return info;
}

// static
device::usb::AlternateInterfaceInfoPtr
TypeConverter<device::usb::AlternateInterfaceInfoPtr,
              device::UsbInterfaceDescriptor>::
    Convert(const device::UsbInterfaceDescriptor& interface) {
  device::usb::AlternateInterfaceInfoPtr info =
      device::usb::AlternateInterfaceInfo::New();
  info->alternate_setting = interface.alternate_setting;
  info->class_code = interface.interface_class;
  info->subclass_code = interface.interface_subclass;
  info->protocol_code = interface.interface_protocol;

  // Filter out control endpoints for the public interface.
  info->endpoints = mojo::Array<device::usb::EndpointInfoPtr>::New(0);
  for (const auto& endpoint : interface.endpoints) {
    if (endpoint.transfer_type != device::USB_TRANSFER_CONTROL)
      info->endpoints.push_back(device::usb::EndpointInfo::From(endpoint));
  }

  return info;
}

// static
mojo::Array<device::usb::InterfaceInfoPtr>
TypeConverter<mojo::Array<device::usb::InterfaceInfoPtr>,
              std::vector<device::UsbInterfaceDescriptor>>::
    Convert(const std::vector<device::UsbInterfaceDescriptor>& interfaces) {
  auto infos = mojo::Array<device::usb::InterfaceInfoPtr>::New(0);

  // Aggregate each alternate setting into an InterfaceInfo corresponding to its
  // interface number.
  std::map<uint8_t, device::usb::InterfaceInfo*> interface_map;
  for (size_t i = 0; i < interfaces.size(); ++i) {
    auto alternate = device::usb::AlternateInterfaceInfo::From(interfaces[i]);
    auto iter = interface_map.find(interfaces[i].interface_number);
    if (iter == interface_map.end()) {
      // This is the first time we're seeing an alternate with this interface
      // number, so add a new InterfaceInfo to the array and map the number.
      auto info = device::usb::InterfaceInfo::New();
      info->interface_number = interfaces[i].interface_number;
      iter = interface_map
                 .insert(
                     std::make_pair(interfaces[i].interface_number, info.get()))
                 .first;
      infos.push_back(std::move(info));
    }
    iter->second->alternates.push_back(std::move(alternate));
  }

  return infos;
}

// static
device::usb::ConfigurationInfoPtr
TypeConverter<device::usb::ConfigurationInfoPtr, device::UsbConfigDescriptor>::
    Convert(const device::UsbConfigDescriptor& config) {
  device::usb::ConfigurationInfoPtr info =
      device::usb::ConfigurationInfo::New();
  info->configuration_value = config.configuration_value;
  info->interfaces =
      mojo::Array<device::usb::InterfaceInfoPtr>::From(config.interfaces);
  return info;
}

// static
device::usb::DeviceInfoPtr
TypeConverter<device::usb::DeviceInfoPtr, device::UsbDevice>::Convert(
    const device::UsbDevice& device) {
  device::usb::DeviceInfoPtr info = device::usb::DeviceInfo::New();
  info->guid = device.guid();
  info->usb_version_major = device.usb_version() >> 8;
  info->usb_version_minor = device.usb_version() >> 4 & 0xf;
  info->usb_version_subminor = device.usb_version() & 0xf;
  info->class_code = device.device_class();
  info->subclass_code = device.device_subclass();
  info->protocol_code = device.device_protocol();
  info->vendor_id = device.vendor_id();
  info->product_id = device.product_id();
  info->device_version_major = device.device_version() >> 8;
  info->device_version_minor = device.device_version() >> 4 & 0xf;
  info->device_version_subminor = device.device_version() & 0xf;
  info->manufacturer_name = base::UTF16ToUTF8(device.manufacturer_string());
  info->product_name = base::UTF16ToUTF8(device.product_string());
  info->serial_number = base::UTF16ToUTF8(device.serial_number());
  const device::UsbConfigDescriptor* config = device.active_configuration();
  info->active_configuration = config ? config->configuration_value : 0;
  info->configurations = mojo::Array<device::usb::ConfigurationInfoPtr>::From(
      device.configurations());
  return info;
}

// static
device::usb::IsochronousPacketPtr
TypeConverter<device::usb::IsochronousPacketPtr,
              device::UsbDeviceHandle::IsochronousPacket>::
    Convert(const device::UsbDeviceHandle::IsochronousPacket& packet) {
  device::usb::IsochronousPacketPtr info =
      device::usb::IsochronousPacket::New();
  info->length = packet.length;
  info->transferred_length = packet.transferred_length;
  info->status = mojo::ConvertTo<device::usb::TransferStatus>(packet.status);
  return info;
}

}  // namespace mojo
