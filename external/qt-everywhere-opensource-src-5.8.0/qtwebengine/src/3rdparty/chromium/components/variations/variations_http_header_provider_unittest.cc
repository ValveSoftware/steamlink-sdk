// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <string>

#include "base/base64.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id) {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));

  AssociateGoogleVariationID(key, trial->trial_name(), trial->group_name(), id);

  return trial;
}

}  // namespace

class VariationsHttpHeaderProviderTest : public ::testing::Test {
 public:
  VariationsHttpHeaderProviderTest() {}

  ~VariationsHttpHeaderProviderTest() override {}

  void TearDown() override { testing::ClearAllVariationIDs(); }
};

TEST_F(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Valid) {
  base::MessageLoop loop;
  VariationsHttpHeaderProvider provider;

  // Valid experiment ids.
  EXPECT_TRUE(provider.SetDefaultVariationIds("12,456,t789"));
  provider.InitVariationIDsCacheIfNeeded();
  std::string variations = provider.GetClientDataHeader();
  EXPECT_FALSE(variations.empty());
  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
}

TEST_F(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Invalid) {
  base::MessageLoop loop;
  VariationsHttpHeaderProvider provider;

  // Invalid experiment ids.
  EXPECT_FALSE(provider.SetDefaultVariationIds("abcd12,456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeader().empty());

  // Invalid trigger experiment id
  EXPECT_FALSE(provider.SetDefaultVariationIds("12,tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeader().empty());
}

TEST_F(VariationsHttpHeaderProviderTest, OnFieldTrialGroupFinalized) {
  base::MessageLoop loop;
  base::FieldTrialList field_trial_list(nullptr);
  VariationsHttpHeaderProvider provider;
  provider.InitVariationIDsCacheIfNeeded();

  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", default_name, GOOGLE_WEB_PROPERTIES, 123));

  ASSERT_EQ(default_name, trial_1->group_name());

  scoped_refptr<base::FieldTrial> trial_2(CreateTrialAndAssociateId(
      "t2", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER, 456));

  ASSERT_EQ(default_name, trial_2->group_name());

  // Run the message loop to make sure OnFieldTrialGroupFinalized is called for
  // the two field trials.
  base::RunLoop().RunUntilIdle();

  std::string variations = provider.GetClientDataHeader();

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(123) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(456) != trigger_ids.end());
}

TEST_F(VariationsHttpHeaderProviderTest, GetVariationsString) {
  base::MessageLoop loop;
  base::FieldTrialList field_trial_list(nullptr);

  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES, 123);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES, 124);

  VariationsHttpHeaderProvider provider;
  provider.SetDefaultVariationIds("100,200");
  EXPECT_EQ(" 100 123 124 200 ", provider.GetVariationsString());
}

}  // namespace variations
