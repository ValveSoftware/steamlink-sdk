// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_perftest_support.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_time_logger.h"
#include "base/test/test_io_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sender.h"

namespace IPC {
namespace test {

// Avoid core 0 due to conflicts with Intel's Power Gadget.
// Setting thread affinity will fail harmlessly on single/dual core machines.
const int kSharedCore = 2;

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

  uint64_t count_;
  base::TimeDelta total_duration_;
  base::TimeDelta max_duration_;

  DISALLOW_COPY_AND_ASSIGN(EventTimeTracker);
};

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public Listener {
 public:
  ChannelReflectorListener()
      : channel_(NULL),
        latency_tracker_("Client messages") {
    VLOG(1) << "Client listener up";
  }

  ~ChannelReflectorListener() override {
    VLOG(1) << "Client listener down";
    latency_tracker_.ShowResults();
  }

  void Init(Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(channel_);

    base::PickleIterator iter(message);
    int64_t time_internal;
    EXPECT_TRUE(iter.ReadInt64(&time_internal));
    int msgid;
    EXPECT_TRUE(iter.ReadInt(&msgid));
    base::StringPiece payload;
    EXPECT_TRUE(iter.ReadStringPiece(&payload));

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

    Message* msg = new Message(0, 2, Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
    return true;
  }

 private:
  Channel* channel_;
  EventTimeTracker latency_tracker_;
};

class PerformanceChannelListener : public Listener {
 public:
  explicit PerformanceChannelListener(const std::string& label)
      : label_(label),
        sender_(NULL),
        msg_count_(0),
        msg_size_(0),
        count_down_(0),
        latency_tracker_("Server messages") {
    VLOG(1) << "Server listener up";
  }

  ~PerformanceChannelListener() override {
    VLOG(1) << "Server listener down";
  }

  void Init(Sender* sender) {
    DCHECK(!sender_);
    sender_ = sender;
  }

  // Call this before running the message loop.
  void SetTestParams(int msg_count, size_t msg_size) {
    DCHECK_EQ(0, count_down_);
    msg_count_ = msg_count;
    msg_size_ = msg_size;
    count_down_ = msg_count_;
    payload_ = std::string(msg_size_, 'a');
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(sender_);

    base::PickleIterator iter(message);
    int64_t time_internal;
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
      std::string test_name =
          base::StringPrintf("IPC_%s_Perf_%dx_%u",
                             label_.c_str(),
                             msg_count_,
                             static_cast<unsigned>(msg_size_));
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

    Message* msg = new Message(0, 2, Message::PRIORITY_NORMAL);
    msg->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    sender_->Send(msg);
    return true;
  }

 private:
  std::string label_;
  Sender* sender_;
  int msg_count_;
  size_t msg_size_;

  int count_down_;
  std::string payload_;
  EventTimeTracker latency_tracker_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;
};

IPCChannelPerfTestBase::IPCChannelPerfTestBase() = default;
IPCChannelPerfTestBase::~IPCChannelPerfTestBase() = default;

std::vector<PingPongTestParams>
IPCChannelPerfTestBase::GetDefaultTestParams() {
  // Test several sizes. We use 12^N for message size, and limit the message
  // count to keep the test duration reasonable.
  std::vector<PingPongTestParams> list;
  list.push_back(PingPongTestParams(12, 50000));
  list.push_back(PingPongTestParams(144, 50000));
  list.push_back(PingPongTestParams(1728, 50000));
  list.push_back(PingPongTestParams(20736, 12000));
  list.push_back(PingPongTestParams(248832, 1000));
  return list;
}

void IPCChannelPerfTestBase::RunTestChannelPingPong(
    const std::vector<PingPongTestParams>& params) {
  Init("PerformanceClient");

  // Set up IPC channel and start client.
  PerformanceChannelListener listener("Channel");
  CreateChannel(&listener);
  listener.Init(channel());
  ASSERT_TRUE(ConnectChannel());
  ASSERT_TRUE(StartClient());

  LockThreadAffinity thread_locker(kSharedCore);
  for (size_t i = 0; i < params.size(); i++) {
    listener.SetTestParams(params[i].message_count(),
                           params[i].message_size());

    // This initial message will kick-start the ping-pong of messages.
    Message* message =
        new Message(0, 2, Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("hello");
    sender()->Send(message);

    // Run message loop.
    base::RunLoop().Run();
  }

  // Send quit message.
  Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
  message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
  message->WriteInt(-1);
  message->WriteString("quit");
  sender()->Send(message);

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

void IPCChannelPerfTestBase::RunTestChannelProxyPingPong(
    const std::vector<PingPongTestParams>& params) {
  io_thread_.reset(new base::TestIOThread(base::TestIOThread::kAutoStart));
  InitWithCustomMessageLoop("PerformanceClient",
                            base::WrapUnique(new base::MessageLoop()));

  // Set up IPC channel and start client.
  PerformanceChannelListener listener("ChannelProxy");
  CreateChannelProxy(&listener, io_thread_->task_runner());
  listener.Init(channel_proxy());
  ASSERT_TRUE(StartClient());

  LockThreadAffinity thread_locker(kSharedCore);
  for (size_t i = 0; i < params.size(); i++) {
    listener.SetTestParams(params[i].message_count(),
                           params[i].message_size());

    // This initial message will kick-start the ping-pong of messages.
    Message* message =
        new Message(0, 2, Message::PRIORITY_NORMAL);
    message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
    message->WriteInt(-1);
    message->WriteString("hello");
    sender()->Send(message);

    // Run message loop.
    base::RunLoop().Run();
  }

  // Send quit message.
  Message* message = new Message(0, 2, Message::PRIORITY_NORMAL);
  message->WriteInt64(base::TimeTicks::Now().ToInternalValue());
  message->WriteInt(-1);
  message->WriteString("quit");
  sender()->Send(message);

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannelProxy();

  io_thread_.reset();
}


PingPongTestClient::PingPongTestClient()
    : listener_(new ChannelReflectorListener()) {
}

PingPongTestClient::~PingPongTestClient() {
}

std::unique_ptr<Channel> PingPongTestClient::CreateChannel(Listener* listener) {
  return Channel::CreateClient(IPCTestBase::GetChannelName("PerformanceClient"),
                               listener);
}

int PingPongTestClient::RunMain() {
  LockThreadAffinity thread_locker(kSharedCore);
  std::unique_ptr<Channel> channel = CreateChannel(listener_.get());
  listener_->Init(channel.get());
  CHECK(channel->Connect());

  base::RunLoop().Run();
  return 0;
}

scoped_refptr<base::TaskRunner> PingPongTestClient::task_runner() {
  return main_message_loop_.task_runner();
}

LockThreadAffinity::LockThreadAffinity(int cpu_number)
    : affinity_set_ok_(false) {
#if defined(OS_WIN)
  const DWORD_PTR thread_mask = static_cast<DWORD_PTR>(1) << cpu_number;
  old_affinity_ = SetThreadAffinityMask(GetCurrentThread(), thread_mask);
  affinity_set_ok_ = old_affinity_ != 0;
#elif defined(OS_LINUX)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_number, &cpuset);
  auto get_result = sched_getaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, get_result);
  auto set_result = sched_setaffinity(0, sizeof(cpuset), &cpuset);
  // Check for get_result failure, even though it should always succeed.
  affinity_set_ok_ = (set_result == 0) && (get_result == 0);
#endif
  if (!affinity_set_ok_)
    LOG(WARNING) << "Failed to set thread affinity to CPU " << cpu_number;
}

LockThreadAffinity::~LockThreadAffinity() {
  if (!affinity_set_ok_)
    return;
#if defined(OS_WIN)
  auto set_result = SetThreadAffinityMask(GetCurrentThread(), old_affinity_);
  DCHECK_NE(0u, set_result);
#elif defined(OS_LINUX)
  auto set_result = sched_setaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, set_result);
#endif
}

}  // namespace test
}  // namespace IPC
