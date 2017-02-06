// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/default_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {
namespace ws {

DefaultAccessPolicy::DefaultAccessPolicy() {}

DefaultAccessPolicy::~DefaultAccessPolicy() {}

void DefaultAccessPolicy::Init(ClientSpecificId client_id,
                               AccessPolicyDelegate* delegate) {
  client_id_ = client_id;
  delegate_ = delegate;
}

bool DefaultAccessPolicy::CanRemoveWindowFromParent(
    const ServerWindow* window) const {
  if (!WasCreatedByThisClient(window))
    return false;  // Can only unparent windows we created.

  return delegate_->HasRootForAccessPolicy(window->parent()) ||
         WasCreatedByThisClient(window->parent());
}

bool DefaultAccessPolicy::CanAddWindow(const ServerWindow* parent,
                                       const ServerWindow* child) const {
  return WasCreatedByThisClient(child) &&
         (delegate_->HasRootForAccessPolicy(parent) ||
          (WasCreatedByThisClient(parent) &&
           !delegate_->IsWindowRootOfAnotherTreeForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanAddTransientWindow(
    const ServerWindow* parent,
    const ServerWindow* child) const {
  return (delegate_->HasRootForAccessPolicy(child) ||
          WasCreatedByThisClient(child)) &&
         (delegate_->HasRootForAccessPolicy(parent) ||
          WasCreatedByThisClient(parent));
}

bool DefaultAccessPolicy::CanRemoveTransientWindowFromParent(
    const ServerWindow* window) const {
  return (delegate_->HasRootForAccessPolicy(window) ||
          WasCreatedByThisClient(window)) &&
         (delegate_->HasRootForAccessPolicy(window->transient_parent()) ||
          WasCreatedByThisClient(window->transient_parent()));
}

bool DefaultAccessPolicy::CanSetModal(const ServerWindow* window) const {
  return delegate_->HasRootForAccessPolicy(window) ||
         WasCreatedByThisClient(window);
}

bool DefaultAccessPolicy::CanReorderWindow(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    mojom::OrderDirection direction) const {
  return WasCreatedByThisClient(window) &&
         WasCreatedByThisClient(relative_window);
}

bool DefaultAccessPolicy::CanDeleteWindow(const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool DefaultAccessPolicy::CanGetWindowTree(const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanDescendIntoWindowForWindowTree(
    const ServerWindow* window) const {
  return (WasCreatedByThisClient(window) &&
          !delegate_->IsWindowRootOfAnotherTreeForAccessPolicy(window)) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanEmbed(const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool DefaultAccessPolicy::CanChangeWindowVisibility(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanChangeWindowOpacity(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetWindowSurface(
    const ServerWindow* window,
    mojom::SurfaceType surface_type) const {
  if (surface_type == mojom::SurfaceType::UNDERLAY)
    return WasCreatedByThisClient(window);

  // Once a window embeds another app, the embedder app is no longer able to
  // call SetWindowSurfaceId() - this ability is transferred to the embedded
  // app.
  if (delegate_->IsWindowRootOfAnotherTreeForAccessPolicy(window))
    return false;
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetWindowBounds(const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool DefaultAccessPolicy::CanSetWindowProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window);
}

bool DefaultAccessPolicy::CanSetWindowTextInputState(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetCapture(const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetFocus(const ServerWindow* window) const {
  return !window || WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetClientArea(const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetHitTestMask(const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetCursorProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerWindow* window,
    const ServerWindow** new_parent,
    const ServerWindow** old_parent) const {
  if (!WasCreatedByThisClient(window))
    return false;

  if (*new_parent && !WasCreatedByThisClient(*new_parent) &&
      !delegate_->HasRootForAccessPolicy((*new_parent))) {
    *new_parent = nullptr;
  }

  if (*old_parent && !WasCreatedByThisClient(*old_parent) &&
      !delegate_->HasRootForAccessPolicy((*old_parent))) {
    *old_parent = nullptr;
  }
  return true;
}

const ServerWindow* DefaultAccessPolicy::GetWindowForFocusChange(
    const ServerWindow* focused) {
  if (WasCreatedByThisClient(focused) ||
      delegate_->HasRootForAccessPolicy(focused))
    return focused;
  return nullptr;
}

bool DefaultAccessPolicy::CanSetWindowManager() const {
  return false;
}

bool DefaultAccessPolicy::WasCreatedByThisClient(
    const ServerWindow* window) const {
  return window->id().client_id == client_id_;
}

bool DefaultAccessPolicy::IsValidIdForNewWindow(
    const ClientWindowId& id) const {
  // Clients using DefaultAccessPolicy only see windows they have created (for
  // the embed point they choose the id), so it's ok for clients to use whatever
  // id they want.
  return true;
}

}  // namespace ws
}  // namespace mus
