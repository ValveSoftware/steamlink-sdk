// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_NET_LOG_TEMP_FILE_H_
#define COMPONENTS_NET_LOG_NET_LOG_TEMP_FILE_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/log/net_log.h"

namespace base {
class DictionaryValue;
}

namespace net {
class WriteToFileNetLogObserver;
}

namespace net_log {

class ChromeNetLog;

// NetLogTempFile logs all the NetLog entries into a temporary file
// "chrome-net-export-log.json" created in base::GetTempDir() directory.
//
// NetLogTempFile maintains the current logging state (state_) and log file type
// (log_type_) of the logging into a chrome-net-export-log.json file.
//
// The following are the possible states
// a) Only Start is allowed (STATE_NOT_LOGGING, LOG_TYPE_NONE).
// b) Only Stop is allowed (STATE_LOGGING).
// c) Either Send or Start is allowed (STATE_NOT_LOGGING, anything but
//    LOG_TYPE_NONE).
//
// This is created/destroyed on the main thread, but all other function calls
// occur on a background thread.
//
// This relies on the UI thread outlasting all other threads for thread safety.
class NetLogTempFile {
 public:
  // This enum lists the UI button commands it could receive.
  enum Command {
    DO_START_LOG_BYTES,  // Call StartNetLog logging all bytes received.
    DO_START,            // Call StartNetLog.
    DO_START_STRIP_PRIVATE_DATA,  // Call StartNetLog stripping private data.
    DO_STOP,                      // Call StopNetLog.
  };

  virtual ~NetLogTempFile();

  // Accepts the button command and executes it.
  void ProcessCommand(Command command);

  // Returns true and the path to the temporary file. If there is no file to
  // send, then it returns false. It also returns false when actively logging to
  // the file.
  bool GetFilePath(base::FilePath* path);

  // Creates a Value summary of the state of the NetLogTempFile. The caller is
  // responsible for deleting the returned value.
  base::DictionaryValue* GetState();

 protected:
  // Constructs a NetLogTempFile. Only one instance is created in browser
  // process.
  NetLogTempFile(ChromeNetLog* chrome_net_log,
                 const base::CommandLine::StringType& command_line_string,
                 const std::string& channel_string);

  // Returns path name to base::GetTempDir() directory. Returns false if
  // base::GetTempDir() fails.
  virtual bool GetNetExportLogBaseDirectory(base::FilePath* path) const;

 private:
  friend class ChromeNetLog;
  friend class NetLogTempFileTest;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitFailure);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitAllowStart);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitAllowStartOrSend);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, ProcessCommandDoStartAndStop);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, DoStartClearsFile);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, CheckAddEvent);

  // This enum lists the possible state NetLogTempFile could be in. It is used
  // to enable/disable "Start", "Stop" and "Send" (email) UI actions.
  enum State {
    STATE_UNINITIALIZED,
    // Not currently logging to file.
    STATE_NOT_LOGGING,
    // Currently logging to file.
    STATE_LOGGING,
  };

  // The type of the current log file on disk.
  enum LogType {
    // There is no current log file.
    LOG_TYPE_NONE,
    // The file predates this session. May or may not have private data.
    // TODO(davidben): This state is kind of silly.
    LOG_TYPE_UNKNOWN,
    // The log includes raw bytes.
    LOG_TYPE_LOG_BYTES,
    // The file includes all data.
    LOG_TYPE_NORMAL,
    // The file has credentials and cookies stripped.
    LOG_TYPE_STRIP_PRIVATE_DATA,
  };

  // Returns the NetLog::CaptureMode corresponding to a LogType.
  static net::NetLogCaptureMode GetCaptureModeForLogType(LogType log_type);

  // Initializes the |state_| to STATE_NOT_LOGGING and |log_type_| to
  // LOG_TYPE_NONE (if there is no temporary file from earlier run) or
  // LOG_TYPE_UNKNOWN (if there is a temporary file from earlier run). Returns
  // false if initialization of |log_path_| fails.
  bool EnsureInit();

  // Start collecting NetLog data into chrome-net-export-log.json file in
  // base::GetTempDir() directory, using the specified capture mode. It is a
  // no-op if we are already collecting data into a file, and |capture_mode| is
  // ignored.
  // TODO(mmenke):  That's rather weird behavior, think about improving it.
  void StartNetLog(LogType log_type);

  // Stop collecting NetLog data into the temporary file. It is a no-op if we
  // are not collecting data into a file.
  void StopNetLog();

  // Updates |log_path_| to be the base::FilePath to use for log files, which
  // will be inside the base::GetTempDir() directory. Returns false if
  // base::GetTempDir() fails, or unable to create a subdirectory for logging
  // withinh that directory.
  bool SetUpNetExportLogPath();

  // Returns true if a file exists at |log_path_|.
  bool NetExportLogExists() const;

  base::ThreadChecker thread_checker_;

  // Helper function for unit tests.
  State state() const { return state_; }
  LogType log_type() const { return log_type_; }

  State state_;       // Current state of NetLogTempFile.
  LogType log_type_;  // Type of current log file on disk.

  base::FilePath log_path_;  // base::FilePath to the temporary file.

  // |write_to_file_observer_| watches the NetLog event stream, and
  // sends all entries to the file created in StartNetLog().
  std::unique_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;

  // The |chrome_net_log_| is owned by the browser process, cached here to avoid
  // using global (g_browser_process).
  ChromeNetLog* chrome_net_log_;

  const base::CommandLine::StringType command_line_string_;
  const std::string channel_string_;

  DISALLOW_COPY_AND_ASSIGN(NetLogTempFile);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_NET_LOG_TEMP_FILE_H_
