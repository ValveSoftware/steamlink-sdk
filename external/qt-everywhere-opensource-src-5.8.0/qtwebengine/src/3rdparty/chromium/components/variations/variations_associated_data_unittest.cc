// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const VariationID TEST_VALUE_A = 3300200;
const VariationID TEST_VALUE_B = 3300201;

// Convenience helper to retrieve the variations::VariationID for a FieldTrial.
// Note that this will do the group assignment in |trial| if not already done.
VariationID GetIDForTrial(IDCollectionKey key, base::FieldTrial* trial) {
  return GetGoogleVariationID(key, trial->trial_name(), trial->group_name());
}

// Call FieldTrialList::FactoryGetFieldTrial() with a future expiry date.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name,
    int* default_group_number) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name,
      base::FieldTrialList::kNoExpirationYear, 1, 1,
      base::FieldTrial::SESSION_RANDOMIZED, default_group_number);
}

void CreateFeatureWithTrial(const base::Feature& feature,
                            base::FeatureList::OverrideState override_state,
                            base::FieldTrial* trial) {
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(feature.name, override_state,
                                           trial);
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace

class VariationsAssociatedDataTest : public ::testing::Test {
 public:
  VariationsAssociatedDataTest() : field_trial_list_(NULL) {
  }

  ~VariationsAssociatedDataTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(VariationsAssociatedDataTest);
};

// Test that if the trial is immediately disabled, GetGoogleVariationID just
// returns the empty ID.
TEST_F(VariationsAssociatedDataTest, DisableImmediately) {
  int default_group_number = -1;
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial("trial", 100, "default", &default_group_number));

  ASSERT_EQ(default_group_number, trial->group());
  ASSERT_EQ(EMPTY_ID, GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial.get()));
}

// Test that successfully associating the FieldTrial with some ID, and then
// disabling the FieldTrial actually makes GetGoogleVariationID correctly
// return the empty ID.
TEST_F(VariationsAssociatedDataTest, DisableAfterInitialization) {
  const std::string default_name = "default";
  const std::string non_default_name = "non_default";

  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial("trial", 100, default_name, NULL));

  trial->AppendGroup(non_default_name, 100);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial->trial_name(),
      default_name, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial->trial_name(),
      non_default_name, TEST_VALUE_B);
  trial->Disable();
  ASSERT_EQ(default_name, trial->group_name());
  ASSERT_EQ(TEST_VALUE_A, GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial.get()));
}

// Test various successful association cases.
TEST_F(VariationsAssociatedDataTest, AssociateGoogleVariationID) {
  const std::string default_name1 = "default";
  scoped_refptr<base::FieldTrial> trial_true(
      CreateFieldTrial("d1", 10, default_name1, NULL));
  const std::string winner = "TheWinner";
  int winner_group = trial_true->AppendGroup(winner, 10);

  // Set GoogleVariationIDs so we can verify that they were chosen correctly.
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      default_name1, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      winner, TEST_VALUE_B);

  EXPECT_EQ(winner_group, trial_true->group());
  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(TEST_VALUE_B,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      CreateFieldTrial("d2", 10, default_name2, NULL));
  const std::string loser = "ALoser";
  const int loser_group = trial_false->AppendGroup(loser, 0);

  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_false->trial_name(),
      default_name2, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_false->trial_name(),
      loser, TEST_VALUE_B);

  EXPECT_NE(loser_group, trial_false->group());
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_false.get()));
}

// Test that not associating a FieldTrial with any IDs ensure that the empty ID
// will be returned.
TEST_F(VariationsAssociatedDataTest, NoAssociation) {
  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> no_id_trial(
      CreateFieldTrial("d3", 10, default_name, NULL));

  const std::string winner = "TheWinner";
  const int winner_group = no_id_trial->AppendGroup(winner, 10);

  // Ensure that despite the fact that a normal winner is elected, it does not
  // have a valid VariationID associated with it.
  EXPECT_EQ(winner_group, no_id_trial->group());
  EXPECT_EQ(winner, no_id_trial->group_name());
  EXPECT_EQ(EMPTY_ID, GetIDForTrial(GOOGLE_WEB_PROPERTIES, no_id_trial.get()));
}

// Ensure that the AssociateGoogleVariationIDForce works as expected.
TEST_F(VariationsAssociatedDataTest, ForceAssociation) {
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group",
                             TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group",
                             TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES, "trial", "group",
                                  TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_B,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
}

