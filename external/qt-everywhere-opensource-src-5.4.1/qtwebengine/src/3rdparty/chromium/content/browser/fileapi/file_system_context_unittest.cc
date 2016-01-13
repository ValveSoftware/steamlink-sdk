// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_context.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "content/browser/quota/mock_quota_manager.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/isolated_context.h"

#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

using fileapi::ExternalMountPoints;
using fileapi::FileSystemBackend;
using fileapi::FileSystemContext;
using fileapi::FileSystemMountOption;
using fileapi::FileSystemURL;
using fileapi::IsolatedContext;

namespace content {

namespace {

const char kTestOrigin[] = "http://chromium.org/";

GURL CreateRawFileSystemURL(const std::string& type_str,
                            const std::string& fs_id) {
  std::string url_str = base::StringPrintf(
      "filesystem:http://chromium.org/%s/%s/root/file",
      type_str.c_str(),
      fs_id.c_str());
  return GURL(url_str);
}

class FileSystemContextTest : public testing::Test {
 public:
  FileSystemContextTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    storage_policy_ = new MockSpecialStoragePolicy();

    mock_quota_manager_ =
        new MockQuotaManager(false /* is_incognito */,
                                    data_dir_.path(),
                                    base::MessageLoopProxy::current().get(),
                                    base::MessageLoopProxy::current().get(),
                                    storage_policy_.get());
  }

 protected:
  FileSystemContext* CreateFileSystemContextForTest(
      fileapi::ExternalMountPoints* external_mount_points) {
    return new FileSystemContext(
        base::MessageLoopProxy::current().get(),
        base::MessageLoopProxy::current().get(),
        external_mount_points,
        storage_policy_.get(),
        mock_quota_manager_->proxy(),
        ScopedVector<FileSystemBackend>(),
        std::vector<fileapi::URLRequestAutoMountHandler>(),
        data_dir_.path(),
        CreateAllowFileAccessOptions());
  }

  // Verifies a *valid* filesystem url has expected values.
  void ExpectFileSystemURLMatches(const FileSystemURL& url,
                                  const GURL& expect_origin,
                                  fileapi::FileSystemType expect_mount_type,
                                  fileapi::FileSystemType expect_type,
                                  const base::FilePath& expect_path,
                                  const base::FilePath& expect_virtual_path,
                                  const std::string& expect_filesystem_id) {
    EXPECT_TRUE(url.is_valid());

    EXPECT_EQ(expect_origin, url.origin());
    EXPECT_EQ(expect_mount_type, url.mount_type());
    EXPECT_EQ(expect_type, url.type());
    EXPECT_EQ(expect_path, url.path());
    EXPECT_EQ(expect_virtual_path, url.virtual_path());
    EXPECT_EQ(expect_filesystem_id, url.filesystem_id());
  }

 private:
  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy_;
  scoped_refptr<MockQuotaManager> mock_quota_manager_;
};

// It is not valid to pass NULL ExternalMountPoints to FileSystemContext on
// ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(FileSystemContextTest, NullExternalMountPoints) {
  scoped_refptr<FileSystemContext> file_system_context(
      CreateFileSystemContextForTest(NULL));

  // Cracking system external mount and isolated mount points should work.
  std::string isolated_name = "root";
  std::string isolated_id =
      IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeNativeLocal,
          std::string(),
          base::FilePath(DRIVE FPL("/test/isolated/root")),
          &isolated_name);
  // Register system external mount point.
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "system",
      fileapi::kFileSystemTypeNativeLocal,
      FileSystemMountOption(),
      base::FilePath(DRIVE FPL("/test/sys/"))));

  FileSystemURL cracked_isolated = file_system_context->CrackURL(
      CreateRawFileSystemURL("isolated", isolated_id));

  ExpectFileSystemURLMatches(
      cracked_isolated,
      GURL(kTestOrigin),
      fileapi::kFileSystemTypeIsolated,
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(
          DRIVE FPL("/test/isolated/root/file")).NormalizePathSeparators(),
      base::FilePath::FromUTF8Unsafe(isolated_id).Append(FPL("root/file")).
          NormalizePathSeparators(),
      isolated_id);

  FileSystemURL cracked_external = file_system_context->CrackURL(
      CreateRawFileSystemURL("external", "system"));

  ExpectFileSystemURLMatches(
      cracked_external,
      GURL(kTestOrigin),
      fileapi::kFileSystemTypeExternal,
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(
          DRIVE FPL("/test/sys/root/file")).NormalizePathSeparators(),
      base::FilePath(FPL("system/root/file")).NormalizePathSeparators(),
      "system");


  IsolatedContext::GetInstance()->RevokeFileSystem(isolated_id);
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem("system");
}
#endif  // !defiend(OS_CHROMEOS)

