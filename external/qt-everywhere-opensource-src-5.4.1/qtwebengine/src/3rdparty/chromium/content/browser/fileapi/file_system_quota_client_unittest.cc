// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/public/test/async_file_test_helper.h"
#include "content/public/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_quota_client.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"
#include "webkit/common/quota/quota_types.h"

using content::AsyncFileTestHelper;
using fileapi::FileSystemQuotaClient;
using fileapi::FileSystemURL;

namespace content {
namespace {

const char kDummyURL1[] = "http://www.dummy.org";
const char kDummyURL2[] = "http://www.example.com";
const char kDummyURL3[] = "http://www.bleh";

// Declared to shorten the variable names.
const quota::StorageType kTemporary = quota::kStorageTypeTemporary;
const quota::StorageType kPersistent = quota::kStorageTypePersistent;

}  // namespace

class FileSystemQuotaClientTest : public testing::Test {
 public:
  FileSystemQuotaClientTest()
      : weak_factory_(this),
        additional_callback_count_(0),
        deletion_status_(quota::kQuotaStatusUnknown) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
            NULL, data_dir_.path());
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64 size;
    const char* origin_url;
    quota::StorageType type;
  };

 protected:
  FileSystemQuotaClient* NewQuotaClient(bool is_incognito) {
    return new FileSystemQuotaClient(file_system_context_.get(), is_incognito);
  }

  void GetOriginUsageAsync(FileSystemQuotaClient* quota_client,
                           const std::string& origin_url,
                           quota::StorageType type) {
    quota_client->GetOriginUsage(
        GURL(origin_url), type,
        base::Bind(&FileSystemQuotaClientTest::OnGetUsage,
                   weak_factory_.GetWeakPtr()));
  }

  int64 GetOriginUsage(FileSystemQuotaClient* quota_client,
                       const std::string& origin_url,
                       quota::StorageType type) {
    GetOriginUsageAsync(quota_client, origin_url, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(
      FileSystemQuotaClient* quota_client,
      quota::StorageType type) {
    origins_.clear();
    quota_client->GetOriginsForType(
        type,
        base::Bind(&FileSystemQuotaClientTest::OnGetOrigins,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(
      FileSystemQuotaClient* quota_client,
      quota::StorageType type,
      const std::string& host) {
    origins_.clear();
    quota_client->GetOriginsForHost(
        type, host,
        base::Bind(&FileSystemQuotaClientTest::OnGetOrigins,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  void RunAdditionalOriginUsageTask(
      FileSystemQuotaClient* quota_client,
      const std::string& origin_url,
      quota::StorageType type) {
    quota_client->GetOriginUsage(
        GURL(origin_url), type,
        base::Bind(&FileSystemQuotaClientTest::OnGetAdditionalUsage,
                   weak_factory_.GetWeakPtr()));
  }

  bool CreateFileSystemDirectory(const base::FilePath& file_path,
                                 const std::string& origin_url,
                                 quota::StorageType storage_type) {
    fileapi::FileSystemType type =
        fileapi::QuotaStorageTypeToFileSystemType(storage_type);
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        GURL(origin_url), type, file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateDirectory(file_system_context_, url);
    return result == base::File::FILE_OK;
  }

  bool CreateFileSystemFile(const base::FilePath& file_path,
                            int64 file_size,
                            const std::string& origin_url,
                            quota::StorageType storage_type) {
    if (file_path.empty())
      return false;

    fileapi::FileSystemType type =
        fileapi::QuotaStorageTypeToFileSystemType(storage_type);
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        GURL(origin_url), type, file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_, url);
    if (result != base::File::FILE_OK)
      return false;

    result = AsyncFileTestHelper::TruncateFile(
        file_system_context_, url, file_size);
    return result == base::File::FILE_OK;
  }

  void InitializeOriginFiles(FileSystemQuotaClient* quota_client,
                             const TestFile* files,
                             int num_files) {
    for (int i = 0; i < num_files; i++) {
      base::FilePath path = base::FilePath().AppendASCII(files[i].name);
      if (files[i].isDirectory) {
        ASSERT_TRUE(CreateFileSystemDirectory(
            path, files[i].origin_url, files[i].type));
        if (path.empty()) {
          // Create the usage cache.
          // HACK--we always create the root [an empty path] first.  If we
          // create it later, this will fail due to a quota mismatch.  If we
          // call this before we create the root, it succeeds, but hasn't
          // actually created the cache.
          ASSERT_EQ(0, GetOriginUsage(
              quota_client, files[i].origin_url, files[i].type));
        }
      } else {
        ASSERT_TRUE(CreateFileSystemFile(
            path, files[i].size, files[i].origin_url, files[i].type));
      }
    }
  }

  // This is a bit fragile--it depends on the test data always creating a
  // directory before adding a file or directory to it, so that we can just
  // count the basename of each addition.  A recursive creation of a path, which
  // created more than one directory in a single shot, would break this.
  int64 ComputeFilePathsCostForOriginAndType(const TestFile* files,
                                             int num_files,
                                             const std::string& origin_url,
                                             quota::StorageType type) {
    int64 file_paths_cost = 0;
    for (int i = 0; i < num_files; i++) {
      if (files[i].type == type &&
          GURL(files[i].origin_url) == GURL(origin_url)) {
        base::FilePath path = base::FilePath().AppendASCII(files[i].name);
        if (!path.empty()) {
          file_paths_cost += fileapi::ObfuscatedFileUtil::ComputeFilePathCost(
              path);
        }
      }
    }
    return file_paths_cost;
  }

  void DeleteOriginData(FileSystemQuotaClient* quota_client,
                        const std::string& origin,
                        quota::StorageType type) {
    deletion_status_ = quota::kQuotaStatusUnknown;
    quota_client->DeleteOriginData(
        GURL(origin), type,
        base::Bind(&FileSystemQuotaClientTest::OnDeleteOrigin,
                   weak_factory_.GetWeakPtr()));
  }

  int64 usage() const { return usage_; }
  quota::QuotaStatusCode status() { return deletion_status_; }
  int additional_callback_count() const { return additional_callback_count_; }
  void set_additional_callback_count(int count) {
    additional_callback_count_ = count;
  }

 private:
  void OnGetUsage(int64 usage) {
    usage_ = usage;
  }

  void OnGetOrigins(const std::set<GURL>& origins) {
    origins_ = origins;
  }

  void OnGetAdditionalUsage(int64 usage_unused) {
    ++additional_callback_count_;
  }

  void OnDeleteOrigin(quota::QuotaStatusCode status) {
    deletion_status_ = status;
  }

  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  base::WeakPtrFactory<FileSystemQuotaClientTest> weak_factory_;
  int64 usage_;
  int additional_callback_count_;
  std::set<GURL> origins_;
  quota::QuotaStatusCode deletion_status_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClientTest);
};

TEST_F(FileSystemQuotaClientTest, NoFileSystemTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));

  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
}

TEST_F(FileSystemQuotaClientTest, NoFileTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, OneFileTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 4921, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + file_paths_cost,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, TwoFilesTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 10310, kDummyURL1, kTemporary},
    {false, "bar", 41, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + file_paths_cost,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, EmptyFilesTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 0, kDummyURL1, kTemporary},
    {false, "bar", 0, kDummyURL1, kTemporary},
    {false, "baz", 0, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(file_paths_cost,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, SubDirectoryTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 11921, kDummyURL1, kTemporary},
    {false, "bar", 4814, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + file_paths_cost,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiTypeTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 133, kDummyURL1, kTemporary},
    {false, "bar", 14, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL1, kPersistent},
    {true, "dirtest", 0, kDummyURL1, kPersistent},
    {false, "dirtest/foo", 193, kDummyURL1, kPersistent},
    {false, "bar", 9, kDummyURL1, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost_temporary = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);
  const int64 file_paths_cost_persistent = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(133 + 14 + file_paths_cost_temporary,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(193 + 9 + file_paths_cost_persistent,
        GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiDomainTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dir1", 0, kDummyURL1, kTemporary},
    {false, "dir1/foo", 1331, kDummyURL1, kTemporary},
    {false, "bar", 134, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL1, kPersistent},
    {true, "dir2", 0, kDummyURL1, kPersistent},
    {false, "dir2/foo", 1903, kDummyURL1, kPersistent},
    {false, "bar", 19, kDummyURL1, kPersistent},
    {true, NULL, 0, kDummyURL2, kTemporary},
    {true, "dom", 0, kDummyURL2, kTemporary},
    {false, "dom/fan", 1319, kDummyURL2, kTemporary},
    {false, "bar", 113, kDummyURL2, kTemporary},
    {true, NULL, 0, kDummyURL2, kPersistent},
    {true, "dom", 0, kDummyURL2, kPersistent},
    {false, "dom/fan", 2013, kDummyURL2, kPersistent},
    {false, "baz", 18, kDummyURL2, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost_temporary1 = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);
  const int64 file_paths_cost_persistent1 =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          kDummyURL1, kPersistent);
  const int64 file_paths_cost_temporary2 = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL2, kTemporary);
  const int64 file_paths_cost_persistent2 =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          kDummyURL2, kPersistent);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(1331 + 134 + file_paths_cost_temporary1,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(1903 + 19 + file_paths_cost_persistent1,
        GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
    EXPECT_EQ(1319 + 113 + file_paths_cost_temporary2,
        GetOriginUsage(quota_client.get(), kDummyURL2, kTemporary));
    EXPECT_EQ(2013 + 18 + file_paths_cost_persistent2,
        GetOriginUsage(quota_client.get(), kDummyURL2, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, GetUsage_MultipleTasks) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo",   11, kDummyURL1, kTemporary},
    {false, "bar",   22, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, ARRAYSIZE_UNSAFE(kFiles), kDummyURL1, kTemporary);

  // Dispatching three GetUsage tasks.
  set_additional_callback_count(0);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());

  // Once more, in a different order.
  set_additional_callback_count(0);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForType) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL2, kTemporary},
    {true, NULL, 0, kDummyURL3, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));

  std::set<GURL> origins = GetOriginsForType(quota_client.get(), kTemporary);
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(origins.find(GURL(kDummyURL1)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kDummyURL2)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kDummyURL3)) == origins.end());
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForHost) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const char* kURL1 = "http://foo.com/";
  const char* kURL2 = "https://foo.com/";
  const char* kURL3 = "http://foo.com:1/";
  const char* kURL4 = "http://foo2.com/";
  const char* kURL5 = "http://foo.com:2/";
  const TestFile kFiles[] = {
    {true, NULL, 0, kURL1, kTemporary},
    {true, NULL, 0, kURL2, kTemporary},
    {true, NULL, 0, kURL3, kTemporary},
    {true, NULL, 0, kURL4, kTemporary},
    {true, NULL, 0, kURL5, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));

  std::set<GURL> origins = GetOriginsForHost(
      quota_client.get(), kTemporary, "foo.com");
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(origins.find(GURL(kURL1)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL2)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL3)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL4)) == origins.end());  // Different host.
  EXPECT_TRUE(origins.find(GURL(kURL5)) == origins.end());  // Different type.
}

