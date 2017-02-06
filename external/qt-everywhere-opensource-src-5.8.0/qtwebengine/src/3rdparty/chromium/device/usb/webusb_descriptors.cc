// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/webusb_descriptors.h"

#include <stddef.h>

#include <iterator>
#include <map>
#include <memory>
#include <set>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_handle.h"
#include "net/base/io_buffer.h"

using net::IOBufferWithSize;

namespace device {

namespace {

// These constants are defined by the Universal Serial Device 3.0 Specification
// Revision 1.0.
const uint8_t kGetDescriptorRequest = 0x06;

const uint8_t kBosDescriptorType = 0x0F;
const uint8_t kDeviceCapabilityDescriptorType = 0x10;

const uint8_t kPlatformDevCapabilityType = 0x05;

// These constants are defined by the WebUSB specification:
// http://wicg.github.io/webusb/
const uint8_t kGetAllowedOriginsRequest = 0x01;
const uint8_t kGetUrlRequest = 0x02;

const uint8_t kWebUsbCapabilityUUID[16] = {
    // Little-endian encoding of {3408b638-09a9-47a0-8bfd-a0768815b665}.
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65};

const int kControlTransferTimeout = 60000;  // 1 minute

using ReadWebUsbDescriptorsCallback =
    base::Callback<void(std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
                        const GURL& landing_page)>;

using ReadWebUsbAllowedOriginsCallback =
    base::Callback<void(std::unique_ptr<WebUsbAllowedOrigins> allowed_origins)>;

// Parses a WebUSB Function Subset Header:
// http://wicg.github.io/webusb/#dfn-function-subset-header
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    length     |     type      | 1st interface |   origin[0]   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    origin[1]  |      ...
// +-+-+-+-+-+-+-+-+-+-+-+------
bool ParseFunction(WebUsbFunctionSubset* function,
                   std::vector<uint8_t>::const_iterator* it,
                   std::vector<uint8_t>::const_iterator end) {
  const uint8_t kDescriptorType = 0x02;
  const uint8_t kDescriptorMinLength = 3;

  // If this isn't the end of the buffer then there must be at least one byte
  // available, the length of the descriptor.
  if (*it == end)
    return false;
  uint8_t length = (*it)[0];

  // Is this a valid Function Subset Header? It must be long enough, fit within
  // the buffer and have the right descriptor type.
  if (length < kDescriptorMinLength || std::distance(*it, end) < length ||
      (*it)[1] != kDescriptorType) {
    return false;
  }

  function->first_interface = (*it)[2];

  // Everything after the mandatory fields are origin indicies.
  std::advance(*it, kDescriptorMinLength);
  uint8_t num_origins = length - kDescriptorMinLength;
  function->origin_ids.reserve(num_origins);
  for (size_t i = 0; i < num_origins; ++i) {
    uint8_t index = *(*it)++;
    if (index == 0)
      return false;
    function->origin_ids.push_back(index);
  }

  return true;
}

// Parses a WebUSB Configuration Subset Header:
// http://wicg.github.io/webusb/#dfn-configuration-subset-header
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    length     |     type      | config value  | num functions |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |   origin[0]   |   origin[1]   |     ...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+------
bool ParseConfiguration(WebUsbConfigurationSubset* configuration,
                        std::vector<uint8_t>::const_iterator* it,
                        std::vector<uint8_t>::const_iterator end) {
  const uint8_t kDescriptorType = 0x01;
  const uint8_t kDescriptorMinLength = 4;

  // If this isn't the end of the buffer then there must be at least one byte
  // available, the length of the descriptor.
  if (*it == end)
    return false;
  uint8_t length = (*it)[0];

  // Is this a valid Configuration Subset Header? It must be long enough, fit
  // within the buffer and have the right descriptor type.
  if (length < kDescriptorMinLength || std::distance(*it, end) < length ||
      (*it)[1] != kDescriptorType) {
    return false;
  }

  configuration->configuration_value = (*it)[2];
  uint8_t num_functions = (*it)[3];

  // The next |length - 4| bytes after the mandatory fields are origin indicies.
  std::advance(*it, kDescriptorMinLength);
  uint8_t num_origins = length - kDescriptorMinLength;
  configuration->origin_ids.reserve(num_origins);
  for (size_t i = 0; i < num_origins; ++i) {
    uint8_t index = *(*it)++;
    if (index == 0)
      return false;
    configuration->origin_ids.push_back(index);
  }

  // |num_functions| function descriptors then follow the descriptor.
  for (size_t i = 0; i < num_functions; ++i) {
    WebUsbFunctionSubset function;
    if (!ParseFunction(&function, it, end))
      return false;
    configuration->functions.push_back(function);
  }

  return true;
}

void OnDoneReadingUrls(std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
                       uint8_t landing_page_id,
                       std::unique_ptr<std::map<uint8_t, GURL>> url_map,
                       const ReadWebUsbDescriptorsCallback& callback) {
  for (uint8_t origin_id : allowed_origins->origin_ids) {
    const auto& it = url_map->find(origin_id);
    if (it != url_map->end())
      allowed_origins->origins.push_back(it->second.GetOrigin());
  }

  for (auto& configuration : allowed_origins->configurations) {
    for (uint8_t origin_id : configuration.origin_ids) {
      const auto& it = url_map->find(origin_id);
      if (it != url_map->end())
        configuration.origins.push_back(it->second.GetOrigin());
    }

    for (auto& function : configuration.functions) {
      for (uint8_t origin_id : function.origin_ids) {
        const auto& it = url_map->find(origin_id);
        if (it != url_map->end())
          function.origins.push_back(it->second.GetOrigin());
      }
    }
  }

  GURL landing_page;
  if (landing_page_id != 0)
    landing_page = (*url_map)[landing_page_id];

  callback.Run(std::move(allowed_origins), landing_page);
}

void OnReadUrlDescriptor(std::map<uint8_t, GURL>* url_map,
                         uint8_t index,
                         const base::Closure& callback,
                         UsbTransferStatus status,
                         scoped_refptr<net::IOBuffer> buffer,
                         size_t length) {
  if (status != USB_TRANSFER_COMPLETED) {
    USB_LOG(EVENT) << "Failed to read WebUSB URL descriptor: " << index;
    callback.Run();
    return;
  }

  GURL url;
  if (ParseWebUsbUrlDescriptor(
          std::vector<uint8_t>(buffer->data(), buffer->data() + length),
          &url)) {
    (*url_map)[index] = url;
  }
  callback.Run();
}

// Reads the descriptor with |index| from the device, adds the value to
// |url_map| and then runs |callback|.
void ReadUrlDescriptor(scoped_refptr<UsbDeviceHandle> device_handle,
                       uint8_t vendor_code,
                       std::map<uint8_t, GURL>* url_map,
                       uint8_t index,
                       const base::Closure& callback) {
  scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(255);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::VENDOR, UsbDeviceHandle::DEVICE,
      vendor_code, index, kGetUrlRequest, buffer, buffer->size(),
      kControlTransferTimeout,
      base::Bind(&OnReadUrlDescriptor, url_map, index, callback));
}

// Reads URL descriptors from the device so that it can fill |allowed_origins|
// with the GURLs matching the indicies already collected.
void ReadUrlDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                        uint8_t vendor_code,
                        uint8_t landing_page_id,
                        const ReadWebUsbDescriptorsCallback& callback,
                        std::unique_ptr<WebUsbAllowedOrigins> allowed_origins) {
  if (!allowed_origins) {
    callback.Run(nullptr, GURL());
    return;
  }

  std::set<uint8_t> to_request;
  if (landing_page_id != 0)
    to_request.insert(landing_page_id);

  to_request.insert(allowed_origins->origin_ids.begin(),
                    allowed_origins->origin_ids.end());
  for (auto& config : allowed_origins->configurations) {
    to_request.insert(config.origin_ids.begin(), config.origin_ids.end());
    for (auto& function : config.functions) {
      to_request.insert(function.origin_ids.begin(), function.origin_ids.end());
    }
  }

  std::unique_ptr<std::map<uint8_t, GURL>> url_map(
      new std::map<uint8_t, GURL>());
  std::map<uint8_t, GURL>* url_map_ptr = url_map.get();
  base::Closure barrier = base::BarrierClosure(
      static_cast<int>(to_request.size()),
      base::Bind(&OnDoneReadingUrls, base::Passed(&allowed_origins),
                 landing_page_id, base::Passed(&url_map), callback));

  for (uint8_t index : to_request) {
    ReadUrlDescriptor(device_handle, vendor_code, url_map_ptr, index, barrier);
  }
}

