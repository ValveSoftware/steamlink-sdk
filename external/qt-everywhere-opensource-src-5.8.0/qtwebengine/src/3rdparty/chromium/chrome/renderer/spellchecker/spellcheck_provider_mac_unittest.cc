// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace {

class SpellCheckProviderMacTest : public SpellCheckProviderTest {};

void FakeMessageArrival(
    SpellCheckProvider* provider,
    const SpellCheckHostMsg_RequestTextCheck::Param& parameters) {
  std::vector<SpellCheckResult> fake_result;
  bool handled = provider->OnMessageReceived(
      SpellCheckMsg_RespondTextCheck(
          0,
          std::get<1>(parameters),
          base::ASCIIToUTF16("test"),
          fake_result));
  EXPECT_TRUE(handled);
}

TEST_F(SpellCheckProviderMacTest, SingleRoundtripSuccess) {
  FakeTextCheckingCompletion completion;

  provider_.RequestTextChecking(blink::WebString("hello "),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters1;
  bool ok = SpellCheckHostMsg_RequestTextCheck::Read(
      provider_.messages_[0], &read_parameters1);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters1), base::UTF8ToUTF16("hello "));

  FakeMessageArrival(&provider_, read_parameters1);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

TEST_F(SpellCheckProviderMacTest, TwoRoundtripSuccess) {
  FakeTextCheckingCompletion completion1;
  provider_.RequestTextChecking(blink::WebString("hello "),
                                &completion1,
                                std::vector<SpellCheckMarker>());
  FakeTextCheckingCompletion completion2;
  provider_.RequestTextChecking(blink::WebString("bye "),
                                &completion2,
                                std::vector<SpellCheckMarker>());

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 2U);

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters1;
  bool ok = SpellCheckHostMsg_RequestTextCheck::Read(
      provider_.messages_[0], &read_parameters1);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters1), base::UTF8ToUTF16("hello "));

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters2;
  ok = SpellCheckHostMsg_RequestTextCheck::Read(
      provider_.messages_[1], &read_parameters2);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters2), base::UTF8ToUTF16("bye "));

  FakeMessageArrival(&provider_, read_parameters1);
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  FakeMessageArrival(&provider_, read_parameters2);
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

}  // namespace
