// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"

#include <stdint.h>

#include <tuple>

#include "components/autofill/content/common/autofill_messages.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const char kTestText[] = "test";

class TestLogger : public RendererSavePasswordProgressLogger {
 public:
  TestLogger() : RendererSavePasswordProgressLogger(&sink_, 0) {}

  using RendererSavePasswordProgressLogger::SendLog;

  // Searches for an |AutofillHostMsg_RecordSavePasswordProgress| message in the
  // queue of sent IPC messages. If none is present, returns false. Otherwise,
  // extracts the first |AutofillHostMsg_RecordSavePasswordProgress| message,
  // fills the output parameter with the value of the message's parameter, and
  // clears the queue of sent messages.
  bool GetLogMessage(std::string* log) {
    const uint32_t kMsgID = AutofillHostMsg_RecordSavePasswordProgress::ID;
    const IPC::Message* message = sink_.GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    std::tuple<std::string> param;
    AutofillHostMsg_RecordSavePasswordProgress::Read(message, &param);
    *log = std::get<0>(param);
    sink_.ClearMessages();
    return true;
  }

 private:
  IPC::TestSink sink_;
};

}  // namespace

TEST(RendererSavePasswordProgressLoggerTest, SendLog) {
  TestLogger logger;
  logger.SendLog(kTestText);
  std::string sent_log;
  EXPECT_TRUE(logger.GetLogMessage(&sent_log));
  EXPECT_EQ(kTestText, sent_log);
}

}  // namespace autofill
