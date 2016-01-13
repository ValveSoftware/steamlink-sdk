// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DEVTOOLS_AGENT_H_

#include <string>

#include "base/basictypes.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
}

namespace blink {
class WebEmbeddedWorker;
class WebString;
}

namespace content {

class EmbeddedWorkerDevToolsAgent : public IPC::Listener {
 public:
  EmbeddedWorkerDevToolsAgent(blink::WebEmbeddedWorker* webworker,
                              int route_id);
  virtual ~EmbeddedWorkerDevToolsAgent();

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnAttach(const std::string& host_id);
  void OnReattach(const std::string& host_id, const std::string& state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnResumeWorkerContext();

  blink::WebEmbeddedWorker* webworker_;
  int route_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DEVTOOLS_AGENT_H_
