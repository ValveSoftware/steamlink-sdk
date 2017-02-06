// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/mime_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace app_file_handler_util {
namespace {

const char kOrigin[] = "chrome-extension://cmalghjoncmjoeakimpfhojhpgemgaje";
const char kJPEGExtensionFilePath[] = "/fake/path/foo.jpg";
const char kJPEGExtensionUpperCaseFilePath[] = "/fake/path/FOO.JPG";

// Saves the returned mime type to a variable.
void OnMimeTypeResult(std::string* output, const std::string& mime_type) {
  *output = mime_type;
}

// Saves returned mime types to a vector.
void OnMimeTypesCollected(
    std::vector<std::string>* output,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  *output = *mime_types;
}

// Creates a native local file system URL for a local path.
storage::FileSystemURL CreateNativeLocalFileSystemURL(
    storage::FileSystemContext* context,
    const base::FilePath local_path) {
  return context->CreateCrackedFileSystemURL(
      GURL(kOrigin), storage::kFileSystemTypeNativeLocal, local_path);
}

}  // namespace

class FileHandlersMimeUtilTest : public testing::Test {
 protected:
  FileHandlersMimeUtilTest() {}
  ~FileHandlersMimeUtilTest() override {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ =
        content::CreateFileSystemContextForTesting(NULL, data_dir_.path());

    EXPECT_TRUE(base::CreateTemporaryFile(&html_mime_file_path_));
    const std::string kSampleContent = "<html><body></body></html>";
    EXPECT_TRUE(base::WriteFile(
        html_mime_file_path_, kSampleContent.c_str(), kSampleContent.size()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  base::ScopedTempDir data_dir_;
  base::FilePath html_mime_file_path_;
};

TEST_F(FileHandlersMimeUtilTest, GetMimeTypeForLocalPath) {
  {
    std::string result;
    GetMimeTypeForLocalPath(
        &profile_,
        base::FilePath::FromUTF8Unsafe(kJPEGExtensionFilePath),
        base::Bind(&OnMimeTypeResult, &result));
    content::RunAllBlockingPoolTasksUntilIdle();
    EXPECT_EQ("image/jpeg", result);
  }

  {
    std::string result;
    GetMimeTypeForLocalPath(
        &profile_,
        base::FilePath::FromUTF8Unsafe(kJPEGExtensionUpperCaseFilePath),
        base::Bind(&OnMimeTypeResult, &result));
    content::RunAllBlockingPoolTasksUntilIdle();
    EXPECT_EQ("image/jpeg", result);
  }

  {
    std::string result;
    GetMimeTypeForLocalPath(&profile_,
                            html_mime_file_path_,
                            base::Bind(&OnMimeTypeResult, &result));
    content::RunAllBlockingPoolTasksUntilIdle();
    EXPECT_EQ("text/html", result);
  }
}

TEST_F(FileHandlersMimeUtilTest, MimeTypeCollector_ForURLs) {
  MimeTypeCollector collector(&profile_);

  std::vector<storage::FileSystemURL> urls;
  urls.push_back(CreateNativeLocalFileSystemURL(
      file_system_context_.get(),
      base::FilePath::FromUTF8Unsafe(kJPEGExtensionFilePath)));
  urls.push_back(CreateNativeLocalFileSystemURL(
      file_system_context_.get(),
      base::FilePath::FromUTF8Unsafe(kJPEGExtensionUpperCaseFilePath)));
  urls.push_back(CreateNativeLocalFileSystemURL(file_system_context_.get(),
                                                html_mime_file_path_));

  std::vector<std::string> result;
  collector.CollectForURLs(urls, base::Bind(&OnMimeTypesCollected, &result));
  content::RunAllBlockingPoolTasksUntilIdle();

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("image/jpeg", result[0]);
  EXPECT_EQ("image/jpeg", result[1]);
  EXPECT_EQ("text/html", result[2]);
}

TEST_F(FileHandlersMimeUtilTest, MimeTypeCollector_ForLocalPaths) {
  MimeTypeCollector collector(&profile_);

  std::vector<base::FilePath> local_paths;
  local_paths.push_back(base::FilePath::FromUTF8Unsafe(kJPEGExtensionFilePath));
  local_paths.push_back(
      base::FilePath::FromUTF8Unsafe(kJPEGExtensionUpperCaseFilePath));
  local_paths.push_back(html_mime_file_path_);

  std::vector<std::string> result;
  collector.CollectForLocalPaths(local_paths,
                                 base::Bind(&OnMimeTypesCollected, &result));
  content::RunAllBlockingPoolTasksUntilIdle();

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("image/jpeg", result[0]);
  EXPECT_EQ("image/jpeg", result[1]);
  EXPECT_EQ("text/html", result[2]);
}

}  // namespace app_file_handler_util
}  // namespace extensions
