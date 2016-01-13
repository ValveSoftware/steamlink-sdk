// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/local_file_stream_reader.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using webkit_blob::LocalFileStreamReader;

namespace content {

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

void ReadFromReader(LocalFileStreamReader* reader,
                    std::string* data, size_t size,
                    int* result) {
  ASSERT_TRUE(reader != NULL);
  ASSERT_TRUE(result != NULL);
  *result = net::OK;
  net::TestCompletionCallback callback;
  size_t total_bytes_read = 0;
  while (total_bytes_read < size) {
    scoped_refptr<net::IOBufferWithSize> buf(
        new net::IOBufferWithSize(size - total_bytes_read));
    int rv = reader->Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      *result = rv;
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data->append(buf->data(), rv);
  }
}

void NeverCalled(int) { ADD_FAILURE(); }
void EmptyCallback() {}

void QuitLoop() {
  base::MessageLoop::current()->Quit();
}

}  // namespace

class LocalFileStreamReaderTest : public testing::Test {
 public:
  LocalFileStreamReaderTest()
      : file_thread_("FileUtilProxyTestFileThread") {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(file_thread_.Start());
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    base::WriteFile(test_path(), kTestData, kTestDataSize);
    base::File::Info info;
    ASSERT_TRUE(base::GetFileInfo(test_path(), &info));
    test_file_modification_time_ = info.last_modified;
  }

  virtual void TearDown() OVERRIDE {
    // Give another chance for deleted streams to perform Close.
    base::RunLoop().RunUntilIdle();
    file_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  LocalFileStreamReader* CreateFileReader(
      const base::FilePath& path,
      int64 initial_offset,
      const base::Time& expected_modification_time) {
    return new LocalFileStreamReader(
            file_task_runner(),
            path,
            initial_offset,
            expected_modification_time);
  }

  void TouchTestFile() {
    base::Time new_modified_time =
        test_file_modification_time() - base::TimeDelta::FromSeconds(1);
    ASSERT_TRUE(base::TouchFile(test_path(),
                                test_file_modification_time(),
                                new_modified_time));
  }

  base::MessageLoopProxy* file_task_runner() const {
    return file_thread_.message_loop_proxy().get();
  }

  base::FilePath test_dir() const { return dir_.path(); }
  base::FilePath test_path() const { return dir_.path().AppendASCII("test"); }
  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void EnsureFileTaskFinished() {
    file_task_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&EmptyCallback), base::Bind(&QuitLoop));
    base::MessageLoop::current()->Run();
  }

 private:
  base::MessageLoopForIO message_loop_;
  base::Thread file_thread_;
  base::ScopedTempDir dir_;
  base::Time test_file_modification_time_;
};

TEST_F(LocalFileStreamReaderTest, NonExistent) {
  base::FilePath nonexistent_path = test_dir().AppendASCII("nonexistent");
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(nonexistent_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(LocalFileStreamReaderTest, Empty) {
  base::FilePath empty_path = test_dir().AppendASCII("empty");
  base::File file(empty_path, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  file.Close();

  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(empty_path, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  net::TestInt64CompletionCallback callback;
  int64 length_result = reader->GetLength(callback.callback());
  if (length_result == net::ERR_IO_PENDING)
    length_result = callback.WaitForResult();
  ASSERT_EQ(0, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthNormal) {
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect.
  TouchTestFile();

  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);

  // With NULL expected modification time this should work.
  reader.reset(CreateFileReader(test_path(), 0, base::Time()));
  result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, GetLengthWithOffset) {
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(LocalFileStreamReaderTest, ReadNormal) {
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(LocalFileStreamReaderTest, ReadAfterModified) {
  // Touch file so that the file's modification time becomes different
  // from what we expect.
  TouchTestFile();

  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  ASSERT_EQ(0U, data.size());

  // With NULL expected modification time this should work.
  data.clear();
  reader.reset(CreateFileReader(test_path(), 0, base::Time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(LocalFileStreamReaderTest, ReadWithOffset) {
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(LocalFileStreamReaderTest, DeleteWithUnfinishedRead) {
  scoped_ptr<LocalFileStreamReader> reader(
      CreateFileReader(test_path(), 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf(
      new net::IOBufferWithSize(kTestDataSize));
  int rv = reader->Read(buf.get(), buf->size(), base::Bind(&NeverCalled));
  ASSERT_TRUE(rv == net::ERR_IO_PENDING || rv >= 0);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader.reset();
  EnsureFileTaskFinished();
}

}  // namespace content
