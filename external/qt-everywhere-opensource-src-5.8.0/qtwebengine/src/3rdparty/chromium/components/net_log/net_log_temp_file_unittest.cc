// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_temp_file.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/net_log/chrome_net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kChannelString[] = "SomeChannel";

}  // namespace

namespace net_log {

class TestNetLogTempFile : public NetLogTempFile {
 public:
  explicit TestNetLogTempFile(ChromeNetLog* chrome_net_log)
      : NetLogTempFile(
            chrome_net_log,
            base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
            kChannelString),
        lie_about_net_export_log_directory_(false) {
    EXPECT_TRUE(net_log_temp_dir_.CreateUniqueTempDir());
  }

  ~TestNetLogTempFile() override { EXPECT_TRUE(net_log_temp_dir_.Delete()); }

  // NetLogTempFile implementation:
  bool GetNetExportLogBaseDirectory(base::FilePath* path) const override {
    if (lie_about_net_export_log_directory_)
      return false;
    *path = net_log_temp_dir_.path();
    return true;
  }

  void set_lie_about_net_export_log_directory(
      bool lie_about_net_export_log_directory) {
    lie_about_net_export_log_directory_ = lie_about_net_export_log_directory;
  }

 private:
  bool lie_about_net_export_log_directory_;

  base::ScopedTempDir net_log_temp_dir_;
};

class NetLogTempFileTest : public ::testing::Test {
 public:
  NetLogTempFileTest()
      : net_log_(new ChromeNetLog(
            base::FilePath(),
            net::NetLogCaptureMode::Default(),
            base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
            kChannelString)),
        net_log_temp_file_(new TestNetLogTempFile(net_log_.get())) {
    EXPECT_TRUE(net_log_temp_file_->SetUpNetExportLogPath());
    net_export_log_ = net_log_temp_file_->log_path_;
  }

  std::string GetStateString() const {
    std::unique_ptr<base::DictionaryValue> dict(net_log_temp_file_->GetState());
    std::string state;
    EXPECT_TRUE(dict->GetString("state", &state));
    return state;
  }

  std::string GetLogTypeString() const {
    std::unique_ptr<base::DictionaryValue> dict(net_log_temp_file_->GetState());
    std::string log_type;
    EXPECT_TRUE(dict->GetString("logType", &log_type));
    return log_type;
  }

  // Make sure the export file has been created and is non-empty, as net
  // constants will always be written to it on creation.
  void VerifyNetExportLogExists() const {
    EXPECT_EQ(net_export_log_, net_log_temp_file_->log_path_);
    ASSERT_TRUE(base::PathExists(net_export_log_));

    int64_t file_size;
    // base::GetFileSize returns proper file size on open handles.
    ASSERT_TRUE(base::GetFileSize(net_export_log_, &file_size));
    EXPECT_GT(file_size, 0);
  }

  // Make sure the export file has been created and a valid JSON file.  This
  // should always be the case once logging has been stopped.
  void VerifyNetExportLogComplete() const {
    VerifyNetExportLogExists();

    std::string log;
    ASSERT_TRUE(ReadFileToString(net_export_log_, &log));
    base::JSONReader reader;
    std::unique_ptr<base::Value> json = base::JSONReader::Read(log);
    EXPECT_TRUE(json);
  }

  // Verify state and GetFilePath return correct values if EnsureInit() fails.
  void VerifyFilePathAndStateAfterEnsureInitFailure() {
    EXPECT_EQ("UNINITIALIZED", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_UNINITIALIZED, net_log_temp_file_->state());

    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
  }

  // When we lie in NetExportLogExists, make sure state and GetFilePath return
  // correct values.
  void VerifyFilePathAndStateAfterEnsureInit() {
    EXPECT_EQ("NOT_LOGGING", GetStateString());
    EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("NONE", GetLogTypeString());
    EXPECT_EQ(NetLogTempFile::LOG_TYPE_NONE, net_log_temp_file_->log_type());

    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
    EXPECT_FALSE(net_log_temp_file_->NetExportLogExists());
  }

  // The following methods make sure the export file has been successfully
  // initialized by a DO_START command of the given type.

  void VerifyFileAndStateAfterDoStart() {
    VerifyFileAndStateAfterStart(
        NetLogTempFile::LOG_TYPE_NORMAL, "NORMAL",
        net::NetLogCaptureMode::IncludeCookiesAndCredentials());
  }

