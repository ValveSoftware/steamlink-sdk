// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

// Set PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL if this platform has a
// specific implementation of NativeViewAccessibility::Create().
#undef PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL

#if defined(OS_WIN)
#define PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_LINUX) && defined(USE_X11) && !defined(OS_CHROMEOS)
#define PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL 1
#endif

namespace views {

class View;
class Widget;

class VIEWS_EXPORT NativeViewAccessibility
    : public ui::AXPlatformNodeDelegate,
      public WidgetObserver {
 public:
  static NativeViewAccessibility* Create(View* view);

  gfx::NativeViewAccessible GetNativeObject();

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  virtual void Destroy();

  void NotifyAccessibilityEvent(ui::AXEvent event_type);

  // ui::AXPlatformNodeDelegate
  const ui::AXNodeData& GetData() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::NativeViewAccessible GetParent() override;
  gfx::Vector2d GetGlobalCoordinateOffset() override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  void DoDefaultAction() override;
  bool SetStringValue(const base::string16& new_value) override;

  // WidgetObserver
  void OnWidgetDestroying(Widget* widget) override;

  Widget* parent_widget() const { return parent_widget_; }
  void SetParentWidget(Widget* parent_widget);

 protected:
  NativeViewAccessibility(View* view);
  ~NativeViewAccessibility() override;

  // Weak. Owns this.
  View* view_;

  // Weak. Uses WidgetObserver to clear. This is set on the root view for
  // a widget that's owned by another widget, so we can walk back up the
  // tree.
  Widget* parent_widget_;

 private:
  void PopulateChildWidgetVector(std::vector<Widget*>* result_child_widgets);

  // We own this, but it is reference-counted on some platforms so we can't use
  // a scoped_ptr. It is dereferenced in the destructor.
  ui::AXPlatformNode* ax_node_;

  ui::AXNodeData data_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibility);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
