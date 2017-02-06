// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_CONSTANTS_H_
#define UI_MESSAGE_CENTER_VIEWS_CONSTANTS_H_

#include <stddef.h>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_style.h"

namespace message_center {

// The text background colors below are used only to keep
// view::Label from modifying the text color and will not actually be drawn.
// See view::Label's RecalculateColors() for details.
const SkColor kRegularTextBackgroundColor = SK_ColorWHITE;
const SkColor kDimTextBackgroundColor = SK_ColorWHITE;
const SkColor kContextTextBackgroundColor = SK_ColorWHITE;

const int kTextBottomPadding = 12;
const int kItemTitleToMessagePadding = 3;
const int kButtonVecticalPadding = 0;
const int kButtonTitleTopPadding = 0;
const int kNotificationSettingsPadding = 5;

// Character limits: Displayed text will be subject to the line limits above,
// but we also remove trailing characters from text to reduce processing cost.
// Character limit = pixels per line * line limit / min. pixels per character.
const int kMinPixelsPerTitleCharacter = 4;
const size_t kMessageCharacterLimit =
    message_center::kNotificationWidth *
        message_center::kMessageExpandedLineLimit / 3;
const size_t kContextMessageCharacterLimit =
    message_center::kNotificationWidth *
    message_center::kContextMessageLineLimit / 3;

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_CONSTANTS_H_
