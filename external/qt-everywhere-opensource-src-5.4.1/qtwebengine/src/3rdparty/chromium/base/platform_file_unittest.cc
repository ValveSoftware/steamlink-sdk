// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Reads from a file the given number of bytes, or until EOF is reached.
// Returns the number of bytes read.
int ReadFully(PlatformFile file, int64 offset, char* data, int size) {
  return ReadPlatformFile(file, offset, data, size);
}

// Writes the given number of bytes to a file.
// Returns the number of bytes written.
int WriteFully(PlatformFile file, int64 offset,
               const char* data, int size) {
  return WritePlatformFile(file, offset, data, size);
}

PlatformFile CreatePlatformFile(const FilePath& path,
                                int flags,
                                bool* created,
                                PlatformFileError* error) {
  File file(path, flags);
  if (!file.IsValid()) {
    if (error)
      *error = static_cast<PlatformFileError>(file.error_details());
    return kInvalidPlatformFileValue;
  }

  if (created)
    *created = file.created();

  if (error)
    *error = PLATFORM_FILE_OK;

  return file.TakePlatformFile();
}

} // namespace

TEST(PlatformFile, CreatePlatformFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.path().AppendASCII("create_file_1");

  // Open a file that doesn't exist.
  PlatformFileError error_code = PLATFORM_FILE_OK;
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
      NULL,
      &error_code);
  EXPECT_EQ(kInvalidPlatformFileValue, file);
  EXPECT_EQ(PLATFORM_FILE_ERROR_NOT_FOUND, error_code);

  // Open or create a file.
  bool created = false;
  error_code = PLATFORM_FILE_OK;
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN_ALWAYS | PLATFORM_FILE_READ,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_TRUE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);
  ClosePlatformFile(file);

  // Open an existing file.
  created = false;
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_FALSE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);
  ClosePlatformFile(file);

  // Create a file that exists.
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE | PLATFORM_FILE_READ,
      &created,
      &error_code);
  EXPECT_EQ(kInvalidPlatformFileValue, file);
  EXPECT_FALSE(created);
  EXPECT_EQ(PLATFORM_FILE_ERROR_EXISTS, error_code);

  // Create or overwrite a file.
  error_code = PLATFORM_FILE_OK;
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE_ALWAYS | PLATFORM_FILE_WRITE,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_TRUE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);
  ClosePlatformFile(file);

  // Create a delete-on-close file.
  created = false;
  file_path = temp_dir.path().AppendASCII("create_file_2");
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN_ALWAYS | PLATFORM_FILE_DELETE_ON_CLOSE |
          PLATFORM_FILE_READ,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_TRUE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);

  EXPECT_TRUE(ClosePlatformFile(file));
  EXPECT_FALSE(PathExists(file_path));
}

TEST(PlatformFile, DeleteOpenFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.path().AppendASCII("create_file_1");

  // Create a file.
  bool created = false;
  PlatformFileError error_code = PLATFORM_FILE_OK;
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN_ALWAYS | PLATFORM_FILE_READ |
          PLATFORM_FILE_SHARE_DELETE,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_TRUE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);

  // Open an existing file and mark it as delete on close.
  created = false;
  PlatformFile same_file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN | PLATFORM_FILE_DELETE_ON_CLOSE |
          PLATFORM_FILE_READ,
      &created,
      &error_code);
  EXPECT_NE(kInvalidPlatformFileValue, file);
  EXPECT_FALSE(created);
  EXPECT_EQ(PLATFORM_FILE_OK, error_code);

  // Close both handles and check that the file is gone.
  ClosePlatformFile(file);
  ClosePlatformFile(same_file);
  EXPECT_FALSE(PathExists(file_path));
}

