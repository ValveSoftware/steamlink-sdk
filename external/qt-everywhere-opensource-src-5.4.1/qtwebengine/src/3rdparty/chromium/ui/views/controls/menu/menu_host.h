// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_

#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeWidget;
class SubmenuView;
class View;
class Widget;

// SubmenuView uses a MenuHost to house the SubmenuView.
//
// SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
// |DestroyMenuHost| is invoked. The one exception to this is if the native
// OS destroys the widget out from under us, in which case |MenuHostDestroyed|
// is invoked back on the SubmenuView and the SubmenuView then drops references
// to the MenuHost.
class MenuHost : public Widget {
 public:
  explicit MenuHost(SubmenuView* submenu);
  virtual ~MenuHost();

  // Initializes and shows the MenuHost.
  // WARNING: |parent| may be NULL.
  void InitMenuHost(Widget* parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture);

  // Returns true if the menu host is visible.
  bool IsMenuHostVisible();

  // Shows the menu host. If |do_capture| is true the menu host should do a
  // mouse grab.
  void ShowMenuHost(bool do_capture);

  // Hides the menu host.
  void HideMenuHost();

  // Destroys and deletes the menu host.
  void DestroyMenuHost();

  // Sets the bounds of the menu host.
  void SetMenuHostBounds(const gfx::Rect& bounds);

  // Releases a mouse grab installed by |ShowMenuHost|.
  void ReleaseMenuHostCapture();

 private:
  // Overridden from Widget:
  virtual internal::RootView* CreateRootView() OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnNativeWidgetDestroyed() OVERRIDE;
  virtual void OnOwnerClosing() OVERRIDE;

  // The view we contain.
  SubmenuView* submenu_;

  // If true, DestroyMenuHost has been invoked.
  bool destroying_;

  // If true and capture is lost we don't notify the delegate.
  bool ignore_capture_lost_;

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_
