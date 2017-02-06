// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include <memory>

#include "base/metrics/field_trial.h"
#include "base/metrics/statistics_recorder.h"
#include "build/build_config.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search {

class EmbeddedSearchFieldTrialTest : public testing::Test {
 protected:
  void SetUp() override {
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::StatisticsRecorder::Initialize();
  }

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoEmptyAndValid) {
  FieldTrialFlags flags;

  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidNumber) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77.2"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidName) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Invalid77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidGroup) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidFlag) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewName) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewNameOverridesOld) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                                     "Group78 foo:5"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoLotsOfFlags) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(3ul, flags.size());
  EXPECT_EQ(true, GetBoolValueForFlagWithDefault("bar", false, flags));
  EXPECT_EQ(7ul, GetUInt64ValueForFlagWithDefault("baz", 0, flags));
  EXPECT_EQ("dogs",
            GetStringValueForFlagWithDefault("cat", std::string(), flags));
  EXPECT_EQ("default",
            GetStringValueForFlagWithDefault("moose", "default", flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoDisabled) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 bar:1 baz:7 cat:dogs DISABLED"));
  EXPECT_FALSE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoControlFlags) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Control77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(3ul, flags.size());
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
typedef EmbeddedSearchFieldTrialTest SearchTest;

TEST_F(SearchTest, ShouldPrefetchSearchResults_InstantExtendedAPIEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 espv:2"));
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
  EXPECT_TRUE(ShouldPrefetchSearchResults());
}

TEST_F(SearchTest, ShouldPrefetchSearchResults_Default) {
  EXPECT_TRUE(ShouldPrefetchSearchResults());
}

TEST_F(SearchTest, ShouldReuseInstantSearchBasePage_Default) {
  EXPECT_TRUE(ShouldReuseInstantSearchBasePage());
}

TEST_F(SearchTest, ShouldAllowPrefetchNonDefaultMatch_DisabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:89 allow_prefetch_non_default_match:0"));
  EXPECT_FALSE(ShouldAllowPrefetchNonDefaultMatch());
  EXPECT_EQ(89ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchTest, ShouldAllowPrefetchNonDefaultMatch_EnabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:80 allow_prefetch_non_default_match:1"));
  EXPECT_TRUE(ShouldAllowPrefetchNonDefaultMatch());
  EXPECT_EQ(80ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchTest, ForceInstantResultsParam) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 espv:2"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_EQ("ion=1&", ForceInstantResultsParam(true));
  EXPECT_EQ(std::string(), ForceInstantResultsParam(false));
}

typedef EmbeddedSearchFieldTrialTest InstantExtendedEnabledParamTest;

TEST_F(InstantExtendedEnabledParamTest, QueryExtractionDisabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 espv:12"));
  // Make sure InstantExtendedEnabledParam() returns an empty string for search
  // requests.
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ("", InstantExtendedEnabledParam(true));
  EXPECT_EQ("espv=12&", InstantExtendedEnabledParam(false));
}

TEST_F(InstantExtendedEnabledParamTest, QueryExtractionEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:10 query_extraction:1"));
  EXPECT_TRUE(IsQueryExtractionEnabled());
  // Make sure InstantExtendedEnabledParam() returns a non-empty param string
  // for search requests.
  EXPECT_EQ("espv=10&", InstantExtendedEnabledParam(true));
  EXPECT_EQ("espv=10&", InstantExtendedEnabledParam(false));
}

TEST_F(InstantExtendedEnabledParamTest, UseDefaultEmbeddedSearchPageVersion) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:-1 query_extraction:1"));
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ("espv=2&", InstantExtendedEnabledParam(true));
  EXPECT_EQ("espv=2&", InstantExtendedEnabledParam(false));
}

typedef EmbeddedSearchFieldTrialTest IsQueryExtractionEnabledTest;

TEST_F(IsQueryExtractionEnabledTest, NotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 espv:2"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, EnabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:1"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, DisabledViaFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:0"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, EnabledViaCommandLine) {
  EnableQueryExtractionForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:0"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

}  // namespace search
