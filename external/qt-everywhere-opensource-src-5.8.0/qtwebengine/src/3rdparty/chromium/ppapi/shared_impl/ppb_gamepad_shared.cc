// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_gamepad_shared.h"

#include <string.h>

#include <algorithm>

namespace ppapi {

const size_t WebKitGamepads::kItemsLengthCap;

void ConvertWebKitGamepadData(const WebKitGamepads& webkit_data,
                              PP_GamepadsSampleData* output_data) {
  size_t length = std::min(WebKitGamepads::kItemsLengthCap,
                           static_cast<const size_t>(webkit_data.length));
  output_data->length = static_cast<unsigned>(length);
  for (unsigned i = 0; i < length; ++i) {
    PP_GamepadSampleData& output_pad = output_data->items[i];
    const WebKitGamepad& webkit_pad = webkit_data.items[i];
    output_pad.connected = webkit_pad.connected ? PP_TRUE : PP_FALSE;
    if (webkit_pad.connected) {
      static_assert(sizeof(output_pad.id) == sizeof(webkit_pad.id),
                    "id size does not match");
      memcpy(output_pad.id, webkit_pad.id, sizeof(output_pad.id));
      output_pad.timestamp = static_cast<double>(webkit_pad.timestamp);
      output_pad.axes_length = webkit_pad.axes_length;
      for (unsigned j = 0; j < webkit_pad.axes_length; ++j)
        output_pad.axes[j] = static_cast<float>(webkit_pad.axes[j]);
      output_pad.buttons_length = webkit_pad.buttons_length;
      for (unsigned j = 0; j < webkit_pad.buttons_length; ++j)
        output_pad.buttons[j] = static_cast<float>(webkit_pad.buttons[j].value);
    }
  }
}

}  // namespace ppapi
