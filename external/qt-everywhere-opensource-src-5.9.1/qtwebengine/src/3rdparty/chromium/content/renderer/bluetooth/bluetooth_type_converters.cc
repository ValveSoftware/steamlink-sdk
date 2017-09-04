// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_type_converters.h"

#include "content/child/mojo/type_converters.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

namespace mojo {

// static
blink::mojom::WebBluetoothScanFilterPtr TypeConverter<
    blink::mojom::WebBluetoothScanFilterPtr,
    blink::WebBluetoothScanFilter>::Convert(const blink::WebBluetoothScanFilter&
                                                web_filter) {
  blink::mojom::WebBluetoothScanFilterPtr filter =
      blink::mojom::WebBluetoothScanFilter::New();

  if (!web_filter.services.isEmpty()) {
    filter->services.emplace();
    for (const auto& service : web_filter.services) {
      filter->services->push_back(device::BluetoothUUID(service.utf8()));
    }
  }

  if (web_filter.hasName)
    filter->name = web_filter.name.utf8();

  if (!web_filter.namePrefix.isEmpty())
    filter->name_prefix = web_filter.namePrefix.utf8();
  return filter;
}

// static
blink::mojom::WebBluetoothRequestDeviceOptionsPtr
TypeConverter<blink::mojom::WebBluetoothRequestDeviceOptionsPtr,
              blink::WebRequestDeviceOptions>::
    Convert(const blink::WebRequestDeviceOptions& web_options) {
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();

  options->accept_all_devices = web_options.acceptAllDevices;

  if (web_options.hasFilters) {
    options->filters.emplace();
    for (const auto& filter : web_options.filters) {
      options->filters->push_back(blink::mojom::WebBluetoothScanFilter::From<
                                  blink::WebBluetoothScanFilter>(filter));
    }
  }

  for (const auto& optional_service : web_options.optionalServices) {
    options->optional_services.push_back(
        device::BluetoothUUID(optional_service.utf8()));
  }

  return options;
}

// static
device::BluetoothUUID
TypeConverter<device::BluetoothUUID, blink::WebString>::Convert(
    const blink::WebString& web_string) {
  device::BluetoothUUID uuid = device::BluetoothUUID(web_string.utf8());

  DCHECK(uuid.IsValid());

  return uuid;
}

}  // namespace mojo
