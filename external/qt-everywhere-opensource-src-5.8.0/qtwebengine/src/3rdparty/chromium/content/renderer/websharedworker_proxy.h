// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_
#define CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebSharedWorkerConnector.h"
#include "url/gurl.h"

namespace IPC {
class MessageRouter;
}

namespace content {

// Implementation of the WebSharedWorker APIs. This object is intended to only
// live long enough to allow the caller to send a "connect" event to the worker
// thread. Once the connect event has been sent, all future communication will
// happen via the WebMessagePortChannel, and the WebSharedWorker instance will
// be freed.
class WebSharedWorkerProxy : public blink::WebSharedWorkerConnector,
                             private IPC::Listener {
 public:
  // If the worker not loaded yet, route_id == MSG_ROUTING_NONE
  WebSharedWorkerProxy(IPC::MessageRouter* router, int route_id);
  ~WebSharedWorkerProxy() override;

  // Implementations of WebSharedWorkerConnector APIs
  void connect(blink::WebMessagePortChannel* channel,
               ConnectListener* listener) override;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Disconnects the worker (stops listening for incoming messages).
  void Disconnect();

  // Sends a message to the worker thread (forwarded via the RenderViewHost).
  // If WorkerStarted() has not yet been called, message is queued.
  bool Send(IPC::Message*);

  // Returns true if there are queued messages.
  bool HasQueuedMessages() { return !queued_messages_.empty(); }

  // Sends any messages currently in the queue.
  void SendQueuedMessages();

  void OnWorkerCreated();
  void OnWorkerScriptLoadFailed();
  void OnWorkerConnected();

  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  IPC::MessageRouter* const router_;

  // Stores messages that were sent before the StartWorkerContext message.
  std::vector<IPC::Message*> queued_messages_;

  ConnectListener* connect_listener_;
  bool created_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_
