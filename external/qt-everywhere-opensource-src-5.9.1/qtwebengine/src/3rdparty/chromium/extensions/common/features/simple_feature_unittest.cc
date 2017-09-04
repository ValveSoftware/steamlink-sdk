// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using version_info::Channel;

namespace extensions {

namespace {

struct IsAvailableTestData {
  std::string extension_id;
  Manifest::Type extension_type;
  Manifest::Location location;
  Feature::Platform platform;
  int manifest_version;
  Feature::AvailabilityResult expected_result;
};

struct FeatureSessionTypeTestData {
  std::string desc;
  Feature::AvailabilityResult expected_availability;
  FeatureSessionType current_session_type;
  std::initializer_list<FeatureSessionType> feature_session_types;
};

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass();
}

Feature::AvailabilityResult IsAvailableInChannel(Channel channel_for_feature,
                                                 Channel channel_for_testing) {
  ScopedCurrentChannel current_channel(channel_for_testing);

  SimpleFeature feature;
  feature.set_channel(channel_for_feature);
  return feature
      .IsAvailableToManifest("random-extension", Manifest::TYPE_UNKNOWN,
                             Manifest::INVALID_LOCATION, -1,
                             Feature::GetCurrentPlatform())
      .result();
}

}  // namespace

class SimpleFeatureTest : public testing::Test {
 protected:
  SimpleFeatureTest() : current_channel_(Channel::UNKNOWN) {}
  bool LocationIsAvailable(SimpleFeature::Location feature_location,
                           Manifest::Location manifest_location) {
    SimpleFeature feature;
    feature.set_location(feature_location);
    Feature::AvailabilityResult availability_result =
        feature.IsAvailableToManifest(std::string(),
                                      Manifest::TYPE_UNKNOWN,
                                      manifest_location,
                                      -1,
                                      Feature::UNSPECIFIED_PLATFORM).result();
    return availability_result == Feature::IS_AVAILABLE;
  }

 private:
  ScopedCurrentChannel current_channel_;
  DISALLOW_COPY_AND_ASSIGN(SimpleFeatureTest);
};

TEST_F(SimpleFeatureTest, IsAvailableNullCase) {
  const IsAvailableTestData tests[] = {
      {"", Manifest::TYPE_UNKNOWN, Manifest::INVALID_LOCATION,
       Feature::UNSPECIFIED_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"random-extension", Manifest::TYPE_UNKNOWN, Manifest::INVALID_LOCATION,
       Feature::UNSPECIFIED_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"", Manifest::TYPE_LEGACY_PACKAGED_APP, Manifest::INVALID_LOCATION,
       Feature::UNSPECIFIED_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"", Manifest::TYPE_UNKNOWN, Manifest::INVALID_LOCATION,
       Feature::UNSPECIFIED_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"", Manifest::TYPE_UNKNOWN, Manifest::COMPONENT,
       Feature::UNSPECIFIED_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"", Manifest::TYPE_UNKNOWN, Manifest::INVALID_LOCATION,
       Feature::CHROMEOS_PLATFORM, -1, Feature::IS_AVAILABLE},
      {"", Manifest::TYPE_UNKNOWN, Manifest::INVALID_LOCATION,
       Feature::UNSPECIFIED_PLATFORM, 25, Feature::IS_AVAILABLE}};

  SimpleFeature feature;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    const IsAvailableTestData& test = tests[i];
    EXPECT_EQ(test.expected_result,
              feature.IsAvailableToManifest(test.extension_id,
                                            test.extension_type,
                                            test.location,
                                            test.manifest_version,
                                            test.platform).result());
  }
}

