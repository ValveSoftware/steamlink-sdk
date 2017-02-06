// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/resource_dispatch_throttler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/test/fake_renderer_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint32_t kRequestsPerFlush = 4;
const double kFlushPeriodSeconds = 1.f / 60;
const int kRoutingId = 1;

typedef ScopedVector<IPC::Message> ScopedMessages;

int GetRequestId(const IPC::Message& msg) {
  int request_id = -1;
  switch (msg.type()) {
    case ResourceHostMsg_RequestResource::ID: {
      base::PickleIterator iter(msg);
      int routing_id = -1;
      if (!iter.ReadInt(&routing_id) || !iter.ReadInt(&request_id))
        NOTREACHED() << "Invalid id for resource request message.";
    } break;

    case ResourceHostMsg_DidChangePriority::ID:
    case ResourceHostMsg_ReleaseDownloadedFile::ID:
    case ResourceHostMsg_CancelRequest::ID:
      if (!base::PickleIterator(msg).ReadInt(&request_id))
        NOTREACHED() << "Invalid id for resource message.";
      break;

    default:
      NOTREACHED() << "Invalid message for resource throttling.";
      break;
  }
  return request_id;
}

class RendererSchedulerForTest : public FakeRendererScheduler {
 public:
  RendererSchedulerForTest() : high_priority_work_anticipated_(false) {}
  ~RendererSchedulerForTest() override {}

  // RendererScheduler implementation:
  bool IsHighPriorityWorkAnticipated() override {
    return high_priority_work_anticipated_;
  }

  void set_high_priority_work_anticipated(bool anticipated) {
    high_priority_work_anticipated_ = anticipated;
  }

 private:
  bool high_priority_work_anticipated_;
};

}  // namespace

class ResourceDispatchThrottlerForTest : public ResourceDispatchThrottler {
 public:
  ResourceDispatchThrottlerForTest(IPC::Sender* sender,
                                   scheduler::RendererScheduler* scheduler)
      : ResourceDispatchThrottler(
            sender,
            scheduler,
            base::TimeDelta::FromSecondsD(kFlushPeriodSeconds),
            kRequestsPerFlush),
        now_(base::TimeTicks() + base::TimeDelta::FromDays(1)),
        flush_scheduled_(false) {}
  ~ResourceDispatchThrottlerForTest() override {}

  void Advance(base::TimeDelta delta) { now_ += delta; }

  bool RunScheduledFlush() {
    if (!flush_scheduled_)
      return false;

    flush_scheduled_ = false;
    Flush();
    return true;
  }

  bool flush_scheduled() const { return flush_scheduled_; }

 private:
  // ResourceDispatchThrottler overrides:
  base::TimeTicks Now() const override { return now_; }
  void ScheduleFlush() override { flush_scheduled_ = true; }

  base::TimeTicks now_;
  bool flush_scheduled_;
};

class ResourceDispatchThrottlerTest : public testing::Test, public IPC::Sender {
 public:
  ResourceDispatchThrottlerTest() : last_request_id_(0) {
    throttler_.reset(new ResourceDispatchThrottlerForTest(this, &scheduler_));
  }
  ~ResourceDispatchThrottlerTest() override {}

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override {
    sent_messages_.push_back(msg);
    return true;
  }

 protected:
  void SetHighPriorityWorkAnticipated(bool anticipated) {
    scheduler_.set_high_priority_work_anticipated(anticipated);
  }

  void Advance(base::TimeDelta delta) { throttler_->Advance(delta); }

  bool RunScheduledFlush() { return throttler_->RunScheduledFlush(); }

  bool FlushScheduled() { return throttler_->flush_scheduled(); }

  bool RequestResource() {
    ResourceRequest request;
    request.download_to_file = true;
    return throttler_->Send(new ResourceHostMsg_RequestResource(
        kRoutingId, ++last_request_id_, request));
  }

  bool RequestResourceSync() {
    SyncLoadResult result;
    return throttler_->Send(new ResourceHostMsg_SyncLoad(
        kRoutingId, ++last_request_id_, ResourceRequest(), &result));
  }

  void RequestResourcesUntilThrottled() {
    SetHighPriorityWorkAnticipated(true);
    GetAndResetSentMessageCount();
    RequestResource();
    while (GetAndResetSentMessageCount())
      RequestResource();
  }

  bool UpdateRequestPriority(int request_id, net::RequestPriority priority) {
    return throttler_->Send(
        new ResourceHostMsg_DidChangePriority(request_id, priority, 0));
  }

  bool ReleaseDownloadedFile(int request_id) {
    return throttler_->Send(
        new ResourceHostMsg_ReleaseDownloadedFile(request_id));
  }

