// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "components/filesystem/files_test_base.h"
#include "mojo/common/common_type_converters.h"

namespace filesystem {
namespace {

using DirectoryImplTest = FilesTestBase;

TEST_F(DirectoryImplTest, Read) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Make some files.
  const struct {
    const char* name;
    uint32_t open_flags;
  } files_to_create[] = {
      {"my_file1", mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file2", mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file3", mojom::kFlagAppend | mojom::kFlagCreate}};
  for (size_t i = 0; i < arraysize(files_to_create); i++) {
    error = mojom::FileError::FAILED;
    directory->OpenFile(files_to_create[i].name, nullptr,
                        files_to_create[i].open_flags, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }
  // Make a directory.
  error = mojom::FileError::FAILED;
  directory->OpenDirectory(
      "my_dir", nullptr,
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  error = mojom::FileError::FAILED;
  mojo::Array<mojom::DirectoryEntryPtr> directory_contents;
  directory->Read(Capture(&error, &directory_contents));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Expected contents of the directory.
  std::map<std::string, mojom::FsFileType> expected_contents;
  expected_contents["my_file1"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_file2"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_file3"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_dir"] = mojom::FsFileType::DIRECTORY;
  // Note: We don't expose ".." or ".".

  EXPECT_EQ(expected_contents.size(), directory_contents.size());
  for (size_t i = 0; i < directory_contents.size(); i++) {
    ASSERT_TRUE(directory_contents[i]);
    ASSERT_TRUE(directory_contents[i]->name);
    auto it = expected_contents.find(directory_contents[i]->name.get());
    ASSERT_TRUE(it != expected_contents.end());
    EXPECT_EQ(it->second, directory_contents[i]->type);
    expected_contents.erase(it);
  }
}

// TODO(vtl): Properly test OpenFile() and OpenDirectory() (including flags).

TEST_F(DirectoryImplTest, BasicRenameDelete) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create my_file.
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", nullptr,
                      mojom::kFlagWrite | mojom::kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Opening my_file should succeed.
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", nullptr, mojom::kFlagRead | mojom::kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Rename my_file to my_new_file.
  directory->Rename("my_file", "my_new_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Opening my_file should fail.

  error = mojom::FileError::FAILED;
  directory->OpenFile("my_file", nullptr, mojom::kFlagRead | mojom::kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::NOT_FOUND, error);

  // Opening my_new_file should succeed.
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_new_file", nullptr,
                      mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Delete my_new_file (no flags).
  directory->Delete("my_new_file", 0, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::OK, error);

  // Opening my_new_file should fail.
  error = mojom::FileError::FAILED;
  directory->OpenFile("my_new_file", nullptr,
                      mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(mojom::FileError::NOT_FOUND, error);
}

TEST_F(DirectoryImplTest, CantOpenDirectoriesAsFiles) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    // Create a directory called 'my_file'
    mojom::DirectoryPtr my_file_directory;
    error = mojom::FileError::FAILED;
    directory->OpenDirectory(
        "my_file", GetProxy(&my_file_directory),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    // Attempt to open that directory as a file. This must fail!
    mojom::FilePtr file;
    error = mojom::FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        mojom::kFlagRead | mojom::kFlagOpen, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, Clone) {
  mojom::DirectoryPtr clone_one;
  mojom::DirectoryPtr clone_two;
  mojom::FileError error;

  {
    mojom::DirectoryPtr directory;
    GetTemporaryRoot(&directory);

    directory->Clone(GetProxy(&clone_one));
    directory->Clone(GetProxy(&clone_two));

    // Original temporary directory goes out of scope here; shouldn't be
    // deleted since it has clones.
  }

  std::string data("one two three");
  {
    clone_one->WriteFile("data", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(clone_one.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    mojo::Array<uint8_t> file_contents;
    clone_two->ReadEntireFile("data", Capture(&error, &file_contents));
    ASSERT_TRUE(clone_two.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    EXPECT_EQ(data, file_contents.To<std::string>());
  }
}

TEST_F(DirectoryImplTest, WriteFileReadFile) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  std::string data("one two three");
  {
    directory->WriteFile("data", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("data", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);

    EXPECT_EQ(data, file_contents.To<std::string>());
  }
}

TEST_F(DirectoryImplTest, ReadEmptyFileIsNotFoundError) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("doesnt_exist", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::NOT_FOUND, error);
  }
}

TEST_F(DirectoryImplTest, CantReadEntireFileOnADirectory) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create a directory
  {
    mojom::DirectoryPtr my_file_directory;
    error = mojom::FileError::FAILED;
    directory->OpenDirectory(
        "my_dir", GetProxy(&my_file_directory),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  // Try to read it as a file
  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("my_dir", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, CantWriteFileOnADirectory) {
  mojom::DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  mojom::FileError error;

  // Create a directory
  {
    mojom::DirectoryPtr my_file_directory;
    error = mojom::FileError::FAILED;
    directory->OpenDirectory(
        "my_dir", GetProxy(&my_file_directory),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::OK, error);
  }

  {
    std::string data("one two three");
    directory->WriteFile("my_dir", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(mojom::FileError::NOT_A_FILE, error);
  }
}

// TODO(vtl): Test delete flags.

}  // namespace
}  // namespace filesystem
