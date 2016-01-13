// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_gamepad_shared.h"

#include "base/basictypes.h"

namespace ppapi {

void ConvertWebKitGamepadData(const WebKitGamepads& webkit_data,
                              PP_GamepadsSampleData* output_data) {
  output_data->length = webkit_data.length;
  for (unsigned i = 0; i < webkit_data.length; ++i) {
    PP_GamepadSampleData& output_pad = output_data->items[i];
    const WebKitGamepad& webkit_pad = webkit_data.items[i];
    output_pad.connected = webkit_pad.connected ? PP_TRUE : PP_FALSE;
    if (webkit_pad.connected) {
      COMPILE_ASSERT(sizeof(output_pad.id) == sizeof(webkit_pad.id),
                     id_size_does_not_match);
      memcpy(output_pad.id, webkit_pad.id, sizeof(output_pad.id));
      output_pad.timestamp = webkit_pad.timestamp;
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
