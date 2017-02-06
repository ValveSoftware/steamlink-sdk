// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_TRANSIENT_WINDOW_CLIENT_H_
#define UI_WM_PUBLIC_TRANSIENT_WINDOW_CLIENT_H_

#include "ui/aura/aura_export.h"

namespace aura {

class Window;

namespace client {

// TransientWindowClient is used to add or remove transient windows. Transient
// children get the following behavior:
// . The transient parent destroys any transient children when it is
//   destroyed. This means a transient child is destroyed if either its parent
//   or transient parent is destroyed.
// . If a transient child and its transient parent share the same parent, then
//   transient children are always ordered above the transient parent.
// Transient windows are typically used for popups and menus.
// TODO(sky): nuke this class and replace with calls to TransientWindowManager.
// This is temporary until we start moving to ui/wm.
class AURA_EXPORT TransientWindowClient {
 public:
  virtual void AddTransientChild(Window* parent, Window* child) = 0;
  virtual void RemoveTransientChild(Window* parent, Window* child) = 0;
  virtual Window* GetTransientParent(Window* window) = 0;
  virtual const Window* GetTransientParent(const Window* window) = 0;

 protected:
  virtual ~TransientWindowClient() {}
};

// Sets/gets the TransientWindowClient. This does *not* take ownership of
// |client|. It is assumed the caller will invoke SetTransientWindowClient(NULL)
// before deleting |client|.
AURA_EXPORT void SetTransientWindowClient(TransientWindowClient* client);
AURA_EXPORT TransientWindowClient* GetTransientWindowClient();

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_TRANSIENT_WINDOW_CLIENT_H_
