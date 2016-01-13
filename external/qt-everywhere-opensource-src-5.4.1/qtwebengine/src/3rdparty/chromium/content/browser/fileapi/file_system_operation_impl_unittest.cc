// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_operation_impl.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/browser/fileapi/mock_file_change_observer.h"
#include "content/browser/quota/mock_quota_manager.h"
#include "content/browser/quota/mock_quota_manager_proxy.h"
#include "content/public/test/async_file_test_helper.h"
#include "content/public/test/sandbox_file_system_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::AsyncFileTestHelper;
using fileapi::FileSystemOperation;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemOperationRunner;
using fileapi::FileSystemURL;
using quota::QuotaManager;
using quota::QuotaManagerProxy;
using webkit_blob::ShareableFileReference;

namespace content {

namespace {

const int kFileOperationStatusNotSet = 1;

void AssertFileErrorEq(const tracked_objects::Location& from_here,
                       base::File::Error expected,
                       base::File::Error actual) {
  ASSERT_EQ(expected, actual) << from_here.ToString();
}

}  // namespace

// Test class for FileSystemOperationImpl.
class FileSystemOperationImplTest
    : public testing::Test {
 public:
  FileSystemOperationImplTest()
      : status_(kFileOperationStatusNotSet),
        weak_factory_(this) {}

 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    change_observers_ = fileapi::MockFileChangeObserver::CreateList(
        &change_observer_);

    base::FilePath base_dir = base_.path().AppendASCII("filesystem");
    quota_manager_ =
        new MockQuotaManager(false /* is_incognito */,
                                    base_dir,
                                    base::MessageLoopProxy::current().get(),
                                    base::MessageLoopProxy::current().get(),
                                    NULL /* special storage policy */);
    quota_manager_proxy_ = new MockQuotaManagerProxy(
        quota_manager(), base::MessageLoopProxy::current().get());
    sandbox_file_system_.SetUp(base_dir, quota_manager_proxy_.get());
    sandbox_file_system_.AddFileChangeObserver(&change_observer_);
  }

  virtual void TearDown() OVERRIDE {
    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_proxy()->SimulateQuotaManagerDestroyed();
    quota_manager_ = NULL;
    quota_manager_proxy_ = NULL;
    sandbox_file_system_.TearDown();
  }

  FileSystemOperationRunner* operation_runner() {
    return sandbox_file_system_.operation_runner();
  }

  int status() const { return status_; }
  const base::File::Info& info() const { return info_; }
  const base::FilePath& path() const { return path_; }
  const std::vector<fileapi::DirectoryEntry>& entries() const {
    return entries_;
  }

  const ShareableFileReference* shareable_file_ref() const {
    return shareable_file_ref_.get();
  }

  MockQuotaManager* quota_manager() {
    return static_cast<MockQuotaManager*>(quota_manager_.get());
  }

  MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<MockQuotaManagerProxy*>(
        quota_manager_proxy_.get());
  }

  fileapi::FileSystemFileUtil* file_util() {
    return sandbox_file_system_.file_util();
  }

  fileapi::MockFileChangeObserver* change_observer() {
    return &change_observer_;
  }

  scoped_ptr<FileSystemOperationContext> NewContext() {
    FileSystemOperationContext* context =
        sandbox_file_system_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return make_scoped_ptr(context);
  }

  FileSystemURL URLForPath(const std::string& path) const {
    return sandbox_file_system_.CreateURLFromUTF8(path);
  }

  base::FilePath PlatformPath(const std::string& path) {
    return sandbox_file_system_.GetLocalPath(
        base::FilePath::FromUTF8Unsafe(path));
  }

  bool FileExists(const std::string& path) {
    return AsyncFileTestHelper::FileExists(
        sandbox_file_system_.file_system_context(), URLForPath(path),
        AsyncFileTestHelper::kDontCheckSize);
  }

  bool DirectoryExists(const std::string& path) {
    return AsyncFileTestHelper::DirectoryExists(
        sandbox_file_system_.file_system_context(), URLForPath(path));
  }

  FileSystemURL CreateFile(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    bool created = false;
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->EnsureFileExists(NewContext().get(),
                                            url, &created));
    EXPECT_TRUE(created);
    return url;
  }

  FileSystemURL CreateDirectory(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->CreateDirectory(NewContext().get(), url,
                                           false /* exclusive */, true));
    return url;
  }

  int64 GetFileSize(const std::string& path) {
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(PlatformPath(path), &info));
    return info.size;
  }

  // Callbacks for recording test results.
  FileSystemOperation::StatusCallback RecordStatusCallback() {
    return base::Bind(&FileSystemOperationImplTest::DidFinish,
                      weak_factory_.GetWeakPtr());
  }

  FileSystemOperation::ReadDirectoryCallback
  RecordReadDirectoryCallback() {
    return base::Bind(&FileSystemOperationImplTest::DidReadDirectory,
                      weak_factory_.GetWeakPtr());
  }

  FileSystemOperation::GetMetadataCallback RecordMetadataCallback() {
    return base::Bind(&FileSystemOperationImplTest::DidGetMetadata,
                      weak_factory_.GetWeakPtr());
  }

  FileSystemOperation::SnapshotFileCallback RecordSnapshotFileCallback() {
    return base::Bind(&FileSystemOperationImplTest::DidCreateSnapshotFile,
                      weak_factory_.GetWeakPtr());
  }

  void DidFinish(base::File::Error status) {
    status_ = status;
  }

  void DidReadDirectory(
      base::File::Error status,
      const std::vector<fileapi::DirectoryEntry>& entries,
      bool /* has_more */) {
    entries_ = entries;
    status_ = status;
  }

  void DidGetMetadata(base::File::Error status,
                      const base::File::Info& info) {
    info_ = info;
    status_ = status;
  }

  void DidCreateSnapshotFile(
      base::File::Error status,
      const base::File::Info& info,
      const base::FilePath& platform_path,
      const scoped_refptr<ShareableFileReference>& shareable_file_ref) {
    info_ = info;
    path_ = platform_path;
    status_ = status;
    shareable_file_ref_ = shareable_file_ref;
  }

  int64 GetDataSizeOnDisk() {
    return sandbox_file_system_.ComputeCurrentOriginUsage() -
        sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  void GetUsageAndQuota(int64* usage, int64* quota) {
    quota::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(quota_manager_.get(),
                                              sandbox_file_system_.origin(),
                                              sandbox_file_system_.type(),
                                              usage,
                                              quota);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

  int64 ComputePathCost(const FileSystemURL& url) {
    int64 base_usage;
    GetUsageAndQuota(&base_usage, NULL);

    AsyncFileTestHelper::CreateFile(
        sandbox_file_system_.file_system_context(), url);
    operation_runner()->Remove(url, false /* recursive */,
                               base::Bind(&AssertFileErrorEq, FROM_HERE,
                                          base::File::FILE_OK));
    base::RunLoop().RunUntilIdle();
    change_observer()->ResetCount();

    int64 total_usage;
    GetUsageAndQuota(&total_usage, NULL);
    return total_usage - base_usage;
  }

  void GrantQuotaForCurrentUsage() {
    int64 usage;
    GetUsageAndQuota(&usage, NULL);
    quota_manager()->SetQuota(sandbox_file_system_.origin(),
                              sandbox_file_system_.storage_type(),
                              usage);
  }

  int64 GetUsage() {
    int64 usage = 0;
    GetUsageAndQuota(&usage, NULL);
    return usage;
  }

  void AddQuota(int64 quota_delta) {
    int64 quota;
    GetUsageAndQuota(NULL, &quota);
    quota_manager()->SetQuota(sandbox_file_system_.origin(),
                              sandbox_file_system_.storage_type(),
                              quota + quota_delta);
  }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;

  SandboxFileSystemTestHelper sandbox_file_system_;

  // For post-operation status.
  int status_;
  base::File::Info info_;
  base::FilePath path_;
  std::vector<fileapi::DirectoryEntry> entries_;
  scoped_refptr<ShareableFileReference> shareable_file_ref_;

  fileapi::MockFileChangeObserver change_observer_;
  fileapi::ChangeObserverList change_observers_;

  base::WeakPtrFactory<FileSystemOperationImplTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationImplTest);
};

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcDoesntExist) {
  change_observer()->ResetCount();
  operation_runner()->Move(URLForPath("a"), URLForPath("b"),
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureContainsPath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("src/dest"));

  operation_runner()->Move(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  operation_runner()->Move(src_dir, dest_file,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest,
       TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  operation_runner()->Move(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL src_file(CreateFile("src/file"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Move(src_file, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FileSystemURL src_dir(CreateDirectory("src"));
  operation_runner()->Move(src_dir, URLForPath("nonexistent/deset"),
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcFileAndOverwrite) {
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_file(CreateFile("dest"));

  operation_runner()->Move(src_file, dest_file,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("dest"));

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcFileAndNew) {
  FileSystemURL src_file(CreateFile("src"));

  operation_runner()->Move(src_file, URLForPath("new"),
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("new"));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirAndOverwrite) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Move(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(DirectoryExists("src"));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_FALSE(DirectoryExists("dest/src"));
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirAndNew) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Move(src_dir, URLForPath("dest/new"),
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(DirectoryExists("src"));
  EXPECT_TRUE(DirectoryExists("dest/new"));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirRecursive) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Move(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/sub"));

  EXPECT_EQ(3, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSamePath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  operation_runner()->Move(src_dir, src_dir,
                           FileSystemOperation::OPTION_NONE,
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/sub"));

  EXPECT_EQ(0, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcDoesntExist) {
  operation_runner()->Copy(URLForPath("a"), URLForPath("b"),
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureContainsPath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("src/dir"));

  operation_runner()->Copy(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  operation_runner()->Copy(src_dir, dest_file,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest,
       TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  operation_runner()->Copy(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Copy(src_file, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FileSystemURL src_dir(CreateDirectory("src"));

  operation_runner()->Copy(src_dir, URLForPath("nonexistent/dest"),
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureByQuota) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL src_file(CreateFile("src/file"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  operation_runner()->Truncate(src_file, 6, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_EQ(6, GetFileSize("src/file"));

  FileSystemURL dest_file(URLForPath("dest/file"));
  int64 dest_path_cost = ComputePathCost(dest_file);
  GrantQuotaForCurrentUsage();
  AddQuota(6 + dest_path_cost - 1);

  operation_runner()->Copy(src_file, dest_file,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, status());
  EXPECT_FALSE(FileExists("dest/file"));
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcFileAndOverwrite) {
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_file(CreateFile("dest"));

  operation_runner()->Copy(src_file, dest_file,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("dest"));
  EXPECT_EQ(2, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcFileAndNew) {
  FileSystemURL src_file(CreateFile("src"));

  operation_runner()->Copy(src_file, URLForPath("new"),
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("new"));
  EXPECT_EQ(2, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirAndOverwrite) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Copy(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_FALSE(DirectoryExists("dest/src"));
  EXPECT_GE(quota_manager_proxy()->notify_storage_accessed_count(), 3);

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirAndNew) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir_new(URLForPath("dest"));

  operation_runner()->Copy(src_dir, dest_dir_new,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_GE(quota_manager_proxy()->notify_storage_accessed_count(), 2);

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirRecursive) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  FileSystemURL dest_dir(CreateDirectory("dest"));

  operation_runner()->Copy(src_dir, dest_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/sub"));

  // For recursive copy we may record multiple read access.
  EXPECT_GE(quota_manager_proxy()->notify_storage_accessed_count(), 1);

  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSamePath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  operation_runner()->Copy(src_dir, src_dir,
                           FileSystemOperation::OPTION_NONE,
                           FileSystemOperationRunner::CopyProgressCallback(),
                           RecordStatusCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/sub"));

  EXPECT_EQ(0, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyInForeignFileSuccess) {
  base::FilePath src_local_disk_file_path;
  base::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  int data_size = ARRAYSIZE_UNSAFE(test_data);
  base::WriteFile(src_local_disk_file_path, test_data, data_size);

  FileSystemURL dest_dir(CreateDirectory("dest"));

  int64 before_usage;
  GetUsageAndQuota(&before_usage, NULL);

  // Check that the file copied and corresponding usage increased.
  operation_runner()->CopyInForeignFile(src_local_disk_file_path,
                                        URLForPath("dest/file"),
                                        RecordStatusCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, change_observer()->create_file_count());
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("dest/file"));
  int64 after_usage;
  GetUsageAndQuota(&after_usage, NULL);
  EXPECT_GT(after_usage, before_usage);

  // Compare contents of src and copied file.
  char buffer[100];
  EXPECT_EQ(data_size, base::ReadFile(PlatformPath("dest/file"),
                                      buffer, data_size));
  for (int i = 0; i < data_size; ++i)
    EXPECT_EQ(test_data[i], buffer[i]);
}

TEST_F(FileSystemOperationImplTest, TestCopyInForeignFileFailureByQuota) {
  base::FilePath src_local_disk_file_path;
  base::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  base::WriteFile(src_local_disk_file_path, test_data,
                       ARRAYSIZE_UNSAFE(test_data));

  FileSystemURL dest_dir(CreateDirectory("dest"));

  GrantQuotaForCurrentUsage();
  operation_runner()->CopyInForeignFile(src_local_disk_file_path,
                                        URLForPath("dest/file"),
                                        RecordStatusCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(FileExists("dest/file"));
  EXPECT_EQ(0, change_observer()->create_file_count());
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, status());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  FileSystemURL file(CreateFile("file"));
  operation_runner()->CreateFile(file, true, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  FileSystemURL file(CreateFile("file"));
  operation_runner()->CreateFile(file, false, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("file"));

  // The file was already there; did nothing.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  operation_runner()->CreateFile(URLForPath("new"), true,
                                 RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(FileExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  operation_runner()->CreateFile(URLForPath("nonexistent"), false,
                                 RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(FileSystemOperationImplTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  operation_runner()->CreateDirectory(
      URLForPath("nonexistent/dir"), false, false,
      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  FileSystemURL dir(CreateDirectory("dir"));
  operation_runner()->CreateDirectory(dir, true, false,
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  FileSystemURL file(CreateFile("file"));
  operation_runner()->CreateDirectory(file, true, false,
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  FileSystemURL dir(CreateDirectory("dir"));
  operation_runner()->CreateDirectory(dir, false, false,
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Dir doesn't exist.
  operation_runner()->CreateDirectory(URLForPath("new"), false, false,
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  operation_runner()->CreateDirectory(URLForPath("new"), true, false,
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(DirectoryExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestExistsAndMetadataFailure) {
  operation_runner()->GetMetadata(URLForPath("nonexistent"),
                                  RecordMetadataCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());

  operation_runner()->FileExists(URLForPath("nonexistent"),
                                 RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());

  operation_runner()->DirectoryExists(URLForPath("nonexistent"),
                                      RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestExistsAndMetadataSuccess) {
  FileSystemURL dir(CreateDirectory("dir"));
  FileSystemURL file(CreateFile("dir/file"));
  int read_access = 0;

  operation_runner()->DirectoryExists(dir, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  ++read_access;

  operation_runner()->GetMetadata(dir, RecordMetadataCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(info().is_directory);
  ++read_access;

  operation_runner()->FileExists(file, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  ++read_access;

  operation_runner()->GetMetadata(file, RecordMetadataCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  ++read_access;

  EXPECT_EQ(read_access,
            quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestTypeMismatchErrors) {
  FileSystemURL dir(CreateDirectory("dir"));
  operation_runner()->FileExists(dir, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE, status());

  FileSystemURL file(CreateFile("file"));
  operation_runner()->DirectoryExists(file, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY, status());
}

TEST_F(FileSystemOperationImplTest, TestReadDirFailure) {
  // Path doesn't exist
  operation_runner()->ReadDirectory(URLForPath("nonexistent"),
                                    RecordReadDirectoryCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());

  // File exists.
  FileSystemURL file(CreateFile("file"));
  operation_runner()->ReadDirectory(file, RecordReadDirectoryCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  FileSystemURL child_file(CreateFile("dir/child_file"));

  operation_runner()->ReadDirectory(parent_dir, RecordReadDirectoryCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_EQ(2u, entries().size());

  for (size_t i = 0; i < entries().size(); ++i) {
    if (entries()[i].is_directory)
      EXPECT_EQ(FILE_PATH_LITERAL("child_dir"), entries()[i].name);
    else
      EXPECT_EQ(FILE_PATH_LITERAL("child_file"), entries()[i].name);
  }
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveFailure) {
  // Path doesn't exist.
  operation_runner()->Remove(URLForPath("nonexistent"), false /* recursive */,
                             RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());

  // It's an error to try to remove a non-empty directory if recursive flag
  // is false.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  FileSystemURL child_file(CreateFile("dir/child_file"));

  operation_runner()->Remove(parent_dir, false /* recursive */,
                             RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveSuccess) {
  FileSystemURL empty_dir(CreateDirectory("empty_dir"));
  EXPECT_TRUE(DirectoryExists("empty_dir"));
  operation_runner()->Remove(empty_dir, false /* recursive */,
                             RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(DirectoryExists("empty_dir"));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveSuccessRecursive) {
  // Removing a non-empty directory with recursive flag == true should be ok.
  //      parent_dir
  //       |       |
  //  child_dir  child_files
  //       |
  //  child_files
  //
  // Verify deleting parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  for (int i = 0; i < 8; ++i)
    CreateFile(base::StringPrintf("dir/file-%d", i));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  for (int i = 0; i < 8; ++i)
    CreateFile(base::StringPrintf("dir/child_dir/file-%d", i));

  operation_runner()->Remove(parent_dir, true /* recursive */,
                             RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(DirectoryExists("parent_dir"));

  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(16, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestTruncate) {
  FileSystemURL file(CreateFile("file"));
  base::FilePath platform_path = PlatformPath("file");

  char test_data[] = "test data";
  int data_size = static_cast<int>(sizeof(test_data));
  EXPECT_EQ(data_size,
            base::WriteFile(platform_path, test_data, data_size));

  // Check that its length is the size of the data written.
  operation_runner()->GetMetadata(file, RecordMetadataCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  operation_runner()->Truncate(file, length, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 17 and that it's all zeroes after the test
  // data.
  EXPECT_EQ(length, GetFileSize("file"));
  char data[100];
  EXPECT_EQ(length, base::ReadFile(platform_path, data, length));
  for (int i = 0; i < length; ++i) {
    if (i < static_cast<int>(sizeof(test_data)))
      EXPECT_EQ(test_data[i], data[i]);
    else
      EXPECT_EQ(0, data[i]);
  }

  // Shorten the file by truncating it.
  length = 3;
  operation_runner()->Truncate(file, length, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 3 and that it contains only bits of test data.
  EXPECT_EQ(length, GetFileSize("file"));
  EXPECT_EQ(length, base::ReadFile(platform_path, data, length));
  for (int i = 0; i < length; ++i)
    EXPECT_EQ(test_data[i], data[i]);

  // Truncate is not a 'read' access.  (Here expected access count is 1
  // since we made 1 read access for GetMetadata.)
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(FileSystemOperationImplTest, TestTruncateFailureByQuota) {
  FileSystemURL dir(CreateDirectory("dir"));
  FileSystemURL file(CreateFile("dir/file"));

  GrantQuotaForCurrentUsage();
  AddQuota(10);

  operation_runner()->Truncate(file, 10, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(10, GetFileSize("dir/file"));

  operation_runner()->Truncate(file, 11, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(10, GetFileSize("dir/file"));
}

TEST_F(FileSystemOperationImplTest, TestTouchFile) {
  FileSystemURL file(CreateFile("file"));
  base::FilePath platform_path = PlatformPath("file");

  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(platform_path, &info));
  EXPECT_FALSE(info.is_directory);
  EXPECT_EQ(0, info.size);
  const base::Time last_modified = info.last_modified;
  const base::Time last_accessed = info.last_accessed;

  const base::Time new_modified_time = base::Time::UnixEpoch();
  const base::Time new_accessed_time = new_modified_time +
      base::TimeDelta::FromHours(77);
  ASSERT_NE(last_modified, new_modified_time);
  ASSERT_NE(last_accessed, new_accessed_time);

  operation_runner()->TouchFile(file, new_accessed_time, new_modified_time,
                                RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(base::GetFileInfo(platform_path, &info));
  // We compare as time_t here to lower our resolution, to avoid false
  // negatives caused by conversion to the local filesystem's native
  // representation and back.
  EXPECT_EQ(new_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  EXPECT_EQ(new_accessed_time.ToTimeT(), info.last_accessed.ToTimeT());
}

TEST_F(FileSystemOperationImplTest, TestCreateSnapshotFile) {
  FileSystemURL dir(CreateDirectory("dir"));

  // Create a file for the testing.
  operation_runner()->DirectoryExists(dir, RecordStatusCallback());
  FileSystemURL file(CreateFile("dir/file"));
  operation_runner()->FileExists(file, RecordStatusCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());

  // See if we can get a 'snapshot' file info for the file.
  // Since FileSystemOperationImpl assumes the file exists in the local
  // directory it should just returns the same metadata and platform_path
  // as the file itself.
  operation_runner()->CreateSnapshotFile(file, RecordSnapshotFileCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath("dir/file"), path());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // The FileSystemOpration implementation does not create a
  // shareable file reference.
  EXPECT_EQ(NULL, shareable_file_ref());
}

TEST_F(FileSystemOperationImplTest,
       TestMoveSuccessSrcDirRecursiveWithQuota) {
  FileSystemURL src(CreateDirectory("src"));
  int src_path_cost = GetUsage();

  FileSystemURL dest(CreateDirectory("dest"));
  FileSystemURL child_file1(CreateFile("src/file1"));
  FileSystemURL child_file2(CreateFile("src/file2"));
  FileSystemURL child_dir(CreateDirectory("src/dir"));
  FileSystemURL grandchild_file1(CreateFile("src/dir/file1"));
  FileSystemURL grandchild_file2(CreateFile("src/dir/file2"));

  int total_path_cost = GetUsage();
  EXPECT_EQ(0, GetDataSizeOnDisk());

  operation_runner()->Truncate(
      child_file1, 5000,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      child_file2, 400,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      grandchild_file1, 30,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      grandchild_file2, 2,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  base::RunLoop().RunUntilIdle();

  const int64 all_file_size = 5000 + 400 + 30 + 2;
  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(all_file_size + total_path_cost, GetUsage());

  operation_runner()->Move(
      src, dest, FileSystemOperation::OPTION_NONE,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(DirectoryExists("src/dir"));
  EXPECT_FALSE(FileExists("src/dir/file2"));
  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/file2"));

  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(all_file_size + total_path_cost - src_path_cost,
            GetUsage());
}

TEST_F(FileSystemOperationImplTest,
       TestCopySuccessSrcDirRecursiveWithQuota) {
  FileSystemURL src(CreateDirectory("src"));
  FileSystemURL dest1(CreateDirectory("dest1"));
  FileSystemURL dest2(CreateDirectory("dest2"));

  int64 usage = GetUsage();
  FileSystemURL child_file1(CreateFile("src/file1"));
  FileSystemURL child_file2(CreateFile("src/file2"));
  FileSystemURL child_dir(CreateDirectory("src/dir"));
  int64 child_path_cost = GetUsage() - usage;
  usage += child_path_cost;

  FileSystemURL grandchild_file1(CreateFile("src/dir/file1"));
  FileSystemURL grandchild_file2(CreateFile("src/dir/file2"));
  int64 total_path_cost = GetUsage();
  int64 grandchild_path_cost = total_path_cost - usage;

  EXPECT_EQ(0, GetDataSizeOnDisk());

  operation_runner()->Truncate(
      child_file1, 8000,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      child_file2, 700,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      grandchild_file1, 60,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  operation_runner()->Truncate(
      grandchild_file2, 5,
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  base::RunLoop().RunUntilIdle();

  const int64 child_file_size = 8000 + 700;
  const int64 grandchild_file_size = 60 + 5;
  const int64 all_file_size = child_file_size + grandchild_file_size;
  int64 expected_usage = all_file_size + total_path_cost;

  usage = GetUsage();
  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, usage);

  // Copy src to dest1.
  operation_runner()->Copy(
      src, dest1, FileSystemOperation::OPTION_NONE,
      FileSystemOperationRunner::CopyProgressCallback(),
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  base::RunLoop().RunUntilIdle();

  expected_usage += all_file_size + child_path_cost + grandchild_path_cost;
  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/file2"));
  EXPECT_TRUE(DirectoryExists("dest1/dir"));
  EXPECT_TRUE(FileExists("dest1/dir/file2"));

  EXPECT_EQ(2 * all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, GetUsage());

  // Copy src/dir to dest2.
  operation_runner()->Copy(
      child_dir, dest2, FileSystemOperation::OPTION_NONE,
      FileSystemOperationRunner::CopyProgressCallback(),
      base::Bind(&AssertFileErrorEq, FROM_HERE, base::File::FILE_OK));
  base::RunLoop().RunUntilIdle();

  expected_usage += grandchild_file_size + grandchild_path_cost;
  usage = GetUsage();
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size,
            GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, usage);
}

}  // namespace content
