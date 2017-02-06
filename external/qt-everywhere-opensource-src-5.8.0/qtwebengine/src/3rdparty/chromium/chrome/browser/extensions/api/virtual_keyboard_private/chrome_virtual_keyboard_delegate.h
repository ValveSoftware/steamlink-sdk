// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"

namespace extensions {

class ChromeVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  ChromeVirtualKeyboardDelegate() {}
  ~ChromeVirtualKeyboardDelegate() override {}
  bool GetKeyboardConfig(base::DictionaryValue* settings) override;
  bool HideKeyboard() override;
  bool InsertText(const base::string16& text) override;
  bool OnKeyboardLoaded() override;
  void SetHotrodKeyboard(bool enable) override;
  bool LockKeyboard(bool state) override;
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override;
  bool ShowLanguageSettings() override;
  bool IsLanguageSettingsEnabled() override;
  bool SetVirtualKeyboardMode(int mode_enum) override;
  bool SetRequestedKeyboardState(int state_enum) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeVirtualKeyboardDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_
