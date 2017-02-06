// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/color_space.h"

namespace gfx {

namespace {
static const size_t kMinProfileLength = 128;
static const size_t kMaxProfileLength = 4 * 1024 * 1024;
}

ColorSpace::ColorSpace() = default;
ColorSpace::ColorSpace(ColorSpace&& other) = default;
ColorSpace::ColorSpace(const ColorSpace& other) = default;
ColorSpace& ColorSpace::operator=(const ColorSpace& other) = default;
ColorSpace::~ColorSpace() = default;

bool ColorSpace::operator==(const ColorSpace& other) const {
  return icc_profile_ == other.icc_profile_;
}

ColorSpace ColorSpace::FromICCProfile(const std::vector<char>& icc_profile) {
  ColorSpace color_space;
  if (IsValidProfileLength(icc_profile.size()))
    color_space.icc_profile_ = icc_profile;
  return color_space;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
ColorSpace ColorSpace::FromBestMonitor() {
  return ColorSpace();
}
#endif

// static
bool ColorSpace::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
