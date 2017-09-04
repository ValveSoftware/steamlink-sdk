// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"

namespace blink {

namespace {

const char* kValidPolicies[] = {
    "{\"feature\": []}",
    "{\"feature\": [\"self\"]}",
    "{\"feature\": [\"*\"]}",
    "{\"feature\": [\"" ORIGIN_A "\"]}",
    "{\"feature\": [\"" ORIGIN_B "\"]}",
    "{\"feature\": [\"" ORIGIN_A "\", \"" ORIGIN_B "\"]}",
    "{\"feature1\": [\"" ORIGIN_A "\"], \"feature2\": [\"self\"]}",
    "{\"feature1\": [\"" ORIGIN_A "\"]}, {\"feature2\": [\"self\"]}"};

const char* kInvalidPolicies[] = {
    "Not A JSON literal",
    "\"Not a JSON object\"",
    "[\"Also\", \"Not a JSON object\"]",
    "1.0",
    "{\"Whitelist\": \"Not a JSON array\"}",
    "{\"feature1\": [\"*\"], \"feature2\": \"Not an array\"}"};

// This is an example of a feature which should be enabled by default in all
// frames.
const FeaturePolicy::Feature kDefaultOnFeature{
    "default-on", FeaturePolicy::FeatureDefault::EnableForAll};

// This is an example of a feature which should be enabled in top-level frames,
// and same-origin child-frames, but must be delegated to all cross-origin
// frames explicitly.
const FeaturePolicy::Feature kDefaultSelfFeature{
    "default-self", FeaturePolicy::FeatureDefault::EnableForSelf};

// This is an example of a feature which should be disabled by default, both in
// top-level and nested frames.
const FeaturePolicy::Feature kDefaultOffFeature{
    "default-off", FeaturePolicy::FeatureDefault::DisableForAll};

}  // namespace

class FeaturePolicyTest : public ::testing::Test {
 protected:
  FeaturePolicyTest()
      : m_frameworkWasEnabled(RuntimeEnabledFeatures::featurePolicyEnabled()),
        m_featureList(
            {&kDefaultOnFeature, &kDefaultSelfFeature, &kDefaultOffFeature}) {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(true);
  }

  ~FeaturePolicyTest() {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(m_frameworkWasEnabled);
  }

  std::unique_ptr<FeaturePolicy> createFromParentPolicy(
      const FeaturePolicy* parent,
      RefPtr<SecurityOrigin> origin) {
    return FeaturePolicy::createFromParentPolicy(parent, origin, m_featureList);
  }

  RefPtr<SecurityOrigin> m_originA = SecurityOrigin::createFromString(ORIGIN_A);
  RefPtr<SecurityOrigin> m_originB = SecurityOrigin::createFromString(ORIGIN_B);
  RefPtr<SecurityOrigin> m_originC = SecurityOrigin::createFromString(ORIGIN_C);

