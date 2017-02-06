// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace supervised_user_error_page {

struct BlockMessageIDTestParameter {
  FilteringBehaviorReason reason;
  bool is_child_account;
  bool single_parent;
  int expected_result;
};

class SupervisedUserErrorPageTest_GetBlockMessageID
    : public ::testing::TestWithParam<BlockMessageIDTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_GetBlockMessageID, GetBlockMessageID) {
  BlockMessageIDTestParameter param = GetParam();
  EXPECT_EQ(param.expected_result,
            GetBlockMessageID(param.reason, param.is_child_account,
                              param.single_parent))
      << "reason = " << param.reason
      << " is_child_account = " << param.is_child_account
      << " single parent = " << param.single_parent;
}

BlockMessageIDTestParameter block_message_id_test_params[] = {
    {DEFAULT, false, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_DEFAULT},
    {DEFAULT, false, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_DEFAULT},
    {DEFAULT, true, true, IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT},
    {DEFAULT, true, false, IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT},
    {ASYNC_CHECKER, false, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {ASYNC_CHECKER, false, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {ASYNC_CHECKER, true, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {ASYNC_CHECKER, true, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {MANUAL, false, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_MANUAL},
    {MANUAL, false, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_MANUAL},
    {MANUAL, true, true, IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT},
    {MANUAL, true, false, IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT},
};

INSTANTIATE_TEST_CASE_P(GetBlockMessageIDParameterized,
                        SupervisedUserErrorPageTest_GetBlockMessageID,
                        ::testing::ValuesIn(block_message_id_test_params));

struct BuildHtmlTestParameter {
  bool allow_access_requests;
  const std::string& profile_image_url;
  const std::string& profile_image_url2;
  const std::string& custodian;
  const std::string& custodian_email;
  const std::string& second_custodian;
  const std::string& second_custodian_email;
  bool is_child_account;
  FilteringBehaviorReason reason;
  bool has_two_parents;
};

class SupervisedUserErrorPageTest_BuildHtml
    : public ::testing::TestWithParam<BuildHtmlTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_BuildHtml, BuildHtml) {
  BuildHtmlTestParameter param = GetParam();
  std::string result =
      BuildHtml(param.allow_access_requests, param.profile_image_url,
                param.profile_image_url2, base::UTF8ToUTF16(param.custodian),
                base::UTF8ToUTF16(param.custodian_email),
                base::UTF8ToUTF16(param.second_custodian),
                base::UTF8ToUTF16(param.second_custodian_email),
                param.is_child_account, param.reason, "");
  // The result should contain the original HTML plus scripts that plug values
  // into it. The test can't easily check that the scripts are correct, but
  // can check that the output contains the expected values.
  std::string html =
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML)
          .as_string();
  EXPECT_THAT(result, testing::HasSubstr(html));
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url));
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url2));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian_email));
  if (param.has_two_parents) {
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian));
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian_email));
  }
#if defined(GOOGLE_CHROME_BUILD)
  if (param.is_child_account &&
      (param.reason == ASYNC_CHECKER || param.reason == BLACKLIST))
    EXPECT_THAT(result, testing::HasSubstr("\"showFeedbackLink\":true"));
  else
#endif
    EXPECT_THAT(result, testing::HasSubstr("\"showFeedbackLink\":false"));
  // Messages containing links aren't tested since they get modified before they
  // are added to the result.
  if (param.allow_access_requests) {
    if (param.is_child_account) {
      if (param.has_two_parents) {
        EXPECT_THAT(result,
                    testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT))));
        EXPECT_THAT(result,
                    testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT)));
        EXPECT_THAT(result,
                    testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_BLOCK_INTERSTITIAL_MESSAGE))));
        EXPECT_THAT(
            result,
            testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED))));
      } else {
        EXPECT_THAT(result,
                    testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT)));
        EXPECT_THAT(result,
                    testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT))));
        EXPECT_THAT(
            result,
            testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED))));
      }
    } else {
      EXPECT_THAT(result,
                  testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                      IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT))));
      EXPECT_THAT(result,
                  testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                      IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT))));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED))));
    }
  } else {
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT))));
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT))));
    EXPECT_THAT(result,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED)));
  }
  if (param.is_child_account) {
    if (param.has_two_parents) {
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT)));
    } else {
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT))));
    }
  } else {
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT))));
  }
}

BuildHtmlTestParameter build_html_test_parameter[] = {
    {true, "url1", "url2", "custodian", "custodian_email", "", "", true,
     DEFAULT, false},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", true, DEFAULT, true},
    {false, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", true, DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", false, DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", false, ASYNC_CHECKER, true},
};

INSTANTIATE_TEST_CASE_P(GetBlockMessageIDParameterized,
                        SupervisedUserErrorPageTest_BuildHtml,
                        ::testing::ValuesIn(build_html_test_parameter));

}  //  namespace supervised_user_error_page
