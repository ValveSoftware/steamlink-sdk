// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/renderer/spellchecker/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"

// Tests for Hunspell functionality in SpellcheckingProvider

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace {

TEST_F(SpellCheckProviderTest, UsingHunspell) {
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(blink::WebString("hello"),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.messages_.size(), 0U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

// Tests that the SpellCheckProvider object sends a spellcheck request when a
// user finishes typing a word. Also this test verifies that this object checks
// only a line being edited by the user.
TEST_F(SpellCheckProviderTest, MultiLineText) {
  FakeTextCheckingCompletion completion;

  // Verify that the SpellCheckProvider class does not spellcheck empty text.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      blink::WebString(), &completion, std::vector<SpellCheckMarker>());
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class does not spellcheck text while we
  // are typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      blink::WebString("First"), &completion, std::vector<SpellCheckMarker>());
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the first word when we
  // type a space key, i.e. when we finish typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(blink::WebString("First "),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(ASCIIToUTF16("First "), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the first line when we
  // type a return key, i.e. when we finish typing a line.
  provider_.ResetResult();
  provider_.RequestTextChecking(blink::WebString("First Second\n"),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(ASCIIToUTF16("First Second\n"), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the lines when we
  // finish typing a word "Third" to the second line.
  provider_.ResetResult();
  provider_.RequestTextChecking(blink::WebString("First Second\nThird "),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird "), provider_.text_);

  // Verify that the SpellCheckProvider class does not send a spellcheck request
  // when a user inserts whitespace characters.
  provider_.ResetResult();
  provider_.RequestTextChecking(blink::WebString("First Second\nThird   "),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the lines when we type
  // a period.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      blink::WebString("First Second\nThird   Fourth."),
      &completion,
      std::vector<SpellCheckMarker>());
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird   Fourth."), provider_.text_);
}

// Tests that the SpellCheckProvider class does not send requests to the
// spelling service when not necessary.
TEST_F(SpellCheckProviderTest, CancelUnnecessaryRequests) {
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(blink::WebString("hello."),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider does not send a request with the same text
  // as above.
  provider_.RequestTextChecking(blink::WebString("hello."),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class cancels an incoming request that
  // does not include any words.
  provider_.RequestTextChecking(blink::WebString(":-)"),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  // Test that the SpellCheckProvider class sends a request when it receives a
  // Russian word.
  const wchar_t kRussianWord[] = L"\x0431\x0451\x0434\x0440\x0430";
  provider_.RequestTextChecking(blink::WebString(WideToUTF16(kRussianWord)),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 4U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 2U);
}

// Tests that the SpellCheckProvider calls didFinishCheckingText() when
// necessary.
TEST_F(SpellCheckProviderTest, CompleteNecessaryRequests) {
  FakeTextCheckingCompletion completion;

  base::string16 text = ASCIIToUTF16("Icland is an icland ");
  provider_.RequestTextChecking(
      blink::WebString(text), &completion, std::vector<SpellCheckMarker>());
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << text << "\"";

  const int kSubstringLength = 18;
  base::string16 substring = text.substr(0, kSubstringLength);
  provider_.RequestTextChecking(blink::WebString(substring),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << substring << "\"";

  provider_.RequestTextChecking(
      blink::WebString(text), &completion, std::vector<SpellCheckMarker>());
  EXPECT_EQ(0U, completion.cancellation_count_) << "Should finish checking \""
                                                << text << "\"";
}

// Tests that the SpellCheckProvider cancels spelling requests in the middle of
// a word.
TEST_F(SpellCheckProviderTest, CancelMidWordRequests) {
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(blink::WebString("hello "),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  provider_.RequestTextChecking(blink::WebString("hello world"),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);

  provider_.RequestTextChecking(blink::WebString("hello world."),
                                &completion,
                                std::vector<SpellCheckMarker>());
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 1U);
  EXPECT_EQ(provider_.spelling_service_call_count_, 2U);
}

}  // namespace
