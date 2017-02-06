// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_MODE_H_
#define UI_BASE_IME_TEXT_INPUT_MODE_H_

namespace ui {

// This mode corrensponds to inputmode
// http://www.whatwg.org/specs/web-apps/current-work/#attr-fe-inputmode
enum TextInputMode {
  TEXT_INPUT_MODE_DEFAULT,
  TEXT_INPUT_MODE_VERBATIM,
  TEXT_INPUT_MODE_LATIN,
  TEXT_INPUT_MODE_LATIN_NAME,
  TEXT_INPUT_MODE_LATIN_PROSE,
  TEXT_INPUT_MODE_FULL_WIDTH_LATIN,
  TEXT_INPUT_MODE_KANA,
  TEXT_INPUT_MODE_KATAKANA,
  TEXT_INPUT_MODE_NUMERIC,
  TEXT_INPUT_MODE_TEL,
  TEXT_INPUT_MODE_EMAIL,
  TEXT_INPUT_MODE_URL,

  TEXT_INPUT_MODE_MAX = TEXT_INPUT_MODE_URL,
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_TYPE_H_
