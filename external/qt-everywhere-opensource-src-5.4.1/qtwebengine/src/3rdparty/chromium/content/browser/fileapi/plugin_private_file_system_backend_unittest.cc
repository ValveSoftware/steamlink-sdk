// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/public/test/async_file_test_helper.h"
#include "content/public/test/test_file_system_context.h"
#include "content/public/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/plugin_private_file_system_backend.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::AsyncFileTestHelper;
using fileapi::FileSystemContext;
using fileapi::FileSystemURL;
using fileapi::IsolatedContext;

namespace content {

namespace {

const GURL kOrigin("http://www.example.com");
const std::string kPlugin1("plugin1");
const std::string kPlugin2("plugin2");
const fileapi::FileSystemType kType = fileapi::kFileSystemTypePluginPrivate;
const std::string kRootName = "pluginprivate";

void DidOpenFileSystem(base::File::Error* error_out,
                       base::File::Error error) {
  *error_out = error;
}

std::string RegisterFileSystem() {
  return IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
      kType, kRootName, base::FilePath());
}

}  // namespace

class PluginPrivateFileSystemBackendTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = CreateFileSystemContextForTesting(
        NULL /* quota_manager_proxy */,
        data_dir_.path());
  }

  FileSystemURL CreateURL(const GURL& root_url, const std::string& relative) {
    FileSystemURL root = context_->CrackURL(root_url);
    return context_->CreateCrackedFileSystemURL(
        root.origin(),
        root.mount_type(),
        root.virtual_path().AppendASCII(relative));
  }

  fileapi::PluginPrivateFileSystemBackend* backend() const {
    return context_->plugin_private_backend();
  }

  const base::FilePath& base_path() const { return backend()->base_path(); }

  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> context_;
  std::string filesystem_id_;
};

TEST_F(PluginPrivateFileSystemBackendTest, OpenFileSystemBasic) {
  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(
      kOrigin, kType, filesystem_id1, kPlugin1,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Run this again with FAIL_IF_NONEXISTENT to see if it succeeds.
  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(
      kOrigin, kType, filesystem_id2, kPlugin1,
      fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::Bind(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const GURL root_url(
      fileapi::GetIsolatedFileSystemRootURIString(
          kOrigin, filesystem_id1, kRootName));
  FileSystemURL file = CreateURL(root_url, "foo");
  base::FilePath platform_path;
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file));
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::GetPlatformPath(context_.get(), file,
                                                 &platform_path));
  EXPECT_TRUE(base_path().AppendASCII("000").AppendASCII(kPlugin1).IsParent(
      platform_path));
}

TEST_F(PluginPrivateFileSystemBackendTest, PluginIsolation) {
  // Open filesystem for kPlugin1 and kPlugin2.
  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(
      kOrigin, kType, filesystem_id1, kPlugin1,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(
      kOrigin, kType, filesystem_id2, kPlugin2,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Create 'foo' in kPlugin1.
  const GURL root_url1(
      fileapi::GetIsolatedFileSystemRootURIString(
          kOrigin, filesystem_id1, kRootName));
  FileSystemURL file1 = CreateURL(root_url1, "foo");
  base::FilePath platform_path;
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file1));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file1, AsyncFileTestHelper::kDontCheckSize));

  // See the same path is not available in kPlugin2.
  const GURL root_url2(
      fileapi::GetIsolatedFileSystemRootURIString(
          kOrigin, filesystem_id2, kRootName));
  FileSystemURL file2 = CreateURL(root_url2, "foo");
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      context_.get(), file2, AsyncFileTestHelper::kDontCheckSize));
}

// TODO(kinuko,nhiroki): also test if DeleteOriginDataOnFileThread
// works fine when there's multiple plugin partitions.

}  // namespace content
