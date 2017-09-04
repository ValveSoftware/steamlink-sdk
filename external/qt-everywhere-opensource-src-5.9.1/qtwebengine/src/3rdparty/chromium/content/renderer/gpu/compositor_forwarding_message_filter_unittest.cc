// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_forwarding_message_filter.h"

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/test/begin_frame_args_test.h"
#include "content/common/view_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CompositorForwardingMessageFilterTestHandler
    : public base::RefCounted<CompositorForwardingMessageFilterTestHandler> {
 public:
  CompositorForwardingMessageFilterTestHandler() : count_(0) {
  }

  void OnPlusMethod(const IPC::Message& msg) {
    count_++;
  }

  void OnMinusMethod(const IPC::Message& msg) {
    count_--;
  }

  int count() { return count_; }

  void ResetCount() { count_ = 0; }

 private:
  friend class base::RefCounted<CompositorForwardingMessageFilterTestHandler>;
  ~CompositorForwardingMessageFilterTestHandler() {}

  int count_;
};

TEST(CompositorForwardingMessageFilterTest, BasicTest) {
  scoped_refptr<CompositorForwardingMessageFilterTestHandler> handler =
      new CompositorForwardingMessageFilterTestHandler;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner);
  int route_id = 0;

  ViewMsg_BeginFrame msg(
      route_id, cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE));

  CompositorForwardingMessageFilter::Handler plus_handler =
      base::Bind(&CompositorForwardingMessageFilterTestHandler::OnPlusMethod,
                 handler);
  CompositorForwardingMessageFilter::Handler minus_handler =
      base::Bind(&CompositorForwardingMessageFilterTestHandler::OnMinusMethod,
                 handler);

  scoped_refptr<CompositorForwardingMessageFilter> filter =
      new CompositorForwardingMessageFilter(task_runner.get());

  filter->AddHandlerOnCompositorThread(route_id, plus_handler);
  filter->OnMessageReceived(msg);
  task_runner->RunPendingTasks();
  EXPECT_EQ(1, handler->count());

  handler->ResetCount();
  EXPECT_EQ(0, handler->count());

  filter->AddHandlerOnCompositorThread(route_id, minus_handler);
  filter->OnMessageReceived(msg);
  task_runner->RunPendingTasks();
  EXPECT_EQ(0, handler->count());

  handler->ResetCount();
  EXPECT_EQ(0, handler->count());

  filter->RemoveHandlerOnCompositorThread(route_id, plus_handler);
  filter->OnMessageReceived(msg);
  task_runner->RunPendingTasks();
  EXPECT_EQ(-1, handler->count());
}

}  // namespace content
