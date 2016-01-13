// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SOCKET_STREAM_DISPATCHER_H_
#define CONTENT_CHILD_SOCKET_STREAM_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ipc/ipc_listener.h"

namespace blink {
class WebSocketStreamHandle;
}

namespace content {

class WebSocketStreamHandleBridge;
class WebSocketStreamHandleDelegate;

// Dispatches socket stream related messages sent to a child process from the
// main browser process.  There is one instance per child process.  Messages
// are dispatched on the main child thread.  The RenderThread class
// creates an instance of SocketStreamDispatcher and delegates calls to it.
class SocketStreamDispatcher : public IPC::Listener {
 public:
  SocketStreamDispatcher();
  virtual ~SocketStreamDispatcher() {}

  static WebSocketStreamHandleBridge* CreateBridge(
      blink::WebSocketStreamHandle* handle,
      WebSocketStreamHandleDelegate* delegate);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  void OnConnected(int socket_id, int max_amount_send_allowed);
  void OnSentData(int socket_id, int amount_sent);
  void OnReceivedData(int socket_id, const std::vector<char>& data);
  void OnClosed(int socket_id);
  void OnFailed(int socket_id, int error_code);

  DISALLOW_COPY_AND_ASSIGN(SocketStreamDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SOCKET_STREAM_DISPATCHER_H_
