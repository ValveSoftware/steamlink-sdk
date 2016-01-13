// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/ime/input_method_delegate.h"

namespace ui {
namespace internal {

class UI_BASE_EXPORT DummyInputMethodDelegate : public InputMethodDelegate {
 public:
  DummyInputMethodDelegate();
  virtual ~DummyInputMethodDelegate();

  // Overridden from InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& key_event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethodDelegate);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_
