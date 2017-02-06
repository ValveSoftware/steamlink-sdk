// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/HostsUsingFeatures.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HostsUsingFeaturesTest, countName)
{
    HostsUsingFeatures hostsUsingFeatures;
    hostsUsingFeatures.countName(HostsUsingFeatures::Feature::EventPath, "test 1");
    EXPECT_EQ(1u, hostsUsingFeatures.valueByName().size());
    hostsUsingFeatures.countName(HostsUsingFeatures::Feature::ElementCreateShadowRoot, "test 1");
    EXPECT_EQ(1u, hostsUsingFeatures.valueByName().size());
    hostsUsingFeatures.countName(HostsUsingFeatures::Feature::EventPath, "test 2");
    EXPECT_EQ(2u, hostsUsingFeatures.valueByName().size());

    EXPECT_TRUE(hostsUsingFeatures.valueByName().get("test 1").get(HostsUsingFeatures::Feature::EventPath));
    EXPECT_TRUE(hostsUsingFeatures.valueByName().get("test 1").get(HostsUsingFeatures::Feature::ElementCreateShadowRoot));
    EXPECT_FALSE(hostsUsingFeatures.valueByName().get("test 1").get(HostsUsingFeatures::Feature::DocumentRegisterElement));
    EXPECT_TRUE(hostsUsingFeatures.valueByName().get("test 2").get(HostsUsingFeatures::Feature::EventPath));
    EXPECT_FALSE(hostsUsingFeatures.valueByName().get("test 2").get(HostsUsingFeatures::Feature::ElementCreateShadowRoot));
    EXPECT_FALSE(hostsUsingFeatures.valueByName().get("test 2").get(HostsUsingFeatures::Feature::DocumentRegisterElement));

    hostsUsingFeatures.clear();
}

} // namespace blink
