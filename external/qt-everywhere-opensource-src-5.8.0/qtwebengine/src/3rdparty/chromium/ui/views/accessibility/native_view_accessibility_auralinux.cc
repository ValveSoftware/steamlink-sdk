// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility_auralinux.h"

#include <algorithm>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

// ATK requires that we have a single root "application" object that's the
// owner of all other windows. This is a simple class that implements the
// AXPlatformNodeDelegate interface so we can create such an application
// object. Every time we create an accessibility object for a View, we add its
// top-level widget to a vector so we can return the list of all top-level
// windows as children of this application object.
class AuraLinuxApplication
    : public ui::AXPlatformNodeDelegate,
      public WidgetObserver {
 public:
  // Get the single instance of this class.
  static AuraLinuxApplication* GetInstance() {
    return base::Singleton<AuraLinuxApplication>::get();
  }

  // Called every time we create a new accessibility on a View.
  // Add the top-level widget to our registry so that we can enumerate all
  // top-level widgets.
  void RegisterWidget(Widget* widget) {
    if (!widget)
      return;

    widget = widget->GetTopLevelWidget();
    if (ContainsValue(widgets_, widget))
      return;

    widgets_.push_back(widget);
    widget->AddObserver(this);
  }

  gfx::NativeViewAccessible GetNativeViewAccessible() {
    return platform_node_->GetNativeViewAccessible();
  }

  //
  // WidgetObserver overrides.
  //

  void OnWidgetDestroying(Widget* widget) override {
    auto iter = std::find(widgets_.begin(), widgets_.end(), widget);
    if (iter != widgets_.end())
      widgets_.erase(iter);
  }

  //
  // ui::AXPlatformNodeDelegate overrides.
  //

  const ui::AXNodeData& GetData() override {
    return data_;
  }

  gfx::NativeViewAccessible GetParent() override {
    return nullptr;
  }

  int GetChildCount() override {
    return static_cast<int>(widgets_.size());
  }

  gfx::NativeViewAccessible ChildAtIndex(int index) override {
    if (index < 0 || index >= GetChildCount())
      return nullptr;

    Widget* widget = widgets_[index];
    CHECK(widget);
    return widget->GetRootView()->GetNativeViewAccessible();
  }

  gfx::Vector2d GetGlobalCoordinateOffset() override {
    return gfx::Vector2d();
  }

  gfx::NativeViewAccessible HitTestSync(int x, int y) override {
    return nullptr;
  }

  gfx::NativeViewAccessible GetFocus() override {
    return nullptr;
  }

  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override {
    return gfx::kNullAcceleratedWidget;
  }

  void DoDefaultAction() override {
  }

  bool SetStringValue(const base::string16& new_value) override {
    return false;
  }

 private:
  friend struct base::DefaultSingletonTraits<AuraLinuxApplication>;

  AuraLinuxApplication()
      : platform_node_(ui::AXPlatformNode::Create(this)) {
    data_.role = ui::AX_ROLE_APPLICATION;
    if (ViewsDelegate::GetInstance()) {
      data_.AddStringAttribute(
          ui::AX_ATTR_NAME,
          ViewsDelegate::GetInstance()->GetApplicationName());
    }
    ui::AXPlatformNodeAuraLinux::SetApplication(platform_node_);
    if (ViewsDelegate::GetInstance()) {
      // This should be on the a blocking pool thread so that we can open
      // libatk-bridge.so without blocking this thread.
      scoped_refptr<base::TaskRunner> init_task_runner =
          ViewsDelegate::GetInstance()->GetBlockingPoolTaskRunner();
      if (init_task_runner)
        ui::AXPlatformNodeAuraLinux::StaticInitialize(init_task_runner);
    }
  }

  ~AuraLinuxApplication() override {
    platform_node_->Destroy();
    platform_node_ = nullptr;
  }

  ui::AXPlatformNode* platform_node_;
  ui::AXNodeData data_;
  std::vector<Widget*> widgets_;

  DISALLOW_COPY_AND_ASSIGN(AuraLinuxApplication);
};

}  // namespace

// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  AuraLinuxApplication::GetInstance()->RegisterWidget(view->GetWidget());
  return new NativeViewAccessibilityAuraLinux(view);
}

NativeViewAccessibilityAuraLinux::NativeViewAccessibilityAuraLinux(View* view)
    : NativeViewAccessibility(view) {
}

NativeViewAccessibilityAuraLinux::~NativeViewAccessibilityAuraLinux() {
}

gfx::NativeViewAccessible NativeViewAccessibilityAuraLinux::GetParent() {
  gfx::NativeViewAccessible parent = NativeViewAccessibility::GetParent();
  if (!parent)
    parent = AuraLinuxApplication::GetInstance()->GetNativeViewAccessible();
  return parent;
}

}  // namespace views
