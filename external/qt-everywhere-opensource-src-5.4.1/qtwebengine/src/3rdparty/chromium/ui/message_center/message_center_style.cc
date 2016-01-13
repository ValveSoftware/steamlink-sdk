// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_style.h"

#include <algorithm>

namespace message_center {

// Exported values /////////////////////////////////////////////////////////////

// Colors.
const SkColor kMessageCenterBorderColor = SkColorSetRGB(0xC7, 0xCA, 0xCE);
const SkColor kMessageCenterShadowColor = SkColorSetARGB(0.5 * 255, 0, 0, 0);

// Within a notification ///////////////////////////////////////////////////////

// Colors.
const SkColor kNotificationBackgroundColor = SkColorSetRGB(255, 255, 255);
const SkColor kIconBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);
const SkColor kImageBackgroundColor = SkColorSetRGB(0x22, 0x22, 0x22);
const SkColor kRegularTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kDimTextColor = SkColorSetRGB(0x7f, 0x7f, 0x7f);
const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);

// Limits.

gfx::Size GetImageSizeForContainerSize(const gfx::Size& container_size,
                                       const gfx::Size& image_size) {
  if (container_size.IsEmpty() || image_size.IsEmpty())
    return gfx::Size();

  gfx::Size scaled_size = image_size;
  double proportion =
      scaled_size.height() / static_cast<double>(scaled_size.width());
  // We never want to return an empty image given a non-empty container and
  // image, so round the height to 1.
  scaled_size.SetSize(container_size.width(),
                      std::max(0.5 + container_size.width() * proportion, 1.0));
  if (scaled_size.height() > container_size.height()) {
    scaled_size.SetSize(
        std::max(0.5 + container_size.height() / proportion, 1.0),
        container_size.height());
  }

  return scaled_size;
}

const size_t kNotificationMaximumItems = 5;

// Timing.
const int kAutocloseDefaultDelaySeconds = 8;
const int kAutocloseHighPriorityDelaySeconds = 25;

// Colors.
const SkColor kBackgroundLightColor = SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor kBackgroundDarkColor = SkColorSetRGB(0xe7, 0xe7, 0xe7);
const SkColor kShadowColor = SkColorSetARGB(0.3 * 255, 0, 0, 0);
const SkColor kMessageCenterBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kFooterDelimiterColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kFooterTextColor = SkColorSetRGB(0x7b, 0x7b, 0x7b);

}  // namespace message_center
