// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_API_H_
#define EXTENSIONS_COMMON_EXTENSION_API_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/url_pattern_set.h"

namespace base {
class DictionaryValue;
class Value;
}

class GURL;

namespace extensions {

class Extension;
class Feature;

// C++ Wrapper for the JSON API definitions in chrome/common/extensions/api/.
//
// WARNING: This class is accessed on multiple threads in the browser process
// (see ExtensionFunctionDispatcher). No state should be modified after
// construction.
class ExtensionAPI {
 public:
  // Returns a single shared instance of this class. This is the typical use
  // case in Chrome.
  //
  // TODO(aa): Make this const to enforce thread-safe usage.
  static ExtensionAPI* GetSharedInstance();

  // Creates a new instance configured the way ExtensionAPI typically is in
  // Chrome. Use the default constructor to get a clean instance.
  static ExtensionAPI* CreateWithDefaultConfiguration();

  // Splits a name like "permission:bookmark" into ("permission", "bookmark").
  // The first part refers to a type of feature, for example "manifest",
  // "permission", or "api". The second part is the full name of the feature.
  //
  // TODO(kalman): ExtensionAPI isn't really the right place for this function.
  static void SplitDependencyName(const std::string& full_name,
                                  std::string* feature_type,
                                  std::string* feature_name);

  class OverrideSharedInstanceForTest {
   public:
    explicit OverrideSharedInstanceForTest(ExtensionAPI* testing_api);
    ~OverrideSharedInstanceForTest();

   private:
    ExtensionAPI* original_api_;
  };

  // Creates a completely clean instance. Configure using RegisterSchema() and
  // RegisterDependencyProvider before use.
  ExtensionAPI();
  virtual ~ExtensionAPI();

  // Add a (non-generated) API schema resource.
  void RegisterSchemaResource(const std::string& api_name, int resource_id);

  // Add a FeatureProvider for APIs. The features are used to specify
  // dependencies and constraints on the availability of APIs.
  void RegisterDependencyProvider(const std::string& name,
                                  const FeatureProvider* provider);

  // Returns true if the API item called |api_full_name| and all of its
  // dependencies are available in |context|.
  //
  // |api_full_name| can be either a namespace name (like "bookmarks") or a
  // member name (like "bookmarks.create").
  //
  // Depending on the configuration of |api| (in _api_features.json), either
  // |extension| or |url| (or both) may determine its availability, but this is
  // up to the configuration of the individual feature.
  //
  // TODO(kalman): This is just an unnecessary combination of finding a Feature
  // then calling Feature::IsAvailableToContext(..) on it. Just provide that
  // FindFeature function and let callers compose if they want.
  Feature::Availability IsAvailable(const std::string& api_full_name,
                                    const Extension* extension,
                                    Feature::Context context,
                                    const GURL& url);

  // Determines whether an API, or any parts of that API, are available in
  // |context|.
  bool IsAnyFeatureAvailableToContext(const Feature& api,
                                      const Extension* extension,
                                      Feature::Context context,
                                      const GURL& url);

  // Returns true if |name| is available to WebUI contexts on |url|.
  bool IsAvailableToWebUI(const std::string& name, const GURL& url);

  // Gets the schema for the extension API with namespace |full_name|.
  // Ownership remains with this object.
  const base::DictionaryValue* GetSchema(const std::string& full_name);

  // Splits a full name from the extension API into its API and child name
  // parts. Some examples:
  //
  // "bookmarks.create" -> ("bookmarks", "create")
  // "experimental.input.ui.cursorUp" -> ("experimental.input.ui", "cursorUp")
  // "storage.sync.set" -> ("storage", "sync.get")
  // "<unknown-api>.monkey" -> ("", "")
  //
  // The |child_name| parameter can be be NULL if you don't need that part.
  std::string GetAPINameFromFullName(const std::string& full_name,
                                     std::string* child_name);

  // Gets a feature from any dependency provider registered with ExtensionAPI.
  // Returns NULL if the feature could not be found.
  Feature* GetFeatureDependency(const std::string& dependency_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, TypesHaveNamespace);
  friend struct base::DefaultSingletonTraits<ExtensionAPI>;

  void InitDefaultConfiguration();

  bool default_configuration_initialized_;

  // Loads a schema.
  void LoadSchema(const std::string& name, const base::StringPiece& schema);

  // Map from each API that hasn't been loaded yet to the schema which defines
  // it. Note that there may be multiple APIs per schema.
  typedef std::map<std::string, int> UnloadedSchemaMap;
  UnloadedSchemaMap unloaded_schemas_;

  // Schemas for each namespace.
  typedef std::map<std::string, linked_ptr<const base::DictionaryValue> >
        SchemaMap;
  SchemaMap schemas_;

  // FeatureProviders used for resolving dependencies.
  typedef std::map<std::string, const FeatureProvider*> FeatureProviderMap;
  FeatureProviderMap dependency_providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAPI);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_API_H_
