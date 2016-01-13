// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_
#define UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_

#include <stdint.h>

#include <string>

#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util_export.h"

typedef unsigned long XID;
typedef XID RROutput;

// Xrandr utility functions to help get EDID information.

namespace ui {

// Gets the EDID data from |output| and generates the display id through
// |GetDisplayIdFromEDID|.
DISPLAY_UTIL_EXPORT bool GetDisplayId(XID output,
                                      uint8_t index,
                                      int64_t* display_id_out);

// Generate the human readable string from EDID obtained from |output|.
DISPLAY_UTIL_EXPORT std::string GetDisplayName(RROutput output);

// Gets the overscan flag from |output| and stores to |flag|. Returns true if
// the flag is found. Otherwise returns false and doesn't touch |flag|. The
// output will produce overscan if |flag| is set to true, but the output may
// still produce overscan even though it returns true and |flag| is set to
// false.
DISPLAY_UTIL_EXPORT bool GetOutputOverscanFlag(RROutput output, bool* flag);

}  // namespace ui

#endif  // UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_