  bool CancelRequest(int request_id) {
    return throttler_->Send(new ResourceHostMsg_CancelRequest(request_id));
  }

  size_t GetAndResetSentMessageCount() {
    size_t sent_message_count = sent_messages_.size();
    sent_messages_.clear();
    return sent_message_count;
  }

  const IPC::Message* LastSentMessage() const {
    return sent_messages_.empty() ? nullptr : sent_messages_.back();
  }

  int LastSentRequestId() const {
    const IPC::Message* msg = LastSentMessage();
    if (!msg)
      return -1;

    int routing_id = -1;
    int request_id = -1;
    base::PickleIterator iter(*msg);
    CHECK(IPC::ReadParam(msg, &iter, &routing_id));
    CHECK(IPC::ReadParam(msg, &iter, &request_id));
    return request_id;
  }

  int last_request_id() const { return last_request_id_; }

  ScopedMessages sent_messages_;

 private:
  std::unique_ptr<ResourceDispatchThrottlerForTest> throttler_;
  RendererSchedulerForTest scheduler_;
  int last_request_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatchThrottlerTest);
};

TEST_F(ResourceDispatchThrottlerTest, NotThrottledByDefault) {
  SetHighPriorityWorkAnticipated(false);
  for (size_t i = 0; i < kRequestsPerFlush * 2; ++i) {
    RequestResource();
    EXPECT_EQ(i + 1, sent_messages_.size());
  }
}

TEST_F(ResourceDispatchThrottlerTest, NotThrottledIfSendLimitNotReached) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush; ++i) {
    RequestResource();
    EXPECT_EQ(i + 1, sent_messages_.size());
  }
}

TEST_F(ResourceDispatchThrottlerTest, ThrottledWhenHighPriorityWork) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  RequestResource();
  EXPECT_EQ(0U, sent_messages_.size());

  EXPECT_TRUE(RunScheduledFlush());
  EXPECT_EQ(1U, sent_messages_.size());
}

TEST_F(ResourceDispatchThrottlerTest,
       ThrottledWhenDeferredMessageQueueNonEmpty) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  RequestResource();
  EXPECT_EQ(0U, sent_messages_.size());
  SetHighPriorityWorkAnticipated(false);
  RequestResource();
  EXPECT_EQ(0U, sent_messages_.size());

  EXPECT_TRUE(RunScheduledFlush());
  EXPECT_EQ(2U, sent_messages_.size());
}

TEST_F(ResourceDispatchThrottlerTest, NotThrottledIfSufficientTimePassed) {
  SetHighPriorityWorkAnticipated(true);

  for (size_t i = 0; i < kRequestsPerFlush * 2; ++i) {
    Advance(base::TimeDelta::FromSecondsD(kFlushPeriodSeconds * 2));
    RequestResource();
    EXPECT_EQ(1U, GetAndResetSentMessageCount());
    EXPECT_FALSE(FlushScheduled());
  }
}

TEST_F(ResourceDispatchThrottlerTest, NotThrottledIfSendRateSufficientlyLow) {
  SetHighPriorityWorkAnticipated(true);

  // Continuous dispatch of resource requests below the allowed send rate
  // should never throttled.
  const base::TimeDelta kAllowedContinuousSendInterval =
      base::TimeDelta::FromSecondsD((kFlushPeriodSeconds / kRequestsPerFlush) +
                                    .00001);
  for (size_t i = 0; i < kRequestsPerFlush * 10; ++i) {
    Advance(kAllowedContinuousSendInterval);
    RequestResource();
    EXPECT_EQ(1U, GetAndResetSentMessageCount());
    EXPECT_FALSE(FlushScheduled());
  }
}

TEST_F(ResourceDispatchThrottlerTest, ThrottledIfSendRateSufficientlyHigh) {
  SetHighPriorityWorkAnticipated(true);

  // Continuous dispatch of resource requests above the allowed send rate
  // should be throttled.
  const base::TimeDelta kThrottledContinuousSendInterval =
      base::TimeDelta::FromSecondsD((kFlushPeriodSeconds / kRequestsPerFlush) -
                                    .00001);

  for (size_t i = 0; i < kRequestsPerFlush * 10; ++i) {
    Advance(kThrottledContinuousSendInterval);
    RequestResource();
    // Only the first batch of requests under the limit should be unthrottled.
    if (i < kRequestsPerFlush) {
      EXPECT_EQ(1U, GetAndResetSentMessageCount());
      EXPECT_FALSE(FlushScheduled());
    } else {
      EXPECT_EQ(0U, GetAndResetSentMessageCount());
      EXPECT_TRUE(FlushScheduled());
    }
  }
}

