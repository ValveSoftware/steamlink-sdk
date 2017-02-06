// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using test_util::BuildExtension;

SimpleFeature* CreateAPIFeature() {
  return new APIFeature();
}

TEST(ExtensionAPITest, Creation) {
  ExtensionAPI* shared_instance = ExtensionAPI::GetSharedInstance();
  EXPECT_EQ(shared_instance, ExtensionAPI::GetSharedInstance());

  std::unique_ptr<ExtensionAPI> new_instance(
      ExtensionAPI::CreateWithDefaultConfiguration());
  EXPECT_NE(new_instance.get(),
            std::unique_ptr<ExtensionAPI>(
                ExtensionAPI::CreateWithDefaultConfiguration())
                .get());

  ExtensionAPI empty_instance;

  struct {
    ExtensionAPI* api;
    bool expect_populated;
  } test_data[] = {
    { shared_instance, true },
    { new_instance.get(), true },
    { &empty_instance, false }
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    EXPECT_EQ(test_data[i].expect_populated,
              test_data[i].api->GetSchema("bookmarks.create") != NULL);
  }
}

TEST(ExtensionAPITest, SplitDependencyName) {
  struct {
    std::string input;
    std::string expected_feature_type;
    std::string expected_feature_name;
  } test_data[] = {{"", "api", ""},  // assumes "api" when no type is present
                   {"foo", "api", "foo"},
                   {"foo:", "foo", ""},
                   {":foo", "", "foo"},
                   {"foo:bar", "foo", "bar"},
                   {"foo:bar.baz", "foo", "bar.baz"}};

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    std::string feature_type;
    std::string feature_name;
    ExtensionAPI::SplitDependencyName(
        test_data[i].input, &feature_type, &feature_name);
    EXPECT_EQ(test_data[i].expected_feature_type, feature_type) << i;
    EXPECT_EQ(test_data[i].expected_feature_name, feature_name) << i;
  }
}

TEST(ExtensionAPITest, APIFeatures) {
  struct {
    std::string api_full_name;
    bool expect_is_available;
    Feature::Context context;
    GURL url;
  } test_data[] = {
    { "test1", false, Feature::WEB_PAGE_CONTEXT, GURL() },
    { "test1", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test2", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test2", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", false, Feature::WEB_PAGE_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test3", false, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo", false, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test5", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test5.blah", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5.blah", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test6", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test6.foo", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test7", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.foo", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },

    // Test parent/child.
    { "parent1", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1", false, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "parent1.child1", false, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1.child1", true, Feature::WEB_PAGE_CONTEXT,
        GURL("http://foo.com") },
    { "parent1.child2", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1.child2", false, Feature::WEB_PAGE_CONTEXT,
        GURL("http://foo.com") },
    { "parent2", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent2", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3", false, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent2.child3", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3", false, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3.child.child", true, Feature::CONTENT_SCRIPT_CONTEXT,
        GURL() },
    { "parent2.child3.child.child", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent2.child3.child.child", true, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent3", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3", false, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent.child", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3.noparent.child", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent3.noparent.child", true, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL() }
  };

  base::FilePath api_features_path;
  PathService::Get(chrome::DIR_TEST_DATA, &api_features_path);
  api_features_path = api_features_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("api_features.json");

  std::string api_features_str;
  ASSERT_TRUE(base::ReadFileToString(
      api_features_path, &api_features_str)) << "api_features.json";

  std::unique_ptr<base::DictionaryValue> value(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::Read(api_features_str).release()));
  BaseFeatureProvider api_feature_provider(*value, CreateAPIFeature);

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    ExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (base::DictionaryValue::Iterator iter(*value); !iter.IsAtEnd();
         iter.Advance()) {
      if (iter.key().find(".") == std::string::npos)
        api.RegisterSchemaResource(iter.key(), 0);
    }

    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);
    bool expected = test_data[i].expect_is_available;
    Feature::Availability availability =
        api.IsAvailable(test_data[i].api_full_name,
                        NULL,
                        test_data[i].context,
                        test_data[i].url);
    EXPECT_EQ(expected, availability.is_available())
        << base::StringPrintf("Test %d: Feature '%s' was %s: %s",
                              static_cast<int>(i),
                              test_data[i].api_full_name.c_str(),
                              expected ? "not available" : "available",
                              availability.message().c_str());
  }
}

