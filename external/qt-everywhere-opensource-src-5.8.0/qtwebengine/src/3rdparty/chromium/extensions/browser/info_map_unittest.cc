// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace keys = extensions::manifest_keys;

namespace extensions {

class InfoMapTest : public testing::Test {
 public:
  InfoMapTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

// Returns a barebones test Extension object with the given name.
static scoped_refptr<Extension> CreateExtension(const std::string& name) {
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);

  base::DictionaryValue manifest;
  manifest.SetString(keys::kVersion, "1.0.0.0");
  manifest.SetString(keys::kName, name);

  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(path.AppendASCII(name),
                        Manifest::INVALID_LOCATION,
                        manifest,
                        Extension::NO_FLAGS,
                        &error);
  EXPECT_TRUE(extension.get()) << error;

  return extension;
}

// Test that the InfoMap handles refcounting properly.
TEST_F(InfoMapTest, RefCounting) {
  scoped_refptr<InfoMap> info_map(new InfoMap());

  // New extensions should have a single reference holding onto them.
  scoped_refptr<Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<Extension> extension2(CreateExtension("extension2"));
  scoped_refptr<Extension> extension3(CreateExtension("extension3"));
  EXPECT_TRUE(extension1->HasOneRef());
  EXPECT_TRUE(extension2->HasOneRef());
  EXPECT_TRUE(extension3->HasOneRef());

  // Add a ref to each extension and give it to the info map.
  info_map->AddExtension(extension1.get(), base::Time(), false, false);
  info_map->AddExtension(extension2.get(), base::Time(), false, false);
  info_map->AddExtension(extension3.get(), base::Time(), false, false);

  // Release extension1, and the info map should have the only ref.
  const Extension* weak_extension1 = extension1.get();
  extension1 = NULL;
  EXPECT_TRUE(weak_extension1->HasOneRef());

  // Remove extension2, and the extension2 object should have the only ref.
  info_map->RemoveExtension(
      extension2->id(), extensions::UnloadedExtensionInfo::REASON_UNINSTALL);
  EXPECT_TRUE(extension2->HasOneRef());

  // Delete the info map, and the extension3 object should have the only ref.
  info_map = NULL;
  EXPECT_TRUE(extension3->HasOneRef());
}

// Tests that we can query a few extension properties from the InfoMap.
TEST_F(InfoMapTest, Properties) {
  scoped_refptr<InfoMap> info_map(new InfoMap());

  scoped_refptr<Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<Extension> extension2(CreateExtension("extension2"));

  info_map->AddExtension(extension1.get(), base::Time(), false, false);
  info_map->AddExtension(extension2.get(), base::Time(), false, false);

  EXPECT_EQ(2u, info_map->extensions().size());
  EXPECT_EQ(extension1.get(), info_map->extensions().GetByID(extension1->id()));
  EXPECT_EQ(extension2.get(), info_map->extensions().GetByID(extension2->id()));
}

// Tests that extension URLs are properly mapped to local file paths.
TEST_F(InfoMapTest, MapUrlToLocalFilePath) {
  scoped_refptr<InfoMap> info_map(new InfoMap());
  scoped_refptr<Extension> app(CreateExtension("platform_app"));
  info_map->AddExtension(app.get(), base::Time(), false, false);

  // Non-extension URLs don't map to anything.
  base::FilePath non_extension_path;
  GURL non_extension_url("http://not-an-extension.com/");
  EXPECT_FALSE(info_map->MapUrlToLocalFilePath(
      non_extension_url, false, &non_extension_path));
  EXPECT_TRUE(non_extension_path.empty());

  // Valid resources return a valid path.
  base::FilePath valid_path;
  GURL valid_url = app->GetResourceURL("manifest.json");
  EXPECT_TRUE(info_map->MapUrlToLocalFilePath(
      valid_url, true /* use_blocking_api */, &valid_path));
  EXPECT_FALSE(valid_path.empty());

  // A file must exist to be mapped to a path using the blocking API.
  base::FilePath does_not_exist_path;
  GURL does_not_exist_url = app->GetResourceURL("does-not-exist.html");
  EXPECT_FALSE(info_map->MapUrlToLocalFilePath(
      does_not_exist_url, true /* use_blocking_api */, &does_not_exist_path));
  EXPECT_TRUE(does_not_exist_path.empty());

  // A file does not need to exist to be mapped to a path with the non-blocking
  // API. This avoids hitting the disk to see if it exists.
  EXPECT_TRUE(info_map->MapUrlToLocalFilePath(
      does_not_exist_url, false /* use_blocking_api */, &does_not_exist_path));
  EXPECT_FALSE(does_not_exist_path.empty());
}

}  // namespace extensions
