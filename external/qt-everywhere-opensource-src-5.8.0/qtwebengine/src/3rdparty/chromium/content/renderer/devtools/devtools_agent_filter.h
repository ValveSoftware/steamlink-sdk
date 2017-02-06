// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_FILTER_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_FILTER_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/macros.h"
#include "ipc/message_filter.h"

struct DevToolsMessageData;

namespace base {
class MessageLoop;
class SingleThreadTaskRunner;
}

namespace content {

// DevToolsAgentFilter is registered as an IPC filter in order to be able to
// dispatch messages while on the IO thread. The reason for that is that while
// debugging, Render thread is being held by the v8 and hence no messages
// are being dispatched there. While holding the thread in a tight loop,
// v8 provides thread-safe Api for controlling debugger. In our case v8's Api
// is being used from this communication agent on the IO thread.
class DevToolsAgentFilter : public IPC::MessageFilter {
 public:
  // There is a single instance of this class instantiated by the RenderThread.
  DevToolsAgentFilter();

  static void SendRpcMessage(const DevToolsMessageData& data);

  // IPC::MessageFilter override. Called on IO thread.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Called on the main thread.
  void AddEmbeddedWorkerRouteOnMainThread(int32_t routing_id);
  void RemoveEmbeddedWorkerRouteOnMainThread(int32_t routing_id);

 protected:
  ~DevToolsAgentFilter() override;

 private:
  void OnDispatchOnInspectorBackend(
      int session_id,
      int call_id,
      const std::string& method,
      const std::string& message);

  // Called on IO thread
  void AddEmbeddedWorkerRoute(int32_t routing_id);
  void RemoveEmbeddedWorkerRoute(int32_t routing_id);

  base::MessageLoop* render_thread_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  int current_routing_id_;

  std::set<int32_t> embedded_worker_routes_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_FILTER_H_
