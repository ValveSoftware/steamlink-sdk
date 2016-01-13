// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_host_root_view.h"

#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

MenuHostRootView::MenuHostRootView(Widget* widget,
                                   SubmenuView* submenu)
    : internal::RootView(widget),
      submenu_(submenu),
      forward_drag_to_menu_controller_(true) {
}

bool MenuHostRootView::OnMousePressed(const ui::MouseEvent& event) {
  forward_drag_to_menu_controller_ =
      !GetLocalBounds().Contains(event.location()) ||
      !RootView::OnMousePressed(event) ||
      DoesEventTargetEmptyMenuItem(event);

  if (forward_drag_to_menu_controller_ && GetMenuController())
    GetMenuController()->OnMousePressed(submenu_, event);
  return true;
}

bool MenuHostRootView::OnMouseDragged(const ui::MouseEvent& event) {
  if (forward_drag_to_menu_controller_ && GetMenuController()) {
    GetMenuController()->OnMouseDragged(submenu_, event);
    return true;
  }
  return RootView::OnMouseDragged(event);
}

void MenuHostRootView::OnMouseReleased(const ui::MouseEvent& event) {
  RootView::OnMouseReleased(event);
  if (forward_drag_to_menu_controller_ && GetMenuController()) {
    forward_drag_to_menu_controller_ = false;
    GetMenuController()->OnMouseReleased(submenu_, event);
  }
}

void MenuHostRootView::OnMouseMoved(const ui::MouseEvent& event) {
  RootView::OnMouseMoved(event);
  if (GetMenuController())
    GetMenuController()->OnMouseMoved(submenu_, event);
}

bool MenuHostRootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return GetMenuController() &&
      GetMenuController()->OnMouseWheel(submenu_, event);
}

void MenuHostRootView::DispatchGestureEvent(ui::GestureEvent* event) {
  RootView::DispatchGestureEvent(event);
  if (event->handled())
    return;
  // ChromeOS uses MenuController to forward events like other
  // mouse events.
  if (!GetMenuController())
    return;
  GetMenuController()->OnGestureEvent(submenu_, event);
}

MenuController* MenuHostRootView::GetMenuController() {
  return submenu_ ? submenu_->GetMenuItem()->GetMenuController() : NULL;
}

bool MenuHostRootView::DoesEventTargetEmptyMenuItem(
    const ui::MouseEvent& event) {
  View* view = GetEventHandlerForPoint(event.location());
  return view && view->id() == MenuItemView::kEmptyMenuItemViewID;
}

}  // namespace views
