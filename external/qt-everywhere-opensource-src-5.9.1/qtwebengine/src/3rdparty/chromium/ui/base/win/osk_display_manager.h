// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_OSK_DISPLAY_MANAGER_H_
#define UI_BASE_WIN_OSK_DISPLAY_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class OnScreenKeyboardDetector;
class OnScreenKeyboardObserver;

// This class provides functionality to display the on screen keyboard on
// Windows 8+. It optionally notifies observers that the OSK is displayed,
// hidden, etc.
class UI_BASE_EXPORT OnScreenKeyboardDisplayManager {
 public:
  static OnScreenKeyboardDisplayManager* GetInstance();

  ~OnScreenKeyboardDisplayManager();

  // Functions to display and dismiss the keyboard.
  // The optional |observer| parameter allows callers to be notified when the
  // keyboard is displayed, dismissed, etc.
  bool DisplayVirtualKeyboard(OnScreenKeyboardObserver* observer);
  // When the keyboard is dismissed, the registered observer if any is removed
  // after notifying it.
  bool DismissVirtualKeyboard();

  // Removes a registered observer.
  void RemoveObserver(OnScreenKeyboardObserver* observer);

  // Returns the path of the on screen keyboard exe (TabTip.exe) in the
  // |osk_path| parameter.
  // Returns true on success.
  bool GetOSKPath(base::string16* osk_path);

 private:
  OnScreenKeyboardDisplayManager();

  std::unique_ptr<OnScreenKeyboardDetector> keyboard_detector_;

  // The location of TabTip.exe.
  base::string16 osk_path_;

  DISALLOW_COPY_AND_ASSIGN(OnScreenKeyboardDisplayManager);
};

}  // namespace ui

#endif  // UI_BASE_WIN_OSK_DISPLAY_MANAGER_H_
