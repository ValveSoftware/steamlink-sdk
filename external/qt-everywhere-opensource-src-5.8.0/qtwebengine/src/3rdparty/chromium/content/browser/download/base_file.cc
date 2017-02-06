// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/secure_hash.h"
#include "net/base/net_errors.h"

namespace content {

BaseFile::BaseFile(const net::BoundNetLog& bound_net_log)
    : bound_net_log_(bound_net_log) {}

BaseFile::~BaseFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (detached_)
    Close();
  else
    Cancel();  // Will delete the file.
}

DownloadInterruptReason BaseFile::Initialize(
    const base::FilePath& full_path,
    const base::FilePath& default_directory,
    base::File file,
    int64_t bytes_so_far,
    const std::string& hash_so_far,
    std::unique_ptr<crypto::SecureHash> hash_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!detached_);

  if (full_path.empty()) {
    base::FilePath initial_directory(default_directory);
    base::FilePath temp_file;
    if (initial_directory.empty()) {
      initial_directory =
          GetContentClient()->browser()->GetDefaultDownloadDirectory();
    }
    // |initial_directory| can still be empty if ContentBrowserClient returned
    // an empty path for the downloads directory.
    if ((initial_directory.empty() ||
         !base::CreateTemporaryFileInDir(initial_directory, &temp_file)) &&
        !base::CreateTemporaryFile(&temp_file)) {
      return LogInterruptReason("Unable to create", 0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
    }
    full_path_ = temp_file;
  } else {
    full_path_ = full_path;
  }

  bytes_so_far_ = bytes_so_far;
  secure_hash_ = std::move(hash_state);
  file_ = std::move(file);

  return Open(hash_so_far);
}

DownloadInterruptReason BaseFile::AppendDataToFile(const char* data,
                                                   size_t data_len) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!detached_);

  // NOTE(benwells): The above DCHECK won't be present in release builds,
  // so we log any occurences to see how common this error is in the wild.
  if (detached_)
    RecordDownloadCount(APPEND_TO_DETACHED_FILE_COUNT);

  if (!file_.IsValid())
    return LogInterruptReason("No file stream on append", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);

  // TODO(phajdan.jr): get rid of this check.
  if (data_len == 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // The Write call below is not guaranteed to write all the data.
  size_t write_count = 0;
  size_t len = data_len;
  const char* current_data = data;
  while (len > 0) {
    write_count++;
    int write_result = file_.WriteAtCurrentPos(current_data, len);
    DCHECK_NE(0, write_result);

    // Report errors on file writes.
    if (write_result < 0)
      return LogSystemError("Write", logging::GetLastSystemErrorCode());

    // Update status.
    size_t write_size = static_cast<size_t>(write_result);
    DCHECK_LE(write_size, len);
    len -= write_size;
    current_data += write_size;
    bytes_so_far_ += write_size;
  }

  RecordDownloadWriteSize(data_len);
  RecordDownloadWriteLoopCount(write_count);

  if (secure_hash_)
    secure_hash_->Update(data, data_len);

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason BaseFile::Rename(const base::FilePath& new_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DownloadInterruptReason rename_result = DOWNLOAD_INTERRUPT_REASON_NONE;

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool was_in_progress = in_progress();

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_RENAMED,
      base::Bind(&FileRenamedNetLogCallback, &full_path_, &new_path));
  Close();
  base::CreateDirectory(new_path.DirName());

  // A simple rename wouldn't work here since we want the file to have
  // permissions / security descriptors that makes sense in the new directory.
  rename_result = MoveFileAndAdjustPermissions(new_path);

  if (rename_result == DOWNLOAD_INTERRUPT_REASON_NONE)
    full_path_ = new_path;

  // Re-open the file if we were still using it regardless of the interrupt
  // reason.
  DownloadInterruptReason open_result = DOWNLOAD_INTERRUPT_REASON_NONE;
  if (was_in_progress)
    open_result = Open(std::string());

  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_RENAMED);
  return rename_result == DOWNLOAD_INTERRUPT_REASON_NONE ? open_result
                                                         : rename_result;
}

void BaseFile::Detach() {
  detached_ = true;
  bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DETACHED);
}

void BaseFile::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!detached_);

  bound_net_log_.AddEvent(net::NetLog::TYPE_CANCELLED);

  Close();

  if (!full_path_.empty()) {
    bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DELETED);
    base::DeleteFile(full_path_, false);
  }

  Detach();
}

std::unique_ptr<crypto::SecureHash> BaseFile::Finish() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  Close();
  return std::move(secure_hash_);
}

// OS_WIN, OS_MACOSX and OS_LINUX have specialized implementations.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
DownloadInterruptReason BaseFile::AnnotateWithSourceInformation(
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url) {
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}
#endif

std::string BaseFile::DebugString() const {
  return base::StringPrintf(
      "{ "
      " full_path_ = \"%" PRFilePath
      "\""
      " bytes_so_far_ = %" PRId64 " detached_ = %c }",
      full_path_.value().c_str(),
      bytes_so_far_,
      detached_ ? 'T' : 'F');
}

