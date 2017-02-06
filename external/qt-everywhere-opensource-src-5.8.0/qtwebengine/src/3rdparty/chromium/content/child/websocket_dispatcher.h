// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBSOCKET_DISPATCHER_H_
#define CONTENT_CHILD_WEBSOCKET_DISPATCHER_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"

namespace content {

class WebSocketBridge;

// Dispatches WebSocket related messages sent to a child process from the
// main browser process.  There is one instance per child process. Messages
// are dispatched on the main child thread.  The ChildThread class
// creates an instance of WebSocketDispatcher and delegates calls to it.
class WebSocketDispatcher : public IPC::Listener {
 public:
  WebSocketDispatcher();
  ~WebSocketDispatcher() override;

  static bool CanHandleMessage(const IPC::Message& msg);

  // Returns a unique channel id
  int AddBridge(WebSocketBridge* bridge);
  void RemoveBridge(int channel_id);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  base::WeakPtr<WebSocketDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  WebSocketBridge* GetBridge(int channel_id, uint32_t type);

  std::map<int, WebSocketBridge*> bridges_;
  int channel_id_max_;
  base::WeakPtrFactory<WebSocketDispatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBSOCKET_DISPATCHER_H_
