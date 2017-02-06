// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter_platform.h"

#include <tuple>

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Fake filter for testing, which stores sent messages and
// allows verification by the test case.
class TestingSpellCheckMessageFilter : public SpellCheckMessageFilterPlatform {
 public:
  explicit TestingSpellCheckMessageFilter(base::MessageLoopForUI* loop)
      : SpellCheckMessageFilterPlatform(0),
        loop_(loop) { }

  bool Send(IPC::Message* message) override {
    sent_messages_.push_back(message);
    loop_->task_runner()->PostTask(FROM_HERE,
                                   base::MessageLoop::QuitWhenIdleClosure());
    return true;
  }

  ScopedVector<IPC::Message> sent_messages_;
  base::MessageLoopForUI* loop_;

 private:
  ~TestingSpellCheckMessageFilter() override {}
};

typedef InProcessBrowserTest SpellCheckMessageFilterPlatformMacBrowserTest;

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckMessageFilterPlatformMacBrowserTest,
                       SpellCheckReturnMessage) {
  scoped_refptr<TestingSpellCheckMessageFilter> target(
      new TestingSpellCheckMessageFilter(base::MessageLoopForUI::current()));

  SpellCheckHostMsg_RequestTextCheck to_be_received(
      123, 456, base::UTF8ToUTF16("zz."), std::vector<SpellCheckMarker>());
  target->OnMessageReceived(to_be_received);

  base::MessageLoopForUI::current()->Run();
  EXPECT_EQ(1U, target->sent_messages_.size());

  SpellCheckMsg_RespondTextCheck::Param params;
  bool ok = SpellCheckMsg_RespondTextCheck::Read(
      target->sent_messages_[0], &params);
  std::vector<SpellCheckResult> sent_results = std::get<2>(params);

  EXPECT_TRUE(ok);
  EXPECT_EQ(1U, sent_results.size());
  EXPECT_EQ(sent_results[0].location, 0);
  EXPECT_EQ(sent_results[0].length, 2);
  EXPECT_EQ(sent_results[0].decoration,
            SpellCheckResult::SPELLING);
}
