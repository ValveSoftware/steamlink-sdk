// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_WEBUSB_DESCRIPTORS_H_
#define DEVICE_USB_WEBUSB_DESCRIPTORS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace device {

class UsbDeviceHandle;

struct WebUsbFunctionSubset {
  WebUsbFunctionSubset();
  WebUsbFunctionSubset(const WebUsbFunctionSubset& other);
  ~WebUsbFunctionSubset();

  uint8_t first_interface;
  std::vector<uint8_t> origin_ids;
  std::vector<GURL> origins;
};

struct WebUsbConfigurationSubset {
  WebUsbConfigurationSubset();
  WebUsbConfigurationSubset(const WebUsbConfigurationSubset& other);
  ~WebUsbConfigurationSubset();

  uint8_t configuration_value;
  std::vector<uint8_t> origin_ids;
  std::vector<GURL> origins;
  std::vector<WebUsbFunctionSubset> functions;
};

struct WebUsbAllowedOrigins {
  WebUsbAllowedOrigins();
  ~WebUsbAllowedOrigins();

  bool Parse(const std::vector<uint8_t>& bytes);

  std::vector<uint8_t> origin_ids;
  std::vector<GURL> origins;
  std::vector<WebUsbConfigurationSubset> configurations;
};

struct WebUsbPlatformCapabilityDescriptor {
  WebUsbPlatformCapabilityDescriptor();
  ~WebUsbPlatformCapabilityDescriptor();

  bool ParseFromBosDescriptor(const std::vector<uint8_t>& bytes);

  uint16_t version;
  uint8_t vendor_code;
  uint8_t landing_page_id;
  GURL landing_page;
};

bool ParseWebUsbUrlDescriptor(const std::vector<uint8_t>& bytes, GURL* output);

void ReadWebUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    const base::Callback<
        void(std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
             const GURL& landing_page)>& callback);

// Check if the origin is allowed.
bool FindInWebUsbAllowedOrigins(
    const device::WebUsbAllowedOrigins* allowed_origins,
    const GURL& origin);

}  // device

#endif  // DEVICE_USB_WEBUSB_DESCRIPTORS_H_