void OnReadWebUsbAllowedOrigins(
    const ReadWebUsbAllowedOriginsCallback& callback,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length) {
  if (status != USB_TRANSFER_COMPLETED) {
    USB_LOG(EVENT) << "Failed to read WebUSB allowed origins.";
    callback.Run(nullptr);
    return;
  }

  std::unique_ptr<WebUsbAllowedOrigins> allowed_origins(
      new WebUsbAllowedOrigins());
  if (allowed_origins->Parse(
          std::vector<uint8_t>(buffer->data(), buffer->data() + length))) {
    callback.Run(std::move(allowed_origins));
  } else {
    callback.Run(nullptr);
  }
}

void OnReadWebUsbAllowedOriginsHeader(
    scoped_refptr<UsbDeviceHandle> device_handle,
    const ReadWebUsbAllowedOriginsCallback& callback,
    uint8_t vendor_code,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length) {
  if (status != USB_TRANSFER_COMPLETED || length != 4) {
    USB_LOG(EVENT) << "Failed to read WebUSB allowed origins header.";
    callback.Run(nullptr);
    return;
  }

  const uint8_t* data = reinterpret_cast<uint8_t*>(buffer->data());
  uint16_t new_length = data[2] | (data[3] << 8);
  scoped_refptr<IOBufferWithSize> new_buffer = new IOBufferWithSize(new_length);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::VENDOR, UsbDeviceHandle::DEVICE,
      vendor_code, 0, kGetAllowedOriginsRequest, new_buffer, new_buffer->size(),
      kControlTransferTimeout,
      base::Bind(&OnReadWebUsbAllowedOrigins, callback));
}

