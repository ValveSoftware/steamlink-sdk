// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_HDR_METADATA_H_
#define MEDIA_BASE_HDR_METADATA_H_

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

// SMPTE ST 2086 mastering metadata.
struct MEDIA_EXPORT MasteringMetadata {
  float primary_r_chromaticity_x = 0;
  float primary_r_chromaticity_y = 0;
  float primary_g_chromaticity_x = 0;
  float primary_g_chromaticity_y = 0;
  float primary_b_chromaticity_x = 0;
  float primary_b_chromaticity_y = 0;
  float white_point_chromaticity_x = 0;
  float white_point_chromaticity_y = 0;
  float luminance_max = 0;
  float luminance_min = 0;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct MEDIA_EXPORT HDRMetadata {
  MasteringMetadata mastering_metadata;
  unsigned max_cll = 0;
  unsigned max_fall = 0;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);
};

}  // namespace media

#endif  // MEDIA_BASE_HDR_METADATA_H_
