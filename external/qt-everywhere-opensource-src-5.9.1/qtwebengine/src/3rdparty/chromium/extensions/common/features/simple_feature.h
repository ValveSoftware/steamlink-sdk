// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <stddef.h>

#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest.h"

namespace extensions {

class BaseFeatureProviderTest;
class ExtensionAPITest;
class ManifestUnitTest;
class SimpleFeatureTest;

class SimpleFeature : public Feature {
 public:
  // Used by tests to override the cached --whitelisted-extension-id.
  class ScopedWhitelistForTest {
   public:
    explicit ScopedWhitelistForTest(const std::string& id);
    ~ScopedWhitelistForTest();

   private:
    std::string* previous_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWhitelistForTest);
  };

  SimpleFeature();
  ~SimpleFeature() override;

  Availability IsAvailableToContext(const Extension* extension,
                                    Context context) const {
    return IsAvailableToContext(extension, context, GURL());
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    Platform platform) const {
    return IsAvailableToContext(extension, context, GURL(), platform);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform());
  }

  // extension::Feature:
  Availability IsAvailableToManifest(const std::string& extension_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version,
                                     Platform platform) const override;
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url,
                                    Platform platform) const override;
  bool IsInternal() const override;
  bool IsIdInBlacklist(const std::string& extension_id) const override;
  bool IsIdInWhitelist(const std::string& extension_id) const override;

  static bool IsIdInArray(const std::string& extension_id,
                          const char* const array[],
                          size_t array_length);

  // Similar to Manifest::Location, these are the classes of locations
  // supported in feature files. These should only be used in this class and in
  // generated files.
  enum Location {
    UNSPECIFIED_LOCATION,
    COMPONENT_LOCATION,
    EXTERNAL_COMPONENT_LOCATION,
    POLICY_LOCATION,
  };

  // Setters used by generated code to create the feature.
  // NOTE: These setters use base::StringPiece and std::initalizer_list rather
  // than std::string and std::vector for binary size reasons. Using STL types
  // directly in the header means that code that doesn't already have that exact
  // type ends up triggering many implicit conversions which are all inlined.
  void set_blacklist(std::initializer_list<const char* const> blacklist);
  void set_channel(version_info::Channel channel) {
    channel_.reset(new version_info::Channel(channel));
  }
  void set_command_line_switch(base::StringPiece command_line_switch);
  void set_component_extensions_auto_granted(bool granted) {
    component_extensions_auto_granted_ = granted;
  }
  void set_contexts(std::initializer_list<Context> contexts);
  void set_dependencies(std::initializer_list<const char* const> dependencies);
  void set_extension_types(std::initializer_list<Manifest::Type> types);
  void set_session_types(std::initializer_list<FeatureSessionType> types);
  void set_internal(bool is_internal) { is_internal_ = is_internal; }
  void set_location(Location location) { location_ = location; }
  // set_matches() is an exception to pass-by-value since we construct an
  // URLPatternSet from the vector of strings.
  // TODO(devlin): Pass in an URLPatternSet directly.
  void set_matches(std::initializer_list<const char* const> matches);
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }
  void set_noparent(bool no_parent) { no_parent_ = no_parent; }
  void set_platforms(std::initializer_list<Platform> platforms);
  void set_whitelist(std::initializer_list<const char* const> whitelist);

 protected:
  // Accessors used by subclasses in feature verification.
  const std::vector<std::string>& blacklist() const { return blacklist_; }
  const std::vector<std::string>& whitelist() const { return whitelist_; }
  const std::vector<Manifest::Type>& extension_types() const {
    return extension_types_;
  }
  const std::vector<Platform>& platforms() const { return platforms_; }
  const std::vector<Context>& contexts() const { return contexts_; }
  const std::vector<std::string>& dependencies() const { return dependencies_; }
  bool has_channel() const { return channel_.get() != nullptr; }
  version_info::Channel channel() const { return *channel_; }
  Location location() const { return location_; }
  int min_manifest_version() const { return min_manifest_version_; }
  int max_manifest_version() const { return max_manifest_version_; }
  const std::string& command_line_switch() const {
    return command_line_switch_;
  }
  bool component_extensions_auto_granted() const {
    return component_extensions_auto_granted_;
  }
  const URLPatternSet& matches() const { return matches_; }

  std::string GetAvailabilityMessage(AvailabilityResult result,
                                     Manifest::Type type,
                                     const GURL& url,
                                     Context context,
                                     version_info::Channel channel,
                                     FeatureSessionType session_type) const;

  // Handy utilities which construct the correct availability message.
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Context context) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  version_info::Channel channel) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  FeatureSessionType session_type) const;

 private:
  friend struct FeatureComparator;
  friend class SimpleFeatureTest;
  FRIEND_TEST_ALL_PREFIXES(BaseFeatureProviderTest, ManifestFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(BaseFeatureProviderTest, PermissionFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  FRIEND_TEST_ALL_PREFIXES(FeaturesGenerationTest, FeaturesTest);
  FRIEND_TEST_ALL_PREFIXES(ManifestUnitTest, Extension);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Blacklist);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, CommandLineSwitch);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ComplexFeatureAvailability);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Context);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, SessionType);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, FeatureValidation);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, HashedIdBlacklist);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, HashedIdWhitelist);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Inheritance);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Location);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ManifestVersion);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, PackageType);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParseContexts);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParseLocation);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParseManifestVersion);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParseNull);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParsePackageTypes);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParsePlatforms);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, ParseWhitelist);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Platform);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, SimpleFeatureAvailability);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Whitelist);

  // Holds String to Enum value mappings.
  struct Mappings;

  static bool IsIdInList(const std::string& extension_id,
                         const std::vector<std::string>& list);

  bool MatchesManifestLocation(Manifest::Location manifest_location) const;

  Availability CheckDependencies(
      const base::Callback<Availability(const Feature*)>& checker) const;

  static bool IsValidExtensionId(const std::string& extension_id);

  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::vector<std::string> blacklist_;
  std::vector<std::string> whitelist_;
  std::vector<std::string> dependencies_;
  std::vector<Manifest::Type> extension_types_;
  std::vector<FeatureSessionType> session_types_;
  std::vector<Context> contexts_;
  std::vector<Platform> platforms_;
  URLPatternSet matches_;
  Location location_;
  int min_manifest_version_;
  int max_manifest_version_;
  bool component_extensions_auto_granted_;
  bool is_internal_;
  std::string command_line_switch_;
  std::unique_ptr<version_info::Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
