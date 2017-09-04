// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback {
 public:
  TestCallback();
  virtual ~TestCallback() {}
  void Set(update_client::UnpackerError error, int extra_code);

  update_client::UnpackerError error_;
  int extra_code_;
  bool called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TestCallback::TestCallback()
    : error_(update_client::UnpackerError::kNone),
      extra_code_(-1),
      called_(false) {}

void TestCallback::Set(update_client::UnpackerError error, int extra_code) {
  error_ = error;
  extra_code_ = extra_code;
  called_ = true;
}

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

}  // namespace

namespace update_client {

class ComponentUnpackerTest : public testing::Test {
 public:
  ComponentUnpackerTest();
  ~ComponentUnpackerTest() override;

  void UnpackComplete(const ComponentUnpacker::Result& result);

 protected:
  void RunThreads();

  base::MessageLoopForUI message_loop_;
  base::RunLoop runloop_;
  base::Closure quit_closure_;

  std::unique_ptr<base::SequencedWorkerPoolOwner> worker_pool_;
  scoped_refptr<update_client::TestConfigurator> config_;

  ComponentUnpacker::Result result_;
};

ComponentUnpackerTest::ComponentUnpackerTest()
    : worker_pool_(new base::SequencedWorkerPoolOwner(2, "test")) {
  quit_closure_ = runloop_.QuitClosure();

  auto pool = worker_pool_->pool();
  config_ = new TestConfigurator(
      pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
      message_loop_.task_runner());
}

ComponentUnpackerTest::~ComponentUnpackerTest() {}

void ComponentUnpackerTest::RunThreads() {
  runloop_.Run();
}

void ComponentUnpackerTest::UnpackComplete(
    const ComponentUnpacker::Result& result) {
  result_ = result;
  message_loop_.task_runner()->PostTask(FROM_HERE, quit_closure_);
}

TEST_F(ComponentUnpackerTest, UnpackFullCrx) {
  scoped_refptr<ComponentUnpacker> component_unpacker = new ComponentUnpacker(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr, nullptr,
      config_->GetSequencedTaskRunner());
  component_unpacker->Unpack(base::Bind(&ComponentUnpackerTest::UnpackComplete,
                                        base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kNone, result_.error);
  EXPECT_EQ(0, result_.extended_error);

  base::FilePath unpack_path = result_.unpack_path;
  EXPECT_FALSE(unpack_path.empty());
  EXPECT_TRUE(base::DirectoryExists(unpack_path));

  int64_t file_size = 0;
  EXPECT_TRUE(
      base::GetFileSize(unpack_path.AppendASCII("component1.dll"), &file_size));
  EXPECT_EQ(1024, file_size);
  EXPECT_TRUE(
      base::GetFileSize(unpack_path.AppendASCII("flashtest.pem"), &file_size));
  EXPECT_EQ(911, file_size);
  EXPECT_TRUE(
      base::GetFileSize(unpack_path.AppendASCII("manifest.json"), &file_size));
  EXPECT_EQ(144, file_size);

  EXPECT_TRUE(base::DeleteFile(unpack_path, true));
}

TEST_F(ComponentUnpackerTest, UnpackFileNotFound) {
  scoped_refptr<ComponentUnpacker> component_unpacker = new ComponentUnpacker(
      std::vector<uint8_t>(std::begin(jebg_hash), std::end(jebg_hash)),
      test_file("file-not-found.crx"), nullptr, nullptr,
      config_->GetSequencedTaskRunner());
  component_unpacker->Unpack(base::Bind(&ComponentUnpackerTest::UnpackComplete,
                                        base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kInvalidFile, result_.error);
  EXPECT_EQ(0, result_.extended_error);

  EXPECT_TRUE(result_.unpack_path.empty());
}

// Tests a mismatch between the public key hash and the id of the component.
TEST_F(ComponentUnpackerTest, UnpackFileHashMismatch) {
  scoped_refptr<ComponentUnpacker> component_unpacker = new ComponentUnpacker(
      std::vector<uint8_t>(std::begin(abag_hash), std::end(abag_hash)),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"), nullptr, nullptr,
      config_->GetSequencedTaskRunner());
  component_unpacker->Unpack(base::Bind(&ComponentUnpackerTest::UnpackComplete,
                                        base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(UnpackerError::kInvalidId, result_.error);
  EXPECT_EQ(0, result_.extended_error);

  EXPECT_TRUE(result_.unpack_path.empty());
}

}  // namespace update_client
