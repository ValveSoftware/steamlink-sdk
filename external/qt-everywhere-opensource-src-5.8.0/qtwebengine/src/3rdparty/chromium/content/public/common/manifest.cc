// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest.h"

namespace content {

// We need to provide a value here which is out of the range of a 32-bit integer
// since otherwise we would not be able to check whether a theme color was valid
// or not. The simplest way to do this is to simply add one to the maximum
// possible 32-bit integer.
const int64_t Manifest::kInvalidOrMissingColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
const size_t Manifest::kMaxIPCStringLength = 4 * 1024;

Manifest::Icon::Icon() { }

Manifest::Icon::Icon(const Icon& other) = default;

Manifest::Icon::~Icon() {
}

Manifest::RelatedApplication::RelatedApplication() {
}

Manifest::RelatedApplication::~RelatedApplication() {
}

Manifest::Manifest()
    : display(blink::WebDisplayModeUndefined),
      orientation(blink::WebScreenOrientationLockDefault),
      prefer_related_applications(false),
      theme_color(Manifest::kInvalidOrMissingColor),
      background_color(Manifest::kInvalidOrMissingColor) {
}

Manifest::Manifest(const Manifest& other) = default;

Manifest::~Manifest() {
}

bool Manifest::IsEmpty() const {
  return name.is_null() &&
         short_name.is_null() &&
         start_url.is_empty() &&
         display == blink::WebDisplayModeUndefined &&
         orientation == blink::WebScreenOrientationLockDefault &&
         icons.empty() &&
         related_applications.empty() &&
         !prefer_related_applications &&
         theme_color == Manifest::kInvalidOrMissingColor &&
         background_color == Manifest::kInvalidOrMissingColor &&
         gcm_sender_id.is_null() &&
         scope.is_empty();
}

} // namespace content
