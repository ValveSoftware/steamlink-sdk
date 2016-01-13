// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/recursive_operation_delegate.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/public/test/sandbox_file_system_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

using fileapi::FileSystemContext;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;

namespace content {
namespace {

class LoggingRecursiveOperation : public fileapi::RecursiveOperationDelegate {
 public:
  struct LogEntry {
    enum Type {
      PROCESS_FILE,
      PROCESS_DIRECTORY,
      POST_PROCESS_DIRECTORY
    };
    Type type;
    FileSystemURL url;
  };

  LoggingRecursiveOperation(FileSystemContext* file_system_context,
                            const FileSystemURL& root,
                            const StatusCallback& callback)
      : fileapi::RecursiveOperationDelegate(file_system_context),
        root_(root),
        callback_(callback),
        weak_factory_(this) {
  }
  virtual ~LoggingRecursiveOperation() {}

  const std::vector<LogEntry>& log_entries() const { return log_entries_; }

  // RecursiveOperationDelegate overrides.
  virtual void Run() OVERRIDE {
    NOTREACHED();
  }

  virtual void RunRecursively() OVERRIDE {
    StartRecursiveOperation(root_, callback_);
  }

  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) OVERRIDE {
    RecordLogEntry(LogEntry::PROCESS_FILE, url);
    operation_runner()->GetMetadata(
        url,
        base::Bind(&LoggingRecursiveOperation::DidGetMetadata,
                   weak_factory_.GetWeakPtr(), callback));
  }

  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) OVERRIDE {
    RecordLogEntry(LogEntry::PROCESS_DIRECTORY, url);
    callback.Run(base::File::FILE_OK);
  }

  virtual void PostProcessDirectory(const FileSystemURL& url,
                                    const StatusCallback& callback) OVERRIDE {
    RecordLogEntry(LogEntry::POST_PROCESS_DIRECTORY, url);
    callback.Run(base::File::FILE_OK);
  }

 private:
  void RecordLogEntry(LogEntry::Type type, const FileSystemURL& url) {
    LogEntry entry;
    entry.type = type;
    entry.url = url;
    log_entries_.push_back(entry);
  }

  void DidGetMetadata(const StatusCallback& callback,
                      base::File::Error result,
                      const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      callback.Run(result);
      return;
    }

    callback.Run(file_info.is_directory ?
                 base::File::FILE_ERROR_NOT_A_FILE :
                 base::File::FILE_OK);
  }

  FileSystemURL root_;
  StatusCallback callback_;
  std::vector<LogEntry> log_entries_;

  base::WeakPtrFactory<LoggingRecursiveOperation> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(LoggingRecursiveOperation);
};

void ReportStatus(base::File::Error* out_error,
                  base::File::Error error) {
  DCHECK(out_error);
  *out_error = error;
}

// To test the Cancel() during operation, calls Cancel() of |operation|
// after |counter| times message posting.
void CallCancelLater(fileapi::RecursiveOperationDelegate* operation,
                     int counter) {
  if (counter > 0) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&CallCancelLater, base::Unretained(operation), counter - 1));
    return;
  }

  operation->Cancel();
}

}  // namespace

class RecursiveOperationDelegateTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    sandbox_file_system_.SetUp(base_.path().AppendASCII("filesystem"));
  }

  virtual void TearDown() OVERRIDE {
    sandbox_file_system_.TearDown();
  }

  scoped_ptr<FileSystemOperationContext> NewContext() {
    FileSystemOperationContext* context =
        sandbox_file_system_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return make_scoped_ptr(context);
  }

  fileapi::FileSystemFileUtil* file_util() {
    return sandbox_file_system_.file_util();
  }

  FileSystemURL URLForPath(const std::string& path) const {
    return sandbox_file_system_.CreateURLFromUTF8(path);
  }

  FileSystemURL CreateFile(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    bool created = false;
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->EnsureFileExists(NewContext().get(),
                                            url, &created));
    EXPECT_TRUE(created);
    return url;
  }

  FileSystemURL CreateDirectory(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->CreateDirectory(NewContext().get(), url,
                                           false /* exclusive */, true));
    return url;
  }

 private:
  base::MessageLoop message_loop_;

  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;
  SandboxFileSystemTestHelper sandbox_file_system_;
};

TEST_F(RecursiveOperationDelegateTest, RootIsFile) {
  FileSystemURL src_file(CreateFile("src"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  scoped_ptr<FileSystemOperationContext> context = NewContext();
  scoped_ptr<LoggingRecursiveOperation> operation(
      new LoggingRecursiveOperation(
          context->file_system_context(), src_file,
          base::Bind(&ReportStatus, &error)));
  operation->RunRecursively();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(1U, log_entries.size());
  const LoggingRecursiveOperation::LogEntry& entry = log_entries[0];
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE, entry.type);
  EXPECT_EQ(src_file, entry.url);
}

TEST_F(RecursiveOperationDelegateTest, RootIsDirectory) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));
  FileSystemURL src_file3(CreateFile("src/dir1/file3"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  scoped_ptr<FileSystemOperationContext> context = NewContext();
  scoped_ptr<LoggingRecursiveOperation> operation(
      new LoggingRecursiveOperation(
          context->file_system_context(), src_root,
          base::Bind(&ReportStatus, &error)));
  operation->RunRecursively();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(8U, log_entries.size());

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[0].type);
  EXPECT_EQ(src_root, log_entries[0].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[1].type);
  EXPECT_EQ(src_root, log_entries[1].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[2].type);
  EXPECT_EQ(src_file1, log_entries[2].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[3].type);
  EXPECT_EQ(src_dir1, log_entries[3].url);

  // The order of src/dir1/file2 and src/dir1/file3 depends on the file system
  // implementation (can be swapped).
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[4].type);
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[5].type);
  EXPECT_TRUE((src_file2 == log_entries[4].url &&
               src_file3 == log_entries[5].url) ||
              (src_file3 == log_entries[4].url &&
               src_file2 == log_entries[5].url));

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[6].type);
  EXPECT_EQ(src_dir1, log_entries[6].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[7].type);
  EXPECT_EQ(src_root, log_entries[7].url);
}

TEST_F(RecursiveOperationDelegateTest, Cancel) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  scoped_ptr<FileSystemOperationContext> context = NewContext();
  scoped_ptr<LoggingRecursiveOperation> operation(
      new LoggingRecursiveOperation(
          context->file_system_context(), src_root,
          base::Bind(&ReportStatus, &error)));
  operation->RunRecursively();

  // Invoke Cancel(), after 5 times message posting.
  CallCancelLater(operation.get(), 5);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_ERROR_ABORT, error);
}

}  // namespace content
