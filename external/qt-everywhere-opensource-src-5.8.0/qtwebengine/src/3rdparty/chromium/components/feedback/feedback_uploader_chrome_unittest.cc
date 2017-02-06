// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader_chrome.h"

#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

class FeedbackUploaderChromeTest : public ::testing::Test {
 protected:
  FeedbackUploaderChromeTest()
      : browser_thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  ~FeedbackUploaderChromeTest() override {
    // Clean up registered ids.
    variations::testing::ClearAllVariationIDs();
  }

  // Registers a field trial with the specified name and group and an associated
  // google web property variation id.
  void CreateFieldTrialWithId(const std::string& trial_name,
                              const std::string& group_name,
                              int variation_id) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES, trial_name, group_name,
        static_cast<variations::VariationID>(variation_id));
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->group();
  }

 private:
  content::TestBrowserThreadBundle browser_thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderChromeTest);
};

TEST_F(FeedbackUploaderChromeTest, VariationHeaders) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  base::FieldTrialList field_trial_list_(NULL);
  CreateFieldTrialWithId("Test", "Group1", 123);

  content::TestBrowserContext context;
  FeedbackUploaderChrome uploader(&context);

  net::TestURLFetcherFactory factory;
  uploader.DispatchReport("test");

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string value;
  EXPECT_TRUE(headers.GetHeader("X-Client-Data", &value));
  EXPECT_FALSE(value.empty());
  // The fetcher's delegate is responsible for freeing the fetcher (and itself).
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
}

}  // namespace feedback
