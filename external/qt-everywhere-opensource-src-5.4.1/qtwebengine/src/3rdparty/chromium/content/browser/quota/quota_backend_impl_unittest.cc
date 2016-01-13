// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_backend_impl.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

using fileapi::FileSystemUsageCache;
using fileapi::ObfuscatedFileUtil;
using fileapi::QuotaBackendImpl;
using fileapi::SandboxFileSystemBackendDelegate;

namespace content {

namespace {

const char kOrigin[] = "http://example.com";

bool DidReserveQuota(bool accepted,
                     base::File::Error* error_out,
                     int64* delta_out,
                     base::File::Error error,
                     int64 delta) {
  DCHECK(error_out);
  DCHECK(delta_out);
  *error_out = error;
  *delta_out = delta;
  return accepted;
}

class MockQuotaManagerProxy : public quota::QuotaManagerProxy {
 public:
  MockQuotaManagerProxy()
      : QuotaManagerProxy(NULL, NULL),
        storage_modified_count_(0),
        usage_(0), quota_(0) {}

  // We don't mock them.
  virtual void NotifyOriginInUse(const GURL& origin) OVERRIDE {}
  virtual void NotifyOriginNoLongerInUse(const GURL& origin) OVERRIDE {}
  virtual void SetUsageCacheEnabled(quota::QuotaClient::ID client_id,
                                    const GURL& origin,
                                    quota::StorageType type,
                                    bool enabled) OVERRIDE {}

  virtual void NotifyStorageModified(
      quota::QuotaClient::ID client_id,
      const GURL& origin,
      quota::StorageType type,
      int64 delta) OVERRIDE {
    ++storage_modified_count_;
    usage_ += delta;
    ASSERT_LE(usage_, quota_);
  }

  virtual void GetUsageAndQuota(
      base::SequencedTaskRunner* original_task_runner,
      const GURL& origin,
      quota::StorageType type,
      const GetUsageAndQuotaCallback& callback) OVERRIDE {
    callback.Run(quota::kQuotaStatusOk, usage_, quota_);
  }

  int storage_modified_count() { return storage_modified_count_; }
  int64 usage() { return usage_; }
  void set_usage(int64 usage) { usage_ = usage; }
  void set_quota(int64 quota) { quota_ = quota; }

 protected:
  virtual ~MockQuotaManagerProxy() {}

 private:
  int storage_modified_count_;
  int64 usage_;
  int64 quota_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerProxy);
};

}  // namespace

class QuotaBackendImplTest : public testing::Test {
 public:
  QuotaBackendImplTest()
      : file_system_usage_cache_(file_task_runner()),
        quota_manager_proxy_(new MockQuotaManagerProxy) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    file_util_.reset(ObfuscatedFileUtil::CreateForTesting(
        NULL, data_dir_.path(), in_memory_env_.get(), file_task_runner()));
    backend_.reset(new QuotaBackendImpl(file_task_runner(),
                                        file_util_.get(),
                                        &file_system_usage_cache_,
                                        quota_manager_proxy_.get()));
  }

  virtual void TearDown() OVERRIDE {
    backend_.reset();
    quota_manager_proxy_ = NULL;
    file_util_.reset();
    message_loop_.RunUntilIdle();
  }

 protected:
  void InitializeForOriginAndType(const GURL& origin,
                                  fileapi::FileSystemType type) {
    ASSERT_TRUE(file_util_->InitOriginDatabase(origin, true /* create */));
    ASSERT_TRUE(file_util_->origin_database_ != NULL);

    std::string type_string =
        SandboxFileSystemBackendDelegate::GetTypeString(type);
    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::FilePath path = file_util_->GetDirectoryForOriginAndType(
        origin, type_string, true /* create */, &error);
    ASSERT_EQ(base::File::FILE_OK, error);

    ASSERT_TRUE(file_system_usage_cache_.UpdateUsage(
        GetUsageCachePath(origin, type), 0));
  }

  base::SequencedTaskRunner* file_task_runner() {
    return base::MessageLoopProxy::current().get();
  }

  base::FilePath GetUsageCachePath(const GURL& origin,
                                   fileapi::FileSystemType type) {
    base::FilePath path;
    base::File::Error error =
        backend_->GetUsageCachePath(origin, type, &path);
    EXPECT_EQ(base::File::FILE_OK, error);
    EXPECT_FALSE(path.empty());
    return path;
  }

  base::MessageLoop message_loop_;
  base::ScopedTempDir data_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;
  scoped_ptr<ObfuscatedFileUtil> file_util_;
  FileSystemUsageCache file_system_usage_cache_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<QuotaBackendImpl> backend_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaBackendImplTest);
};

TEST_F(QuotaBackendImplTest, ReserveQuota_Basic) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  quota_manager_proxy_->set_quota(10000);

  int64 delta = 0;

  const int64 kDelta1 = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(GURL(kOrigin), type, kDelta1,
                         base::Bind(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta1, delta);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());

  const int64 kDelta2 = -300;
  error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(GURL(kOrigin), type, kDelta2,
                         base::Bind(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta2, delta);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReserveQuota_NoSpace) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  quota_manager_proxy_->set_quota(100);

  int64 delta = 0;

  const int64 kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(GURL(kOrigin), type, kDelta,
                         base::Bind(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(100, delta);
  EXPECT_EQ(100, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReserveQuota_Revert) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  quota_manager_proxy_->set_quota(10000);

  int64 delta = 0;

  const int64 kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(GURL(kOrigin), type, kDelta,
                         base::Bind(&DidReserveQuota, false, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta, delta);
  EXPECT_EQ(0, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReleaseReservedQuota) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  const int64 kInitialUsage = 2000;
  quota_manager_proxy_->set_usage(kInitialUsage);
  quota_manager_proxy_->set_quota(10000);

  const int64 kSize = 1000;
  backend_->ReleaseReservedQuota(GURL(kOrigin), type, kSize);
  EXPECT_EQ(kInitialUsage - kSize, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, CommitQuotaUsage) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  quota_manager_proxy_->set_quota(10000);
  base::FilePath path = GetUsageCachePath(GURL(kOrigin), type);

  const int64 kDelta1 = 1000;
  backend_->CommitQuotaUsage(GURL(kOrigin), type, kDelta1);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());
  int64 usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1, usage);

  const int64 kDelta2 = -300;
  backend_->CommitQuotaUsage(GURL(kOrigin), type, kDelta2);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());
  usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1 + kDelta2, usage);

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, DirtyCount) {
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  InitializeForOriginAndType(GURL(kOrigin), type);
  base::FilePath path = GetUsageCachePath(GURL(kOrigin), type);

  backend_->IncrementDirtyCount(GURL(kOrigin), type);
  uint32 dirty = 0;
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(1u, dirty);

  backend_->DecrementDirtyCount(GURL(kOrigin), type);
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(0u, dirty);
}

}  // namespace content
