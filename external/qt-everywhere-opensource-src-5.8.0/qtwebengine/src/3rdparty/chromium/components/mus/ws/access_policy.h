// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ACCESS_POLICY_H_
#define COMPONENTS_MUS_WS_ACCESS_POLICY_H_

#include <stdint.h>

#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/ids.h"

namespace mus {
namespace ws {

class AccessPolicyDelegate;
class ServerWindow;

// AccessPolicy is used by WindowTree to determine what the WindowTree is
// allowed to do.
class AccessPolicy {
 public:
  virtual ~AccessPolicy() {}

  virtual void Init(ClientSpecificId client_id,
                    AccessPolicyDelegate* delegate) = 0;

  // Unless otherwise mentioned all arguments have been validated. That is the
  // |window| arguments are non-null unless otherwise stated (eg CanSetWindow()
  // is allowed to take a NULL window).
  virtual bool CanRemoveWindowFromParent(const ServerWindow* window) const = 0;
  virtual bool CanAddWindow(const ServerWindow* parent,
                            const ServerWindow* child) const = 0;
  virtual bool CanAddTransientWindow(const ServerWindow* parent,
                                     const ServerWindow* child) const = 0;
  virtual bool CanRemoveTransientWindowFromParent(
      const ServerWindow* window) const = 0;
  virtual bool CanSetModal(const ServerWindow* window) const = 0;
  virtual bool CanReorderWindow(const ServerWindow* window,
                                const ServerWindow* relative_window,
                                mojom::OrderDirection direction) const = 0;
  virtual bool CanDeleteWindow(const ServerWindow* window) const = 0;
  virtual bool CanGetWindowTree(const ServerWindow* window) const = 0;
  // Used when building a window tree (GetWindowTree()) to decide if we should
  // descend into |window|.
  virtual bool CanDescendIntoWindowForWindowTree(
      const ServerWindow* window) const = 0;
  virtual bool CanEmbed(const ServerWindow* window) const = 0;
  virtual bool CanChangeWindowVisibility(const ServerWindow* window) const = 0;
  virtual bool CanChangeWindowOpacity(const ServerWindow* window) const = 0;
  virtual bool CanSetWindowSurface(const ServerWindow* window,
                                   mojom::SurfaceType surface_type) const = 0;
  virtual bool CanSetWindowBounds(const ServerWindow* window) const = 0;
  virtual bool CanSetWindowProperties(const ServerWindow* window) const = 0;
  virtual bool CanSetWindowTextInputState(const ServerWindow* window) const = 0;
  virtual bool CanSetCapture(const ServerWindow* window) const = 0;
  virtual bool CanSetFocus(const ServerWindow* window) const = 0;
  virtual bool CanSetClientArea(const ServerWindow* window) const = 0;
  virtual bool CanSetHitTestMask(const ServerWindow* window) const = 0;
  // Used for all client controllable cursor properties; which cursor should be
  // displayed, visibility, locking, etc.
  virtual bool CanSetCursorProperties(const ServerWindow* window) const = 0;

  // Returns whether the client should notify on a hierarchy change.
  // |new_parent| and |old_parent| are initially set to the new and old parents
  // but may be altered so that the client only sees a certain set of windows.
  virtual bool ShouldNotifyOnHierarchyChange(
      const ServerWindow* window,
      const ServerWindow** new_parent,
      const ServerWindow** old_parent) const = 0;
  virtual bool CanSetWindowManager() const = 0;

  // Returns the window to supply to the client when focus changes to |focused|.
  virtual const ServerWindow* GetWindowForFocusChange(
      const ServerWindow* focused) = 0;

  virtual bool IsValidIdForNewWindow(const ClientWindowId& id) const = 0;
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ACCESS_POLICY_H_
