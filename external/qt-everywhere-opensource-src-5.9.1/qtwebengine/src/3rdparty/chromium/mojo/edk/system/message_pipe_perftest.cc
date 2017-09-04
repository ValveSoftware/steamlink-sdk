// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_time_logger.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/test_utils.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

class MessagePipePerfTest : public test::MojoTestBase {
 public:
  MessagePipePerfTest() : message_count_(0), message_size_(0) {}

  void SetUpMeasurement(int message_count, size_t message_size) {
    message_count_ = message_count;
    message_size_ = message_size;
    payload_ = std::string(message_size, '*');
    read_buffer_.resize(message_size * 2);
  }

 protected:
  void WriteWaitThenRead(MojoHandle mp) {
    CHECK_EQ(MojoWriteMessage(mp, payload_.data(),
                              static_cast<uint32_t>(payload_.size()), nullptr,
                              0, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    HandleSignalsState hss;
    CHECK_EQ(MojoWait(mp, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE,
                      &hss),
             MOJO_RESULT_OK);
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer_.size());
    CHECK_EQ(MojoReadMessage(mp, &read_buffer_[0], &read_buffer_size, nullptr,
                             nullptr, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(read_buffer_size, static_cast<uint32_t>(payload_.size()));
  }

  void SendQuitMessage(MojoHandle mp) {
    CHECK_EQ(MojoWriteMessage(mp, "", 0, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

  void Measure(MojoHandle mp) {
    // Have one ping-pong to ensure channel being established.
    WriteWaitThenRead(mp);

    std::string test_name =
        base::StringPrintf("IPC_Perf_%dx_%u", message_count_,
                           static_cast<unsigned>(message_size_));
    base::PerfTimeLogger logger(test_name.c_str());

    for (int i = 0; i < message_count_; ++i)
      WriteWaitThenRead(mp);

    logger.Done();
  }

 protected:
  void RunPingPongServer(MojoHandle mp) {
    // This values are set to align with one at ipc_pertests.cc for comparison.
    const size_t kMsgSize[5] = {12, 144, 1728, 20736, 248832};
    const int kMessageCount[5] = {50000, 50000, 50000, 12000, 1000};

    for (size_t i = 0; i < 5; i++) {
      SetUpMeasurement(kMessageCount[i], kMsgSize[i]);
      Measure(mp);
    }

    SendQuitMessage(mp);
  }

  static int RunPingPongClient(MojoHandle mp) {
    std::string buffer(1000000, '\0');
    int rv = 0;
    while (true) {
      // Wait for our end of the message pipe to be readable.
      HandleSignalsState hss;
      MojoResult result =
          MojoWait(mp, MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_DEADLINE_INDEFINITE, &hss);
      if (result != MOJO_RESULT_OK) {
        rv = result;
        break;
      }

      uint32_t read_size = static_cast<uint32_t>(buffer.size());
      CHECK_EQ(MojoReadMessage(mp, &buffer[0],
                               &read_size, nullptr,
                               0, MOJO_READ_MESSAGE_FLAG_NONE),
               MOJO_RESULT_OK);

      // Empty message indicates quit.
      if (read_size == 0)
        break;

      CHECK_EQ(MojoWriteMessage(mp, &buffer[0],
                                read_size,
                                nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE),
               MOJO_RESULT_OK);
    }

    return rv;
  }

 private:
  int message_count_;
  size_t message_size_;
  std::string payload_;
  std::string read_buffer_;
  std::unique_ptr<base::PerfTimeLogger> perf_logger_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipePerfTest);
};

TEST_F(MessagePipePerfTest, PingPong) {
  MojoHandle server_handle, client_handle;
  CreateMessagePipe(&server_handle, &client_handle);

  base::Thread client_thread("PingPongClient");
  client_thread.Start();
  client_thread.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RunPingPongClient), client_handle));

  RunPingPongServer(server_handle);
}

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
DEFINE_TEST_CLIENT_WITH_PIPE(PingPongClient, MessagePipePerfTest, h) {
  return RunPingPongClient(h);
}

// Repeatedly sends messages as previous one got replied by the child.
// Waits for the child to close its end before quitting once specified
// number of messages has been sent.
TEST_F(MessagePipePerfTest, MultiprocessPingPong) {
  RUN_CHILD_ON_PIPE(PingPongClient, h)
    RunPingPongServer(h);
  END_CHILD()
}

}  // namespace
}  // namespace edk
}  // namespace mojo
