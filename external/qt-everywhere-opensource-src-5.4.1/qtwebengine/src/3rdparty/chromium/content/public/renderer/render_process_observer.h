// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_PROCESS_OBSERVER_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

// Base class for objects that want to filter control IPC messages and get
// notified of events.
class CONTENT_EXPORT RenderProcessObserver {
 public:
  RenderProcessObserver() {}
  virtual ~RenderProcessObserver() {}

  // Allows filtering of control messages.
  virtual bool OnControlMessageReceived(const IPC::Message& message);

  // Notification that the render process is shutting down.
  virtual void OnRenderProcessShutdown() {}

  // Called right after the WebKit API is initialized.
  virtual void WebKitInitialized() {}

  // Called when the renderer cache of the plug-in list has changed.
  virtual void PluginListChanged() {}

  virtual void IdleNotification() {}

  // Called when the network state changes.
  virtual void NetworkStateChanged(bool online) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcessObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_PROCESS_OBSERVER_H_
