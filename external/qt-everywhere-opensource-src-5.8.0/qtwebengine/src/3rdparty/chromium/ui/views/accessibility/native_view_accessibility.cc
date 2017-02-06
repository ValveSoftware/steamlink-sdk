// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

#if !defined(PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL)
// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  return new NativeViewAccessibility(view);
}
#endif  // !defined(PLATFORM_HAS_NATIVE_VIEW_ACCESSIBILITY_IMPL)

NativeViewAccessibility::NativeViewAccessibility(View* view)
    : view_(view),
      parent_widget_(nullptr),
      ax_node_(nullptr) {
  ax_node_ = ui::AXPlatformNode::Create(this);
}

NativeViewAccessibility::~NativeViewAccessibility() {
  if (ax_node_)
    ax_node_->Destroy();
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
}

gfx::NativeViewAccessible NativeViewAccessibility::GetNativeObject() {
  return ax_node_ ? ax_node_->GetNativeViewAccessible() : nullptr;
}

void NativeViewAccessibility::Destroy() {
  delete this;
}

void NativeViewAccessibility::NotifyAccessibilityEvent(ui::AXEvent event_type) {
  if (ax_node_)
    ax_node_->NotifyAccessibilityEvent(event_type);
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& NativeViewAccessibility::GetData() {
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  data_ = ui::AXNodeData();
  data_.role = state.role;
  data_.state = state.state();
  data_.location = view_->GetBoundsInScreen();
  data_.AddStringAttribute(ui::AX_ATTR_NAME, base::UTF16ToUTF8(state.name));
  data_.AddStringAttribute(ui::AX_ATTR_VALUE, base::UTF16ToUTF8(state.value));
  data_.AddStringAttribute(ui::AX_ATTR_ACTION,
                           base::UTF16ToUTF8(state.default_action));
  data_.AddStringAttribute(ui::AX_ATTR_SHORTCUT,
                           base::UTF16ToUTF8(state.keyboard_shortcut));
  data_.AddStringAttribute(ui::AX_ATTR_PLACEHOLDER,
                           base::UTF16ToUTF8(state.placeholder));
  data_.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, state.selection_start);
  data_.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, state.selection_end);

  data_.state |= (1 << ui::AX_STATE_FOCUSABLE);

  if (!view_->enabled())
    data_.state |= (1 << ui::AX_STATE_DISABLED);

  if (!view_->visible())
    data_.state |= (1 << ui::AX_STATE_INVISIBLE);

  return data_;
}

int NativeViewAccessibility::GetChildCount() {
  int child_count = view_->child_count();

  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  child_count += child_widgets.size();

  return child_count;
}

gfx::NativeViewAccessible NativeViewAccessibility::ChildAtIndex(int index) {
  // If this is a root view, our widget might have child widgets. Include
  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  int child_widget_count = static_cast<int>(child_widgets.size());

  if (index < view_->child_count()) {
    return view_->child_at(index)->GetNativeViewAccessible();
  } else if (index < view_->child_count() + child_widget_count) {
    Widget* child_widget = child_widgets[index - view_->child_count()];
    return child_widget->GetRootView()->GetNativeViewAccessible();
  }

  return nullptr;
}

gfx::NativeViewAccessible NativeViewAccessibility::GetParent() {
  if (view_->parent())
    return view_->parent()->GetNativeViewAccessible();

  // TODO: move this to NativeViewAccessibilityMac.
#if defined(OS_MACOSX)
  if (view_->GetWidget())
    return view_->GetWidget()->GetNativeView();
#endif

  if (parent_widget_)
    return parent_widget_->GetRootView()->GetNativeViewAccessible();

  return nullptr;
}

gfx::Vector2d NativeViewAccessibility::GetGlobalCoordinateOffset() {
  return gfx::Vector2d(0, 0);  // location is already in screen coordinates.
}