DownloadInterruptReason BaseFile::CalculatePartialHash(
    const std::string& hash_to_expect) {
  secure_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  if (bytes_so_far_ == 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  if (file_.Seek(base::File::FROM_BEGIN, 0) != 0)
    return LogSystemError("Seek partial file",
                          logging::GetLastSystemErrorCode());

  const size_t kMinBufferSize = secure_hash_->GetHashLength();
  const size_t kMaxBufferSize = 1024 * 512;
  static_assert(kMaxBufferSize <= std::numeric_limits<int>::max(),
                "kMaxBufferSize must fit on an int");

  // The size of the buffer is:
  // - at least kMinBufferSize so that we can use it to hold the hash as well.
  // - at most kMaxBufferSize so that there's a reasonable bound.
  // - not larger than |bytes_so_far_| unless bytes_so_far_ is less than the
  //   hash size.
  std::vector<char> buffer(std::max<int64_t>(
      kMinBufferSize, std::min<int64_t>(kMaxBufferSize, bytes_so_far_)));

  int64_t current_position = 0;
  while (current_position < bytes_so_far_) {
    // While std::min needs to work with int64_t, the result is always at most
    // kMaxBufferSize, which fits on an int.
    int bytes_to_read =
        std::min<int64_t>(buffer.size(), bytes_so_far_ - current_position);
    int length = file_.ReadAtCurrentPos(&buffer.front(), bytes_to_read);
    if (length == -1) {
      return LogInterruptReason("Reading partial file",
                                logging::GetLastSystemErrorCode(),
                                DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
    }

    if (length == 0)
      break;

    secure_hash_->Update(&buffer.front(), length);
    current_position += length;
  }

  if (current_position != bytes_so_far_) {
    return LogInterruptReason(
        "Verifying prefix hash", 0, DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
  }

  if (!hash_to_expect.empty()) {
    DCHECK_EQ(secure_hash_->GetHashLength(), hash_to_expect.size());
    DCHECK(buffer.size() >= secure_hash_->GetHashLength());
    std::unique_ptr<crypto::SecureHash> partial_hash(secure_hash_->Clone());
    partial_hash->Finish(&buffer.front(), buffer.size());

    if (memcmp(&buffer.front(),
               hash_to_expect.c_str(),
               partial_hash->GetHashLength())) {
      return LogInterruptReason("Verifying prefix hash",
                                0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH);
    }
  }

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason BaseFile::Open(const std::string& hash_so_far) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  // Create a new file if it is not provided.
  if (!file_.IsValid()) {
    file_.Initialize(full_path_,
                     base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
                         base::File::FLAG_READ);
    if (!file_.IsValid()) {
      return LogNetError("Open/Initialize File",
                         net::FileErrorToNetError(file_.error_details()));
    }
  }

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_OPENED,
      base::Bind(&FileOpenedNetLogCallback, &full_path_, bytes_so_far_));

  if (!secure_hash_) {
    DownloadInterruptReason reason = CalculatePartialHash(hash_so_far);
    if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
      ClearFile();
      return reason;
    }
  }

  int64_t file_size = file_.Seek(base::File::FROM_END, 0);
  if (file_size < 0) {
    logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
    ClearFile();
    return LogSystemError("Seeking to end", error);
  } else if (file_size > bytes_so_far_) {
    // The file is larger than we expected.
    // This is OK, as long as we don't use the extra.
    // Truncate the file.
    if (!file_.SetLength(bytes_so_far_) ||
        file_.Seek(base::File::FROM_BEGIN, bytes_so_far_) != bytes_so_far_) {
      logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
      ClearFile();
      return LogSystemError("Truncating to last known offset", error);
    }
  } else if (file_size < bytes_so_far_) {
    // The file is shorter than we expected.  Our hashes won't be valid.
    ClearFile();
    return LogInterruptReason("Unable to seek to last written point", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
  }

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

void BaseFile::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_CLOSED);

  if (file_.IsValid()) {
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_.Flush();
    ClearFile();
  }
}

void BaseFile::ClearFile() {
  // This should only be called when we have a stream.
  DCHECK(file_.IsValid());
  file_.Close();
  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_OPENED);
}

DownloadInterruptReason BaseFile::LogNetError(
    const char* operation,
    net::Error error) {
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_ERROR,
      base::Bind(&FileErrorNetLogCallback, operation, error));
  return ConvertNetErrorToInterruptReason(error, DOWNLOAD_INTERRUPT_FROM_DISK);
}

DownloadInterruptReason BaseFile::LogSystemError(
    const char* operation,
    logging::SystemErrorCode os_error) {
  // There's no direct conversion from a system error to an interrupt reason.
  base::File::Error file_error = base::File::OSErrorToFileError(os_error);
  return LogInterruptReason(
      operation, os_error,
      ConvertFileErrorToInterruptReason(file_error));
}

DownloadInterruptReason BaseFile::LogInterruptReason(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason) {
  DVLOG(1) << __FUNCTION__ << "() operation:" << operation
           << " os_error:" << os_error
           << " reason:" << DownloadInterruptReasonToString(reason);
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_ERROR,
      base::Bind(&FileInterruptedNetLogCallback, operation, os_error, reason));
  return reason;
}

}  // namespace content