TEST(ExtensionAPITest, IsAnyFeatureAvailableToContext) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "app")
                  .Set("app",
                       DictionaryBuilder()
                           .Set("background",
                                DictionaryBuilder()
                                    .Set("scripts", ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Build())
          .Build();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  struct {
    std::string api_full_name;
    bool expect_is_available;
    Feature::Context context;
    const Extension* extension;
    GURL url;
  } test_data[] = {
    { "test1", false, Feature::WEB_PAGE_CONTEXT, NULL, GURL() },
    { "test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, NULL, GURL() },
    { "test1", false, Feature::UNBLESSED_EXTENSION_CONTEXT, app.get(), GURL() },
    { "test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, extension.get(),
        GURL() },
    { "test2", true, Feature::CONTENT_SCRIPT_CONTEXT, NULL, GURL() },
    { "test2", true, Feature::WEB_PAGE_CONTEXT, NULL,
        GURL("http://google.com") },
    { "test2.foo", false, Feature::WEB_PAGE_CONTEXT, NULL,
        GURL("http://google.com") },
    { "test3", true, Feature::CONTENT_SCRIPT_CONTEXT, NULL, GURL() },
    { "test3", true, Feature::WEB_PAGE_CONTEXT, NULL, GURL("http://foo.com") },
    { "test4.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, NULL, GURL() },
    { "test7", false, Feature::WEB_PAGE_CONTEXT, NULL,
        GURL("http://google.com") },
    { "test7", true, Feature::WEB_PAGE_CONTEXT, NULL, GURL("http://foo.com") },
    { "test7", false, Feature::WEB_PAGE_CONTEXT, NULL, GURL("http://bar.com") }
  };

  base::FilePath api_features_path;
  PathService::Get(chrome::DIR_TEST_DATA, &api_features_path);
  api_features_path = api_features_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("api_features.json");

  std::string api_features_str;
  ASSERT_TRUE(base::ReadFileToString(
      api_features_path, &api_features_str)) << "api_features.json";

  std::unique_ptr<base::DictionaryValue> value(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::Read(api_features_str).release()));
  BaseFeatureProvider api_feature_provider(*value, CreateAPIFeature);

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    ExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (base::DictionaryValue::Iterator iter(*value); !iter.IsAtEnd();
         iter.Advance()) {
      if (iter.key().find(".") == std::string::npos)
        api.RegisterSchemaResource(iter.key(), 0);
    }

    Feature* test_feature =
        api_feature_provider.GetFeature(test_data[i].api_full_name);
    ASSERT_TRUE(test_feature);
    EXPECT_EQ(test_data[i].expect_is_available,
              api.IsAnyFeatureAvailableToContext(*test_feature,
                                                 test_data[i].extension,
                                                 test_data[i].context,
                                                 test_data[i].url))
        << i;
  }
}

TEST(ExtensionAPITest, LazyGetSchema) {
  std::unique_ptr<ExtensionAPI> apis(
      ExtensionAPI::CreateWithDefaultConfiguration());

  EXPECT_EQ(NULL, apis->GetSchema(std::string()));
  EXPECT_EQ(NULL, apis->GetSchema(std::string()));
  EXPECT_EQ(NULL, apis->GetSchema("experimental"));
  EXPECT_EQ(NULL, apis->GetSchema("experimental"));
  EXPECT_EQ(NULL, apis->GetSchema("foo"));
  EXPECT_EQ(NULL, apis->GetSchema("foo"));

  EXPECT_TRUE(apis->GetSchema("dns"));
  EXPECT_TRUE(apis->GetSchema("dns"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("storage"));
  EXPECT_TRUE(apis->GetSchema("storage"));
}

scoped_refptr<Extension> CreateExtensionWithPermissions(
    const std::set<std::string>& permissions) {
  base::DictionaryValue manifest;
  manifest.SetString("name", "extension");
  manifest.SetString("version", "1.0");
  manifest.SetInteger("manifest_version", 2);
  {
    std::unique_ptr<base::ListValue> permissions_list(new base::ListValue());
    for (std::set<std::string>::const_iterator i = permissions.begin();
        i != permissions.end(); ++i) {
      permissions_list->AppendString(*i);
    }
    manifest.Set("permissions", permissions_list.release());
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::UNPACKED,
      manifest, Extension::NO_FLAGS, &error));
  CHECK(extension.get());
  CHECK(error.empty());

  return extension;
}

scoped_refptr<Extension> CreateExtensionWithPermission(
    const std::string& permission) {
  std::set<std::string> permissions;
  permissions.insert(permission);
  return CreateExtensionWithPermissions(permissions);
}

TEST(ExtensionAPITest, ExtensionWithUnprivilegedAPIs) {
  scoped_refptr<Extension> extension;
  {
    std::set<std::string> permissions;
    permissions.insert("storage");
    permissions.insert("history");
    extension = CreateExtensionWithPermissions(permissions);
  }

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  const FeatureProvider& api_features = *FeatureProvider::GetAPIFeatures();

  // "storage" is completely unprivileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"),
      NULL,
      Feature::BLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"),
      NULL,
      Feature::UNBLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"),
      NULL,
      Feature::CONTENT_SCRIPT_CONTEXT,
      GURL()));

  // "extension" is partially unprivileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"),
      NULL,
      Feature::BLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"),
      NULL,
      Feature::UNBLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"),
      NULL,
      Feature::CONTENT_SCRIPT_CONTEXT,
      GURL()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension.getURL"),
      NULL,
      Feature::CONTENT_SCRIPT_CONTEXT,
      GURL()));

  // "history" is entirely privileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"),
      NULL,
      Feature::BLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"),
      NULL,
      Feature::UNBLESSED_EXTENSION_CONTEXT,
      GURL()));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"),
      NULL,
      Feature::CONTENT_SCRIPT_CONTEXT,
      GURL()));
}

