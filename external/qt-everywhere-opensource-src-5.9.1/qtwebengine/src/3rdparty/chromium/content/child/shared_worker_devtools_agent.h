// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SHARED_WORKER_DEVTOOLS_AGENT_H_
#define CONTENT_CHILD_SHARED_WORKER_DEVTOOLS_AGENT_H_

#include <string>

#include "base/macros.h"

namespace IPC {
class Message;
}

namespace blink {
class WebSharedWorker;
class WebString;
}

namespace content {

class SharedWorkerDevToolsAgent {
 public:
  SharedWorkerDevToolsAgent(int route_id, blink::WebSharedWorker*);
  ~SharedWorkerDevToolsAgent();

  // Called on the Worker thread.
  bool OnMessageReceived(const IPC::Message& message);
  void SendDevToolsMessage(int session_id,
                           int call_id,
                           const blink::WebString& message,
                           const blink::WebString& post_state);

 private:
  void OnAttach(const std::string& host_id, int session_id);
  void OnReattach(const std::string& host_id,
                  int session_id,
                  const std::string& state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(
      int session_id,
      int call_id,
      const std::string& method,
      const std::string& message);

  bool Send(IPC::Message* message);
  const int route_id_;
  blink::WebSharedWorker* webworker_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_CHILD_SHARED_WORKER_DEVTOOLS_AGENT_H_