TEST_F(FileSystemQuotaClientTest, IncognitoTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(true));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 10, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));

  // Having files in the usual directory wouldn't affect the result
  // queried in incognito mode.
  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));

  std::set<GURL> origins = GetOriginsForType(quota_client.get(), kTemporary);
  EXPECT_EQ(0U, origins.size());
  origins = GetOriginsForHost(quota_client.get(), kTemporary, "www.dummy.org");
  EXPECT_EQ(0U, origins.size());
}

TEST_F(FileSystemQuotaClientTest, DeleteOriginTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL,  0, "http://foo.com/",  kTemporary},
    {false, "a",  1, "http://foo.com/",  kTemporary},
    {true, NULL,  0, "https://foo.com/", kTemporary},
    {false, "b",  2, "https://foo.com/", kTemporary},
    {true, NULL,  0, "http://foo.com/",  kPersistent},
    {false, "c",  4, "http://foo.com/",  kPersistent},
    {true, NULL,  0, "http://bar.com/",  kTemporary},
    {false, "d",  8, "http://bar.com/",  kTemporary},
    {true, NULL,  0, "http://bar.com/",  kPersistent},
    {false, "e", 16, "http://bar.com/",  kPersistent},
    {true, NULL,  0, "https://bar.com/", kPersistent},
    {false, "f", 32, "https://bar.com/", kPersistent},
    {true, NULL,  0, "https://bar.com/", kTemporary},
    {false, "g", 64, "https://bar.com/", kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, ARRAYSIZE_UNSAFE(kFiles));
  const int64 file_paths_cost_temporary_foo_https =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          "https://foo.com/", kTemporary);
  const int64 file_paths_cost_persistent_foo =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          "http://foo.com/", kPersistent);
  const int64 file_paths_cost_temporary_bar =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          "http://bar.com/", kTemporary);
  const int64 file_paths_cost_temporary_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          "https://bar.com/", kTemporary);
  const int64 file_paths_cost_persistent_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, ARRAYSIZE_UNSAFE(kFiles),
          "https://bar.com/", kPersistent);

  DeleteOriginData(quota_client.get(), "http://foo.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  DeleteOriginData(quota_client.get(), "http://bar.com/", kPersistent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  DeleteOriginData(quota_client.get(), "http://buz.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://foo.com/", kTemporary));
  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://bar.com/", kPersistent));
  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://buz.com/", kTemporary));

  EXPECT_EQ(2 + file_paths_cost_temporary_foo_https,
            GetOriginUsage(quota_client.get(),
                           "https://foo.com/",
                           kTemporary));
  EXPECT_EQ(4 + file_paths_cost_persistent_foo,
            GetOriginUsage(quota_client.get(),
                           "http://foo.com/",
                           kPersistent));
  EXPECT_EQ(8 + file_paths_cost_temporary_bar,
            GetOriginUsage(quota_client.get(),
                           "http://bar.com/",
                           kTemporary));
  EXPECT_EQ(32 + file_paths_cost_persistent_bar_https,
            GetOriginUsage(quota_client.get(),
                           "https://bar.com/",
                           kPersistent));
  EXPECT_EQ(64 + file_paths_cost_temporary_bar_https,
            GetOriginUsage(quota_client.get(),
                           "https://bar.com/",
                           kTemporary));
}

}  // namespace content
