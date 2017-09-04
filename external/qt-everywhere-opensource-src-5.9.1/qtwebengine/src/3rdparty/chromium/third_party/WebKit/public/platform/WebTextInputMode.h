// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTextInputMode_h
#define WebTextInputMode_h

namespace blink {

// This mode corrensponds to inputmode
// https://html.spec.whatwg.org/#attr-fe-inputmode
enum WebTextInputMode {
  kWebTextInputModeDefault,
  kWebTextInputModeVerbatim,
  kWebTextInputModeLatin,
  kWebTextInputModeLatinName,
  kWebTextInputModeLatinProse,
  kWebTextInputModeFullWidthLatin,
  kWebTextInputModeKana,
  kWebTextInputModeKanaName,
  kWebTextInputModeKataKana,
  kWebTextInputModeNumeric,
  kWebTextInputModeTel,
  kWebTextInputModeEmail,
  kWebTextInputModeUrl,
  kWebTextInputModeMax = kWebTextInputModeUrl,
};

}  // namespace blink

#endif  // WebTextInputMode_h
