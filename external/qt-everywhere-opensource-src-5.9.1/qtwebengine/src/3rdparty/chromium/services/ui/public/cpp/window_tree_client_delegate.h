// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_

#include <string>

#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {
class Event;
}

namespace ui {

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
  // connection). Afer this call |root| is deleted. If the associated
  // WindowTreeClient was created from a WindowTreeClientRequest then
  // OnEmbedRootDestroyed() is called after the window is deleted.
  virtual void OnUnembed(Window* root);

  // Called when the embed root has been destroyed on the server side (or
  // another client was embedded in the same window). This is only called if the
  // WindowTreeClient was created from an InterfaceRequest. Generally when this
  // is called the delegate should delete the WindowTreeClient.
  virtual void OnEmbedRootDestroyed(Window* root) = 0;

  // Called when the connection to the window server has been lost. After this
  // call the windows are still valid, and you can still do things, but they
  // have no real effect. Generally when this is called clients should delete
  // the corresponding WindowTreeClient.
  virtual void OnLostConnection(WindowTreeClient* client) = 0;

  // Called when the WindowTreeClient receives an input event observed via
  // StartPointerWatcher(). |target| may be null for events that were sent to
  // windows owned by other processes.
  virtual void OnPointerEventObserved(const ui::PointerEvent& event,
                                      Window* target) = 0;

 protected:
  virtual ~WindowTreeClientDelegate() {}
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_DELEGATE_H_
