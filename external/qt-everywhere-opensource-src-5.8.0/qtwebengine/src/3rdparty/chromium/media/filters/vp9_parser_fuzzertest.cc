// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const uint8_t* ivf_payload = nullptr;
  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_file_header;
  media::IvfFrameHeader ivf_frame_header;

  if (!ivf_parser.Initialize(data, size, &ivf_file_header))
    return 0;

  // Parse until the end of stream/unsupported stream/error in stream is found.
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &ivf_payload)) {
    media::Vp9Parser vp9_parser;
    media::Vp9FrameHeader vp9_frame_header;
    vp9_parser.SetStream(ivf_payload, ivf_frame_header.frame_size);
    while (vp9_parser.ParseNextFrame(&vp9_frame_header) ==
           media::Vp9Parser::kOk) {
      // Repeat until all frames processed.
    }
  }

  return 0;
}
