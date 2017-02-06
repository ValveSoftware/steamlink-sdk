// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
#define CONTENT_RENDERER_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_

#include "base/optional.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom.h"

namespace blink {
struct WebBluetoothScanFilter;
struct WebRequestDeviceOptions;
}

namespace mojo {

template <>
struct TypeConverter<blink::mojom::WebBluetoothScanFilterPtr,
                     blink::WebBluetoothScanFilter> {
  static blink::mojom::WebBluetoothScanFilterPtr Convert(
      const blink::WebBluetoothScanFilter& web_filter);
};

template <>
struct TypeConverter<blink::mojom::WebBluetoothRequestDeviceOptionsPtr,
                     blink::WebRequestDeviceOptions> {
  static blink::mojom::WebBluetoothRequestDeviceOptionsPtr Convert(
      const blink::WebRequestDeviceOptions& web_options);
};

template <>
struct TypeConverter<base::Optional<device::BluetoothUUID>, blink::WebString> {
  static base::Optional<device::BluetoothUUID> Convert(
      const blink::WebString& web_string);
};

}  // namespace mojo

#endif  // CONTENT_RENDERER_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
