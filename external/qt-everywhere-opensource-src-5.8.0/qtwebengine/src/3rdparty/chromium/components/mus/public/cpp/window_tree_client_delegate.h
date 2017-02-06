// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_

#include <string>

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace ui {
class Event;
}

namespace mus {

class Window;
class WindowTreeClient;

// Interface implemented by an application using mus.
class WindowTreeClientDelegate {
 public:
  // Called when the application implementing this interface is embedded at
  // |root|.
  // NOTE: this is only invoked if the WindowTreeClient is created with an
  // InterfaceRequest.
  virtual void OnEmbed(Window* root) = 0;

  // Sent when another app is embedded in |root| (one of the roots of the
  // connection). Afer this call |root| is deleted. If |root| is the only root
  // and the connection is configured to delete when there are no roots (the
  // default), then after |root| is destroyed the connection is destroyed as
  // well.
  virtual void OnUnembed(Window* root);

  // Called from the destructor of WindowTreeClient after all the Windows have
  // been destroyed. |client| is no longer valid after this call.
  virtual void OnWindowTreeClientDestroyed(WindowTreeClient* client) = 0;

  // Called when the WindowTreeClient receives an input event observed via
  // SetEventObserver(). |target| may be null for events that were sent to
  // windows owned by other processes.
  virtual void OnEventObserved(const ui::Event& event, Window* target) = 0;

 protected:
  virtual ~WindowTreeClientDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_