TEST_F(FileSystemContextTest, FileSystemContextKeepsMountPointsAlive) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  // Register system external mount point.
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "system",
      fileapi::kFileSystemTypeNativeLocal,
      FileSystemMountOption(),
      base::FilePath(DRIVE FPL("/test/sys/"))));

  scoped_refptr<FileSystemContext> file_system_context(
      CreateFileSystemContextForTest(mount_points.get()));

  // Release a MountPoints reference created in the test.
  mount_points = NULL;

  // FileSystemContext should keep a reference to the |mount_points|, so it
  // should be able to resolve the URL.
  FileSystemURL cracked_external = file_system_context->CrackURL(
      CreateRawFileSystemURL("external", "system"));

  ExpectFileSystemURLMatches(
      cracked_external,
      GURL(kTestOrigin),
      fileapi::kFileSystemTypeExternal,
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(
          DRIVE FPL("/test/sys/root/file")).NormalizePathSeparators(),
      base::FilePath(FPL("system/root/file")).NormalizePathSeparators(),
      "system");

  // No need to revoke the registered filesystem since |mount_points| lifetime
  // is bound to this test.
}

TEST_F(FileSystemContextTest, CrackFileSystemURL) {
  scoped_refptr<ExternalMountPoints> external_mount_points(
      ExternalMountPoints::CreateRefCounted());
  scoped_refptr<FileSystemContext> file_system_context(
      CreateFileSystemContextForTest(external_mount_points.get()));

  // Register an isolated mount point.
  std::string isolated_file_system_name = "root";
  const std::string kIsolatedFileSystemID =
      IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeNativeLocal,
          std::string(),
          base::FilePath(DRIVE FPL("/test/isolated/root")),
          &isolated_file_system_name);
  // Register system external mount point.
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "system",
      fileapi::kFileSystemTypeDrive,
      FileSystemMountOption(),
      base::FilePath(DRIVE FPL("/test/sys/"))));
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "ext",
      fileapi::kFileSystemTypeNativeLocal,
      FileSystemMountOption(),
      base::FilePath(DRIVE FPL("/test/ext"))));
  // Register a system external mount point with the same name/id as the
  // registered isolated mount point.
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kIsolatedFileSystemID,
      fileapi::kFileSystemTypeRestrictedNativeLocal,
      FileSystemMountOption(),
      base::FilePath(DRIVE FPL("/test/system/isolated"))));
  // Add a mount points with the same name as a system mount point to
  // FileSystemContext's external mount points.
  ASSERT_TRUE(external_mount_points->RegisterFileSystem(
      "ext",
       fileapi::kFileSystemTypeNativeLocal,
       FileSystemMountOption(),
       base::FilePath(DRIVE FPL("/test/local/ext/"))));

  const GURL kTestOrigin = GURL("http://chromium.org/");
  const base::FilePath kVirtualPathNoRoot = base::FilePath(FPL("root/file"));

  struct TestCase {
    // Test case values.
    std::string root;
    std::string type_str;

    // Expected test results.
    bool expect_is_valid;
    fileapi::FileSystemType expect_mount_type;
    fileapi::FileSystemType expect_type;
    const base::FilePath::CharType* expect_path;
    std::string expect_filesystem_id;
  };

  const TestCase kTestCases[] = {
      // Following should not be handled by the url crackers:
      {
        "pers_mount", "persistent", true /* is_valid */,
        fileapi::kFileSystemTypePersistent, fileapi::kFileSystemTypePersistent,
        FPL("pers_mount/root/file"),
        std::string()  /* filesystem id */
      },
      {
        "temp_mount", "temporary", true /* is_valid */,
        fileapi::kFileSystemTypeTemporary, fileapi::kFileSystemTypeTemporary,
        FPL("temp_mount/root/file"),
        std::string()  /* filesystem id */
      },
      // Should be cracked by isolated mount points:
      {
        kIsolatedFileSystemID, "isolated", true /* is_valid */,
        fileapi::kFileSystemTypeIsolated, fileapi::kFileSystemTypeNativeLocal,
        DRIVE FPL("/test/isolated/root/file"),
        kIsolatedFileSystemID
      },
      // Should be cracked by system mount points:
      {
        "system", "external", true /* is_valid */,
        fileapi::kFileSystemTypeExternal, fileapi::kFileSystemTypeDrive,
        DRIVE FPL("/test/sys/root/file"),
        "system"
      },
      {
        kIsolatedFileSystemID, "external", true /* is_valid */,
        fileapi::kFileSystemTypeExternal,
        fileapi::kFileSystemTypeRestrictedNativeLocal,
        DRIVE FPL("/test/system/isolated/root/file"),
        kIsolatedFileSystemID
      },
      // Should be cracked by FileSystemContext's ExternalMountPoints.
      {
        "ext", "external", true /* is_valid */,
        fileapi::kFileSystemTypeExternal, fileapi::kFileSystemTypeNativeLocal,
        DRIVE FPL("/test/local/ext/root/file"),
        "ext"
      },
      // Test for invalid filesystem url (made invalid by adding invalid
      // filesystem type).
      {
        "sytem", "external", false /* is_valid */,
        // The rest of values will be ignored.
        fileapi::kFileSystemTypeUnknown, fileapi::kFileSystemTypeUnknown,
        FPL(""), std::string()
      },
      // Test for URL with non-existing filesystem id.
      {
        "invalid", "external", false /* is_valid */,
        // The rest of values will be ignored.
        fileapi::kFileSystemTypeUnknown, fileapi::kFileSystemTypeUnknown,
        FPL(""), std::string()
      },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    const base::FilePath virtual_path =
        base::FilePath::FromUTF8Unsafe(
            kTestCases[i].root).Append(kVirtualPathNoRoot);

    GURL raw_url =
        CreateRawFileSystemURL(kTestCases[i].type_str, kTestCases[i].root);
    FileSystemURL cracked_url = file_system_context->CrackURL(raw_url);

    SCOPED_TRACE(testing::Message() << "Test case " << i << ": "
                                    << "Cracking URL: " << raw_url);

    EXPECT_EQ(kTestCases[i].expect_is_valid, cracked_url.is_valid());
    if (!kTestCases[i].expect_is_valid)
      continue;

    ExpectFileSystemURLMatches(
        cracked_url,
        GURL(kTestOrigin),
        kTestCases[i].expect_mount_type,
        kTestCases[i].expect_type,
        base::FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
        virtual_path.NormalizePathSeparators(),
        kTestCases[i].expect_filesystem_id);
  }

  IsolatedContext::GetInstance()->RevokeFileSystemByPath(
      base::FilePath(DRIVE FPL("/test/isolated/root")));
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem("system");
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem("ext");
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kIsolatedFileSystemID);
}

