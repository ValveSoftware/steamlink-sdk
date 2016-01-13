// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "net/disk_cache/blockfile/file_block.h"
#include "net/disk_cache/blockfile/mapped_file.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Implementation of FileIOCallback for the tests.
class FileCallbackTest: public disk_cache::FileIOCallback {
 public:
  FileCallbackTest(int id, MessageLoopHelper* helper, int* max_id)
      : id_(id),
        helper_(helper),
        max_id_(max_id) {
  }
  virtual ~FileCallbackTest() {}

  virtual void OnFileIOComplete(int bytes_copied) OVERRIDE;

 private:
  int id_;
  MessageLoopHelper* helper_;
  int* max_id_;
};

void FileCallbackTest::OnFileIOComplete(int bytes_copied) {
  if (id_ > *max_id_) {
    NOTREACHED();
    helper_->set_callback_reused_error(true);
  }

  helper_->CallbackWasCalled();
}

class TestFileBlock : public disk_cache::FileBlock {
 public:
  TestFileBlock() {
    CacheTestFillBuffer(buffer_, sizeof(buffer_), false);
  }
  virtual ~TestFileBlock() {}

  // FileBlock interface.
  virtual void* buffer() const OVERRIDE { return const_cast<char*>(buffer_); }
  virtual size_t size() const OVERRIDE { return sizeof(buffer_); }
  virtual int offset() const OVERRIDE { return 1024; }

 private:
  char buffer_[20];
};

}  // namespace

TEST_F(DiskCacheTest, MappedFile_SyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 8192));
  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 8192));
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_AsyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  int max_id = 0;
  MessageLoopHelper helper;
  FileCallbackTest callback(1, &helper, &max_id);

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  bool completed;
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 1024 * 1024, &callback,
              &completed));
  int expected = completed ? 0 : 1;

  max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 1024 * 1024, &callback,
              &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, helper.callbacks_called());
  EXPECT_FALSE(helper.callback_reused_error());
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_AsyncLoadStore) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  int max_id = 0;
  MessageLoopHelper helper;
  FileCallbackTest callback(1, &helper, &max_id);

  TestFileBlock file_block1;
  TestFileBlock file_block2;
  base::strlcpy(static_cast<char*>(file_block1.buffer()), "the data",
                file_block1.size());
  bool completed;
  EXPECT_TRUE(file->Store(&file_block1, &callback, &completed));
  int expected = completed ? 0 : 1;

  max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Load(&file_block2, &callback, &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, helper.callbacks_called());
  EXPECT_FALSE(helper.callback_reused_error());
  EXPECT_STREQ(static_cast<char*>(file_block1.buffer()),
               static_cast<char*>(file_block2.buffer()));
}