TEST_F(ResourceDispatchThrottlerTest, NotThrottledIfSyncMessage) {
  SetHighPriorityWorkAnticipated(true);

  RequestResourceSync();
  EXPECT_EQ(1U, GetAndResetSentMessageCount());

  // Saturate the queue.
  for (size_t i = 0; i < kRequestsPerFlush * 2; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  // Synchronous messages should flush any previously throttled messages.
  RequestResourceSync();
  EXPECT_EQ(1U + kRequestsPerFlush, GetAndResetSentMessageCount());
  RequestResourceSync();
  EXPECT_EQ(1U, GetAndResetSentMessageCount());

  // Previously throttled messages should already have been flushed.
  RunScheduledFlush();
  EXPECT_EQ(0U, GetAndResetSentMessageCount());
}

TEST_F(ResourceDispatchThrottlerTest, MultipleFlushes) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush * 4; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  for (size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(RunScheduledFlush());
    EXPECT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());
  }

  EXPECT_FALSE(FlushScheduled());
  EXPECT_EQ(0U, sent_messages_.size());
}

TEST_F(ResourceDispatchThrottlerTest, MultipleFlushesWhileReceiving) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush * 4; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  for (size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(RunScheduledFlush());
    EXPECT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());
    for (size_t j = 0; j < kRequestsPerFlush; ++j)
      RequestResource();
    EXPECT_EQ(0U, sent_messages_.size());
  }

  for (size_t i = 0; i < 3; ++i) {
    EXPECT_TRUE(RunScheduledFlush());
    EXPECT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());
  }

  EXPECT_FALSE(FlushScheduled());
  EXPECT_EQ(0U, sent_messages_.size());
}

TEST_F(ResourceDispatchThrottlerTest, NonRequestsNeverTriggerThrottling) {
  RequestResource();
  ASSERT_EQ(1U, GetAndResetSentMessageCount());

  for (size_t i = 0; i < kRequestsPerFlush * 3; ++i)
    UpdateRequestPriority(last_request_id(), net::HIGHEST);
  EXPECT_EQ(kRequestsPerFlush * 3, sent_messages_.size());

  RequestResource();
  EXPECT_EQ(1U + kRequestsPerFlush * 3, GetAndResetSentMessageCount());
}

TEST_F(ResourceDispatchThrottlerTest, NonRequestsDeferredWhenThrottling) {
  RequestResource();
  ASSERT_EQ(1U, GetAndResetSentMessageCount());

  RequestResourcesUntilThrottled();
  UpdateRequestPriority(last_request_id(), net::HIGHEST);
  ReleaseDownloadedFile(last_request_id());
  CancelRequest(last_request_id());

  EXPECT_TRUE(RunScheduledFlush());
  EXPECT_EQ(4U, GetAndResetSentMessageCount());
  EXPECT_FALSE(FlushScheduled());
}

TEST_F(ResourceDispatchThrottlerTest, MessageOrderingPreservedWhenThrottling) {
  SetHighPriorityWorkAnticipated(true);
  for (size_t i = 0; i < kRequestsPerFlush; ++i)
    RequestResource();
  ASSERT_EQ(kRequestsPerFlush, GetAndResetSentMessageCount());

  for (size_t i = 0; i < kRequestsPerFlush; ++i) {
    RequestResource();
    UpdateRequestPriority(last_request_id(), net::HIGHEST);
    CancelRequest(last_request_id() - 1);
  }
  ASSERT_EQ(0U, sent_messages_.size());

  EXPECT_TRUE(RunScheduledFlush());
  ASSERT_EQ(kRequestsPerFlush * 3, sent_messages_.size());
  for (size_t i = 0; i < sent_messages_.size(); i += 3) {
    SCOPED_TRACE(i);
    const auto& request_msg = *sent_messages_[i];
    const auto& priority_msg = *sent_messages_[i + 1];
    const auto& cancel_msg = *sent_messages_[i + 2];

    EXPECT_EQ(request_msg.type(), ResourceHostMsg_RequestResource::ID);
    EXPECT_EQ(priority_msg.type(), ResourceHostMsg_DidChangePriority::ID);
    EXPECT_EQ(cancel_msg.type(), ResourceHostMsg_CancelRequest::ID);

    EXPECT_EQ(GetRequestId(request_msg), GetRequestId(priority_msg));
    EXPECT_EQ(GetRequestId(request_msg) - 1, GetRequestId(cancel_msg));
  }
  EXPECT_FALSE(FlushScheduled());
}

}  // namespace content