TEST(PlatformFile, ReadWritePlatformFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.path().AppendASCII("read_write_file");
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE | PLATFORM_FILE_READ |
          PLATFORM_FILE_WRITE,
      NULL,
      NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  char data_to_write[] = "test";
  const int kTestDataSize = 4;

  // Write 0 bytes to the file.
  int bytes_written = WriteFully(file, 0, data_to_write, 0);
  EXPECT_EQ(0, bytes_written);

  // Write "test" to the file.
  bytes_written = WriteFully(file, 0, data_to_write, kTestDataSize);
  EXPECT_EQ(kTestDataSize, bytes_written);

  // Read from EOF.
  char data_read_1[32];
  int bytes_read = ReadFully(file, kTestDataSize, data_read_1, kTestDataSize);
  EXPECT_EQ(0, bytes_read);

  // Read from somewhere in the middle of the file.
  const int kPartialReadOffset = 1;
  bytes_read = ReadFully(file, kPartialReadOffset, data_read_1, kTestDataSize);
  EXPECT_EQ(kTestDataSize - kPartialReadOffset, bytes_read);
  for (int i = 0; i < bytes_read; i++)
    EXPECT_EQ(data_to_write[i + kPartialReadOffset], data_read_1[i]);

  // Read 0 bytes.
  bytes_read = ReadFully(file, 0, data_read_1, 0);
  EXPECT_EQ(0, bytes_read);

  // Read the entire file.
  bytes_read = ReadFully(file, 0, data_read_1, kTestDataSize);
  EXPECT_EQ(kTestDataSize, bytes_read);
  for (int i = 0; i < bytes_read; i++)
    EXPECT_EQ(data_to_write[i], data_read_1[i]);

  // Read again, but using the trivial native wrapper.
  bytes_read = ReadPlatformFileNoBestEffort(file, 0, data_read_1,
                                                  kTestDataSize);
  EXPECT_LE(bytes_read, kTestDataSize);
  for (int i = 0; i < bytes_read; i++)
    EXPECT_EQ(data_to_write[i], data_read_1[i]);

  // Write past the end of the file.
  const int kOffsetBeyondEndOfFile = 10;
  const int kPartialWriteLength = 2;
  bytes_written = WriteFully(file, kOffsetBeyondEndOfFile,
                             data_to_write, kPartialWriteLength);
  EXPECT_EQ(kPartialWriteLength, bytes_written);

  // Make sure the file was extended.
  int64 file_size = 0;
  EXPECT_TRUE(GetFileSize(file_path, &file_size));
  EXPECT_EQ(kOffsetBeyondEndOfFile + kPartialWriteLength, file_size);

  // Make sure the file was zero-padded.
  char data_read_2[32];
  bytes_read = ReadFully(file, 0, data_read_2, static_cast<int>(file_size));
  EXPECT_EQ(file_size, bytes_read);
  for (int i = 0; i < kTestDataSize; i++)
    EXPECT_EQ(data_to_write[i], data_read_2[i]);
  for (int i = kTestDataSize; i < kOffsetBeyondEndOfFile; i++)
    EXPECT_EQ(0, data_read_2[i]);
  for (int i = kOffsetBeyondEndOfFile; i < file_size; i++)
    EXPECT_EQ(data_to_write[i - kOffsetBeyondEndOfFile], data_read_2[i]);

  // Close the file handle to allow the temp directory to be deleted.
  ClosePlatformFile(file);
}

TEST(PlatformFile, AppendPlatformFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.path().AppendASCII("append_file");
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE | PLATFORM_FILE_APPEND,
      NULL,
      NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  char data_to_write[] = "test";
  const int kTestDataSize = 4;

  // Write 0 bytes to the file.
  int bytes_written = WriteFully(file, 0, data_to_write, 0);
  EXPECT_EQ(0, bytes_written);

  // Write "test" to the file.
  bytes_written = WriteFully(file, 0, data_to_write, kTestDataSize);
  EXPECT_EQ(kTestDataSize, bytes_written);

  ClosePlatformFile(file);
  file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_OPEN | PLATFORM_FILE_READ |
          PLATFORM_FILE_APPEND,
      NULL,
      NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  char append_data_to_write[] = "78";
  const int kAppendDataSize = 2;

  // Append "78" to the file.
  bytes_written = WriteFully(file, 0, append_data_to_write, kAppendDataSize);
  EXPECT_EQ(kAppendDataSize, bytes_written);

  // Read the entire file.
  char data_read_1[32];
  int bytes_read = ReadFully(file, 0, data_read_1,
                             kTestDataSize + kAppendDataSize);
  EXPECT_EQ(kTestDataSize + kAppendDataSize, bytes_read);
  for (int i = 0; i < kTestDataSize; i++)
    EXPECT_EQ(data_to_write[i], data_read_1[i]);
  for (int i = 0; i < kAppendDataSize; i++)
    EXPECT_EQ(append_data_to_write[i], data_read_1[kTestDataSize + i]);

  // Close the file handle to allow the temp directory to be deleted.
  ClosePlatformFile(file);
}


TEST(PlatformFile, TruncatePlatformFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.path().AppendASCII("truncate_file");
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE | PLATFORM_FILE_READ |
          PLATFORM_FILE_WRITE,
      NULL,
      NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  // Write "test" to the file.
  char data_to_write[] = "test";
  int kTestDataSize = 4;
  int bytes_written = WriteFully(file, 0, data_to_write, kTestDataSize);
  EXPECT_EQ(kTestDataSize, bytes_written);

  // Extend the file.
  const int kExtendedFileLength = 10;
  int64 file_size = 0;
  EXPECT_TRUE(TruncatePlatformFile(file, kExtendedFileLength));
  EXPECT_TRUE(GetFileSize(file_path, &file_size));
  EXPECT_EQ(kExtendedFileLength, file_size);

  // Make sure the file was zero-padded.
  char data_read[32];
  int bytes_read = ReadFully(file, 0, data_read, static_cast<int>(file_size));
  EXPECT_EQ(file_size, bytes_read);
  for (int i = 0; i < kTestDataSize; i++)
    EXPECT_EQ(data_to_write[i], data_read[i]);
  for (int i = kTestDataSize; i < file_size; i++)
    EXPECT_EQ(0, data_read[i]);

  // Truncate the file.
  const int kTruncatedFileLength = 2;
  EXPECT_TRUE(TruncatePlatformFile(file, kTruncatedFileLength));
  EXPECT_TRUE(GetFileSize(file_path, &file_size));
  EXPECT_EQ(kTruncatedFileLength, file_size);

  // Make sure the file was truncated.
  bytes_read = ReadFully(file, 0, data_read, kTestDataSize);
  EXPECT_EQ(file_size, bytes_read);
  for (int i = 0; i < file_size; i++)
    EXPECT_EQ(data_to_write[i], data_read[i]);

  // Close the file handle to allow the temp directory to be deleted.
  ClosePlatformFile(file);
}

