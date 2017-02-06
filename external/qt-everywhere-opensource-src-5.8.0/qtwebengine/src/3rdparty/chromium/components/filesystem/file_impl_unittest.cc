// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "components/filesystem/files_test_base.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace filesystem {
namespace {

using FileImplTest = FilesTestBase;

TEST_F(FileImplTest, CreateWriteCloseRenameOpenRead) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagWrite | mojom::kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  // Rename it.
  error = mojom::FileError::FAILED;
  directory->Rename("my_file", "your_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  {
    // Open my_file again.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("your_file", GetProxy(&file),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = mojom::FileError::FAILED;
    file->Read(3, 1, mojom::Whence::FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    ASSERT_EQ(3u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[2]);
  }

  // TODO(vtl): Test read/write offset options.
}

TEST_F(FileImplTest, CantWriteInReadMode) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));

  {
    // Create my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagWrite | mojom::kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Open my_file again, this time with read only mode.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Try to write in read mode; it should fail.
    error = mojom::FileError::OK;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));

    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::FAILED, error);
    EXPECT_EQ(0u, num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }
}

TEST_F(FileImplTest, OpenInAppendMode) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagWrite | mojom::kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Append to my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagAppend | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Open my_file again.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = mojom::FileError::FAILED;
    file->Read(12, 0, mojom::Whence::FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    ASSERT_EQ(12u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[4]);
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read[5]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[6]);
  }
}

TEST_F(FileImplTest, OpenInTruncateMode) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagWrite | mojom::kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Append to my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagWrite | mojom::kFlagOpenTruncated,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = mojom::FileError::FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                mojom::Whence::FROM_CURRENT,
                Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = mojom::FileError::FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Open my_file again.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = mojom::FileError::FAILED;
    file->Read(7, 0, mojom::Whence::FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    ASSERT_EQ(7u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[2]);
    EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read[3]);
  }
}

// Note: Ignore nanoseconds, since it may not always be supported. We expect at
// least second-resolution support though.
TEST_F(FileImplTest, StatTouch) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file),
                      mojom::kFlagWrite | mojom::kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Stat it.
  error = mojom::FileError::FAILED;
  mojom::FileInformationPtr file_info;
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(mojom::FsFileType::REGULAR_FILE, file_info->type);
  EXPECT_EQ(0, file_info->size);
  EXPECT_GT(file_info->atime, 0);  // Expect that it's not 1970-01-01.
  EXPECT_GT(file_info->mtime, 0);
  double first_mtime = file_info->mtime;

  // Touch only the atime.
  error = mojom::FileError::FAILED;
  mojom::TimespecOrNowPtr t(mojom::TimespecOrNow::New());
  t->now = false;
  const int64_t kPartyTime1 = 1234567890;  // Party like it's 2009-02-13.
  t->seconds = kPartyTime1;
  file->Touch(std::move(t), nullptr, Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Stat again.
  error = mojom::FileError::FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(first_mtime, file_info->mtime);

  // Touch only the mtime.
  t = mojom::TimespecOrNow::New();
  t->now = false;
  const int64_t kPartyTime2 = 1425059525;  // No time like the present.
  t->seconds = kPartyTime2;
  file->Touch(nullptr, std::move(t), Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Stat again.
  error = mojom::FileError::FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(kPartyTime2, file_info->mtime);

  // TODO(vtl): Also test non-zero file size.
  // TODO(vtl): Also test Touch() "now" options.
  // TODO(vtl): Also test touching both atime and mtime.
}

TEST_F(FileImplTest, TellSeek) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file),
                      mojom::kFlagWrite | mojom::kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(1000, '!');
  error = mojom::FileError::FAILED;
  uint32_t num_bytes_written = 0;
  file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
              mojom::Whence::FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int size = static_cast<int>(num_bytes_written);

  // Tell.
  error = mojom::FileError::FAILED;
  int64_t position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  // Should be at the end.
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(size, position);

  // Seek back 100.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Seek(-100, mojom::Whence::FROM_CURRENT, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(size - 100, position);

  // Tell.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(size - 100, position);

  // Seek to 123 from start.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Seek(123, mojom::Whence::FROM_BEGIN, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(123, position);

  // Tell.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(123, position);

  // Seek to 123 back from end.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Seek(-123, mojom::Whence::FROM_END, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(size - 123, position);

  // Tell.
  error = mojom::FileError::FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(size - 123, position);

  // TODO(vtl): Check that seeking actually affects reading/writing.
  // TODO(vtl): Check that seeking can extend the file?
}

