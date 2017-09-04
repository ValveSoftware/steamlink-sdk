// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace {

class SpellCheckProviderCacheTest : public SpellCheckProviderTest {};

TEST_F(SpellCheckProviderCacheTest, SubstringWithoutMisspellings) {
  FakeTextCheckingCompletion completion;

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(base::ASCIIToUTF16("This is a"),
                                                &completion));
  EXPECT_EQ(completion.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, SubstringWithMisspellings) {
  FakeTextCheckingCompletion completion;

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(blink::WebTextCheckingResult(
      blink::WebTextDecorationTypeSpelling, 5, 3, blink::WebString("isq")));
  last_results.assign(results);
  provider_.SetLastResults(base::ASCIIToUTF16("This isq a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("This isq a"), &completion));
  EXPECT_EQ(completion.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, ShorterTextNotSubstring) {
  FakeTextCheckingCompletion completion;

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_FALSE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("That is a"), &completion));
  EXPECT_EQ(completion.completion_count_, 0U);
}

}  // namespace
