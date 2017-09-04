// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_disk_cache.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AppCacheDiskCacheTest : public testing::Test {
 public:
  AppCacheDiskCacheTest() {}

  void SetUp() override {
    // Use the current thread for the DiskCache's cache_thread.
    message_loop_.reset(new base::MessageLoopForIO());
    cache_thread_ = base::ThreadTaskRunnerHandle::Get();
    ASSERT_TRUE(directory_.CreateUniqueTempDir());
    completion_callback_ = base::Bind(
        &AppCacheDiskCacheTest::OnComplete,
        base::Unretained(this));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    message_loop_.reset(NULL);
  }

  void FlushCacheTasks() {
    base::RunLoop().RunUntilIdle();
  }

  void OnComplete(int err) {
    completion_results_.push_back(err);
  }

  base::ScopedTempDir directory_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;
  net::CompletionCallback completion_callback_;
  std::vector<int> completion_results_;

  static const int k10MBytes = 10 * 1024 * 1024;
};

TEST_F(AppCacheDiskCacheTest, DisablePriorToInitCompletion) {
  AppCacheDiskCache::Entry* entry = NULL;

  // Create an instance and start it initializing, queue up
  // one of each kind of "entry" function.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  cache_thread_, completion_callback_);
  disk_cache->CreateEntry(1, &entry, completion_callback_);
  disk_cache->OpenEntry(2, &entry, completion_callback_);
  disk_cache->DoomEntry(3, completion_callback_);

  // Pull the plug on all that.
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->Disable();
  EXPECT_TRUE(disk_cache->is_disabled());

  FlushCacheTasks();

  EXPECT_EQ(NULL, entry);
  EXPECT_EQ(4u, completion_results_.size());
  for (std::vector<int>::const_iterator iter = completion_results_.begin();
       iter < completion_results_.end(); ++iter) {
    EXPECT_EQ(net::ERR_ABORTED, *iter);
  }

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));
}

TEST_F(AppCacheDiskCacheTest, DisableAfterInitted) {
  // Create an instance and let it fully init.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  cache_thread_, completion_callback_);
  FlushCacheTasks();
  EXPECT_EQ(1u, completion_results_.size());
  EXPECT_EQ(net::OK, completion_results_[0]);

  // Pull the plug
  disk_cache->Disable();
  FlushCacheTasks();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));

  // Methods should return immediately when disabled and not invoke
  // the callback at all.
  AppCacheDiskCache::Entry* entry = NULL;
  completion_results_.clear();
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->CreateEntry(1, &entry, completion_callback_));
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->OpenEntry(2, &entry, completion_callback_));
  EXPECT_EQ(net::ERR_ABORTED,
            disk_cache->DoomEntry(3, completion_callback_));
  FlushCacheTasks();
  EXPECT_TRUE(completion_results_.empty());
}

// Flaky on Android: http://crbug.com/339534
TEST_F(AppCacheDiskCacheTest, DISABLED_DisableWithEntriesOpen) {
  // Create an instance and let it fully init.
  std::unique_ptr<AppCacheDiskCache> disk_cache(new AppCacheDiskCache);
  EXPECT_FALSE(disk_cache->is_disabled());
  disk_cache->InitWithDiskBackend(directory_.GetPath(), k10MBytes, false,
                                  cache_thread_, completion_callback_);
  FlushCacheTasks();
  EXPECT_EQ(1u, completion_results_.size());
  EXPECT_EQ(net::OK, completion_results_[0]);

  // Note: We don't have detailed expectations of the DiskCache
  // operations because on android it's really SimpleCache which
  // does behave differently.
  //
  // What matters for the corruption handling and service reinitiazation
  // is that the directory can be deleted after the calling Disable() method,
  // and we do have expectations about that.

  // Create/open some entries.
  AppCacheDiskCache::Entry* entry1 = NULL;
  AppCacheDiskCache::Entry* entry2 = NULL;
  disk_cache->CreateEntry(1, &entry1, completion_callback_);
  disk_cache->CreateEntry(2, &entry2, completion_callback_);
  FlushCacheTasks();
  EXPECT_TRUE(entry1);
  EXPECT_TRUE(entry2);

  // Write something to one of the entries and flush it.
  const char* kData = "Hello";
  const int kDataLen = strlen(kData) + 1;
  scoped_refptr<net::IOBuffer> write_buf(new net::WrappedIOBuffer(kData));
  entry1->Write(0, 0, write_buf.get(), kDataLen, completion_callback_);
  FlushCacheTasks();

  // Queue up a read and a write.
  scoped_refptr<net::IOBuffer> read_buf = new net::IOBuffer(kDataLen);
  entry1->Read(0, 0, read_buf.get(), kDataLen, completion_callback_);
  entry2->Write(0, 0, write_buf.get(), kDataLen, completion_callback_);

  // Pull the plug
  disk_cache->Disable();
  FlushCacheTasks();

  // Ensure the directory can be deleted at this point.
  EXPECT_TRUE(base::DirectoryExists(directory_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(directory_.GetPath()));
  EXPECT_TRUE(base::DeleteFile(directory_.GetPath(), true));
  EXPECT_FALSE(base::DirectoryExists(directory_.GetPath()));

  disk_cache.reset(NULL);

  // Also, new IO operations should fail immediately.
  EXPECT_EQ(
      net::ERR_ABORTED,
      entry1->Read(0, 0, read_buf.get(), kDataLen, completion_callback_));
  entry1->Close();
  entry2->Close();

  FlushCacheTasks();
}

}  // namespace content