 private:
  const bool m_frameworkWasEnabled;

  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  FeaturePolicy::FeatureList m_featureList;
};

TEST_F(FeaturePolicyTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kValidPolicies) {
    messages.clear();
    std::unique_ptr<FeaturePolicy> policy =
        createFromParentPolicy(nullptr, m_originA);
    policy->setHeaderPolicy(policyString, messages);
    EXPECT_EQ(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kInvalidPolicies) {
    messages.clear();
    std::unique_ptr<FeaturePolicy> policy =
        createFromParentPolicy(nullptr, m_originA);
    policy->setHeaderPolicy(policyString, messages);
    EXPECT_NE(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, TestInitialPolicy) {
  // +-------------+
  // |(1)Origin A  |
  // |No Policy    |
  // +-------------+
  // Default-on and top-level-only features should be enabled in top-level
  // frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialSameOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin A  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on and Default-self features should be enabled in a same-origin
  // child frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialCrossOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin B  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on features should be enabled in child frame. Default-self and
  // Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestCrossOriginChildCannotEnableFeature) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |No Policy                              |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |Policy: {"default-self": ["self"]} | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Default-self feature should be disabled in cross origin frame, even if no
  // policy was specified in the parent frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-self\": [\"self\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFrameSelfInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["self"]}        |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin A     |  |(4) Origin B     | |
  // | |No Policy        |  |No Policy        | |
  // | | +-------------+ |  | +-------------+ | |
  // | | |(3)Origin A  | |  | |(5)Origin B  | | |
  // | | |No Policy    | |  | |No Policy    | | |
  // | | +-------------+ |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled at the top-level, and through the chain of
  // same-origin frames 2 and 3. It should be disabled in frames 4 and 5, as
  // they are at a different origin.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy5 =
      createFromParentPolicy(policy4.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy5->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestReflexiveFrameSelfInheritance) {
  // +-----------------------------------+
  // |(1) Origin A                       |
  // |Policy: {"default-self": ["self"]} |
  // | +-----------------+               |
  // | |(2) Origin B     |               |
  // | |No Policy        |               |
  // | | +-------------+ |               |
  // | | |(3)Origin A  | |               |
  // | | |No Policy    | |               |
  // | | +-------------+ |               |
  // | +-----------------+               |
  // +-----------------------------------+
  // Feature which is enabled at top-level should be disabled in frame 3, as
  // it is embedded by frame 2, for which the feature is not enabled.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestSelectiveFrameInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["Origin B"]}    |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled in second level Origin B frame, but disabled in
  // Frame 4, because it is embedded by frame 3, where the feature is not
  // enabled.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"" ORIGIN_B "\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy1.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy3.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestPolicyCanBlockSelf) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // +----------------------------+
  // Default-on feature should be disabled in top-level frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": []}", messages);
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksSameOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin A  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Feature should be disabled in child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": []}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockSelf) {
  // +--------------------------------+
  // |(1)Origin A                     |
  // |No Policy                       |
  // | +----------------------------+ |
  // | |(2)Origin B                 | |
  // | |Policy: {"default-on": []}  | |
  // | +----------------------------+ |
  // +--------------------------------+
  // Default-on feature should be disabled by cross-origin child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-on\": []}", messages);
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockChildren) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-on": ["self"]}  | |
  // | | +-------------+                  | |
  // | | |(3)Origin C  |                  | |
  // | | |No Policy    |                  | |
  // | | +-------------+                  | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be enabled in frames 1 and 2; disabled in frame
  // 3 by child frame policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-on\": [\"self\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksCrossOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin B  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Default-on feature should be disabled in cross-origin child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": []}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestEnableForAllOrigins) {
  // +--------------------------------+
  // |(1) Origin A                    |
  // |Policy: {"default-self": ["*"]} |
  // | +-----------------+            |
  // | |(2) Origin B     |            |
  // | |No Policy        |            |
  // | | +-------------+ |            |
  // | | |(3)Origin A  | |            |
  // | | |No Policy    | |            |
  // | | +-------------+ |            |
  // | +-----------------+            |
  // +--------------------------------+
  // Feature should be enabled in top and second level; disabled in frame 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"*\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOnEnablesForAllAncestors) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-on": ["Origin B"]}   |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frame 1; enabled in frames 2, 3 and 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": [\"" ORIGIN_B "\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy4->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultSelfRespectsSameOriginEmbedding) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-self": ["Origin B"]} |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frames 1 and 4; enabled in frames 2 and 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"" ORIGIN_B "\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOffMustBeDelegatedToAllCrossOriginFrames) {
  // +------------------------------------------------------------+
  // |(1) Origin A                                                |
  // |Policy: {"default-off": ["Origin B"]}                       |
  // | +--------------------------------------------------------+ |
  // | |(2) Origin B                                            | |
  // | |Policy: {"default-off": ["self"]}                       | |
  // | | +-------------+   +----------------------------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C                       | | |
  // | | |No Policy    |   |Policy: {"default-off": ["self"]} | | |
  // | | +-------------+   +----------------------------------+ | |
  // | +--------------------------------------------------------+ |
  // +------------------------------------------------------------+
  // Feature should be disabled in frames 1, 3 and 4; enabled in frame 2 only.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-off\": [\"" ORIGIN_B "\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-off\": [\"self\"]}", messages);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  policy4->setHeaderPolicy("{\"default-off\": [\"self\"]}", messages);
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestReenableForAllOrigins) {
  // +------------------------------------+
  // |(1) Origin A                        |
  // |Policy: {"default-self": ["*"]}     |
  // | +--------------------------------+ |
  // | |(2) Origin B                    | |
  // | |Policy: {"default-self": ["*"]} | |
  // | | +-------------+                | |
  // | | |(3)Origin A  |                | |
  // | | |No Policy    |                | |
  // | | +-------------+                | |
  // | +--------------------------------+ |
  // +------------------------------------+
  // Feature should be enabled in all frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"*\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-self\": [\"*\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestBlockedFrameCannotReenable) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Policy: {"default-self": ["self"]}    |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-self": ["*"]}   | |
  // | | +-------------+  +-------------+ | |
  // | | |(3)Origin A  |  |(4)Origin C  | | |
  // | | |No Policy    |  |No Policy    | | |
  // | | +-------------+  +-------------+ | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Feature should be enabled at the top level; disabled in all other frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-self\": [\"*\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegate) {
  // +---------------------------------------------------+
  // |(1) Origin A                                       |
  // |Policy: {"default-self": ["self", "Origin B"]}     |
  // | +-----------------------------------------------+ |
  // | |(2) Origin B                                   | |
  // | |Policy: {"default-self": ["self", "Origin C"]} | |
  // | | +-------------+                               | |
  // | | |(3)Origin C  |                               | |
  // | | |No Policy    |                               | |
  // | | +-------------+                               | |
  // | +-----------------------------------------------+ |
  // +---------------------------------------------------+
  // Feature should be enabled in all frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\", \"" ORIGIN_B "\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy("{\"default-self\": [\"self\", \"" ORIGIN_C "\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-on": ["self", "Origin B"]}   |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1, 2, and 3, and disabled in frame 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": [\"self\", \"" ORIGIN_B "\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestNonNestedFeaturesDontDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"]} |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1 and 2, and disabled in frames 3 and
  // 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\", \"" ORIGIN_B "\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFeaturesAreIndependent) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"], |
  // |         "default-on": ["self"]}               |
  // | +-------------------------------------------+ |
  // | |(2) Origin B                               | |
  // | |Policy: {"default-self": ["*"],            | |
  // | |         "default-on": ["*"]}              | |
  // | | +-------------+                           | |
  // | | |(3)Origin C  |                           | |
  // | | |No Policy    |                           | |
  // | | +-------------+                           | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be enabled in all frames; Default-on feature
  // should be enabled in frame 1, and disabled in frames 2 and 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-self\": [\"self\", \"" ORIGIN_B
                           "\"], \"default-on\": [\"self\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(
      "{\"default-self\": [\"*\"], \"default-on\": [\"*\"]}", messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestFeatureEnabledForOrigin) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-off": ["self", "Origin B"]}  |
  // +-----------------------------------------------+
  // Features should be enabled by the policy in frame 1 for origins A and B,
  // and disabled for origin C.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-off\": [\"self\", \"" ORIGIN_B "\"]}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
}

}  // namespace blink