  void VerifyFileAndStateAfterDoStartStripPrivateData() const {
    VerifyFileAndStateAfterStart(NetLogTempFile::LOG_TYPE_STRIP_PRIVATE_DATA,
                                 "STRIP_PRIVATE_DATA",
                                 net::NetLogCaptureMode::Default());
  }

  void VerifyFileAndStateAfterDoStartLogBytes() const {
    VerifyFileAndStateAfterStart(NetLogTempFile::LOG_TYPE_LOG_BYTES,
                                 "LOG_BYTES",
                                 net::NetLogCaptureMode::IncludeSocketBytes());
  }

  // Make sure the export file has been successfully initialized after DO_STOP
  // command following a DO_START command of the given type.

  void VerifyFileAndStateAfterDoStop() const {
    VerifyFileAndStateAfterDoStop(NetLogTempFile::LOG_TYPE_NORMAL, "NORMAL");
  }

  void VerifyFileAndStateAfterDoStopWithStripPrivateData() const {
    VerifyFileAndStateAfterDoStop(NetLogTempFile::LOG_TYPE_STRIP_PRIVATE_DATA,
                                  "STRIP_PRIVATE_DATA");
  }

  void VerifyFileAndStateAfterDoStopWithLogBytes() const {
    VerifyFileAndStateAfterDoStop(NetLogTempFile::LOG_TYPE_LOG_BYTES,
                                  "LOG_BYTES");
  }

  std::unique_ptr<ChromeNetLog> net_log_;
  // |net_log_temp_file_| is initialized after |net_log_| so that it can stop
  // obvserving on destruction.
  std::unique_ptr<TestNetLogTempFile> net_log_temp_file_;
  base::FilePath net_export_log_;

 private:
  // Checks state after one of the DO_START* commands.
  void VerifyFileAndStateAfterStart(
      NetLogTempFile::LogType expected_log_type,
      const std::string& expected_log_type_string,
      net::NetLogCaptureMode expected_capture_mode) const {
    EXPECT_EQ(NetLogTempFile::STATE_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("LOGGING", GetStateString());
    EXPECT_EQ(expected_log_type, net_log_temp_file_->log_type());
    EXPECT_EQ(expected_log_type_string, GetLogTypeString());
    EXPECT_EQ(expected_capture_mode,
              net_log_temp_file_->write_to_file_observer_->capture_mode());

    // Check GetFilePath returns false when still writing to the file.
    base::FilePath net_export_file_path;
    EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));

    VerifyNetExportLogExists();
  }

  void VerifyFileAndStateAfterDoStop(
      NetLogTempFile::LogType expected_log_type,
      const std::string& expected_log_type_string) const {
    EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
    EXPECT_EQ("NOT_LOGGING", GetStateString());
    EXPECT_EQ(expected_log_type, net_log_temp_file_->log_type());
    EXPECT_EQ(expected_log_type_string, GetLogTypeString());

    base::FilePath net_export_file_path;
    EXPECT_TRUE(net_log_temp_file_->GetFilePath(&net_export_file_path));
    EXPECT_EQ(net_export_log_, net_export_file_path);

    VerifyNetExportLogComplete();
  }

  base::MessageLoop message_loop_;
};

TEST_F(NetLogTempFileTest, EnsureInitFailure) {
  net_log_temp_file_->set_lie_about_net_export_log_directory(true);

  EXPECT_FALSE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInitFailure();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFilePathAndStateAfterEnsureInitFailure();
}

TEST_F(NetLogTempFileTest, EnsureInitAllowStart) {
  EXPECT_TRUE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInit();

  // Calling EnsureInit() second time should be a no-op.
  EXPECT_TRUE(net_log_temp_file_->EnsureInit());
  VerifyFilePathAndStateAfterEnsureInit();

  // GetFilePath() should failed when there's no temp file.
  base::FilePath net_export_file_path;
  EXPECT_FALSE(net_log_temp_file_->GetFilePath(&net_export_file_path));
}