// Flakily fails: http://crbug.com/86494
#if defined(OS_ANDROID)
TEST(PlatformFile, TouchGetInfoPlatformFile) {
#else
TEST(PlatformFile, DISABLED_TouchGetInfoPlatformFile) {
#endif
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  PlatformFile file = CreatePlatformFile(
      temp_dir.path().AppendASCII("touch_get_info_file"),
      PLATFORM_FILE_CREATE | PLATFORM_FILE_WRITE |
          PLATFORM_FILE_WRITE_ATTRIBUTES,
      NULL,
      NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  // Get info for a newly created file.
  PlatformFileInfo info;
  EXPECT_TRUE(GetPlatformFileInfo(file, &info));

  // Add 2 seconds to account for possible rounding errors on
  // filesystems that use a 1s or 2s timestamp granularity.
  Time now = Time::Now() + TimeDelta::FromSeconds(2);
  EXPECT_EQ(0, info.size);
  EXPECT_FALSE(info.is_directory);
  EXPECT_FALSE(info.is_symbolic_link);
  EXPECT_LE(info.last_accessed.ToInternalValue(), now.ToInternalValue());
  EXPECT_LE(info.last_modified.ToInternalValue(), now.ToInternalValue());
  EXPECT_LE(info.creation_time.ToInternalValue(), now.ToInternalValue());
  Time creation_time = info.creation_time;

  // Write "test" to the file.
  char data[] = "test";
  const int kTestDataSize = 4;
  int bytes_written = WriteFully(file, 0, data, kTestDataSize);
  EXPECT_EQ(kTestDataSize, bytes_written);

  // Change the last_accessed and last_modified dates.
  // It's best to add values that are multiples of 2 (in seconds)
  // to the current last_accessed and last_modified times, because
  // FATxx uses a 2s timestamp granularity.
  Time new_last_accessed =
      info.last_accessed + TimeDelta::FromSeconds(234);
  Time new_last_modified =
      info.last_modified + TimeDelta::FromMinutes(567);

  EXPECT_TRUE(TouchPlatformFile(file, new_last_accessed, new_last_modified));

  // Make sure the file info was updated accordingly.
  EXPECT_TRUE(GetPlatformFileInfo(file, &info));
  EXPECT_EQ(info.size, kTestDataSize);
  EXPECT_FALSE(info.is_directory);
  EXPECT_FALSE(info.is_symbolic_link);

  // ext2/ext3 and HPS/HPS+ seem to have a timestamp granularity of 1s.
#if defined(OS_POSIX)
  EXPECT_EQ(info.last_accessed.ToTimeVal().tv_sec,
            new_last_accessed.ToTimeVal().tv_sec);
  EXPECT_EQ(info.last_modified.ToTimeVal().tv_sec,
            new_last_modified.ToTimeVal().tv_sec);
#else
  EXPECT_EQ(info.last_accessed.ToInternalValue(),
            new_last_accessed.ToInternalValue());
  EXPECT_EQ(info.last_modified.ToInternalValue(),
            new_last_modified.ToInternalValue());
#endif

  EXPECT_EQ(info.creation_time.ToInternalValue(),
            creation_time.ToInternalValue());

  // Close the file handle to allow the temp directory to be deleted.
  ClosePlatformFile(file);
}

TEST(PlatformFile, ReadFileAtCurrentPosition) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path =
      temp_dir.path().AppendASCII("read_file_at_current_position");
  PlatformFile file = CreatePlatformFile(
      file_path,
      PLATFORM_FILE_CREATE | PLATFORM_FILE_READ |
          PLATFORM_FILE_WRITE,
      NULL, NULL);
  EXPECT_NE(kInvalidPlatformFileValue, file);

  const char kData[] = "test";
  const int kDataSize = arraysize(kData) - 1;
  EXPECT_EQ(kDataSize, WriteFully(file, 0, kData, kDataSize));

  EXPECT_EQ(0, SeekPlatformFile(file, PLATFORM_FILE_FROM_BEGIN, 0));

  char buffer[kDataSize];
  int first_chunk_size = kDataSize / 2;
  EXPECT_EQ(first_chunk_size,
            ReadPlatformFileAtCurrentPos(
                file, buffer, first_chunk_size));
  EXPECT_EQ(kDataSize - first_chunk_size,
            ReadPlatformFileAtCurrentPos(
                file, buffer + first_chunk_size,
                kDataSize - first_chunk_size));
  EXPECT_EQ(std::string(buffer, buffer + kDataSize),
            std::string(kData));

  ClosePlatformFile(file);
}

}  // namespace base
