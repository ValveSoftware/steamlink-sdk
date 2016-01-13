// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

#include "ui/base/ui_base_export.h"

namespace ui {

class KeyEvent;

namespace internal {

// An interface implemented by the object that handles events sent back from an
// ui::InputMethod implementation.
class UI_BASE_EXPORT InputMethodDelegate {
 public:
  virtual ~InputMethodDelegate() {}

  // Dispatch a key event already processed by the input method.
  // Returns true if the event was processed.
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& key_event) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