void ReadWebUsbAllowedOrigins(
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8_t vendor_code,
    const ReadWebUsbAllowedOriginsCallback& callback) {
  scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(4);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::VENDOR, UsbDeviceHandle::DEVICE,
      vendor_code, 0, kGetAllowedOriginsRequest, buffer, buffer->size(),
      kControlTransferTimeout,
      base::Bind(&OnReadWebUsbAllowedOriginsHeader, device_handle, callback,
                 vendor_code));
}

void OnReadBosDescriptor(scoped_refptr<UsbDeviceHandle> device_handle,
                         const ReadWebUsbDescriptorsCallback& callback,
                         UsbTransferStatus status,
                         scoped_refptr<net::IOBuffer> buffer,
                         size_t length) {
  if (status != USB_TRANSFER_COMPLETED) {
    USB_LOG(EVENT) << "Failed to read BOS descriptor.";
    callback.Run(nullptr, GURL());
    return;
  }

  WebUsbPlatformCapabilityDescriptor descriptor;
  if (!descriptor.ParseFromBosDescriptor(
          std::vector<uint8_t>(buffer->data(), buffer->data() + length))) {
    callback.Run(nullptr, GURL());
    return;
  }

  ReadWebUsbAllowedOrigins(
      device_handle, descriptor.vendor_code,
      base::Bind(&ReadUrlDescriptors, device_handle, descriptor.vendor_code,
                 descriptor.landing_page_id, callback));
}

