// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/secure_hash.h"
#include "net/base/net_errors.h"

namespace content {

// This will initialize the entire array to zero.
const unsigned char BaseFile::kEmptySha256Hash[] = { 0 };

BaseFile::BaseFile(const base::FilePath& full_path,
                   const GURL& source_url,
                   const GURL& referrer_url,
                   int64 received_bytes,
                   bool calculate_hash,
                   const std::string& hash_state_bytes,
                   base::File file,
                   const net::BoundNetLog& bound_net_log)
    : full_path_(full_path),
      source_url_(source_url),
      referrer_url_(referrer_url),
      file_(file.Pass()),
      bytes_so_far_(received_bytes),
      start_tick_(base::TimeTicks::Now()),
      calculate_hash_(calculate_hash),
      detached_(false),
      bound_net_log_(bound_net_log) {
  memcpy(sha256_hash_, kEmptySha256Hash, crypto::kSHA256Length);
  if (calculate_hash_) {
    secure_hash_.reset(crypto::SecureHash::Create(crypto::SecureHash::SHA256));
    if ((bytes_so_far_ > 0) &&  // Not starting at the beginning.
        (!IsEmptyHash(hash_state_bytes))) {
      Pickle hash_state(hash_state_bytes.c_str(), hash_state_bytes.size());
      PickleIterator data_iterator(hash_state);
      secure_hash_->Deserialize(&data_iterator);
    }
  }
}

BaseFile::~BaseFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (detached_)
    Close();
  else
    Cancel();  // Will delete the file.
}

DownloadInterruptReason BaseFile::Initialize(
    const base::FilePath& default_directory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  if (full_path_.empty()) {
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
  }

  return Open();
}

DownloadInterruptReason BaseFile::AppendDataToFile(const char* data,
                                                   size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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

  if (calculate_hash_)
    secure_hash_->Update(data, data_len);

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason BaseFile::Rename(const base::FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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

  if (rename_result == DOWNLOAD_INTERRUPT_REASON_NONE) {
    full_path_ = new_path;
    // Re-open the file if we were still using it.
    if (was_in_progress)
      rename_result = Open();
  }

  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_RENAMED);
  return rename_result;
}

void BaseFile::Detach() {
  detached_ = true;
  bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DETACHED);
}

void BaseFile::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  bound_net_log_.AddEvent(net::NetLog::TYPE_CANCELLED);

  Close();

  if (!full_path_.empty()) {
    bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DELETED);
    base::DeleteFile(full_path_, false);
  }

  Detach();
}

void BaseFile::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (calculate_hash_)
    secure_hash_->Finish(sha256_hash_, crypto::kSHA256Length);

  Close();
}

void BaseFile::SetClientGuid(const std::string& guid) {
  client_guid_ = guid;
}

// OS_WIN, OS_MACOSX and OS_LINUX have specialized implementations.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
DownloadInterruptReason BaseFile::AnnotateWithSourceInformation() {
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}
#endif

bool BaseFile::GetHash(std::string* hash) {
  DCHECK(!detached_);
  hash->assign(reinterpret_cast<const char*>(sha256_hash_),
               sizeof(sha256_hash_));
  return (calculate_hash_ && !in_progress());
}

std::string BaseFile::GetHashState() {
  if (!calculate_hash_)
    return std::string();

  Pickle hash_state;
  if (!secure_hash_->Serialize(&hash_state))
    return std::string();

  return std::string(reinterpret_cast<const char*>(hash_state.data()),
                     hash_state.size());
}

// static
bool BaseFile::IsEmptyHash(const std::string& hash) {
  return (hash.size() == crypto::kSHA256Length &&
          0 == memcmp(hash.data(), kEmptySha256Hash, crypto::kSHA256Length));
}

std::string BaseFile::DebugString() const {
  return base::StringPrintf("{ source_url_ = \"%s\""
                            " full_path_ = \"%" PRFilePath "\""
                            " bytes_so_far_ = %" PRId64
                            " detached_ = %c }",
                            source_url_.spec().c_str(),
                            full_path_.value().c_str(),
                            bytes_so_far_,
                            detached_ ? 'T' : 'F');
}

DownloadInterruptReason BaseFile::Open() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_OPENED,
      base::Bind(&FileOpenedNetLogCallback, &full_path_, bytes_so_far_));

  // Create a new file if it is not provided.
  if (!file_.IsValid()) {
    file_.Initialize(
        full_path_, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
    if (!file_.IsValid()) {
      return LogNetError("Open",
                         net::FileErrorToNetError(file_.error_details()));
    }
  }

  // We may be re-opening the file after rename. Always make sure we're
  // writing at the end of the file.
  int64 file_size = file_.Seek(base::File::FROM_END, 0);
  if (file_size < 0) {
    logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
    ClearFile();
    return LogSystemError("Seek", error);
  } else if (file_size > bytes_so_far_) {
    // The file is larger than we expected.
    // This is OK, as long as we don't use the extra.
    // Truncate the file.
    if (!file_.SetLength(bytes_so_far_) ||
        file_.Seek(base::File::FROM_BEGIN, bytes_so_far_) != bytes_so_far_) {
      logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
      ClearFile();
      return LogSystemError("Truncate",  error);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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
  net::Error net_error = net::MapSystemError(os_error);
  return LogInterruptReason(
      operation, os_error,
      ConvertNetErrorToInterruptReason(
          net_error, DOWNLOAD_INTERRUPT_FROM_DISK));
}

DownloadInterruptReason BaseFile::LogInterruptReason(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason) {
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_ERROR,
      base::Bind(&FileInterruptedNetLogCallback, operation, os_error, reason));
  return reason;
}

}  // namespace content
