// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace transport {

static const int64 kStartMillisecond = INT64_C(12345678900000);

class FakePacketSender : public transport::PacketSender {
 public:
  FakePacketSender() {}

  virtual bool SendPacket(PacketRef packet, const base::Closure& cb) OVERRIDE {
    return true;
  }
};

class CastTransportSenderImplTest : public ::testing::Test {
 protected:
  CastTransportSenderImplTest()
      : num_times_callback_called_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(&testing_clock_);
  }

  virtual ~CastTransportSenderImplTest() {}

  void InitWithoutLogging() {
    transport_sender_.reset(
        new CastTransportSenderImpl(NULL,
                                    &testing_clock_,
                                    net::IPEndPoint(),
                                    base::Bind(&UpdateCastTransportStatus),
                                    BulkRawEventsCallback(),
                                    base::TimeDelta(),
                                    task_runner_,
                                    &transport_));
    task_runner_->RunTasks();
  }

  void InitWithLogging() {
    transport_sender_.reset(new CastTransportSenderImpl(
        NULL,
        &testing_clock_,
        net::IPEndPoint(),
        base::Bind(&UpdateCastTransportStatus),
        base::Bind(&CastTransportSenderImplTest::LogRawEvents,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(10),
        task_runner_,
        &transport_));
    task_runner_->RunTasks();
  }

  void LogRawEvents(const std::vector<PacketEvent>& packet_events) {
    num_times_callback_called_++;
    if (num_times_callback_called_ == 3) {
      run_loop_.Quit();
    }
  }

  static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<CastTransportSenderImpl> transport_sender_;
  FakePacketSender transport_;
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;
  int num_times_callback_called_;
};

TEST_F(CastTransportSenderImplTest, InitWithoutLogging) {
  InitWithoutLogging();
  message_loop_.PostDelayedTask(FROM_HERE,
                                run_loop_.QuitClosure(),
                                base::TimeDelta::FromMilliseconds(50));
  run_loop_.Run();
  EXPECT_EQ(0, num_times_callback_called_);
}

TEST_F(CastTransportSenderImplTest, InitWithLogging) {
  InitWithLogging();
  message_loop_.PostDelayedTask(FROM_HERE,
                                run_loop_.QuitClosure(),
                                base::TimeDelta::FromMilliseconds(50));
  run_loop_.Run();
  EXPECT_GT(num_times_callback_called_, 1);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
