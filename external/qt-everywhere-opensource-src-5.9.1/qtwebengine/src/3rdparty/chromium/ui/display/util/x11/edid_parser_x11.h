// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_
#define UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util_export.h"

typedef unsigned long XID;
typedef XID RROutput;

// Xrandr utility functions to help get EDID information.

namespace display {

// Xrandr utility class to help get EDID information.
class DISPLAY_UTIL_EXPORT EDIDParserX11 {
 public:
  EDIDParserX11(XID output_id);
  ~EDIDParserX11();

  // Sets |out_display_id| to the display ID from the EDID of this output.
  // Returns true if successful, false otherwise.
  bool GetDisplayId(uint8_t index, int64_t* out_display_id) const;

  // Generate the human readable string from EDID obtained from |output|.
  // Returns an empty string upon error.
  std::string GetDisplayName() const;

  // Gets the overscan flag and stores to |out_flag|. Returns true if the flag
  // is found. Otherwise returns false and doesn't touch |out_flag|. The output
  // will produce overscan if |out_flag| is set to true, but the output may
  // still produce overscan even though it returns true and |out_flag| is set to
  // false.
  bool GetOutputOverscanFlag(bool* out_flag) const;

  XID output_id() const { return output_id_; }
  const std::vector<uint8_t>& edid() const { return edid_; }

 private:
  XID output_id_;

  // This will be an empty vector upon failure to get the EDID from the
  // |output_id_|.
  std::vector<uint8_t> edid_;

  DISALLOW_COPY_AND_ASSIGN(EDIDParserX11);
};

}  // namespace display

#endif  // UI_DISPLAY_UTIL_X11_EDID_PARSER_X11_H_