TEST_F(SimpleFeatureTest, Whitelist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBaz("bazabbbbccccddddeeeeffffgggghhhh");
  SimpleFeature feature;
  feature.whitelist_.push_back(kIdFoo);
  feature.whitelist_.push_back(kIdBar);

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(kIdFoo,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(kIdBar,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest(kIdBaz,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  feature.extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest(kIdBaz,
                                    Manifest::TYPE_LEGACY_PACKAGED_APP,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, HashedIdWhitelist) {
  // echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
  //   sha1sum | tr '[:lower:]' '[:upper:]'
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdFooHashed("55BC7228A0D502A2A48C9BB16B07062A01E62897");
  SimpleFeature feature;

  feature.whitelist_.push_back(kIdFooHashed);

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(kIdFoo,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(kIdFooHashed,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest("slightlytoooolongforanextensionid",
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest("tooshortforanextensionid",
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, Blacklist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBaz("bazabbbbccccddddeeeeffffgggghhhh");
  SimpleFeature feature;
  feature.blacklist_.push_back(kIdFoo);
  feature.blacklist_.push_back(kIdBar);

  EXPECT_EQ(
      Feature::FOUND_IN_BLACKLIST,
      feature.IsAvailableToManifest(kIdFoo,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::FOUND_IN_BLACKLIST,
      feature.IsAvailableToManifest(kIdBar,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(kIdBaz,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, HashedIdBlacklist) {
  // echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
  //   sha1sum | tr '[:lower:]' '[:upper:]'
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdFooHashed("55BC7228A0D502A2A48C9BB16B07062A01E62897");
  SimpleFeature feature;

  feature.blacklist_.push_back(kIdFooHashed);

  EXPECT_EQ(
      Feature::FOUND_IN_BLACKLIST,
      feature.IsAvailableToManifest(kIdFoo,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_NE(
      Feature::FOUND_IN_BLACKLIST,
      feature.IsAvailableToManifest(kIdFooHashed,
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest("slightlytoooolongforanextensionid",
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest("tooshortforanextensionid",
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, PackageType) {
  SimpleFeature feature;
  feature.extension_types_.push_back(Manifest::TYPE_EXTENSION);
  feature.extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_EXTENSION,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_LEGACY_PACKAGED_APP,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_THEME,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, Context) {
  SimpleFeature feature;
  feature.set_name("somefeature");
  feature.contexts_.push_back(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);
  feature.platforms_.push_back(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(21);
  feature.set_max_manifest_version(25);

  base::DictionaryValue manifest;
  manifest.SetString("name", "test");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 21);
  manifest.SetString("app.launch.local_path", "foo.html");

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(extension.get());

  feature.whitelist_.push_back("monkey");
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.whitelist_.clear();

  feature.extension_types_.clear();
  feature.extension_types_.push_back(Manifest::TYPE_THEME);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_TYPE, availability.result());
    EXPECT_EQ("'somefeature' is only allowed for themes, "
              "but this is a legacy packaged app.",
              availability.message());
  }

  feature.extension_types_.clear();
  feature.extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);
  feature.contexts_.clear();
  feature.contexts_.push_back(Feature::UNBLESSED_EXTENSION_CONTEXT);
  feature.contexts_.push_back(Feature::CONTENT_SCRIPT_CONTEXT);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_CONTEXT, availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes and "
              "content scripts, but this is a privileged page",
              availability.message());
  }

  feature.contexts_.push_back(Feature::WEB_PAGE_CONTEXT);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_CONTEXT, availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes, "
              "content scripts, and web pages, but this is a privileged page",
              availability.message());
  }

  feature.contexts_.clear();
  feature.contexts_.push_back(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.set_location(SimpleFeature::COMPONENT_LOCATION);
  EXPECT_EQ(Feature::INVALID_LOCATION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_location(SimpleFeature::UNSPECIFIED_LOCATION);

  EXPECT_EQ(Feature::INVALID_PLATFORM, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::UNSPECIFIED_PLATFORM).result());

  feature.set_min_manifest_version(22);
  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_min_manifest_version(21);

  feature.set_max_manifest_version(18);
  EXPECT_EQ(Feature::INVALID_MAX_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_max_manifest_version(25);
}

TEST_F(SimpleFeatureTest, SessionType) {
  base::DictionaryValue manifest;
  manifest.SetString("name", "test");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 2);
  manifest.SetString("app.launch.local_path", "foo.html");

  std::string error;
  scoped_refptr<const Extension> extension(
      Extension::Create(base::FilePath(), Manifest::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(extension.get());

  const FeatureSessionTypeTestData kTestData[] = {
      {"kiosk_feature in kiosk session",
       Feature::IS_AVAILABLE,
       FeatureSessionType::KIOSK,
       {FeatureSessionType::KIOSK}},
      {"kiosk feature in regular session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::REGULAR,
       {FeatureSessionType::KIOSK}},
      {"kiosk feature in unknown session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::UNKNOWN,
       {FeatureSessionType::KIOSK}},
      {"kiosk feature in initial session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::INITIAL,
       {FeatureSessionType::KIOSK}},
      {"non kiosk feature in kiosk session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::KIOSK,
       {FeatureSessionType::REGULAR}},
      {"non kiosk feature in regular session",
       Feature::IS_AVAILABLE,
       FeatureSessionType::REGULAR,
       {FeatureSessionType::REGULAR}},
      {"non kiosk feature in unknown session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::UNKNOWN,
       {FeatureSessionType::REGULAR}},
      {"non kiosk feature in initial session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::INITIAL,
       {FeatureSessionType::REGULAR}},
      {"session agnostic feature in kiosk session",
       Feature::IS_AVAILABLE,
       FeatureSessionType::KIOSK,
       {}},
      {"session agnostic feature in regular session",
       Feature::IS_AVAILABLE,
       FeatureSessionType::REGULAR,
       {}},
      {"session agnostic feature in unknown session",
       Feature::IS_AVAILABLE,
       FeatureSessionType::UNKNOWN,
       {}},
      {"feature with multiple session types",
       Feature::IS_AVAILABLE,
       FeatureSessionType::REGULAR,
       {FeatureSessionType::REGULAR, FeatureSessionType::KIOSK}},
      {"feature with multiple session types in unknown session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::UNKNOWN,
       {FeatureSessionType::REGULAR, FeatureSessionType::KIOSK}},
      {"feature with multiple session types in initial session",
       Feature::INVALID_SESSION_TYPE,
       FeatureSessionType::INITIAL,
       {FeatureSessionType::REGULAR, FeatureSessionType::KIOSK}}};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    std::unique_ptr<base::AutoReset<FeatureSessionType>> current_session(
        ScopedCurrentFeatureSessionType(kTestData[i].current_session_type));

    SimpleFeature feature;
    feature.set_session_types(kTestData[i].feature_session_types);

    EXPECT_EQ(kTestData[i].expected_availability,
              feature
                  .IsAvailableToContext(extension.get(),
                                        Feature::BLESSED_EXTENSION_CONTEXT,
                                        Feature::CHROMEOS_PLATFORM)
                  .result())
        << "Failed test '" << kTestData[i].desc << "'.";

    EXPECT_EQ(
        kTestData[i].expected_availability,
        feature
            .IsAvailableToManifest(extension->id(), Manifest::TYPE_UNKNOWN,
                                   Manifest::INVALID_LOCATION, -1,
                                   Feature::CHROMEOS_PLATFORM)
            .result())
        << "Failed test '" << kTestData[i].desc << "'.";
  }
}

TEST_F(SimpleFeatureTest, Location) {
  // Component extensions can access any location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                  Manifest::COMPONENT));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::EXTERNAL_COMPONENT_LOCATION,
                                  Manifest::COMPONENT));
  EXPECT_TRUE(
      LocationIsAvailable(SimpleFeature::POLICY_LOCATION, Manifest::COMPONENT));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::UNSPECIFIED_LOCATION,
                                  Manifest::COMPONENT));

  // Only component extensions can access the "component" location.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::INVALID_LOCATION));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::UNPACKED));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::EXTERNAL_COMPONENT));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::EXTERNAL_PREF_DOWNLOAD));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::EXTERNAL_POLICY));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::COMPONENT_LOCATION,
                                   Manifest::EXTERNAL_POLICY_DOWNLOAD));

  // Policy extensions can access the "policy" location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                  Manifest::EXTERNAL_POLICY));
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                  Manifest::EXTERNAL_POLICY_DOWNLOAD));

  // Non-policy (except component) extensions cannot access policy.
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   Manifest::EXTERNAL_COMPONENT));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   Manifest::INVALID_LOCATION));
  EXPECT_FALSE(
      LocationIsAvailable(SimpleFeature::POLICY_LOCATION, Manifest::UNPACKED));
  EXPECT_FALSE(LocationIsAvailable(SimpleFeature::POLICY_LOCATION,
                                   Manifest::EXTERNAL_PREF_DOWNLOAD));

  // External component extensions can access the "external_component"
  // location.
  EXPECT_TRUE(LocationIsAvailable(SimpleFeature::EXTERNAL_COMPONENT_LOCATION,
                                  Manifest::EXTERNAL_COMPONENT));
}

TEST_F(SimpleFeatureTest, Platform) {
  SimpleFeature feature;
  feature.platforms_.push_back(Feature::CHROMEOS_PLATFORM);
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature.IsAvailableToManifest(std::string(),
                                          Manifest::TYPE_UNKNOWN,
                                          Manifest::INVALID_LOCATION,
                                          -1,
                                          Feature::CHROMEOS_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_PLATFORM,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, ManifestVersion) {
  SimpleFeature feature;
  feature.set_min_manifest_version(5);

  EXPECT_EQ(
      Feature::INVALID_MIN_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    0,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_MIN_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    4,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    5,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    10,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  feature.set_max_manifest_version(8);

  EXPECT_EQ(
      Feature::INVALID_MAX_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    10,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    8,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Manifest::INVALID_LOCATION,
                                    7,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(SimpleFeatureTest, CommandLineSwitch) {
  SimpleFeature feature;
  feature.set_command_line_switch("laser-beams");
  {
    EXPECT_EQ(Feature::MISSING_COMMAND_LINE_SWITCH,
              feature.IsAvailableToEnvironment().result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams");
    EXPECT_EQ(Feature::MISSING_COMMAND_LINE_SWITCH,
              feature.IsAvailableToEnvironment().result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "enable-laser-beams");
    EXPECT_EQ(Feature::IS_AVAILABLE,
              feature.IsAvailableToEnvironment().result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "disable-laser-beams");
    EXPECT_EQ(Feature::MISSING_COMMAND_LINE_SWITCH,
              feature.IsAvailableToEnvironment().result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams=1");
    EXPECT_EQ(Feature::IS_AVAILABLE,
              feature.IsAvailableToEnvironment().result());
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch("laser-beams=0");
    EXPECT_EQ(Feature::MISSING_COMMAND_LINE_SWITCH,
              feature.IsAvailableToEnvironment().result());
  }
}

TEST_F(SimpleFeatureTest, IsIdInArray) {
  EXPECT_FALSE(SimpleFeature::IsIdInArray("", {}, 0));
  EXPECT_FALSE(SimpleFeature::IsIdInArray(
      "bbbbccccdddddddddeeeeeeffffgghhh", {}, 0));

  const char* const kIdArray[] = {
    "bbbbccccdddddddddeeeeeeffffgghhh",
    // aaaabbbbccccddddeeeeffffgggghhhh
    "9A0417016F345C934A1A88F55CA17C05014EEEBA"
  };
  EXPECT_FALSE(SimpleFeature::IsIdInArray("", kIdArray, arraysize(kIdArray)));
  EXPECT_FALSE(SimpleFeature::IsIdInArray(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kIdArray, arraysize(kIdArray)));
  EXPECT_TRUE(SimpleFeature::IsIdInArray(
      "bbbbccccdddddddddeeeeeeffffgghhh", kIdArray, arraysize(kIdArray)));
  EXPECT_TRUE(SimpleFeature::IsIdInArray(
      "aaaabbbbccccddddeeeeffffgggghhhh", kIdArray, arraysize(kIdArray)));
}

// Tests that all combinations of feature channel and Chrome channel correctly
// compute feature availability.
TEST_F(SimpleFeatureTest, SupportedChannel) {
  // stable supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::STABLE, Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::STABLE, Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::STABLE, Channel::DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::STABLE, Channel::BETA));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::STABLE, Channel::STABLE));

  // beta supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::BETA, Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::BETA, Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::BETA, Channel::DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::BETA, Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::BETA, Channel::STABLE));

  // dev supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::DEV, Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::DEV, Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::DEV, Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::DEV, Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::DEV, Channel::STABLE));

  // canary supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::CANARY, Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::CANARY, Channel::CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::CANARY, Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::CANARY, Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::CANARY, Channel::STABLE));

  // trunk supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::UNKNOWN));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel(Channel::UNKNOWN, Channel::STABLE));
}

// Tests simple feature availability across channels.
TEST_F(SimpleFeatureTest, SimpleFeatureAvailability) {
  std::unique_ptr<ComplexFeature> complex_feature;
  {
    std::unique_ptr<SimpleFeature> feature1(CreateFeature<SimpleFeature>());
    feature1->channel_.reset(new Channel(Channel::BETA));
    feature1->extension_types_.push_back(Manifest::TYPE_EXTENSION);
    std::unique_ptr<SimpleFeature> feature2(CreateFeature<SimpleFeature>());
    feature2->channel_.reset(new Channel(Channel::BETA));
    feature2->extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);
    std::vector<Feature*> list;
    list.push_back(feature1.release());
    list.push_back(feature2.release());
    complex_feature.reset(new ComplexFeature(&list));
  }

  Feature* feature = static_cast<Feature*>(complex_feature.get());
  // Make sure both rules are applied correctly.
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("2",
                                       Manifest::TYPE_LEGACY_PACKAGED_APP,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::STABLE);
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("2",
                                       Manifest::TYPE_LEGACY_PACKAGED_APP,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
  }
}

// Tests complex feature availability across channels.
TEST_F(SimpleFeatureTest, ComplexFeatureAvailability) {
  std::unique_ptr<ComplexFeature> complex_feature;
  {
    // Rule: "extension", channel trunk.
    std::unique_ptr<SimpleFeature> feature1(CreateFeature<SimpleFeature>());
    feature1->channel_.reset(new Channel(Channel::UNKNOWN));
    feature1->extension_types_.push_back(Manifest::TYPE_EXTENSION);
    std::unique_ptr<SimpleFeature> feature2(CreateFeature<SimpleFeature>());
    // Rule: "legacy_packaged_app", channel stable.
    feature2->channel_.reset(new Channel(Channel::STABLE));
    feature2->extension_types_.push_back(Manifest::TYPE_LEGACY_PACKAGED_APP);
    std::vector<Feature*> list;
    list.push_back(feature1.release());
    list.push_back(feature2.release());
    complex_feature.reset(new ComplexFeature(&list));
  }

  Feature* feature = static_cast<Feature*>(complex_feature.get());
  {
    ScopedCurrentChannel current_channel(Channel::UNKNOWN);
    EXPECT_EQ(Feature::IS_AVAILABLE,
              feature
                  ->IsAvailableToManifest("1", Manifest::TYPE_EXTENSION,
                                          Manifest::INVALID_LOCATION,
                                          Feature::UNSPECIFIED_PLATFORM)
                  .result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_EQ(Feature::IS_AVAILABLE,
              feature
                  ->IsAvailableToManifest(
                      "2", Manifest::TYPE_LEGACY_PACKAGED_APP,
                      Manifest::INVALID_LOCATION, Feature::UNSPECIFIED_PLATFORM)
                  .result());
  }
  {
    ScopedCurrentChannel current_channel(Channel::BETA);
    EXPECT_NE(Feature::IS_AVAILABLE,
              feature
                  ->IsAvailableToManifest("1", Manifest::TYPE_EXTENSION,
                                          Manifest::INVALID_LOCATION,
                                          Feature::UNSPECIFIED_PLATFORM)
                  .result());
  }
}

}  // namespace extensions
