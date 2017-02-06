// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling_private_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"


namespace utils = extension_function_test_utils;

namespace extensions {

typedef ExtensionApiUnittest ExperienceSamplingPrivateTest;

// Tests that chrome.experienceSamplingPrivate.getBrowserInfo() returns expected
// field trials and groups.
TEST_F(ExperienceSamplingPrivateTest, GetBrowserInfoTest) {
  // Start with an empty FieldTrialList.
  std::unique_ptr<base::FieldTrialList> trial_list(
      new base::FieldTrialList(NULL));
  std::unique_ptr<base::DictionaryValue> result(RunFunctionAndReturnDictionary(
      new ExperienceSamplingPrivateGetBrowserInfoFunction(), "[]"));
  ASSERT_TRUE(result->HasKey("variations"));
  std::string trials_string;
  EXPECT_TRUE(result->GetStringWithoutPathExpansion("variations",
                                                    &trials_string));
  ASSERT_EQ("", trials_string);

  // Set field trials using a string.
  base::FieldTrialList::CreateTrialsFromString(
      "*Some name/Winner/*xxx/yyyy/*zzz/default/",
      std::set<std::string>());
  result = RunFunctionAndReturnDictionary(
      new ExperienceSamplingPrivateGetBrowserInfoFunction(), "[]");
  ASSERT_TRUE(result->HasKey("variations"));
  EXPECT_TRUE(result->GetStringWithoutPathExpansion("variations",
                                                    &trials_string));
  ASSERT_EQ("Some name/Winner/xxx/yyyy/zzz/default/", trials_string);
}

}  // namespace extensions
