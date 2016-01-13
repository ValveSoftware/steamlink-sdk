// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MESSAGE_FILTER_H_

#include "base/callback_forward.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class WorkerDevToolsMessageFilter : public BrowserMessageFilter {
 public:
  explicit WorkerDevToolsMessageFilter(int worker_process_host_id);

 private:
  virtual ~WorkerDevToolsMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  // Message handlers.
  void OnDispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRumtimeState(const std::string& state);

  int worker_process_host_id_;
  int current_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MESSAGE_FILTER_H_
