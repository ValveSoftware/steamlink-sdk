// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_

#include "base/macros.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/view.h"

namespace views {

class NativeViewAccessibilityWin : public NativeViewAccessibility {
 public:
  NativeViewAccessibilityWin(View* view);
  ~NativeViewAccessibilityWin() override;

  // NativeViewAccessibility.
  gfx::NativeViewAccessible GetParent() override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibilityWin);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_WIN_H_