scoped_refptr<Extension> CreateHostedApp() {
  base::DictionaryValue values;
  values.SetString(manifest_keys::kName, "test");
  values.SetString(manifest_keys::kVersion, "0.1");
  values.Set(manifest_keys::kWebURLs, new base::ListValue());
  values.SetString(manifest_keys::kLaunchWebURL,
                   "http://www.example.com");
  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, values, Extension::NO_FLAGS,
      &error));
  CHECK(extension.get());
  return extension;
}

scoped_refptr<Extension> CreatePackagedAppWithPermissions(
    const std::set<std::string>& permissions) {
  base::DictionaryValue values;
  values.SetString(manifest_keys::kName, "test");
  values.SetString(manifest_keys::kVersion, "0.1");
  values.SetString(manifest_keys::kPlatformAppBackground,
      "http://www.example.com");

  base::DictionaryValue* app = new base::DictionaryValue();
  base::DictionaryValue* background = new base::DictionaryValue();
  base::ListValue* scripts = new base::ListValue();
  scripts->AppendString("test.js");
  background->Set("scripts", scripts);
  app->Set("background", background);
  values.Set(manifest_keys::kApp, app);
  {
    std::unique_ptr<base::ListValue> permissions_list(new base::ListValue());
    for (std::set<std::string>::const_iterator i = permissions.begin();
        i != permissions.end(); ++i) {
      permissions_list->AppendString(*i);
    }
    values.Set("permissions", permissions_list.release());
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, values, Extension::NO_FLAGS,
      &error));
  CHECK(extension.get()) << error;
  return extension;
}

