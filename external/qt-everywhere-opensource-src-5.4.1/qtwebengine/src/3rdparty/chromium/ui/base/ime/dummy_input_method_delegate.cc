// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_input_method_delegate.h"

namespace ui {
namespace internal {

DummyInputMethodDelegate::DummyInputMethodDelegate() {}
DummyInputMethodDelegate::~DummyInputMethodDelegate() {}

bool DummyInputMethodDelegate::DispatchKeyEventPostIME(
    const ui::KeyEvent& key_event) {
  return true;
}

}  // namespace internal
}  // namespace ui
