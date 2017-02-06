// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_DELEGATE_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

class VirtualKeyboardDelegate {
 public:
  virtual ~VirtualKeyboardDelegate() {}

  // Fetch information about the preferred configuration of the keyboard. On
  // exit, |settings| is populated with the keyboard configuration. Returns true
  // if successful.
  virtual bool GetKeyboardConfig(base::DictionaryValue* settings) = 0;

  // Dismiss the virtual keyboard without changing input focus. Returns true if
  // successful.
  virtual bool HideKeyboard() = 0;

  // Insert |text| verbatim into a text area. Returns true if successful.
  virtual bool InsertText(const base::string16& text) = 0;

  // Notifiy system that keyboard loading is complete. Used in UMA stats to
  // track loading performance. Returns true if the notification was handled.
  virtual bool OnKeyboardLoaded() = 0;

  // Indicate if settings are accessible and enabled based on current state.
  // For example, settings should be blocked when the session is locked.
  virtual bool IsLanguageSettingsEnabled() = 0;

  // Sets the state of the hotrod virtual keyboad.
  virtual void SetHotrodKeyboard(bool enable) = 0;

  // Activate and lock the virtual keyboad on screen or dismiss the keyboard
  // regardless of the state of text focus. Used in a11y mode to allow typing
  // hotkeys without the need for text focus. Returns true if successful.
  virtual bool LockKeyboard(bool state) = 0;

  // Dispatches a virtual key event. |type| indicates if the event is a keydown
  // or keyup event. |char_value| is the unicode value for the key. |key_code|
  // is the code assigned to the key, which is independent of the state of
  // modifier keys. |key_name| is the standardized w3c name for the key.
  // |modifiers| indicates which modifier keys are active. Returns true if
  // successful.
  virtual bool SendKeyEvent(const std::string& type,
                            int char_value,
                            int key_code,
                            const std::string& key_name,
                            int modifiers) = 0;

  // Launches the settings app. Returns true if successful.
  virtual bool ShowLanguageSettings() = 0;

  // Sets virtual keyboard window mode.
  virtual bool SetVirtualKeyboardMode(int mode_enum) = 0;

  // Sets requested virtual keyboard state.
  virtual bool SetRequestedKeyboardState(int state_enum) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_DELEGATE_H_
