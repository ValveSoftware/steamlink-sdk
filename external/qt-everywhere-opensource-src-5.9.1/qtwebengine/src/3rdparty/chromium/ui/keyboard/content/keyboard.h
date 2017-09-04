// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_H_
#define UI_KEYBOARD_KEYBOARD_H_

#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

// Initializes the keyboard module. This includes adding the necessary pak files
// for loading resources used in for the virtual keyboard. This becomes a no-op
// after the first call.
KEYBOARD_EXPORT void InitializeKeyboard();

}  // namespace keyboard

#endif  //  UI_KEYBOARD_KEYBOARD_H_