TEST_F(FileImplTest, Dup) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file1;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file1),
                      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));
  error = mojom::FileError::FAILED;
  uint32_t num_bytes_written = 0;
  file1->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
               mojom::Whence::FROM_CURRENT,
               Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int end_hello_pos = static_cast<int>(num_bytes_written);

  // Dup it.
  mojom::FilePtr file2;
  error = mojom::FileError::FAILED;
  file1->Dup(GetProxy(&file2), Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // |file2| should have the same position.
  error = mojom::FileError::FAILED;
  int64_t position = -1;
  file2->Tell(Capture(&error, &position));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(end_hello_pos, position);

  // Write using |file2|.
  std::vector<uint8_t> more_bytes_to_write;
  more_bytes_to_write.push_back(static_cast<uint8_t>('w'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('o'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('r'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('l'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('d'));
  error = mojom::FileError::FAILED;
  num_bytes_written = 0;
  file2->Write(mojo::Array<uint8_t>::From(more_bytes_to_write), 0,
               mojom::Whence::FROM_CURRENT,
               Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(more_bytes_to_write.size(), num_bytes_written);
  const int end_world_pos = end_hello_pos + static_cast<int>(num_bytes_written);

  // |file1| should have the same position.
  error = mojom::FileError::FAILED;
  position = -1;
  file1->Tell(Capture(&error, &position));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(end_world_pos, position);

  // Close |file1|.
  error = mojom::FileError::FAILED;
  file1->Close(Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Read everything using |file2|.
  mojo::Array<uint8_t> bytes_read;
  error = mojom::FileError::FAILED;
  file2->Read(1000, 0, mojom::Whence::FROM_BEGIN, Capture(&error, &bytes_read));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_EQ(static_cast<size_t>(end_world_pos), bytes_read.size());
  // Just check the first and last bytes.
  EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read[0]);
  EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read[end_world_pos - 1]);

  // TODO(vtl): Test that |file2| has the same open options as |file1|.
}

TEST_F(FileImplTest, Truncate) {
  const uint32_t kInitialSize = 1000;
  const uint32_t kTruncatedSize = 654;

  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file),
                      mojom::kFlagWrite | mojom::kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(kInitialSize, '!');
  error = mojom::FileError::FAILED;
  uint32_t num_bytes_written = 0;
  file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
              mojom::Whence::FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  EXPECT_EQ(kInitialSize, num_bytes_written);

  // Stat it.
  error = mojom::FileError::FAILED;
  mojom::FileInformationPtr file_info;
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kInitialSize, file_info->size);

  // Truncate it.
  error = mojom::FileError::FAILED;
  file->Truncate(kTruncatedSize, Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Stat again.
  error = mojom::FileError::FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kTruncatedSize, file_info->size);
}

TEST_F(FileImplTest, AsHandle) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create my_file.
    mojom::FilePtr file1;
    error = mojom::FileError::FAILED;
    directory->OpenFile(
        "my_file", GetProxy(&file1),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Fetch the handle
    error = mojom::FileError::FAILED;
    mojo::ScopedHandle handle;
    file1->AsHandle(Capture(&error, &handle));
    ASSERT_TRUE(file1.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Pull a file descriptor out of the scoped handle.
    base::PlatformFile platform_file;
    MojoResult unwrap_result = mojo::UnwrapPlatformFile(std::move(handle),
                                                        &platform_file);
    EXPECT_EQ(MOJO_RESULT_OK, unwrap_result);

    // Pass this raw file descriptor to a base::File.
    base::File raw_file(platform_file);
    ASSERT_TRUE(raw_file.IsValid());
    EXPECT_EQ(5, raw_file.WriteAtCurrentPos("hello", 5));
  }

  {
    // Reopen my_file.
    mojom::FilePtr file2;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file2),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Verify that we wrote data raw on the file descriptor.
    mojo::Array<uint8_t> bytes_read;
    error = mojom::FileError::FAILED;
    file2->Read(5, 0, mojom::Whence::FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file2.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
    ASSERT_EQ(5u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[2]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[4]);
  }
}

TEST_F(FileImplTest, SimpleLockUnlock) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file),
                      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Lock the file.
  error = mojom::FileError::FAILED;
  file->Lock(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Unlock the file.
  error = mojom::FileError::FAILED;
  file->Unlock(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);
}

TEST_F(FileImplTest, CantDoubleLock) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  mojom::FilePtr file;
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", GetProxy(&file),
                      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Lock the file.
  error = mojom::FileError::FAILED;
  file->Lock(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Lock the file again.
  error = mojom::FileError::OK;
  file->Lock(Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::FAILED, error);
}

TEST_F(FileImplTest, ClosingFileClearsLock) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create my_file.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile(
        "my_file", GetProxy(&file),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagOpenAlways,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // Lock the file.
    error = mojom::FileError::FAILED;
    file->Lock(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Open the file again.
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile(
        "my_file", GetProxy(&file),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagOpenAlways,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    // The file shouldn't be locked (and we check by trying to lock it).
    error = mojom::FileError::FAILED;
    file->Lock(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }
}

}  // namespace
}  // namespace filesystem
