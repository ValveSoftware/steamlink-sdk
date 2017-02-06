// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_temp_file.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/net_log/chrome_net_log.h"
#include "net/log/write_to_file_net_log_observer.h"

namespace net_log {

// Path of logs relative to base::GetTempDir(). Must be kept in sync
// with chrome/android/java/res/xml/file_paths.xml
base::FilePath::CharType kLogRelativePath[] =
    FILE_PATH_LITERAL("net-export/chrome-net-export-log.json");

// Old path used by net-export. Used to delete old files.
// TODO(mmenke): Should remove at some point. Added in M46.
base::FilePath::CharType kOldLogRelativePath[] =
    FILE_PATH_LITERAL("chrome-net-export-log.json");

NetLogTempFile::~NetLogTempFile() {
  if (write_to_file_observer_)
    write_to_file_observer_->StopObserving(nullptr);
}

void NetLogTempFile::ProcessCommand(Command command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!EnsureInit())
    return;

  switch (command) {
    case DO_START_LOG_BYTES:
      StartNetLog(LOG_TYPE_LOG_BYTES);
      break;
    case DO_START:
      StartNetLog(LOG_TYPE_NORMAL);
      break;
    case DO_START_STRIP_PRIVATE_DATA:
      StartNetLog(LOG_TYPE_STRIP_PRIVATE_DATA);
      break;
    case DO_STOP:
      StopNetLog();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool NetLogTempFile::GetFilePath(base::FilePath* path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (log_type_ == LOG_TYPE_NONE || state_ == STATE_LOGGING)
    return false;

  if (!NetExportLogExists())
    return false;

  DCHECK(!log_path_.empty());
#if defined(OS_POSIX)
  // Users, group and others can read, write and traverse.
  int mode = base::FILE_PERMISSION_MASK;
  base::SetPosixFilePermissions(log_path_, mode);
#endif  // defined(OS_POSIX)

  *path = log_path_;
  return true;
}

base::DictionaryValue* NetLogTempFile::GetState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::DictionaryValue* dict = new base::DictionaryValue;

  EnsureInit();

#ifndef NDEBUG
  dict->SetString("file", log_path_.LossyDisplayName());
#endif  // NDEBUG

  switch (state_) {
    case STATE_NOT_LOGGING:
      dict->SetString("state", "NOT_LOGGING");
      break;
    case STATE_LOGGING:
      dict->SetString("state", "LOGGING");
      break;
    case STATE_UNINITIALIZED:
      dict->SetString("state", "UNINITIALIZED");
      break;
  }

  switch (log_type_) {
    case LOG_TYPE_NONE:
      dict->SetString("logType", "NONE");
      break;
    case LOG_TYPE_UNKNOWN:
      dict->SetString("logType", "UNKNOWN");
      break;
    case LOG_TYPE_LOG_BYTES:
      dict->SetString("logType", "LOG_BYTES");
      break;
    case LOG_TYPE_NORMAL:
      dict->SetString("logType", "NORMAL");
      break;
    case LOG_TYPE_STRIP_PRIVATE_DATA:
      dict->SetString("logType", "STRIP_PRIVATE_DATA");
      break;
  }

  return dict;
}

NetLogTempFile::NetLogTempFile(
    ChromeNetLog* chrome_net_log,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string)
    : state_(STATE_UNINITIALIZED),
      log_type_(LOG_TYPE_NONE),
      chrome_net_log_(chrome_net_log),
      command_line_string_(command_line_string),
      channel_string_(channel_string) {
  // NetLogTempFile can be created on one thread and used on another.
  thread_checker_.DetachFromThread();
}

bool NetLogTempFile::GetNetExportLogBaseDirectory(base::FilePath* path) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::GetTempDir(path);
}

net::NetLogCaptureMode NetLogTempFile::GetCaptureModeForLogType(
    LogType log_type) {
  switch (log_type) {
    case LOG_TYPE_LOG_BYTES:
      return net::NetLogCaptureMode::IncludeSocketBytes();
    case LOG_TYPE_NORMAL:
      return net::NetLogCaptureMode::IncludeCookiesAndCredentials();
    case LOG_TYPE_STRIP_PRIVATE_DATA:
      return net::NetLogCaptureMode::Default();
    case LOG_TYPE_NONE:
    case LOG_TYPE_UNKNOWN:
      NOTREACHED();
  }
  return net::NetLogCaptureMode::Default();
}

bool NetLogTempFile::EnsureInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != STATE_UNINITIALIZED)
    return true;

  if (!SetUpNetExportLogPath())
    return false;

  state_ = STATE_NOT_LOGGING;
  if (NetExportLogExists())
    log_type_ = LOG_TYPE_UNKNOWN;
  else
    log_type_ = LOG_TYPE_NONE;

  return true;
}

void NetLogTempFile::StartNetLog(LogType log_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == STATE_LOGGING)
    return;

  DCHECK_NE(STATE_UNINITIALIZED, state_);
  DCHECK(!log_path_.empty());

  // Try to make sure we can create the file.
  // TODO(rtenneti): Find a better for doing the following. Surface some error
  // to the user if we couldn't create the file.
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  if (!file)
    return;

  log_type_ = log_type;
  state_ = STATE_LOGGING;

  std::unique_ptr<base::Value> constants(
      ChromeNetLog::GetConstants(command_line_string_, channel_string_));
  write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
  write_to_file_observer_->set_capture_mode(GetCaptureModeForLogType(log_type));
  write_to_file_observer_->StartObserving(chrome_net_log_, std::move(file),
                                          constants.get(), nullptr);
}

void NetLogTempFile::StopNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != STATE_LOGGING)
    return;

  write_to_file_observer_->StopObserving(nullptr);
  write_to_file_observer_.reset();
  state_ = STATE_NOT_LOGGING;
}

bool NetLogTempFile::SetUpNetExportLogPath() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath temp_dir;
  if (!GetNetExportLogBaseDirectory(&temp_dir))
    return false;

  // Delete log file at old location, if present.
  DeleteFile(temp_dir.Append(kOldLogRelativePath), false);

  base::FilePath log_path = temp_dir.Append(kLogRelativePath);

  if (!base::CreateDirectoryAndGetError(log_path.DirName(), nullptr)) {
    return false;
  }

  log_path_ = log_path;
  return true;
}

bool NetLogTempFile::NetExportLogExists() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!log_path_.empty());
  return base::PathExists(log_path_);
}

}  // namespace net_log
