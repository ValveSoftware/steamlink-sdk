// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_

#include "ui/accessibility/ax_enums.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;

class VIEWS_EXPORT NativeViewAccessibility {
 public:
  static NativeViewAccessibility* Create(View* view);

  virtual void NotifyAccessibilityEvent(
      ui::AXEvent event_type) {}

  virtual gfx::NativeViewAccessible GetNativeObject();

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  virtual void Destroy();

  // WebViews need to be registered because they implement their own
  // tree of accessibility objects, and we need to check them when
  // mapping a child id to a NativeViewAccessible.
  static void RegisterWebView(View* web_view);
  static void UnregisterWebView(View* web_view);

 protected:
  NativeViewAccessibility();
  virtual ~NativeViewAccessibility();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibility);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
