// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_FORWARDING_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_FORWARDING_MESSAGE_FILTER_H_

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"

namespace content {

// This class can be used to intercept routed messages and
// deliver them to a compositor task runner than they would otherwise
// be sent. Messages are filtered based on type. To route these messages,
// add a Handler to the filter.
//
// The user of this class implements CompositorForwardingMessageFilter::Handler,
// which will receive the intercepted messages, on the compositor thread.
// The caller must ensure that each handler in |multi_handlers_| outlives the
// lifetime of the filter.
// User can add multiple handlers for specific routing id. When messages have
// arrived, all handlers in |multi_handlers_| for routing id will be executed.
//
// Note: When we want to add new message, add a new switch case in
// OnMessageReceived() then add a new Handler for it.
class CONTENT_EXPORT CompositorForwardingMessageFilter
    : public IPC::MessageFilter {
 public:
  // The handler is invoked on the compositor thread with messages that were
  // intercepted by this filter.
  typedef base::Callback<void(const IPC::Message&)> Handler;

  // This filter will intercept messages defined in OnMessageReceived() and run
  // them by ProcessMessageOnCompositorThread on compositor thread.
  CompositorForwardingMessageFilter(base::TaskRunner* compositor_task_runner);

  void AddHandlerOnCompositorThread(int routing_id, const Handler& handler);
  void RemoveHandlerOnCompositorThread(int routing_id, const Handler& handler);

  // MessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~CompositorForwardingMessageFilter() override;

 private:
  void ProcessMessageOnCompositorThread(const IPC::Message& message);

  // The handler only gets run on the compositor thread.
  scoped_refptr<base::TaskRunner> compositor_task_runner_;

  // This is used to check some functions that are only running on compositor
  // thread.
  base::ThreadChecker compositor_thread_checker_;

  // Maps the routing_id for which messages should be filtered and handlers
  // which will be routed.
  std::multimap<int, Handler> multi_handlers_;

  DISALLOW_COPY_AND_ASSIGN(CompositorForwardingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_FORWARDING_MESSAGE_FILTER_H_