void OnReadBosDescriptorHeader(scoped_refptr<UsbDeviceHandle> device_handle,
                               const ReadWebUsbDescriptorsCallback& callback,
                               UsbTransferStatus status,
                               scoped_refptr<net::IOBuffer> buffer,
                               size_t length) {
  if (status != USB_TRANSFER_COMPLETED || length != 5) {
    USB_LOG(EVENT) << "Failed to read BOS descriptor header.";
    callback.Run(nullptr, GURL());
    return;
  }

  const uint8_t* data = reinterpret_cast<uint8_t*>(buffer->data());
  uint16_t new_length = data[2] | (data[3] << 8);
  scoped_refptr<IOBufferWithSize> new_buffer = new IOBufferWithSize(new_length);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD, UsbDeviceHandle::DEVICE,
      kGetDescriptorRequest, kBosDescriptorType << 8, 0, new_buffer,
      new_buffer->size(), kControlTransferTimeout,
      base::Bind(&OnReadBosDescriptor, device_handle, callback));
}

}  // namespace

WebUsbFunctionSubset::WebUsbFunctionSubset() : first_interface(0) {}

WebUsbFunctionSubset::WebUsbFunctionSubset(const WebUsbFunctionSubset& other) =
    default;

WebUsbFunctionSubset::~WebUsbFunctionSubset() {}

WebUsbConfigurationSubset::WebUsbConfigurationSubset()
    : configuration_value(0) {}

WebUsbConfigurationSubset::WebUsbConfigurationSubset(
    const WebUsbConfigurationSubset& other) = default;

WebUsbConfigurationSubset::~WebUsbConfigurationSubset() {}

WebUsbAllowedOrigins::WebUsbAllowedOrigins() {}

WebUsbAllowedOrigins::~WebUsbAllowedOrigins() {}

// Parses a WebUSB Allowed Origins Header:
// http://wicg.github.io/webusb/#dfn-allowed-origins-header
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    length     |     type      |         total length          |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  num configs  |   origin[0]   |   origin[1]   |     ...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+------
bool WebUsbAllowedOrigins::Parse(const std::vector<uint8_t>& bytes) {
  const uint8_t kDescriptorType = 0x00;
  const uint8_t kDescriptorMinLength = 5;

  // The buffer must be at least the length of this descriptor's mandatory
  // fields.
  if (bytes.size() < kDescriptorMinLength)
    return false;

  // Validate that the length of this descriptor and the total length of the
  // entire block of descriptors is consistent with the length of the buffer.
  uint8_t length = bytes[0];
  uint16_t total_length = bytes[2] + (bytes[3] << 8);
  if (length < 5 || length > bytes.size() ||  // bLength
      bytes[1] != kDescriptorType ||          // bDescriptorType
      total_length < length || total_length > bytes.size()) {  // wTotalLength
    return false;
  }

  std::vector<uint8_t>::const_iterator it = bytes.begin();
  uint8_t num_configurations = bytes[4];

  // The next |length - 5| bytes after the mandatory fields are origin indicies.
  std::advance(it, kDescriptorMinLength);
  uint8_t num_origins = length - kDescriptorMinLength;
  origin_ids.reserve(num_origins);
  for (size_t i = 0; i < num_origins; ++i) {
    uint8_t index = *it++;
    if (index == 0)
      return false;
    origin_ids.push_back(index);
  }

  // |num_configurations| configuration descriptors then follow the descriptor.
  for (size_t i = 0; i < num_configurations; ++i) {
    WebUsbConfigurationSubset configuration;
    if (!ParseConfiguration(&configuration, &it, bytes.end()))
      return false;
    configurations.push_back(configuration);
  }

  return true;
}

WebUsbPlatformCapabilityDescriptor::WebUsbPlatformCapabilityDescriptor()
    : version(0), vendor_code(0) {}

WebUsbPlatformCapabilityDescriptor::~WebUsbPlatformCapabilityDescriptor() {}

