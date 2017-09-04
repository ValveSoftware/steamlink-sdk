// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> edid;
  edid.assign(data, data + size);
  uint16_t manufacturer_id, product_code;
  std::string human_readable_name;
  gfx::Size active_pixel_size, physical_display_size;
  bool overscan;

  display::ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                 &human_readable_name, &active_pixel_size,
                                 &physical_display_size);

  display::ParseOutputOverscanFlag(edid, &overscan);
  return 0;
}
