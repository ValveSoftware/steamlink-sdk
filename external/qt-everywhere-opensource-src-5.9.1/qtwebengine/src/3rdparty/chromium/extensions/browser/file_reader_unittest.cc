// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

class FileReaderTest : public testing::Test {
 public:
  FileReaderTest() : file_thread_(BrowserThread::FILE) {
    file_thread_.Start();
  }
 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;
};

class Receiver {
 public:
  Receiver() : succeeded_(false) {
  }

  FileReader::DoneCallback NewCallback() {
    return base::Bind(&Receiver::DidReadFile, base::Unretained(this));
  }

  bool succeeded() const { return succeeded_; }
  const std::string& data() const { return *data_; }

 private:
  void DidReadFile(bool success, std::unique_ptr<std::string> data) {
    succeeded_ = success;
    data_ = std::move(data);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  bool succeeded_;
  std::unique_ptr<std::string> data_;
};

void RunBasicTest(const char* filename) {
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  std::string extension_id = crx_file::id_util::GenerateId("test");
  ExtensionResource resource(
      extension_id, path, base::FilePath().AppendASCII(filename));
  path = path.AppendASCII(filename);

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(path, &file_contents));

  Receiver receiver;

  scoped_refptr<FileReader> file_reader(
      new FileReader(resource, FileReader::OptionalFileThreadTaskCallback(),
                     receiver.NewCallback()));
  file_reader->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(receiver.succeeded());
  EXPECT_EQ(file_contents, receiver.data());
}

TEST_F(FileReaderTest, SmallFile) {
  RunBasicTest("smallfile");
}

TEST_F(FileReaderTest, BiggerFile) {
  RunBasicTest("bigfile");
}

TEST_F(FileReaderTest, NonExistantFile) {
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  std::string extension_id = crx_file::id_util::GenerateId("test");
  ExtensionResource resource(extension_id, path, base::FilePath(
      FILE_PATH_LITERAL("file_that_does_not_exist")));
  path = path.AppendASCII("file_that_does_not_exist");

  Receiver receiver;

  scoped_refptr<FileReader> file_reader(
      new FileReader(resource, FileReader::OptionalFileThreadTaskCallback(),
                     receiver.NewCallback()));
  file_reader->Start();

  base::RunLoop().Run();

  EXPECT_FALSE(receiver.succeeded());
}

}  // namespace extensions
