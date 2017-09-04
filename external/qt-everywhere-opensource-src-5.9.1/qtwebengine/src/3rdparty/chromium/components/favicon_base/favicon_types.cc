// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_types.h"

#include "components/favicon_base/fallback_icon_style.h"

namespace favicon_base {

// ---------------------------------------------------------
// FaviconImageResult

FaviconImageResult::FaviconImageResult() {}

FaviconImageResult::~FaviconImageResult() {}

// --------------------------------------------------------
// FaviconRawBitmapResult

FaviconRawBitmapResult::FaviconRawBitmapResult()
    : expired(false), icon_type(INVALID_ICON) {
}

FaviconRawBitmapResult::FaviconRawBitmapResult(
    const FaviconRawBitmapResult& other) = default;

FaviconRawBitmapResult::~FaviconRawBitmapResult() {}

// --------------------------------------------------------
// LargeIconResult

LargeIconResult::LargeIconResult(const FaviconRawBitmapResult& bitmap_in)
    : bitmap(bitmap_in) {}

LargeIconResult::LargeIconResult(FallbackIconStyle* fallback_icon_style_in)
    : fallback_icon_style(fallback_icon_style_in) {}

LargeIconResult::~LargeIconResult() {}

}  // namespace favicon_base
