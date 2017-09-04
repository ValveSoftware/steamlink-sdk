// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this file to assert that *_list.h enums that are meant to do the bridge
// from Blink are valid.

#include "base/macros.h"
#include "content/common/input/touch_action.h"
#include "content/public/common/screen_orientation_values.h"
#include "media/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebTextInputMode.h"
#include "third_party/WebKit/public/platform/WebTextInputType.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/web/WebFrameSerializerCacheControlPolicy.h"
#include "third_party/WebKit/public/web/WebTouchAction.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

// ScreenOrientationValues
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockDefault,
                   SCREEN_ORIENTATION_VALUES_DEFAULT);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortraitPrimary,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortraitSecondary,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscapePrimary,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscapeSecondary,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockAny,
                   SCREEN_ORIENTATION_VALUES_ANY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscape,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortrait,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockNatural,
                   SCREEN_ORIENTATION_VALUES_NATURAL);

// WebTextInputMode
STATIC_ASSERT_ENUM(blink::kWebTextInputModeDefault,
                   ui::TEXT_INPUT_MODE_DEFAULT);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeVerbatim,
                   ui::TEXT_INPUT_MODE_VERBATIM);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeLatin, ui::TEXT_INPUT_MODE_LATIN);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeLatinName,
                   ui::TEXT_INPUT_MODE_LATIN_NAME);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeLatinProse,
                   ui::TEXT_INPUT_MODE_LATIN_PROSE);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeFullWidthLatin,
                   ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeKana, ui::TEXT_INPUT_MODE_KANA);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeKanaName,
                   ui::TEXT_INPUT_MODE_KANA_NAME);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeKataKana,
                   ui::TEXT_INPUT_MODE_KATAKANA);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeTel, ui::TEXT_INPUT_MODE_TEL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeEmail, ui::TEXT_INPUT_MODE_EMAIL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeUrl, ui::TEXT_INPUT_MODE_URL);

// WebTextInputType
STATIC_ASSERT_ENUM(blink::WebTextInputTypeNone, ui::TEXT_INPUT_TYPE_NONE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeText, ui::TEXT_INPUT_TYPE_TEXT);
STATIC_ASSERT_ENUM(blink::WebTextInputTypePassword,
                   ui::TEXT_INPUT_TYPE_PASSWORD);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeSearch, ui::TEXT_INPUT_TYPE_SEARCH);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeEmail, ui::TEXT_INPUT_TYPE_EMAIL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeNumber, ui::TEXT_INPUT_TYPE_NUMBER);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTelephone,
                   ui::TEXT_INPUT_TYPE_TELEPHONE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeURL, ui::TEXT_INPUT_TYPE_URL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDate, ui::TEXT_INPUT_TYPE_DATE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTime,
                   ui::TEXT_INPUT_TYPE_DATE_TIME);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTimeLocal,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeMonth, ui::TEXT_INPUT_TYPE_MONTH);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTime, ui::TEXT_INPUT_TYPE_TIME);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeWeek, ui::TEXT_INPUT_TYPE_WEEK);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTextArea,
                   ui::TEXT_INPUT_TYPE_TEXT_AREA);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeContentEditable,
                   ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTimeField,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD);

// Check blink::WebTouchAction and content::TouchAction is kept in sync.
STATIC_ASSERT_ENUM(blink::WebTouchActionNone, TOUCH_ACTION_NONE);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanLeft, TOUCH_ACTION_PAN_LEFT);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanRight, TOUCH_ACTION_PAN_RIGHT);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanX, TOUCH_ACTION_PAN_X);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanUp, TOUCH_ACTION_PAN_UP);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanDown, TOUCH_ACTION_PAN_DOWN);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanY, TOUCH_ACTION_PAN_Y);
STATIC_ASSERT_ENUM(blink::WebTouchActionPan, TOUCH_ACTION_PAN);
STATIC_ASSERT_ENUM(blink::WebTouchActionPinchZoom, TOUCH_ACTION_PINCH_ZOOM);
STATIC_ASSERT_ENUM(blink::WebTouchActionManipulation,
                   TOUCH_ACTION_MANIPULATION);
STATIC_ASSERT_ENUM(blink::WebTouchActionDoubleTapZoom,
                   TOUCH_ACTION_DOUBLE_TAP_ZOOM);
STATIC_ASSERT_ENUM(blink::WebTouchActionAuto, TOUCH_ACTION_AUTO);

} // namespace content