bool WebUsbPlatformCapabilityDescriptor::ParseFromBosDescriptor(
    const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 5) {
    // Too short for the BOS descriptor header.
    return false;
  }

  // Validate the BOS descriptor, defined in Table 9-12 of the Universal Serial
  // Bus 3.1 Specification, Revision 1.0.
  uint16_t total_length = bytes[2] + (bytes[3] << 8);
  if (bytes[0] != 5 ||                                    // bLength
      bytes[1] != kBosDescriptorType ||                   // bDescriptorType
      5 > total_length || total_length > bytes.size()) {  // wTotalLength
    return false;
  }

  uint8_t num_device_caps = bytes[4];
  std::vector<uint8_t>::const_iterator it = bytes.begin();
  std::vector<uint8_t>::const_iterator end = it + total_length;
  std::advance(it, 5);

  uint8_t length = 0;
  for (size_t i = 0; i < num_device_caps; ++i, std::advance(it, length)) {
    if (it == end) {
      return false;
    }

    // Validate the Device Capability descriptor, defined in Table 9-13 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    length = it[0];
    if (length < 3 || std::distance(it, end) < length ||  // bLength
        it[1] != kDeviceCapabilityDescriptorType) {       // bDescriptorType
      return false;
    }

    if (it[2] != kPlatformDevCapabilityType) {  // bDevCapabilityType
      continue;
    }

    // Validate the Platform Capability Descriptor, defined in Table 9-18 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    if (length < 20) {
      // Platform capability descriptors must be at least 20 bytes.
      return false;
    }

    if (memcmp(&it[4], kWebUsbCapabilityUUID, sizeof(kWebUsbCapabilityUUID)) !=
        0) {  // PlatformCapabilityUUID
      continue;
    }

    if (length < 22) {
      // The WebUSB capability descriptor must be at least 22 bytes (to allow
      // for future versions).
      return false;
    }

    version = it[20] + (it[21] << 8);  // bcdVersion
    if (version < 0x0100) {
      continue;
    }

    // Version 1.0 defines two fields for a total length of 24 bytes.
    if (length != 24) {
      return false;
    }

    vendor_code = it[22];
    landing_page_id = it[23];
    return true;
  }

  return false;
}

// Parses a WebUSB URL Descriptor:
// http://wicg.github.io/webusb/#dfn-url-descriptor
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     length    |     type      |    prefix     |    data[0]    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     data[1]   |      ...
// +-+-+-+-+-+-+-+-+-+-+-+------
bool ParseWebUsbUrlDescriptor(const std::vector<uint8_t>& bytes, GURL* output) {
  const uint8_t kDescriptorType = 0x03;
  const uint8_t kDescriptorMinLength = 3;

  if (bytes.size() < kDescriptorMinLength) {
    return false;
  }

  // Validate that the length is consistent and fits within the buffer.
  uint8_t length = bytes[0];
  if (length < kDescriptorMinLength || length > bytes.size() ||
      bytes[1] != kDescriptorType) {
    return false;
  }

  // Look up the URL prefix and append the rest of the data in the descriptor.
  std::string url;
  switch (bytes[2]) {
    case 0:
      url.append("http://");
      break;
    case 1:
      url.append("https://");
      break;
    default:
      return false;
  }
  url.append(reinterpret_cast<const char*>(bytes.data() + 3), length - 3);

  *output = GURL(url);
  if (!output->is_valid()) {
    return false;
  }

  return true;
}

void ReadWebUsbDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                           const ReadWebUsbDescriptorsCallback& callback) {
  scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(5);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD, UsbDeviceHandle::DEVICE,
      kGetDescriptorRequest, kBosDescriptorType << 8, 0, buffer, buffer->size(),
      kControlTransferTimeout,
      base::Bind(&OnReadBosDescriptorHeader, device_handle, callback));
}

bool FindInWebUsbAllowedOrigins(
    const device::WebUsbAllowedOrigins* allowed_origins,
    const GURL& origin) {
  if (!allowed_origins)
    return false;

  if (ContainsValue(allowed_origins->origins, origin))
    return true;

  for (const auto& config : allowed_origins->configurations) {
    if (ContainsValue(config.origins, origin))
      return true;

    for (const auto& function : config.functions) {
      if (ContainsValue(function.origins, origin))
        return true;
    }
  }

  return false;
}

}  // namespace device