TEST_F(NetLogTempFileTest, EnsureInitAllowStartOrSend) {
  // Create and close an empty log file, to simulate an old log file already
  // existing.
  base::ScopedFILE created_file(base::OpenFile(net_export_log_, "w"));
  ASSERT_TRUE(created_file.get());
  created_file.reset();

  EXPECT_TRUE(net_log_temp_file_->EnsureInit());

  EXPECT_EQ("NOT_LOGGING", GetStateString());
  EXPECT_EQ(NetLogTempFile::STATE_NOT_LOGGING, net_log_temp_file_->state());
  EXPECT_EQ("UNKNOWN", GetLogTypeString());
  EXPECT_EQ(NetLogTempFile::LOG_TYPE_UNKNOWN, net_log_temp_file_->log_type());
  EXPECT_EQ(net_export_log_, net_log_temp_file_->log_path_);
  EXPECT_TRUE(base::PathExists(net_export_log_));

  base::FilePath net_export_file_path;
  EXPECT_TRUE(net_log_temp_file_->GetFilePath(&net_export_file_path));
  EXPECT_TRUE(base::PathExists(net_export_file_path));
  EXPECT_EQ(net_export_log_, net_export_file_path);
}

TEST_F(NetLogTempFileTest, ProcessCommandDoStartAndStop) {
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Calling a second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // starting with other log levels should also be no-ops.
  net_log_temp_file_->ProcessCommand(
      NetLogTempFile::DO_START_STRIP_PRIVATE_DATA);
  VerifyFileAndStateAfterDoStart();
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START_LOG_BYTES);
  VerifyFileAndStateAfterDoStart();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  // Calling DO_STOP second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();
}

TEST_F(NetLogTempFileTest,
       ProcessCommandDoStartAndStopWithPrivateDataStripping) {
  net_log_temp_file_->ProcessCommand(
      NetLogTempFile::DO_START_STRIP_PRIVATE_DATA);
  VerifyFileAndStateAfterDoStartStripPrivateData();

  // Calling a second time should be a no-op.
  net_log_temp_file_->ProcessCommand(
      NetLogTempFile::DO_START_STRIP_PRIVATE_DATA);
  VerifyFileAndStateAfterDoStartStripPrivateData();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStopWithStripPrivateData();

  // Calling DO_STOP second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStopWithStripPrivateData();
}

TEST_F(NetLogTempFileTest, ProcessCommandDoStartAndStopWithByteLogging) {
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START_LOG_BYTES);
  VerifyFileAndStateAfterDoStartLogBytes();

  // Calling a second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START_LOG_BYTES);
  VerifyFileAndStateAfterDoStartLogBytes();

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStopWithLogBytes();

  // Calling DO_STOP second time should be a no-op.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStopWithLogBytes();
}

TEST_F(NetLogTempFileTest, DoStartClearsFile) {
  // Verify file sizes after two consecutive starts/stops are the same (even if
  // we add some junk data in between).
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  int64_t start_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &start_file_size));

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &stop_file_size));
  EXPECT_GE(stop_file_size, start_file_size);

  // Add some junk at the end of the file.
  std::string junk_data("Hello");
  EXPECT_TRUE(
      base::AppendToFile(net_export_log_, junk_data.c_str(), junk_data.size()));

  int64_t junk_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &junk_file_size));
  EXPECT_GT(junk_file_size, stop_file_size);

  // Execute DO_START/DO_STOP commands and make sure the file is back to the
  // size before addition of junk data.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  int64_t new_start_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_start_file_size));
  EXPECT_EQ(new_start_file_size, start_file_size);

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_stop_file_size));
  EXPECT_EQ(new_stop_file_size, stop_file_size);
}

TEST_F(NetLogTempFileTest, CheckAddEvent) {
  // Add an event to |net_log_| and then test to make sure that, after we stop
  // logging, the file is larger than the file created without that event.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Get file size without the event.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64_t stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &stop_file_size));

  // Perform DO_START and add an Event and then DO_STOP and then compare
  // file sizes.
  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_START);
  VerifyFileAndStateAfterDoStart();

  // Log an event.
  net_log_->AddGlobalEntry(net::NetLog::TYPE_CANCELLED);

  net_log_temp_file_->ProcessCommand(NetLogTempFile::DO_STOP);
  VerifyFileAndStateAfterDoStop();

  int64_t new_stop_file_size;
  EXPECT_TRUE(base::GetFileSize(net_export_log_, &new_stop_file_size));
  EXPECT_GE(new_stop_file_size, stop_file_size);
}

}  // namespace net_log
