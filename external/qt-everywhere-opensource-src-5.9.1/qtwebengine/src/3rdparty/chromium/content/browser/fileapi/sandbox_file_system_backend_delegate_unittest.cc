// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/sandbox_file_system_backend_delegate.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_file_system_options.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::FileSystemURL;

namespace content {

namespace {

FileSystemURL CreateFileSystemURL(const char* path) {
  const GURL kOrigin("http://foo/");
  return storage::FileSystemURL::CreateForTest(
      kOrigin,
      storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class SandboxFileSystemBackendDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    delegate_.reset(new storage::SandboxFileSystemBackendDelegate(
        NULL /* quota_manager_proxy */,
        base::ThreadTaskRunnerHandle::Get().get(), data_dir_.GetPath(),
        NULL /* special_storage_policy */, CreateAllowFileAccessOptions()));
  }

  bool IsAccessValid(const FileSystemURL& url) const {
    return delegate_->IsAccessValid(url);
  }

  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  std::unique_ptr<storage::SandboxFileSystemBackendDelegate> delegate_;
};

TEST_F(SandboxFileSystemBackendDelegateTest, IsAccessValid) {
  // Normal case.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("a")));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("a/../b")));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_FALSE(IsAccessValid(
      FileSystemURL::CreateForTest(GURL("unknown://bar"),
                                   storage::kFileSystemTypeTemporary,
                                   base::FilePath::FromUTF8Unsafe("foo"))));

  // Access with restricted name should be disallowed.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(".")));
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("..")));

  // This is also disallowed due to Windows XP parent path handling.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL("...")));

  // These are identified as unsafe cases due to weird path handling
  // on Windows.
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(" ..")));
  EXPECT_FALSE(IsAccessValid(CreateFileSystemURL(".. ")));

  // Similar but safe cases.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(" .")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(". ")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("b.")));
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL(".b")));

  // A path that looks like a drive letter.
  EXPECT_TRUE(IsAccessValid(CreateFileSystemURL("c:")));
}

}  // namespace content
