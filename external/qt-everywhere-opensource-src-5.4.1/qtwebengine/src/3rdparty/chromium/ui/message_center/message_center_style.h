// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"
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
const int kSettingsIconSize = 16;
const int kSmallImageSize = 16;
const int kSmallImagePadding = 4;

// Limits.
const size_t kMaxVisibleMessageCenterNotifications = 100;
const size_t kMaxVisiblePopupNotifications = 3;

// DIP dimension; H size of the whole card.
const int kNotificationWidth = 360;
const int kMinScrollViewHeight = 100;

// Colors.
MESSAGE_CENTER_EXPORT extern const SkColor kMessageCenterBorderColor;
MESSAGE_CENTER_EXPORT extern const SkColor kMessageCenterShadowColor;

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

// Text sizes.
const int kTitleFontSize = 14;             // For title only.
const int kEmptyCenterFontSize = 13;       // For empty message only.
const int kTitleLineHeight = 20;           // In DIPs.
const int kMessageFontSize = 12;           // For everything but title.
const int kMessageLineHeight = 18;         // In DIPs.

// Colors.
extern const SkColor kNotificationBackgroundColor; // Background of the card.
extern const SkColor kImageBackgroundColor;        // Background of the image.
extern const SkColor kIconBackgroundColor;         // Used behind icons smaller
                                                   // than the icon view.
extern const SkColor kRegularTextColor;            // Title, message, ...
extern const SkColor kDimTextColor;
extern const SkColor kFocusBorderColor;  // The focus border.

// Limits.

// Given the size of an image, returns the size of the properly scaled-up image
// which fits into |container_size|.
gfx::Size GetImageSizeForContainerSize(const gfx::Size& container_size,
                                       const gfx::Size& image_size);

extern const int kNotificationMaximumImageHeight;  // For image notifications.
extern const size_t kNotificationMaximumItems;     // For list notifications.

// Timing.
extern const int kAutocloseDefaultDelaySeconds;
extern const int kAutocloseHighPriorityDelaySeconds;

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
const SkColor kProgressBarBackgroundColor = SkColorSetRGB(216, 216, 216);
const SkColor kProgressBarSliceColor = SkColorSetRGB(120, 120, 120);

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
extern const SkColor kBackgroundLightColor;  // Behind notifications, gradient
extern const SkColor kBackgroundDarkColor;   // from light to dark.

extern const SkColor kShadowColor;           // Shadow in the tray.

extern const SkColor kMessageCenterBackgroundColor;
extern const SkColor kFooterDelimiterColor;  // Separator color for the tray.
extern const SkColor kFooterTextColor;       // Text color for tray labels.

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