TEST_F(FileSystemContextTest, CanServeURLRequest) {
  scoped_refptr<ExternalMountPoints> external_mount_points(
      ExternalMountPoints::CreateRefCounted());
  scoped_refptr<FileSystemContext> context(
      CreateFileSystemContextForTest(external_mount_points.get()));

  // A request for a sandbox mount point should be served.
  FileSystemURL cracked_url =
      context->CrackURL(CreateRawFileSystemURL("persistent", "pers_mount"));
  EXPECT_EQ(fileapi::kFileSystemTypePersistent, cracked_url.mount_type());
  EXPECT_TRUE(context->CanServeURLRequest(cracked_url));

  // A request for an isolated mount point should NOT be served.
  std::string isolated_fs_name = "root";
  std::string isolated_fs_id =
      IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeNativeLocal,
          std::string(),
          base::FilePath(DRIVE FPL("/test/isolated/root")),
          &isolated_fs_name);
  cracked_url = context->CrackURL(
      CreateRawFileSystemURL("isolated", isolated_fs_id));
  EXPECT_EQ(fileapi::kFileSystemTypeIsolated, cracked_url.mount_type());
  EXPECT_FALSE(context->CanServeURLRequest(cracked_url));

  // A request for an external mount point should be served.
  const std::string kExternalMountName = "ext_mount";
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kExternalMountName, fileapi::kFileSystemTypeDrive,
      FileSystemMountOption(),
      base::FilePath()));
  cracked_url = context->CrackURL(
      CreateRawFileSystemURL("external", kExternalMountName));
  EXPECT_EQ(fileapi::kFileSystemTypeExternal, cracked_url.mount_type());
  EXPECT_TRUE(context->CanServeURLRequest(cracked_url));

  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kExternalMountName);
  IsolatedContext::GetInstance()->RevokeFileSystem(isolated_fs_id);
}

}  // namespace

}  // namespace content
