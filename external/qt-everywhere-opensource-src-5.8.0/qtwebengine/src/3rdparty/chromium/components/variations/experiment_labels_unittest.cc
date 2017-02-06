// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_labels.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(ExperimentLabelsTest, BuildGoogleUpdateExperimentLabel) {
  const variations::VariationID TEST_VALUE_A = 3300200;
  const variations::VariationID TEST_VALUE_B = 3300201;
  const variations::VariationID TEST_VALUE_C = 3300202;
  const variations::VariationID TEST_VALUE_D = 3300203;

  struct {
    const char* active_group_pairs;
    const char* expected_ids;
  } test_cases[] = {
    // Empty group.
    {"", ""},
    // Group of 1.
    {"FieldTrialA#Default", "3300200"},
    // Group of 1, doesn't have an associated ID.
    {"FieldTrialA#DoesNotExist", ""},
    // Group of 3.
    {"FieldTrialA#Default#FieldTrialB#Group1#FieldTrialC#Default",
     "3300200#3300201#3300202"},
    // Group of 3, one doesn't have an associated ID.
    {"FieldTrialA#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     "3300200#3300202"},
    // Group of 3, all three don't have an associated ID.
    {"FieldTrialX#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     "3300202"},
  };

  // Register a few VariationIDs.
  AssociateGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE, "FieldTrialA",
                             "Default", TEST_VALUE_A);
  AssociateGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE, "FieldTrialB",
                             "Group1", TEST_VALUE_B);
  AssociateGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE, "FieldTrialC",
                             "Default", TEST_VALUE_C);
  AssociateGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE, "FieldTrialD",
                             "Default", TEST_VALUE_D);  // Not actually used.

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    // Parse the input groups.
    base::FieldTrial::ActiveGroups groups;
    std::vector<std::string> group_data = base::SplitString(
        test_cases[i].active_group_pairs, "#",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(0U, group_data.size() % 2);
    for (size_t j = 0; j < group_data.size(); j += 2) {
      base::FieldTrial::ActiveGroup group;
      group.trial_name = group_data[j];
      group.group_name = group_data[j + 1];
      groups.push_back(group);
    }

    // Parse the expected output.
    std::vector<std::string> expected_ids_list = base::SplitString(
        test_cases[i].expected_ids, "#",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string experiment_labels_string = base::UTF16ToUTF8(
        BuildGoogleUpdateExperimentLabel(groups));

    // Split the VariationIDs from the labels for verification below.
    std::set<std::string> parsed_ids;
    for (const std::string& label : base::SplitString(
             experiment_labels_string, ";",
             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // The ID is precisely between the '=' and '|' characters in each label.
      size_t index_of_equals = label.find('=');
      size_t index_of_pipe = label.find('|');
      ASSERT_NE(std::string::npos, index_of_equals);
      ASSERT_NE(std::string::npos, index_of_pipe);
      ASSERT_GT(index_of_pipe, index_of_equals);
      parsed_ids.insert(label.substr(index_of_equals + 1,
                                     index_of_pipe - index_of_equals - 1));
    }

    // Verify that the resulting string contains each of the expected labels,
    // and nothing more. Note that the date is stripped out and ignored.
    for (std::vector<std::string>::const_iterator it =
             expected_ids_list.begin(); it != expected_ids_list.end(); ++it) {
      std::set<std::string>::iterator it2 = parsed_ids.find(*it);
      EXPECT_TRUE(parsed_ids.end() != it2);
      parsed_ids.erase(it2);
    }
    EXPECT_TRUE(parsed_ids.empty());
  }  // for
}

TEST(ExperimentLabelsTest, CombineExperimentLabels) {
  struct {
    const char* variations_labels;
    const char* other_labels;
    const char* expected_label;
  } test_cases[] = {
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     "C=D|Tue, 21 Jan 2014 15:30:21 GMT;A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     "P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT",
     "P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT;"
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"",
     "",
     ""},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    std::string result = base::UTF16ToUTF8(CombineExperimentLabels(
        base::ASCIIToUTF16(test_cases[i].variations_labels),
        base::ASCIIToUTF16(test_cases[i].other_labels)));
    EXPECT_EQ(test_cases[i].expected_label, result);
  }
}

TEST(ExperimentLabelsTest, ExtractNonVariationLabels) {
  struct {
    const char* input_label;
    const char* expected_output;
  } test_cases[] = {
    // Empty
    {"", ""},
    // One
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Three
    {"CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation, flipped
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Sandwiched
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Only Variations
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     ""},
    // Empty values
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Trailing semicolon
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;",  // Note the semi here.
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Semis
    {";;;;", ""},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    std::string non_variation_labels = base::UTF16ToUTF8(
        ExtractNonVariationLabels(
            base::ASCIIToUTF16(test_cases[i].input_label)));
    EXPECT_EQ(test_cases[i].expected_output, non_variation_labels);
  }
}

}  // namespace variations