// Ensure that two collections can coexist without affecting each other.
TEST_F(VariationsAssociatedDataTest, CollectionsCoexist) {
  const std::string default_name = "default";
  int default_group_number = -1;
  scoped_refptr<base::FieldTrial> trial_true(
      CreateFieldTrial("d1", 10, default_name, &default_group_number));
  ASSERT_EQ(default_group_number, trial_true->group());
  ASSERT_EQ(default_name, trial_true->group_name());

  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES_TRIGGER, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(GOOGLE_UPDATE_SERVICE, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(CHROME_SYNC_SERVICE, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  trial_true = CreateFieldTrial("d2", 10, default_name, &default_group_number);
  ASSERT_EQ(default_group_number, trial_true->group());
  ASSERT_EQ(default_name, trial_true->group_name());

  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES_TRIGGER,
                             trial_true->trial_name(), default_name,
                             TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES_TRIGGER, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(GOOGLE_UPDATE_SERVICE, trial_true->trial_name(),
                             default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES_TRIGGER, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(CHROME_SYNC_SERVICE, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES_TRIGGER, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(CHROME_SYNC_SERVICE, trial_true.get()));
}

TEST_F(VariationsAssociatedDataTest, AssociateVariationParams) {
  const std::string kTrialName = "AssociateVariationParams";

  {
    std::map<std::string, std::string> params;
    params["a"] = "10";
    params["b"] = "test";
    ASSERT_TRUE(AssociateVariationParams(kTrialName, "A", params));
  }
  {
    std::map<std::string, std::string> params;
    params["a"] = "5";
    ASSERT_TRUE(AssociateVariationParams(kTrialName, "B", params));
  }

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("5", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("5", params["a"]);
}

TEST_F(VariationsAssociatedDataTest, AssociateVariationParams_Fail) {
  const std::string kTrialName = "AssociateVariationParams_Fail";
  const std::string kGroupName = "A";

  std::map<std::string, std::string> params;
  params["a"] = "10";
  ASSERT_TRUE(AssociateVariationParams(kTrialName, kGroupName, params));
  params["a"] = "1";
  params["b"] = "2";
  ASSERT_FALSE(AssociateVariationParams(kTrialName, kGroupName, params));

  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  EXPECT_EQ("10", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
}

TEST_F(VariationsAssociatedDataTest, AssociateVariationParams_TrialActiveFail) {
  const std::string kTrialName = "AssociateVariationParams_TrialActiveFail";
  base::FieldTrialList::CreateFieldTrial(kTrialName, "A");
  ASSERT_EQ("A", base::FieldTrialList::FindFullName(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_FALSE(AssociateVariationParams(kTrialName, "B", params));
  EXPECT_FALSE(AssociateVariationParams(kTrialName, "A", params));
}

TEST_F(VariationsAssociatedDataTest,
       AssociateVariationParams_DoesntActivateTrial) {
  const std::string kTrialName = "AssociateVariationParams_DoesntActivateTrial";

  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_TRUE(AssociateVariationParams(kTrialName, "A", params));
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParams_NoTrial) {
  const std::string kTrialName = "GetVariationParams_NoParams";

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "y"));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParams_NoParams) {
  const std::string kTrialName = "GetVariationParams_NoParams";

  base::FieldTrialList::CreateFieldTrial(kTrialName, "A");

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "y"));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParams_ActivatesTrial) {
  const std::string kTrialName = "GetVariationParams_ActivatesTrial";

  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParamValue_ActivatesTrial) {
  const std::string kTrialName = "GetVariationParamValue_ActivatesTrial";

  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParamsByFeature) {
  const std::string kTrialName = "GetVariationParamsByFeature";
  const base::Feature kFeature{"TestFeature",
                               base::FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  variations::AssociateVariationParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));

  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_TRUE(GetVariationParamsByFeature(kFeature, &actualParams));
  EXPECT_EQ(params, actualParams);
}

TEST_F(VariationsAssociatedDataTest, GetVariationParamValueByFeature) {
  const std::string kTrialName = "GetVariationParamsByFeature";
  const base::Feature kFeature{"TestFeature",
                               base::FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  variations::AssociateVariationParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));

  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_EQ(params["x"], GetVariationParamValueByFeature(kFeature, "x"));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParamsByFeature_Disable) {
  const std::string kTrialName = "GetVariationParamsByFeature";
  const base::Feature kFeature{"TestFeature",
                               base::FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  variations::AssociateVariationParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));

  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_FALSE(GetVariationParamsByFeature(kFeature, &actualParams));
}

TEST_F(VariationsAssociatedDataTest, GetVariationParamValueByFeature_Disable) {
  const std::string kTrialName = "GetVariationParamsByFeature";
  const base::Feature kFeature{"TestFeature",
                               base::FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  variations::AssociateVariationParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));

  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_EQ(std::string(), GetVariationParamValueByFeature(kFeature, "x"));
}

}  // namespace variations
