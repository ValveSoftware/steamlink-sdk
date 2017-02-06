// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {
namespace ws {

WindowManagerAccessPolicy::WindowManagerAccessPolicy() {}

WindowManagerAccessPolicy::~WindowManagerAccessPolicy() {}

void WindowManagerAccessPolicy::Init(ClientSpecificId client_id,
                                     AccessPolicyDelegate* delegate) {
  client_id_ = client_id;
  delegate_ = delegate;
}

bool WindowManagerAccessPolicy::CanRemoveWindowFromParent(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddWindow(const ServerWindow* parent,
                                             const ServerWindow* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddTransientWindow(
    const ServerWindow* parent,
    const ServerWindow* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanRemoveTransientWindowFromParent(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanSetModal(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanReorderWindow(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    mojom::OrderDirection direction) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDeleteWindow(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanGetWindowTree(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDescendIntoWindowForWindowTree(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanEmbed(const ServerWindow* window) const {
  return !delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::CanChangeWindowVisibility(
    const ServerWindow* window) const {
  if (WasCreatedByThisClient(window))
    return true;
  // The WindowManager can change the visibility of the WindowManager root.
  const ServerWindow* root = window->GetRoot();
  return root && window->parent() == root;
}

bool WindowManagerAccessPolicy::CanChangeWindowOpacity(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanSetWindowSurface(
    const ServerWindow* window,
    mus::mojom::SurfaceType surface_type) const {
  if (surface_type == mojom::SurfaceType::UNDERLAY)
    return WasCreatedByThisClient(window);

  if (delegate_->IsWindowRootOfAnotherTreeForAccessPolicy(window))
    return false;
  return WasCreatedByThisClient(window) ||
         (delegate_->HasRootForAccessPolicy(window));
}

bool WindowManagerAccessPolicy::CanSetWindowBounds(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanSetWindowProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanSetWindowTextInputState(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanSetCapture(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool WindowManagerAccessPolicy::CanSetFocus(const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanSetClientArea(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::CanSetHitTestMask(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::CanSetCursorProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerWindow* window,
    const ServerWindow** new_parent,
    const ServerWindow** old_parent) const {
  // Notify if we've already told the window manager about the window, or if
  // we've
  // already told the window manager about the parent. The later handles the
  // case of a window that wasn't parented to the root getting added to the
  // root.
  return IsWindowKnown(window) || (*new_parent && IsWindowKnown(*new_parent));
}

bool WindowManagerAccessPolicy::CanSetWindowManager() const {
  return true;
}

const ServerWindow* WindowManagerAccessPolicy::GetWindowForFocusChange(
    const ServerWindow* focused) {
  return focused;
}

bool WindowManagerAccessPolicy::IsWindowKnown(
    const ServerWindow* window) const {
  return delegate_->IsWindowKnownForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::IsValidIdForNewWindow(
    const ClientWindowId& id) const {
  // The WindowManager see windows created from other clients. If the WM doesn't
  // use the client id when creating windows the WM could end up with two
  // windows with the same id. Because of this the wm must use the same
  // client id for all windows it creates.
  return WindowIdFromTransportId(id.id).client_id == client_id_;
}

bool WindowManagerAccessPolicy::WasCreatedByThisClient(
    const ServerWindow* window) const {
  return window->id().client_id == client_id_;
}

}  // namespace ws
}  // namespace mus
