// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_
#define COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_

#include "base/memory/ref_counted_memory.h"
#include "third_party/skia/include/core/SkColor.h"

namespace favicon_base {

// Styling specifications of a fallback icon. The icon is composed of a solid
// rounded square containing a single letter. The specification excludes the
// icon URL and size, which are given when the icon is rendered.
struct FallbackIconStyle {
  FallbackIconStyle();
  ~FallbackIconStyle();

  // If any member changes, also update FallbackIconStyleBuilder.

  // Icon background fill color.
  SkColor background_color;
  bool is_default_background_color;

  // Icon text color.
  SkColor text_color;

  // Ratio in [0.0, 1.0] of the text font size (pixels) to the icon size.
  double font_size_ratio;

  // The roundness of the icon's corners. 0 => square icon, 1 => circle icon.
  double roundness;

  bool operator==(const FallbackIconStyle& other) const;
};

// Reassigns |style|'s |text_color| to matches well against |background_color|.
void MatchFallbackIconTextColorAgainstBackgroundColor(FallbackIconStyle* style);

// Returns whether |style| values are within bounds.
bool ValidateFallbackIconStyle(const FallbackIconStyle& style);

// Set |style|'s background color to the dominant color of |bitmap_data|,
// clamping luminance down to a reasonable maximum value so that light text is
// readable.
void SetDominantColorAsBackground(
    const scoped_refptr<base::RefCountedMemory>& bitmap_data,
    FallbackIconStyle* style);

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FALLBACK_ICON_STYLE_H_
