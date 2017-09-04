// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <string>
#include <utility>

#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ComplexFeatureTest, MultipleRulesWhitelist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  std::vector<Feature*> features;

  {
    // Rule: "extension", whitelist "foo".
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_whitelist({kIdFoo.c_str()});
    simple_feature->set_extension_types({Manifest::TYPE_EXTENSION});
    features.push_back(simple_feature.release());
  }

  {
    // Rule: "legacy_packaged_app", whitelist "bar".
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_whitelist({kIdBar.c_str()});
    simple_feature->set_extension_types({Manifest::TYPE_LEGACY_PACKAGED_APP});
    features.push_back(simple_feature.release());
  }

  std::unique_ptr<ComplexFeature> feature(new ComplexFeature(&features));

  // Test match 1st rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdFoo,
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Test match 2nd rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdBar,
                                     Manifest::TYPE_LEGACY_PACKAGED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Test whitelist with wrong extension type.
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdBar,
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdFoo,
                                     Manifest::TYPE_LEGACY_PACKAGED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
}

// Tests that dependencies are correctly checked.
TEST(ComplexFeatureTest, Dependencies) {
  std::vector<Feature*> features;

  {
    // Rule which depends on an extension-only feature
    // (content_security_policy).
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_dependencies({"manifest:content_security_policy"});
    features.push_back(simple_feature.release());
  }

  {
    // Rule which depends on an platform-app-only feature (serial).
    std::unique_ptr<SimpleFeature> simple_feature(new SimpleFeature());
    simple_feature->set_dependencies({"permission:serial"});
    features.push_back(simple_feature.release());
  }

  std::unique_ptr<ComplexFeature> feature(new ComplexFeature(&features));

  // Available to extensions because of the content_security_policy rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest("extensionid",
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Available to platform apps because of the serial rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest("platformappid",
                                     Manifest::TYPE_PLATFORM_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Not available to hosted apps.
  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature->IsAvailableToManifest("hostedappid",
                                     Manifest::TYPE_HOSTED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
}

}  // namespace extensions
