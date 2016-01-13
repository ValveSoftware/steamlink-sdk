// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_FORWARDING_MESSAGE_FILTER_H_
#define IPC_IPC_FORWARDING_MESSAGE_FILTER_H_

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "ipc/message_filter.h"

namespace IPC {

// This class can be used to intercept routed messages and
// deliver them to a different task runner than they would otherwise
// be sent. Messages are filtered based on type. To route these messages,
// add a MessageRouter to the handler.
//
// The user of this class implements ForwardingMessageFilter::Client,
// which will receive the intercepted messages, on the specified target thread.
class IPC_EXPORT ForwardingMessageFilter : public MessageFilter {
 public:

  // The handler is invoked on the thread associated with
  // |target_task_runner| with messages that were intercepted by this filter.
  typedef base::Callback<void(const Message&)> Handler;

  // This filter will intercept |message_ids_to_filter| and post
  // them to the provided |target_task_runner|, where they will be given
  // to |handler|.
  //
  // The caller must ensure that |handler| outlives the lifetime of the filter.
  ForwardingMessageFilter(
      const uint32* message_ids_to_filter,
      size_t num_message_ids_to_filter,
      base::TaskRunner* target_task_runner);

  // Define the message routes to be filtered.
  void AddRoute(int routing_id, const Handler& handler);
  void RemoveRoute(int routing_id);

  // MessageFilter methods:
  virtual bool OnMessageReceived(const Message& message) OVERRIDE;

 private:
  virtual ~ForwardingMessageFilter();

  std::set<int> message_ids_to_filter_;

  // The handler_ only gets Run on the thread corresponding to
  // target_task_runner_.
  scoped_refptr<base::TaskRunner> target_task_runner_;

  // Protects access to routes_.
  base::Lock handlers_lock_;

  // Indicates the routing_ids for which messages should be filtered.
  std::map<int, Handler> handlers_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingMessageFilter);
};

}  // namespace IPC

#endif  // IPC_IPC_FORWARDING_MESSAGE_FILTER_H_