gfx::NativeViewAccessible NativeViewAccessibility::HitTestSync(int x, int y) {
  if (!view_ || !view_->GetWidget())
    return nullptr;

  // Search child widgets first, since they're on top in the z-order.
  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  for (Widget* child_widget : child_widgets) {
    View* child_root_view = child_widget->GetRootView();
    gfx::Point point(x, y);
    View::ConvertPointFromScreen(child_root_view, &point);
    if (child_root_view->HitTestPoint(point))
      return child_root_view->GetNativeViewAccessible();
  }

  gfx::Point point(x, y);
  View::ConvertPointFromScreen(view_, &point);
  if (!view_->HitTestPoint(point))
    return nullptr;

  // Check if the point is within any of the immediate children of this
  // view. We don't have to search further because AXPlatformNode will
  // do a recursive hit test if we return anything other than |this| or NULL.
  for (int i = view_->child_count() - 1; i >= 0; --i) {
    View* child_view = view_->child_at(i);
    if (!child_view->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    view_->ConvertPointToTarget(view_, child_view, &point_in_child_coords);
    if (child_view->HitTestPoint(point_in_child_coords))
      return child_view->GetNativeViewAccessible();
  }

  // If it's not inside any of our children, it's inside this view.
  return GetNativeObject();
}

gfx::NativeViewAccessible NativeViewAccessibility::GetFocus() {
  FocusManager* focus_manager = view_->GetFocusManager();
  View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  return focused_view ? focused_view->GetNativeViewAccessible() : nullptr;
}

gfx::AcceleratedWidget
NativeViewAccessibility::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

void NativeViewAccessibility::DoDefaultAction() {
  gfx::Point center = view_->GetLocalBounds().CenterPoint();
  view_->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                                       center,
                                       center,
                                       ui::EventTimeForNow(),
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                       ui::EF_LEFT_MOUSE_BUTTON));
  view_->OnMouseReleased(ui::MouseEvent(ui::ET_MOUSE_RELEASED,
                                        center,
                                        center,
                                        ui::EventTimeForNow(),
                                        ui::EF_LEFT_MOUSE_BUTTON,
                                        ui::EF_LEFT_MOUSE_BUTTON));
}

bool NativeViewAccessibility::SetStringValue(const base::string16& new_value) {
  // Return an error if the view can't set the value.
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  if (state.set_value_callback.is_null())
    return false;

  state.set_value_callback.Run(new_value);
  return true;
}

void NativeViewAccessibility::OnWidgetDestroying(Widget* widget) {
  if (parent_widget_ == widget) {
    parent_widget_->RemoveObserver(this);
    parent_widget_ = nullptr;
  }
}

void NativeViewAccessibility::SetParentWidget(Widget* parent_widget) {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  parent_widget_ = parent_widget;
  parent_widget_->AddObserver(this);
}

void NativeViewAccessibility::PopulateChildWidgetVector(
    std::vector<Widget*>* result_child_widgets) {
  // Only attach child widgets to the root view.
  Widget* widget = view_->GetWidget();
  if (!widget || widget->GetRootView() != view_)
    return;

  std::set<Widget*> child_widgets;
  Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
  for (auto iter = child_widgets.begin(); iter != child_widgets.end(); ++iter) {
    Widget* child_widget = *iter;
    DCHECK_NE(widget, child_widget);

    if (!child_widget->IsVisible())
      continue;

    if (widget->GetNativeWindowProperty(kWidgetNativeViewHostKey))
      continue;

    gfx::NativeViewAccessible child_widget_accessible =
        child_widget->GetRootView()->GetNativeViewAccessible();
    ui::AXPlatformNode* child_widget_platform_node =
        ui::AXPlatformNode::FromNativeViewAccessible(child_widget_accessible);
    if (child_widget_platform_node) {
      NativeViewAccessibility* child_widget_view_accessibility =
          static_cast<NativeViewAccessibility*>(
              child_widget_platform_node->GetDelegate());
      if (child_widget_view_accessibility->parent_widget() != widget)
        child_widget_view_accessibility->SetParentWidget(widget);
    }

    result_child_widgets->push_back(child_widget);
  }
}

}  // namespace views
