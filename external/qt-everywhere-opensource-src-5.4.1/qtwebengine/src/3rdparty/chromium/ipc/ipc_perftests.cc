// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_time_logger.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_base.h"

namespace {

// This test times the roundtrip IPC message cycle.
//
// TODO(brettw): Make this test run by default.

class IPCChannelPerfTest : public IPCTestBase {
};

// This class simply collects stats about abstract "events" (each of which has a
// start time and an end time).
class EventTimeTracker {
 public:
  explicit EventTimeTracker(const char* name)
      : name_(name),
        count_(0) {
  }

  void AddEvent(const base::TimeTicks& start, const base::TimeTicks& end) {
    DCHECK(end >= start);
    count_++;
    base::TimeDelta duration = end - start;
    total_duration_ += duration;
    max_duration_ = std::max(max_duration_, duration);
  }

  void ShowResults() const {
    VLOG(1) << name_ << " count: " << count_;
    VLOG(1) << name_ << " total duration: "
            << total_duration_.InMillisecondsF() << " ms";
    VLOG(1) << name_ << " average duration: "
            << (total_duration_.InMillisecondsF() / static_cast<double>(count_))
            << " ms";
    VLOG(1) << name_ << " maximum duration: "
            << max_duration_.InMillisecondsF() << " ms";
  }

  void Reset() {
    count_ = 0;
    total_duration_ = base::TimeDelta();
    max_duration_ = base::TimeDelta();
  }

 private:
  const std::string name_;

  uint64 count_;
  base::TimeDelta total_duration_;
  base::TimeDelta max_duration_;

  DISALLOW_COPY_AND_ASSIGN(EventTimeTracker);
};

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public IPC::Listener {
 public:
  ChannelReflectorListener()
      : channel_(NULL),
        latency_tracker_("Client messages") {
    VLOG(1) << "Client listener up";
  }

  virtual ~ChannelReflectorListener() {
    VLOG(1) << "Client listener down";
    latency_tracker_.ShowResults();
  }

  void Init(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    CHECK(channel_);

    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string payload;
    EXPECT_TRUE(iter.ReadString(&payload));

    // Include message deserialization in latency.
    base::TimeTicks now = base::TimeTicks::Now();

    if (payload == "hello") {
      latency_tracker_.Reset();
    } else if (payload == "quit") {
      latency_tracker_.ShowResults();
      base::MessageLoop::current()->QuitWhenIdle();
      return true;
    } else {
      // Don't track hello and quit messages.
      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);
    }

    IPC::Message* msg = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
    return true;
  }

 private:
  IPC::Channel* channel_;
  EventTimeTracker latency_tracker_;
};

class PerformanceChannelListener : public IPC::Listener {
 public:
  PerformanceChannelListener()
      : channel_(NULL),
        msg_count_(0),
        msg_size_(0),
        count_down_(0),
        latency_tracker_("Server messages") {
    VLOG(1) << "Server listener up";
  }

  virtual ~PerformanceChannelListener() {
    VLOG(1) << "Server listener down";
  }

  void Init(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  // Call this before running the message loop.
  void SetTestParams(int msg_count, size_t msg_size) {
    DCHECK_EQ(0, count_down_);
    msg_count_ = msg_count;
    msg_size_ = msg_size;
    count_down_ = msg_count_;
    payload_ = std::string(msg_size_, 'a');
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    CHECK(channel_);

    PickleIterator iter(message);
    int64 time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    std::string reflected_payload;
    EXPECT_TRUE(iter.ReadString(&reflected_payload));

    // Include message deserialization in latency.
    base::TimeTicks now = base::TimeTicks::Now();

    if (reflected_payload == "hello") {
      // Start timing on hello.
      latency_tracker_.Reset();
      DCHECK(!perf_logger_.get());
      std::string test_name = base::StringPrintf(
          "IPC_Perf_%dx_%u", msg_count_, static_cast<unsigned>(msg_size_));
      perf_logger_.reset(new base::PerfTimeLogger(test_name.c_str()));
    } else {
      DCHECK_EQ(payload_.size(), reflected_payload.size());

      latency_tracker_.AddEvent(
          base::TimeTicks::FromInternalValue(time_internal), now);

      CHECK(count_down_ > 0);
      count_down_--;
      if (count_down_ == 0) {
        perf_logger_.reset();  // Stop the perf timer now.
        latency_tracker_.ShowResults();
        base::MessageLoop::current()->QuitWhenIdle();
        return true;
      }
    }

    IPC::Message* msg = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    channel_->Send(msg);
    return true;
  }

 private:
  IPC::Channel* channel_;
  int msg_count_;
  size_t msg_size_;

  int count_down_;
  std::string payload_;
  EventTimeTracker latency_tracker_;
  scoped_ptr<base::PerfTimeLogger> perf_logger_;
};

TEST_F(IPCChannelPerfTest, Performance) {
  Init("PerformanceClient");

  // Set up IPC channel and start client.
  PerformanceChannelListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  // Test several sizes. We use 12^N for message size, and limit the message
  // count to keep the test duration reasonable.
  const size_t kMsgSize[5] = {12, 144, 1728, 20736, 248832};
  const int kMessageCount[5] = {50000, 50000, 50000, 12000, 1000};

  for (size_t i = 0; i < 5; i++) {
    listener.SetTestParams(kMessageCount[i], kMsgSize[i]);

    // This initial message will kick-start the ping-pong of messages.
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("hello");
    sender()->Send(message);

    // Run message loop.
    base::MessageLoop::current()->Run();
  }

  // Send quit message.
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
  message->WriteInt(-1);
  message->WriteString("quit");
  sender()->Send(message);

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

// This message loop bounces all messages back to the sender.
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(PerformanceClient) {
  base::MessageLoopForIO main_message_loop;
  ChannelReflectorListener listener;
  scoped_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName("PerformanceClient"), &listener));
  listener.Init(channel.get());
  CHECK(channel->Connect());

  base::MessageLoop::current()->Run();
  return 0;
}

}  // namespace
