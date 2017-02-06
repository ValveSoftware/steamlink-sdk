// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_UTIL_H_
#define UI_KEYBOARD_KEYBOARD_UTIL_H_

#include <string>

// TODO(beng): replace with forward decl once RootWindow is renamed.
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_export.h"

namespace aura {
class WindowTreeHost;
}

class GURL;

namespace keyboard {

// Enumeration of swipe directions.
enum CursorMoveDirection {
  kCursorMoveRight = 0x01,
  kCursorMoveLeft = 0x02,
  kCursorMoveUp = 0x04,
  kCursorMoveDown = 0x08
};

// An enumeration of different keyboard control events that should be logged.
enum KeyboardControlEvent {
  KEYBOARD_CONTROL_SHOW = 0,
  KEYBOARD_CONTROL_HIDE_AUTO,
  KEYBOARD_CONTROL_HIDE_USER,
  KEYBOARD_CONTROL_MAX,
};

// An enumeration of keyboard overscroll override value.
enum KeyboardOverscrolOverride {
  KEYBOARD_OVERSCROLL_OVERRIDE_DISABLED = 0,
  KEYBOARD_OVERSCROLL_OVERRIDE_ENABLED,
  KEYBOARD_OVERSCROLL_OVERRIDE_NONE,
};

// An enumeration of keyboard policy settings.
enum KeyboardShowOverride {
  KEYBOARD_SHOW_OVERRIDE_DISABLED = 0,
  KEYBOARD_SHOW_OVERRIDE_ENABLED,
  KEYBOARD_SHOW_OVERRIDE_NONE,
};

// An enumeration of keyboard states.
enum KeyboardState {
  // Default state. System decides whether to show the keyboard or not.
  KEYBOARD_STATE_AUTO = 0,
  // Request virtual keyboard be deployed.
  KEYBOARD_STATE_ENABLED,
  // Request virtual keyboard be suppressed.
  KEYBOARD_STATE_DISABLED,
};

// Gets the calculated keyboard bounds from |root_bounds|. The keyboard height
// is specified by |keyboard_height|. This should be only called when keyboard
// is in FULL_WDITH mode.
KEYBOARD_EXPORT gfx::Rect FullWidthKeyboardBoundsFromRootBounds(
    const gfx::Rect& root_bounds, int keyboard_height);

// Sets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT void SetAccessibilityKeyboardEnabled(bool enabled);

// Gets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT bool GetAccessibilityKeyboardEnabled();

// Sets the state of the hotrod onscreen keyboard.
KEYBOARD_EXPORT void SetHotrodKeyboardEnabled(bool enabled);

// Gets the state of the hotrod onscreen keyboard.
KEYBOARD_EXPORT bool GetHotrodKeyboardEnabled();

// Sets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT void SetTouchKeyboardEnabled(bool enabled);

// Gets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT bool GetTouchKeyboardEnabled();

// Sets the requested state of the keyboard.
KEYBOARD_EXPORT void SetRequestedKeyboardState(KeyboardState state);

// Gets the requested state of the keyboard.
KEYBOARD_EXPORT int GetRequestedKeyboardState();

// Gets the default keyboard layout.
KEYBOARD_EXPORT std::string GetKeyboardLayout();

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Returns true if smart deployment of the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsSmartDeployEnabled();

// Returns true if keyboard overscroll mode is enabled.
KEYBOARD_EXPORT bool IsKeyboardOverscrollEnabled();

// Sets temporary keyboard overscroll override.
KEYBOARD_EXPORT void SetKeyboardOverscrollOverride(
    KeyboardOverscrolOverride override);

// Sets policy override on whether to show the keyboard.
KEYBOARD_EXPORT void SetKeyboardShowOverride(KeyboardShowOverride override);

// Returns true if an IME extension can specify a custom input view for the
// virtual keyboard window.
KEYBOARD_EXPORT bool IsInputViewEnabled();

// Returns true if experimental features are enabled for IME input-views.
KEYBOARD_EXPORT bool IsExperimentalInputViewEnabled();

// Returns true if floating virtual keyboard feature is enabled.
KEYBOARD_EXPORT bool IsFloatingVirtualKeyboardEnabled();

// Returns true if gesture typing option is enabled for virtual keyboard.
KEYBOARD_EXPORT bool IsGestureTypingEnabled();

// Returns true if gesture editing option is enabled for virtual keyboard.
KEYBOARD_EXPORT bool IsGestureEditingEnabled();

// Returns true if voice input is enabled for the keyboard.
KEYBOARD_EXPORT bool IsVoiceInputEnabled();

// Insert |text| into the active TextInputClient if there is one. Returns true
// if |text| was successfully inserted.
KEYBOARD_EXPORT bool InsertText(const base::string16& text);

// Move cursor when swipe on the virtualkeyboard. Returns true if cursor was
// successfully moved according to |swipe_direction|.
KEYBOARD_EXPORT bool MoveCursor(int swipe_direction,
                                int modifier_flags,
                                aura::WindowTreeHost* host);

// Sends a fabricated key event, where |type| is the event type, |key_value|
// is the unicode value of the character, |key_code| is the legacy key code
// value, |key_name| is the name of the key as defined in the DOM3 key event
// specification, and |modifier| indicates if any modifier keys are being
// virtually pressed. The event is dispatched to the active TextInputClient
// associated with |root_window|. The type may be "keydown" or "keyup".
KEYBOARD_EXPORT bool SendKeyEvent(std::string type,
                                   int key_value,
                                   int key_code,
                                   std::string key_name,
                                   int modifiers,
                                   aura::WindowTreeHost* host);

// Marks that the keyboard load has started. This is used to measure the time it
// takes to fully load the keyboard. This should be called before
// MarkKeyboardLoadFinished.
KEYBOARD_EXPORT void MarkKeyboardLoadStarted();

// Marks that the keyboard load has ended. This finishes measuring that the
// keyboard is loaded.
KEYBOARD_EXPORT void MarkKeyboardLoadFinished();

// Sets the override content url.
// This is used by for input view for extension IMEs.
KEYBOARD_EXPORT void SetOverrideContentUrl(const GURL& url);

// Gets the override content url.
KEYBOARD_EXPORT const GURL& GetOverrideContentUrl();

// Logs the keyboard control event as a UMA stat.
void LogKeyboardControlEvent(KeyboardControlEvent event);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
