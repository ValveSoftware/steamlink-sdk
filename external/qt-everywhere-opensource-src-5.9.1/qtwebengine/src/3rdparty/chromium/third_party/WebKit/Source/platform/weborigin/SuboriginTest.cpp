// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/weborigin/Suborigin.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/dtoa/utils.h"

namespace blink {

bool policyContainsOnly(const Suborigin& suborigin,
                        const Suborigin::SuboriginPolicyOptions options[],
                        size_t optionsSize) {
  unsigned optionsMask = 0;
  for (size_t i = 0; i < optionsSize; i++)
    optionsMask |= static_cast<unsigned>(options[i]);

  return optionsMask == suborigin.optionsMask();
}

TEST(SuboriginTest, PolicyTests) {
  Suborigin suboriginUnsafePostmessageSend;
  Suborigin suboriginUnsafePostmessageReceive;
  Suborigin suboriginAllUnsafePostmessage;

  const Suborigin::SuboriginPolicyOptions emptyPolicies[] = {
      Suborigin::SuboriginPolicyOptions::None};
  const Suborigin::SuboriginPolicyOptions unsafePostmessageSendPolicy[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend};
  const Suborigin::SuboriginPolicyOptions unsafePostmessageReceivePolicy[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions allUnsafePostmessagePolicies[] = {
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive};

  EXPECT_TRUE(policyContainsOnly(suboriginUnsafePostmessageSend, emptyPolicies,
                                 ARRAY_SIZE(emptyPolicies)));

  suboriginUnsafePostmessageSend.addPolicyOption(
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend);
  EXPECT_TRUE(policyContainsOnly(suboriginUnsafePostmessageSend,
                                 unsafePostmessageSendPolicy,
                                 ARRAY_SIZE(unsafePostmessageSendPolicy)));
  EXPECT_FALSE(policyContainsOnly(suboriginUnsafePostmessageSend,
                                  allUnsafePostmessagePolicies,
                                  ARRAY_SIZE(allUnsafePostmessagePolicies)));

  suboriginUnsafePostmessageReceive.addPolicyOption(
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive);
  EXPECT_TRUE(policyContainsOnly(suboriginUnsafePostmessageReceive,
                                 unsafePostmessageReceivePolicy,
                                 ARRAY_SIZE(unsafePostmessageReceivePolicy)));
  EXPECT_FALSE(policyContainsOnly(suboriginUnsafePostmessageReceive,
                                  allUnsafePostmessagePolicies,
                                  ARRAY_SIZE(allUnsafePostmessagePolicies)));

  suboriginAllUnsafePostmessage.addPolicyOption(
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageSend);
  suboriginAllUnsafePostmessage.addPolicyOption(
      Suborigin::SuboriginPolicyOptions::UnsafePostMessageReceive);
  EXPECT_TRUE(policyContainsOnly(suboriginAllUnsafePostmessage,
                                 allUnsafePostmessagePolicies,
                                 ARRAY_SIZE(allUnsafePostmessagePolicies)));
}

}  // namespace blink
