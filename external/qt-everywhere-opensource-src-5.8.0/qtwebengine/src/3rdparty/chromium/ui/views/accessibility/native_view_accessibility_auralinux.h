// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_AURALINUX_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_AURALINUX_H_

#include "base/macros.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/view.h"

namespace views {

class NativeViewAccessibilityAuraLinux : public NativeViewAccessibility {
 public:
  NativeViewAccessibilityAuraLinux(View* view);
  ~NativeViewAccessibilityAuraLinux() override;

  // NativeViewAccessibility.
  gfx::NativeViewAccessible GetParent() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibilityAuraLinux);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_AURALINUX_H_
