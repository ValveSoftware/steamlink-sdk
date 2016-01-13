// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/public/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/transient_file_util.h"
#include "webkit/common/blob/scoped_file.h"

using fileapi::FileSystemURL;

namespace content {

class TransientFileUtilTest : public testing::Test {
 public:
  TransientFileUtilTest() {}
  virtual ~TransientFileUtilTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_context_ = CreateFileSystemContextForTesting(
        NULL, base::FilePath(FILE_PATH_LITERAL("dummy")));
    transient_file_util_.reset(new fileapi::TransientFileUtil);

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    file_system_context_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void CreateAndRegisterTemporaryFile(
      FileSystemURL* file_url,
      base::FilePath* file_path) {
    EXPECT_TRUE(base::CreateTemporaryFileInDir(data_dir_.path(), file_path));
    fileapi::IsolatedContext* isolated_context =
        fileapi::IsolatedContext::GetInstance();
    std::string name = "tmp";
    std::string fsid = isolated_context->RegisterFileSystemForPath(
        fileapi::kFileSystemTypeForTransientFile,
        std::string(),
        *file_path,
        &name);
    ASSERT_TRUE(!fsid.empty());
    base::FilePath virtual_path = isolated_context->CreateVirtualRootPath(
        fsid).AppendASCII(name);
    *file_url = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://foo"),
        fileapi::kFileSystemTypeIsolated,
        virtual_path);
  }

  scoped_ptr<fileapi::FileSystemOperationContext> NewOperationContext() {
    return make_scoped_ptr(
        new fileapi::FileSystemOperationContext(file_system_context_.get()));
  }

  fileapi::FileSystemFileUtil* file_util() {
    return transient_file_util_.get();
  }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_ptr<fileapi::TransientFileUtil> transient_file_util_;

  DISALLOW_COPY_AND_ASSIGN(TransientFileUtilTest);
};

TEST_F(TransientFileUtilTest, TransientFile) {
  FileSystemURL temp_url;
  base::FilePath temp_path;

  CreateAndRegisterTemporaryFile(&temp_url, &temp_path);

  base::File::Error error;
  base::File::Info file_info;
  base::FilePath path;

  // Make sure the file is there.
  ASSERT_TRUE(temp_url.is_valid());
  ASSERT_TRUE(base::PathExists(temp_path));
  ASSERT_FALSE(base::DirectoryExists(temp_path));

  // Create a snapshot file.
  {
    webkit_blob::ScopedFile scoped_file =
        file_util()->CreateSnapshotFile(NewOperationContext().get(),
                                        temp_url,
                                        &error,
                                        &file_info,
                                        &path);
    ASSERT_EQ(base::File::FILE_OK, error);
    ASSERT_EQ(temp_path, path);
    ASSERT_FALSE(file_info.is_directory);

    // The file should be still there.
    ASSERT_TRUE(base::PathExists(temp_path));
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(NewOperationContext().get(),
                                       temp_url, &file_info, &path));
    ASSERT_EQ(temp_path, path);
    ASSERT_FALSE(file_info.is_directory);
  }

  // The file's now scoped out.
  base::RunLoop().RunUntilIdle();

  // Now the temporary file and the transient filesystem must be gone too.
  ASSERT_FALSE(base::PathExists(temp_path));
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_util()->GetFileInfo(NewOperationContext().get(),
                                     temp_url, &file_info, &path));
}

}  // namespace content
