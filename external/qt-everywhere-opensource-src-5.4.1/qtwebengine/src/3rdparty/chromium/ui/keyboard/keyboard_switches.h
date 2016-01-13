// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_SWITCHES_H_
#define UI_KEYBOARD_KEYBOARD_SWITCHES_H_

#include "ui/keyboard/keyboard_export.h"

namespace keyboard {
namespace switches {

// Enables the swipe selection feature on the virtual keyboard.
KEYBOARD_EXPORT extern const char kEnableSwipeSelection[];

// Disables IME extension APIs from overriding the URL for specifying the
// contents of the virtual keyboard container.
KEYBOARD_EXPORT extern const char kDisableInputView[];

// Enables an IME extension API to set a URL for specifying the contents
// of the virtual keyboard container.
KEYBOARD_EXPORT extern const char kEnableInputView[];

// Enables experimental features for IME extensions.
KEYBOARD_EXPORT extern const char kEnableExperimentalInputViewFeatures[];

// Enables the virtual keyboard.
KEYBOARD_EXPORT extern const char kEnableVirtualKeyboard[];

// Disabled overscrolling of web content when the virtual keyboard is displayed.
// If disabled, the work area is resized to restrict windows from overlapping
// with the keybaord area.
KEYBOARD_EXPORT extern const char kDisableVirtualKeyboardOverscroll[];

// Enable overscrolling of web content when the virtual keyboard is displayed
// to provide access to content that would otherwise be occluded.
KEYBOARD_EXPORT extern const char kEnableVirtualKeyboardOverscroll[];

// Enables the keyboard usability experiment. This is an experimental mode for
// testing the usability of various experimental keyboard layouts.
KEYBOARD_EXPORT extern const char kKeyboardUsabilityExperiment[];

}  // namespace switches
}  // namespace keyboard

#endif  //  UI_KEYBOARD_KEYBOARD_SWITCHES_H_
