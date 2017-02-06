// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature_filter.h"
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

  // Dependency resolution is a property of Features that is preferrably
  // handled internally to avoid temptation, but FeatureFilters may need
  // to know if there are any at all.
  bool HasDependencies() const;

  // Adds a filter to this feature. The feature takes ownership of the filter.
  void AddFilter(std::unique_ptr<SimpleFeatureFilter> filter);

  // Parses the JSON representation of a feature into the fields of this object.
  // Unspecified values in the JSON are not modified in the object. This allows
  // us to implement inheritance by parsing one value after another. Returns
  // the error found, or an empty string on success.
  virtual std::string Parse(const base::DictionaryValue* dictionary);

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

  std::string GetAvailabilityMessage(AvailabilityResult result,
                                     Manifest::Type type,
                                     const GURL& url,
                                     Context context) const override;

  bool IsInternal() const override;

  bool IsIdInBlacklist(const std::string& extension_id) const override;
  bool IsIdInWhitelist(const std::string& extension_id) const override;

  static bool IsIdInArray(const std::string& extension_id,
                          const char* const array[],
                          size_t array_length);

 protected:
  // Similar to Manifest::Location, these are the classes of locations
  // supported in feature files. Production code should never directly access
  // these.
  enum Location {
    UNSPECIFIED_LOCATION,
    COMPONENT_LOCATION,
    EXTERNAL_COMPONENT_LOCATION,
    POLICY_LOCATION,
  };

  // Accessors defined for testing.
  std::vector<std::string>* blacklist() { return &blacklist_; }
  const std::vector<std::string>* blacklist() const { return &blacklist_; }
  std::vector<std::string>* whitelist() { return &whitelist_; }
  const std::vector<std::string>* whitelist() const { return &whitelist_; }
  std::vector<Manifest::Type>* extension_types() { return &extension_types_; }
  const std::vector<Manifest::Type>* extension_types() const {
    return &extension_types_;
  }
  std::vector<Context>* contexts() { return &contexts_; }
  const std::vector<Context>* contexts() const { return &contexts_; }
  std::vector<Platform>* platforms() { return &platforms_; }
  Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }
  int min_manifest_version() const { return min_manifest_version_; }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }
  int max_manifest_version() const { return max_manifest_version_; }
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }
  const std::string& command_line_switch() const {
    return command_line_switch_;
  }
  void set_command_line_switch(const std::string& command_line_switch) {
    command_line_switch_ = command_line_switch;
  }

  // Handy utilities which construct the correct availability message.
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Context context) const;

 private:
  friend class SimpleFeatureTest;
  FRIEND_TEST_ALL_PREFIXES(BaseFeatureProviderTest, ManifestFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(BaseFeatureProviderTest, PermissionFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  FRIEND_TEST_ALL_PREFIXES(ManifestUnitTest, Extension);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Blacklist);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, CommandLineSwitch);
  FRIEND_TEST_ALL_PREFIXES(SimpleFeatureTest, Context);
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
  std::vector<Context> contexts_;
  std::vector<Platform> platforms_;
  URLPatternSet matches_;
  Location location_;
  int min_manifest_version_;
  int max_manifest_version_;
  bool component_extensions_auto_granted_;
  std::string command_line_switch_;

  std::vector<std::unique_ptr<SimpleFeatureFilter>> filters_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
