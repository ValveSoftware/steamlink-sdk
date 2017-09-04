// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/document_subresource_filter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

const char kTestAlphaURL[] = "http://example.com/alpha";
const char kTestAlphaDataURI[] = "data:text/plain,alpha";
const char kTestBetaURL[] = "http://example.com/beta";

const char kTestAlphaURLPathSuffix[] = "alpha";

class TestCallbackReceiver {
 public:
  TestCallbackReceiver() = default;
  base::Closure closure() {
    return base::Bind(&TestCallbackReceiver::CallbackMethod,
                      base::Unretained(this));
  }
  size_t callback_count() const { return callback_count_; }

 private:
  void CallbackMethod() { ++callback_count_; }

  size_t callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestCallbackReceiver);
};

}  // namespace

class DocumentSubresourceFilterTest : public ::testing::Test {
 public:
  DocumentSubresourceFilterTest() {}

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestAlphaURLPathSuffix));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(base::StringPiece suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_ = new MemoryMappedRuleset(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilterTest);
};

TEST_F(DocumentSubresourceFilterTest, DryRun) {
  blink::WebURLRequest::RequestContext request_context =
      blink::WebURLRequest::RequestContextImage;
  TestCallbackReceiver first_disallowed_load_callback_receiver;
  DocumentSubresourceFilter filter(
      ActivationState::DRYRUN, ruleset(), std::vector<GURL>(),
      first_disallowed_load_callback_receiver.closure());
  EXPECT_TRUE(filter.allowLoad(GURL(kTestAlphaURL), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestAlphaDataURI), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestBetaURL), request_context));
  EXPECT_EQ(3u, filter.num_loads_total());
  EXPECT_EQ(2u, filter.num_loads_evaluated());
  EXPECT_EQ(1u, filter.num_loads_matching_rules());
  EXPECT_EQ(0u, filter.num_loads_disallowed());
  EXPECT_EQ(0u, first_disallowed_load_callback_receiver.callback_count());
}

TEST_F(DocumentSubresourceFilterTest, Enabled) {
  blink::WebURLRequest::RequestContext request_context =
      blink::WebURLRequest::RequestContextImage;
  DocumentSubresourceFilter filter(ActivationState::ENABLED, ruleset(),
                                   std::vector<GURL>(), base::Closure());
  EXPECT_FALSE(filter.allowLoad(GURL(kTestAlphaURL), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestAlphaDataURI), request_context));
  EXPECT_TRUE(filter.allowLoad(GURL(kTestBetaURL), request_context));
  EXPECT_EQ(3u, filter.num_loads_total());
  EXPECT_EQ(2u, filter.num_loads_evaluated());
  EXPECT_EQ(1u, filter.num_loads_matching_rules());
  EXPECT_EQ(1u, filter.num_loads_disallowed());
}

TEST_F(DocumentSubresourceFilterTest,
       CallbackFiredExactlyOnceAfterFirstDisallowedLoad) {
  blink::WebURLRequest::RequestContext request_context =
      blink::WebURLRequest::RequestContextImage;
  TestCallbackReceiver first_disallowed_load_callback_receiver;
  DocumentSubresourceFilter filter(
      ActivationState::ENABLED, ruleset(), std::vector<GURL>(),
      first_disallowed_load_callback_receiver.closure());
  EXPECT_TRUE(filter.allowLoad(GURL(kTestAlphaDataURI), request_context));
  EXPECT_EQ(0u, first_disallowed_load_callback_receiver.callback_count());
  EXPECT_FALSE(filter.allowLoad(GURL(kTestAlphaURL), request_context));
  EXPECT_EQ(1u, first_disallowed_load_callback_receiver.callback_count());
  EXPECT_FALSE(filter.allowLoad(GURL(kTestAlphaURL), request_context));
  EXPECT_EQ(1u, first_disallowed_load_callback_receiver.callback_count());
}

}  // namespace subresource_filter
