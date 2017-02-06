// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot.h"

#include <algorithm>

namespace ui {

namespace {

// The display serial number beginning byte position and its length in the
// EDID number as defined in the spec.
// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
const size_t kSerialNumberBeginingByte = 12U;
const size_t kSerialNumberLengthInBytes = 4U;

}  // namespace

DisplaySnapshot::DisplaySnapshot(
    int64_t display_id,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    DisplayConnectionType type,
    bool is_aspect_preserving_scaling,
    bool has_overscan,
    bool has_color_correction_matrix,
    std::string display_name,
    const base::FilePath& sys_path,
    std::vector<std::unique_ptr<const DisplayMode>> modes,
    const std::vector<uint8_t>& edid,
    const DisplayMode* current_mode,
    const DisplayMode* native_mode)
    : display_id_(display_id),
      origin_(origin),
      physical_size_(physical_size),
      type_(type),
      is_aspect_preserving_scaling_(is_aspect_preserving_scaling),
      has_overscan_(has_overscan),
      has_color_correction_matrix_(has_color_correction_matrix),
      display_name_(display_name),
      sys_path_(sys_path),
      modes_(std::move(modes)),
      edid_(edid),
      current_mode_(current_mode),
      native_mode_(native_mode),
      product_id_(kInvalidProductID) {
  // We must explicitly clear out the bytes that represent the serial number.
  const size_t end =
      std::min(kSerialNumberBeginingByte + kSerialNumberLengthInBytes,
               edid_.size());
  for (size_t i = kSerialNumberBeginingByte; i < end; ++i)
    edid_[i] = 0;
}

DisplaySnapshot::~DisplaySnapshot() {}

}  // namespace ui
