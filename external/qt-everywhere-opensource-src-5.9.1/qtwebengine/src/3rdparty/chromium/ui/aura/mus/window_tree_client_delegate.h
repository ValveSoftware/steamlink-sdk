// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_

#include <memory>
#include <string>

#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/aura_export.h"

namespace aura {

class PropertyConverter;
class Window;
class WindowTreeClient;
class WindowTreeHostMus;

namespace client {
class CaptureClient;
}

// Interface implemented by an application using mus.
class AURA_EXPORT WindowTreeClientDelegate {
 public:
  // Called when the application implementing this interface is embedded.
  // NOTE: this is only invoked if the WindowTreeClient is created with an
  // InterfaceRequest.
  virtual void OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) = 0;

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

  // Mus expects a single CaptureClient is used for all WindowTreeHosts. This
  // returns it. GetCaptureClient() is called from the constructor.
  virtual client::CaptureClient* GetCaptureClient() = 0;

  virtual PropertyConverter* GetPropertyConverter() = 0;

 protected:
  virtual ~WindowTreeClientDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_CLIENT_DELEGATE_H_
