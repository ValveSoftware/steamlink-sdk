// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include <stddef.h>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// Exported values /////////////////////////////////////////////////////////////

// Square image sizes in DIPs.
const int kNotificationButtonIconSize = 16;
const int kNotificationIconSize = 80;
// A border is applied to images that have a non-preferred aspect ratio.
const int kNotificationImageBorderSize = 10;
const int kNotificationPreferredImageWidth = 360;
const int kNotificationPreferredImageHeight = 240;
const int kSmallImageSize = 16;
const int kSmallImagePadding = 4;

// Limits.
const size_t kMaxVisibleMessageCenterNotifications = 100;
const size_t kMaxVisiblePopupNotifications = 3;

// DIP dimension; H size of the whole card.
const int kNotificationWidth = 360;
const int kMinScrollViewHeight = 100;

// Colors.
const SkColor kMessageCenterBorderColor = SkColorSetRGB(0xC7, 0xCA, 0xCE);
const SkColor kMessageCenterShadowColor = SkColorSetARGB(0.5 * 255, 0, 0, 0);

// Settings dialog constants.
namespace settings {

const SkColor kEntrySeparatorColor = SkColorSetARGB(0.1 * 255, 0, 0, 0);
const int kEntryHeight = 45;
const int kEntrySeparatorHeight = 1;
const int kHorizontalMargin = 10;
const int kTopMargin = 20;
const int kTitleToDescriptionSpace = 20;
const int kEntryIconSize = 16;
const int kDescriptionToSwitcherSpace = 15;
const int kInternalHorizontalSpacing = 10;
const int kCheckboxSizeWithPadding = 24;

}  // namespace settings

// Within a notification ///////////////////////////////////////////////////////

// DIP dimensions (H = horizontal, V = vertical).

const int kControlButtonSize = 29;  // Square size of close & expand buttons.
const int kIconToTextPadding = 16;  // H space between icon & title/message.
const int kTextTopPadding = 12;     // V space between text elements.
const int kIconBottomPadding = 16;  // Minimum non-zero V space between icon
                                    // and frame.
// H space between the context message and the end of the card.
const int kTextRightPadding = 23;
const int kTextLeftPadding = kNotificationIconSize + kIconToTextPadding;
const int kContextMessageViewWidth =
    kNotificationWidth - kTextLeftPadding - kTextRightPadding;

// Text sizes.
const int kTitleFontSize = 14;             // For title only.
const int kEmptyCenterFontSize = 13;       // For empty message only.
const int kTitleLineHeight = 20;           // In DIPs.
const int kMessageFontSize = 12;           // For everything but title.
const int kMessageLineHeight = 18;         // In DIPs.

// Colors.
// Background of the card.
const SkColor kNotificationBackgroundColor = SkColorSetRGB(255, 255, 255);
// Background of the image.
const SkColor kImageBackgroundColor = SkColorSetRGB(0x22, 0x22, 0x22);
// Used behind icons smaller than the icon view.
const SkColor kIconBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);
// Title, message, ...
const SkColor kRegularTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kDimTextColor = SkColorSetRGB(0x7f, 0x7f, 0x7f);
// The focus border.
const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);
// Foreground of small icon image.
const SkColor kSmallImageMaskForegroundColor = SK_ColorWHITE;
// Background of small icon image.
const SkColor kSmallImageMaskBackgroundColor = SkColorSetRGB(0xa3, 0xa3, 0xa);

// Limits.

// Given the size of an image, returns the size of the properly scaled-up image
// which fits into |container_size|.
MESSAGE_CENTER_EXPORT gfx::Size GetImageSizeForContainerSize(
    const gfx::Size& container_size,
    const gfx::Size& image_size);

const size_t kNotificationMaximumItems = 5;     // For list notifications.

// Timing.
const int kAutocloseDefaultDelaySeconds = 8;
const int kAutocloseHighPriorityDelaySeconds = 25;
// Web notifications use a larger timeout for now, which improves re-engagement.
// TODO(johnme): Use Finch to experiment with different values, then consider
// replacing kAutocloseDefaultDelaySeconds with this (https://crbug.com/530697)
const int kAutocloseWebPageDelaySeconds = 20;

// Buttons.
const int kButtonHeight = 38;              // In DIPs.
const int kButtonHorizontalPadding = 16;   // In DIPs.
const int kButtonIconTopPadding = 11;      // In DIPs.
const int kButtonIconToTitlePadding = 16;  // In DIPs.

#if !defined(OS_LINUX) || defined(USE_AURA)
const SkColor kButtonSeparatorColor = SkColorSetRGB(234, 234, 234);
const SkColor kHoveredButtonBackgroundColor = SkColorSetRGB(243, 243, 243);
#endif

// Progress bar.
const int kProgressBarThickness = 5;
const int kProgressBarTopPadding = 16;
const int kProgressBarCornerRadius = 3;
const SkColor kProgressBarBackgroundColor = SkColorSetARGB(26, 0, 0, 0);
const SkColor kProgressBarSliceColor = SkColorSetRGB(26, 194, 34);

// Line limits.
const int kMaxTitleLines = 2;
const int kMessageCollapsedLineLimit = 2;
const int kMessageExpandedLineLimit = 5;
const int kContextMessageLineLimit = 1;

// Around notifications ////////////////////////////////////////////////////////

// DIP dimensions (H = horizontal, V = vertical).
const int kMarginBetweenItems = 10;  // H & V space around & between
                                     // notifications.

// Colors.
// Behind notifications, gradient
const SkColor kBackgroundLightColor = SkColorSetRGB(0xf1, 0xf1, 0xf1);
// from light to dark.
const SkColor kBackgroundDarkColor = SkColorSetRGB(0xe7, 0xe7, 0xe7);

// Shadow in the tray.
const SkColor kShadowColor = SkColorSetARGB(0.3 * 255, 0, 0, 0);

const SkColor kMessageCenterBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
// Separator color for the tray.
const SkColor kFooterDelimiterColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
// Text color for tray labels.
const SkColor kFooterTextColor = SkColorSetRGB(0x7b, 0x7b, 0x7b);

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
