// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/client_policy_controller.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

namespace {
const char kUndefinedNamespace[] = "undefined";

bool isTemporary(const OfflinePageClientPolicy& policy) {
  // Check if policy has a expire period > 0 or a limited number
  // of pages allowed.
  return (policy.lifetime_policy.page_limit > kUnlimitedPages ||
          !policy.lifetime_policy.expiration_period.is_zero());
}

}  // namespace

class ClientPolicyControllerTest : public testing::Test {
 public:
  ClientPolicyController* controller() { return controller_.get(); }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<ClientPolicyController> controller_;
};

void ClientPolicyControllerTest::SetUp() {
  controller_.reset(new ClientPolicyController());
}

void ClientPolicyControllerTest::TearDown() {
  controller_.reset();
}

TEST_F(ClientPolicyControllerTest, FallbackTest) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kUndefinedNamespace);
  EXPECT_EQ(policy.name_space, kDefaultNamespace);
  EXPECT_TRUE(isTemporary(policy));
}

TEST_F(ClientPolicyControllerTest, CheckBookmarkDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kBookmarkNamespace);
  EXPECT_EQ(policy.name_space, kBookmarkNamespace);
  EXPECT_TRUE(isTemporary(policy));
}

TEST_F(ClientPolicyControllerTest, CheckLastNDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kLastNNamespace);
  EXPECT_EQ(policy.name_space, kLastNNamespace);
  EXPECT_TRUE(isTemporary(policy));
}

}  // namespace offline_pages