TEST(ExtensionAPITest, HostedAppPermissions) {
  scoped_refptr<Extension> extension = CreateHostedApp();

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // "runtime" and "tabs" should not be available in hosted apps.
  EXPECT_FALSE(extension_api->IsAvailable("runtime",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("runtime.id",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("runtime.sendMessage",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("runtime.sendNativeMessage",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("tabs.create",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
}

TEST(ExtensionAPITest, AppAndFriendsAvailability) {
  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // Make sure chrome.app.runtime and chrome.app.window are available to apps,
  // and chrome.app is not.
  {
    std::set<std::string> permissions;
    permissions.insert("app.runtime");
    permissions.insert("app.window");
    scoped_refptr<Extension> extension =
        CreatePackagedAppWithPermissions(permissions);
    EXPECT_FALSE(extension_api->IsAvailable(
        "app",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
    EXPECT_TRUE(extension_api->IsAvailable(
        "app.runtime",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
    EXPECT_TRUE(extension_api->IsAvailable(
        "app.window",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
  }
  // Make sure chrome.app.runtime and chrome.app.window are not available to
  // extensions, and chrome.app is.
  {
    std::set<std::string> permissions;
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermissions(permissions);
    EXPECT_TRUE(extension_api->IsAvailable(
        "app",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
    EXPECT_FALSE(extension_api->IsAvailable(
        "app.runtime",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
    EXPECT_FALSE(extension_api->IsAvailable(
        "app.window",
        extension.get(),
        Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://foo.com")).is_available());
  }
}

TEST(ExtensionAPITest, ExtensionWithDependencies) {
  // Extension with the "ttsEngine" permission but not the "tts" permission; it
  // should not automatically get "tts" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("ttsEngine");
    std::unique_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_TRUE(api->IsAvailable("ttsEngine",
                                 extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT,
                                 GURL()).is_available());
    EXPECT_FALSE(api->IsAvailable("tts",
                                  extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL()).is_available());
  }

  // Conversely, extension with the "tts" permission but not the "ttsEngine"
  // permission shouldn't get the "ttsEngine" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("tts");
    std::unique_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_FALSE(api->IsAvailable("ttsEngine",
                                  extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL()).is_available());
    EXPECT_TRUE(api->IsAvailable("tts",
                                 extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT,
                                 GURL()).is_available());
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  return api->IsAvailable(
      api_name, NULL, Feature::WEB_PAGE_CONTEXT, GURL(url)).is_available();
}

TEST(ExtensionAPITest, URLMatching) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // "app" API is available to all URLs that content scripts can be injected.
  EXPECT_TRUE(MatchesURL(api.get(), "app", "http://example.com/example.html"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "https://blah.net"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "file://somefile.html"));

  // Also to internal URLs.
  EXPECT_TRUE(MatchesURL(api.get(), "app", "about:flags"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "chrome://flags"));

  // "app" should be available to chrome-extension URLs.
  EXPECT_TRUE(MatchesURL(api.get(), "app",
                          "chrome-extension://fakeextension"));

  // "storage" API (for example) isn't available to any URLs.
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "http://example.com/example.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "https://blah.net"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "file://somefile.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "about:flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "chrome-extension://fakeextension"));
}

TEST(ExtensionAPITest, GetAPINameFromFullName) {
  struct {
    std::string input;
    std::string api_name;
    std::string child_name;
  } test_data[] = {
    { "", "", "" },
    { "unknown", "", "" },
    { "bookmarks", "bookmarks", "" },
    { "bookmarks.", "bookmarks", "" },
    { ".bookmarks", "", "" },
    { "bookmarks.create", "bookmarks", "create" },
    { "bookmarks.create.", "bookmarks", "create." },
    { "bookmarks.create.monkey", "bookmarks", "create.monkey" },
    { "bookmarkManagerPrivate", "bookmarkManagerPrivate", "" },
    { "bookmarkManagerPrivate.copy", "bookmarkManagerPrivate", "copy" }
  };

  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());
  for (size_t i = 0; i < arraysize(test_data); ++i) {
    std::string child_name;
    std::string api_name = api->GetAPINameFromFullName(test_data[i].input,
                                                       &child_name);
    EXPECT_EQ(test_data[i].api_name, api_name) << test_data[i].input;
    EXPECT_EQ(test_data[i].child_name, child_name) << test_data[i].input;
  }
}

TEST(ExtensionAPITest, DefaultConfigurationFeatures) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  SimpleFeature* bookmarks = static_cast<SimpleFeature*>(
      api->GetFeatureDependency("api:bookmarks"));
  SimpleFeature* bookmarks_create = static_cast<SimpleFeature*>(
      api->GetFeatureDependency("api:bookmarks.create"));

  struct {
    SimpleFeature* feature;
    // TODO(aa): More stuff to test over time.
  } test_data[] = {
    { bookmarks },
    { bookmarks_create }
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SimpleFeature* feature = test_data[i].feature;
    ASSERT_TRUE(feature) << i;

    EXPECT_TRUE(feature->whitelist()->empty());
    EXPECT_TRUE(feature->extension_types()->empty());

    EXPECT_EQ(SimpleFeature::UNSPECIFIED_LOCATION, feature->location());
    EXPECT_TRUE(feature->platforms()->empty());
    EXPECT_EQ(0, feature->min_manifest_version());
    EXPECT_EQ(0, feature->max_manifest_version());
  }
}

TEST(ExtensionAPITest, FeaturesRequireContexts) {
  // TODO(cduvall): Make this check API featues.
  std::unique_ptr<base::DictionaryValue> api_features1(
      new base::DictionaryValue());
  std::unique_ptr<base::DictionaryValue> api_features2(
      new base::DictionaryValue());
  base::DictionaryValue* test1 = new base::DictionaryValue();
  base::DictionaryValue* test2 = new base::DictionaryValue();
  base::ListValue* contexts = new base::ListValue();
  contexts->AppendString("content_script");
  test1->Set("contexts", contexts);
  test1->SetString("channel", "stable");
  test2->SetString("channel", "stable");
  api_features1->Set("test", test1);
  api_features2->Set("test", test2);

  struct {
    base::DictionaryValue* api_features;
    bool expect_success;
  } test_data[] = {
    { api_features1.get(), true },
    { api_features2.get(), false }
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    BaseFeatureProvider api_feature_provider(*test_data[i].api_features,
                                             CreateAPIFeature);
    Feature* feature = api_feature_provider.GetFeature("test");
    EXPECT_EQ(test_data[i].expect_success, feature != NULL) << i;
  }
}

static void GetDictionaryFromList(const base::DictionaryValue* schema,
                                  const std::string& list_name,
                                  const int list_index,
                                  const base::DictionaryValue** out) {
  const base::ListValue* list;
  EXPECT_TRUE(schema->GetList(list_name, &list));
  EXPECT_TRUE(list->GetDictionary(list_index, out));
}

TEST(ExtensionAPITest, TypesHaveNamespace) {
  base::FilePath manifest_path;
  PathService::Get(chrome::DIR_TEST_DATA, &manifest_path);
  manifest_path = manifest_path.AppendASCII("extensions")
      .AppendASCII("extension_api_unittest")
      .AppendASCII("types_have_namespace.json");

  std::string manifest_str;
  ASSERT_TRUE(base::ReadFileToString(manifest_path, &manifest_str))
      << "Failed to load: " << manifest_path.value();

  ExtensionAPI api;
  api.RegisterSchemaResource("test.foo", 0);
  api.LoadSchema("test.foo", manifest_str);

  const base::DictionaryValue* schema = api.GetSchema("test.foo");

  const base::DictionaryValue* dict;
  const base::DictionaryValue* sub_dict;
  std::string type;

  GetDictionaryFromList(schema, "types", 0, &dict);
  EXPECT_TRUE(dict->GetString("id", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetString("customBindings", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetDictionary("properties", &sub_dict));
  const base::DictionaryValue* property;
  EXPECT_TRUE(sub_dict->GetDictionary("foo", &property));
  EXPECT_TRUE(property->GetString("$ref", &type));
  EXPECT_EQ("test.foo.OtherType", type);
  EXPECT_TRUE(sub_dict->GetDictionary("bar", &property));
  EXPECT_TRUE(property->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);

  GetDictionaryFromList(schema, "functions", 0, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);
  EXPECT_TRUE(dict->GetDictionary("returns", &sub_dict));
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);

  GetDictionaryFromList(schema, "functions", 1, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);
  EXPECT_TRUE(dict->GetDictionary("returns", &sub_dict));
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);

  GetDictionaryFromList(schema, "events", 0, &dict);
  GetDictionaryFromList(dict, "parameters", 0, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("test.foo.TestType", type);
  GetDictionaryFromList(dict, "parameters", 1, &sub_dict);
  EXPECT_TRUE(sub_dict->GetString("$ref", &type));
  EXPECT_EQ("fully.qualified.Type", type);
}

// Tests API availability with an empty manifest.
TEST(ExtensionAPITest, NoPermissions) {
  const struct {
    const char* permission_name;
    bool expect_success;
  } kTests[] = {
    // Test default module/package permission.
    { "extension",      true },
    { "i18n",           true },
    { "permissions",    true },
    { "runtime",        true },
    { "test",           true },
    // These require manifest keys.
    { "browserAction",  false },
    { "pageAction",     false },
    { "pageActions",    false },
    // Some negative tests.
    { "bookmarks",      false },
    { "cookies",        false },
    { "history",        false },
    // Make sure we find the module name after stripping '.'
    { "runtime.abcd.onStartup",  true },
    // Test Tabs/Windows functions.
    { "tabs.create",      true },
    { "tabs.duplicate",   true },
    { "tabs.onRemoved",   true },
    { "tabs.remove",      true },
    { "tabs.update",      true },
    { "tabs.getSelected", true },
    { "tabs.onUpdated",   true },
    { "windows.get",      true },
    { "windows.create",   true },
    { "windows.remove",   true },
    { "windows.update",   true },
    // Test some whitelisted functions. These require no permissions.
    { "app.getDetails",           true },
    { "app.getIsInstalled",       true },
    { "app.installState",         true },
    { "app.runningState",         true },
    { "management.getPermissionWarningsByManifest", true },
    { "management.uninstallSelf", true },
    // But other functions in those modules do.
    { "management.getPermissionWarningsById", false },
    { "runtime.connectNative", false },
  };

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());
  scoped_refptr<Extension> extension =
      BuildExtension(ExtensionBuilder()).Build();

  for (size_t i = 0; i < arraysize(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              extension_api->IsAvailable(kTests[i].permission_name,
                                         extension.get(),
                                         Feature::BLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available())
        << "Permission being tested: " << kTests[i].permission_name;
  }
}

// Tests that permissions that require manifest keys are available when those
// keys are present.
TEST(ExtensionAPITest, ManifestKeys) {
  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  scoped_refptr<Extension> extension =
      BuildExtension(ExtensionBuilder())
          .MergeManifest(DictionaryBuilder()
                             .Set("browser_action", DictionaryBuilder().Build())
                             .Build())
          .Build();

  EXPECT_TRUE(extension_api->IsAvailable("browserAction",
                                         extension.get(),
                                         Feature::BLESSED_EXTENSION_CONTEXT,
                                         GURL()).is_available());
  EXPECT_FALSE(extension_api->IsAvailable("pageAction",
                                          extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT,
                                          GURL()).is_available());
}

}  // namespace extensions
