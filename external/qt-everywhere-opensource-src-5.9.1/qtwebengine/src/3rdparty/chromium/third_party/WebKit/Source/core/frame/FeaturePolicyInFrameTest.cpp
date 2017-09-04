// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"

namespace blink {

namespace {

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

class FeaturePolicyInFrameTest : public ::testing::Test {
 protected:
  FeaturePolicyInFrameTest()
      : m_frameworkWasEnabled(RuntimeEnabledFeatures::featurePolicyEnabled()),
        m_featureList(
            {&kDefaultOnFeature, &kDefaultSelfFeature, &kDefaultOffFeature}) {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(true);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_dummyPageHolder->document().setSecurityOrigin(m_originA);
  }

  ~FeaturePolicyInFrameTest() {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(m_frameworkWasEnabled);
  }

  std::unique_ptr<FeaturePolicy> createFromParentPolicy(
      const FeaturePolicy* parent,
      RefPtr<SecurityOrigin> origin) {
    return FeaturePolicy::createFromParentPolicy(parent, origin, m_featureList);
  }

  Document& document() { return m_dummyPageHolder->document(); }

  LocalFrame* frame() { return m_dummyPageHolder->document().frame(); }

  RefPtr<SecurityOrigin> m_originA = SecurityOrigin::createFromString(ORIGIN_A);
  RefPtr<SecurityOrigin> m_originB = SecurityOrigin::createFromString(ORIGIN_B);

 private:
  const bool m_frameworkWasEnabled;

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;

  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  FeaturePolicy::FeatureList m_featureList;
};

TEST_F(FeaturePolicyInFrameTest, TestFeatureDefaultsInTopLevelFrame) {
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  document().setFeaturePolicy(std::move(policy1));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultOnFeature, frame()));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultSelfFeature, frame()));
  EXPECT_FALSE(isFeatureEnabledInFrame(kDefaultOffFeature, frame()));
}

TEST_F(FeaturePolicyInFrameTest, TestFeatureDefaultsInCrossOriginFrame) {
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originB);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  document().setFeaturePolicy(std::move(policy2));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultOnFeature, frame()));
  EXPECT_FALSE(isFeatureEnabledInFrame(kDefaultSelfFeature, frame()));
  EXPECT_FALSE(isFeatureEnabledInFrame(kDefaultOffFeature, frame()));
}

TEST_F(FeaturePolicyInFrameTest, TestFeatureOverriddenInFrame) {
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-off\": [\"self\"], \"default-on\": []}",
                           messages);
  EXPECT_EQ(0UL, messages.size());
  document().setFeaturePolicy(std::move(policy1));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultOffFeature, frame()));
  EXPECT_FALSE(isFeatureEnabledInFrame(kDefaultOnFeature, frame()));
}

// Ensure that features are enabled / disabled correctly when the FP runtime
// flag is turned off.
TEST_F(FeaturePolicyInFrameTest, TestFeatureDefaultsWhenFPDisabled) {
  RuntimeEnabledFeatures::setFeaturePolicyEnabled(false);
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultOnFeature, frame()));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultSelfFeature, frame()));
  EXPECT_FALSE(isFeatureEnabledInFrame(kDefaultOffFeature, frame()));
}

// Ensure that feature policy has no effect when the FP runtime flag is turned
// off.
TEST_F(FeaturePolicyInFrameTest, TestPolicyInactiveWhenFPDisabled) {
  RuntimeEnabledFeatures::setFeaturePolicyEnabled(false);
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy("{\"default-on\": []}", messages);
  EXPECT_EQ(0UL, messages.size());
  document().setFeaturePolicy(std::move(policy1));
  EXPECT_TRUE(isFeatureEnabledInFrame(kDefaultOnFeature, frame()));
}

}  // namespace blink
