// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_descriptors.h"
#include "mojo/public/cpp/bindings/array.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  device::UsbDeviceDescriptor desc;
  desc.Parse(std::vector<uint8_t>(data, data + size));
  mojo::Array<device::usb::ConfigurationInfoPtr>::From(desc.configurations);
  return 0;
}
