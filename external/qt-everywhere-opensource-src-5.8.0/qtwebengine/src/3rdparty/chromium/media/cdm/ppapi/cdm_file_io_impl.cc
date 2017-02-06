// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/cdm_file_io_impl.h"

#include <sstream>

#include "media/cdm/ppapi/cdm_logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/url_util_dev.h"

namespace media {

// Arbitrary choice based on the following heuristic ideas:
// - not too big to avoid unnecessarily large memory allocation;
// - not too small to avoid breaking most reads into multiple read operations.
const int kReadSize = 8 * 1024;

// Call func_call and check the result. If the result is not
// PP_OK_COMPLETIONPENDING, print out logs, call OnError() and return.
#define CHECK_PP_OK_COMPLETIONPENDING(func_call, error_type)            \
  do {                                                                  \
    int32_t result = func_call;                                         \
    PP_DCHECK(result != PP_OK);                                         \
    if (result != PP_OK_COMPLETIONPENDING) {                            \
      CDM_DLOG() << #func_call << " failed with result: " << result;    \
      state_ = STATE_ERROR;                                             \
      OnError(error_type);                                              \
      return;                                                           \
    }                                                                   \
  } while (0)

#if !defined(NDEBUG)
// PPAPI calls should only be made on the main thread. In this file, main thread
// checking is only performed in public APIs and the completion callbacks. This
// ensures all functions are running on the main thread since internal methods
// are called either by the public APIs or by the completion callbacks.
static bool IsMainThread() {
  return pp::Module::Get()->core()->IsMainThread();
}
#endif  // !defined(NDEBUG)

// Posts a task to run |cb| on the main thread. The task is posted even if the
// current thread is the main thread.
static void PostOnMain(pp::CompletionCallback cb) {
  pp::Module::Get()->core()->CallOnMainThread(0, cb, PP_OK);
}

CdmFileIOImpl::FileLockMap* CdmFileIOImpl::file_lock_map_ = NULL;

CdmFileIOImpl::ResourceTracker::ResourceTracker() {
  // Do nothing here since we lazy-initialize CdmFileIOImpl::file_lock_map_
  // in CdmFileIOImpl::AcquireFileLock().
}

CdmFileIOImpl::ResourceTracker::~ResourceTracker() {
  delete CdmFileIOImpl::file_lock_map_;
}

CdmFileIOImpl::CdmFileIOImpl(
    cdm::FileIOClient* client,
    PP_Instance pp_instance,
    const pp::CompletionCallback& first_file_read_cb)
    : state_(STATE_UNOPENED),
      client_(client),
      pp_instance_handle_(pp_instance),
      io_offset_(0),
      first_file_read_reported_(false),
      first_file_read_cb_(first_file_read_cb),
      callback_factory_(this) {
  PP_DCHECK(IsMainThread());
  PP_DCHECK(pp_instance);  // 0 indicates a "NULL handle".
}

CdmFileIOImpl::~CdmFileIOImpl() {
  // The destructor is private. |this| can only be destructed through Close().
  PP_DCHECK(state_ == STATE_CLOSED);
}

// Call sequence: Open() -> OpenFileSystem() -> STATE_FILE_SYSTEM_OPENED.
// Note: This only stores file name and opens the file system. The real file
// open is deferred to when Read() or Write() is called.
void CdmFileIOImpl::Open(const char* file_name, uint32_t file_name_size) {
  CDM_DLOG() << __FUNCTION__;
  PP_DCHECK(IsMainThread());

  if (state_ != STATE_UNOPENED) {
    CDM_DLOG() << "Open() called in an invalid state.";
    OnError(OPEN_ERROR);
    return;
  }

  // File name should not (1) be empty, (2) start with '_', or (3) contain any
  // path separators.
  std::string file_name_str(file_name, file_name_size);
  if (file_name_str.empty() ||
      file_name_str[0] == '_' ||
      file_name_str.find('/') != std::string::npos ||
      file_name_str.find('\\') != std::string::npos) {
    CDM_DLOG() << "Invalid file name.";
    state_ = STATE_ERROR;
    OnError(OPEN_ERROR);
    return;
  }

  // pp::FileRef only accepts path that begins with a '/' character.
  file_name_ = '/' + file_name_str;

  if (!AcquireFileLock()) {
    CDM_DLOG() << "File is in use by other cdm::FileIO objects.";
    OnError(OPEN_WHILE_IN_USE);
    return;
  }

  state_ = STATE_OPENING_FILE_SYSTEM;
  OpenFileSystem();
}

// Call sequence:
// Read() -> OpenFileForRead() -> ReadFile() -> Done.
void CdmFileIOImpl::Read() {
  CDM_DLOG() << __FUNCTION__;
  PP_DCHECK(IsMainThread());

  if (state_ == STATE_READING || state_ == STATE_WRITING) {
    CDM_DLOG() << "Read() called during pending read/write.";
    OnError(READ_WHILE_IN_USE);
    return;
  }

  if (state_ != STATE_FILE_SYSTEM_OPENED) {
    CDM_DLOG() << "Read() called in an invalid state.";
    OnError(READ_ERROR);
    return;
  }

  PP_DCHECK(io_offset_ == 0);
  PP_DCHECK(io_buffer_.empty());
  PP_DCHECK(cumulative_read_buffer_.empty());
  io_buffer_.resize(kReadSize);
  io_offset_ = 0;

  state_ = STATE_READING;
  OpenFileForRead();
}

// Call sequence:
// Write() -> OpenTempFileForWrite() -> WriteTempFile() -> RenameTempFile().
// The file name of the temporary file is /_<requested_file_name>.
void CdmFileIOImpl::Write(const uint8_t* data, uint32_t data_size) {
  CDM_DLOG() << __FUNCTION__;
  PP_DCHECK(IsMainThread());

  if (state_ == STATE_READING || state_ == STATE_WRITING) {
    CDM_DLOG() << "Write() called during pending read/write.";
    OnError(WRITE_WHILE_IN_USE);
    return;
  }

  if (state_ != STATE_FILE_SYSTEM_OPENED) {
    CDM_DLOG() << "Write() called in an invalid state.";
    OnError(WRITE_ERROR);
    return;
  }

  PP_DCHECK(io_offset_ == 0);
  PP_DCHECK(io_buffer_.empty());
  if (data_size > 0)
    io_buffer_.assign(data, data + data_size);
  else
    PP_DCHECK(!data);

  state_ = STATE_WRITING;
  OpenTempFileForWrite();
}

void CdmFileIOImpl::Close() {
  CDM_DLOG() << __FUNCTION__;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ != STATE_CLOSED);
  Reset();
  state_ = STATE_CLOSED;
  ReleaseFileLock();
  // All pending callbacks are canceled since |callback_factory_| is destroyed.
  delete this;
}

bool CdmFileIOImpl::SetFileID() {
  PP_DCHECK(file_id_.empty());
  PP_DCHECK(!file_name_.empty() && file_name_[0] == '/');

  // Not taking ownership of |url_util_dev| (which is a singleton).
  const pp::URLUtil_Dev* url_util_dev = pp::URLUtil_Dev::Get();
  PP_URLComponents_Dev components;
  pp::Var url_var =
      url_util_dev->GetDocumentURL(pp_instance_handle_, &components);
  if (!url_var.is_string())
    return false;
  std::string url = url_var.AsString();

  file_id_.append(url, components.scheme.begin, components.scheme.len);
  file_id_ += ':';
  file_id_.append(url, components.host.begin, components.host.len);
  file_id_ += ':';
  file_id_.append(url, components.port.begin, components.port.len);
  file_id_ += file_name_;

  return true;
}

bool CdmFileIOImpl::AcquireFileLock() {
  PP_DCHECK(IsMainThread());

  if (file_id_.empty() && !SetFileID())
    return false;

  if (!file_lock_map_) {
    file_lock_map_ = new FileLockMap();
  } else {
    FileLockMap::iterator found = file_lock_map_->find(file_id_);
    if (found != file_lock_map_->end() && found->second)
      return false;
  }

  (*file_lock_map_)[file_id_] = true;
  return true;
}

void CdmFileIOImpl::ReleaseFileLock() {
  PP_DCHECK(IsMainThread());

  if (!file_lock_map_)
    return;

  FileLockMap::iterator found = file_lock_map_->find(file_id_);
  if (found != file_lock_map_->end() && found->second)
    found->second = false;
}

void CdmFileIOImpl::OpenFileSystem() {
  PP_DCHECK(state_ == STATE_OPENING_FILE_SYSTEM);

  pp::CompletionCallbackWithOutput<pp::FileSystem> cb =
      callback_factory_.NewCallbackWithOutput(
          &CdmFileIOImpl::OnFileSystemOpened);
  isolated_file_system_ = pp::IsolatedFileSystemPrivate(
      pp_instance_handle_, PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_PLUGINPRIVATE);

  CHECK_PP_OK_COMPLETIONPENDING(isolated_file_system_.Open(cb), OPEN_ERROR);
}

void CdmFileIOImpl::OnFileSystemOpened(int32_t result,
                                       pp::FileSystem file_system) {
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_OPENING_FILE_SYSTEM);

  if (result != PP_OK) {
    CDM_DLOG() << "File system open failed asynchronously.";
    ReleaseFileLock();
    state_ = STATE_ERROR;
    OnError(OPEN_ERROR);
    return;
  }

  file_system_ = file_system;

  state_ = STATE_FILE_SYSTEM_OPENED;
  client_->OnOpenComplete(cdm::FileIOClient::kSuccess);
}

void CdmFileIOImpl::OpenFileForRead() {
  PP_DCHECK(state_ == STATE_READING);

  PP_DCHECK(file_io_.is_null());
  PP_DCHECK(file_ref_.is_null());
  file_io_ = pp::FileIO(pp_instance_handle_);
  file_ref_ = pp::FileRef(file_system_, file_name_.c_str());

  // Open file for read. If file doesn't exist, PP_ERROR_FILENOTFOUND will be
  // returned.
  int32_t file_open_flag = PP_FILEOPENFLAG_READ;

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&CdmFileIOImpl::OnFileOpenedForRead);
  CHECK_PP_OK_COMPLETIONPENDING(file_io_.Open(file_ref_, file_open_flag, cb),
                                READ_ERROR);
}

void CdmFileIOImpl::OnFileOpenedForRead(int32_t result) {
  CDM_DLOG() << __FUNCTION__ << ": " << result;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_READING);

  if (result != PP_OK && result != PP_ERROR_FILENOTFOUND) {
    CDM_DLOG() << "File open failed.";
    state_ = STATE_ERROR;
    OnError(OPEN_ERROR);
    return;
  }

  // File doesn't exist.
  if (result == PP_ERROR_FILENOTFOUND) {
    Reset();
    state_ = STATE_FILE_SYSTEM_OPENED;
    client_->OnReadComplete(cdm::FileIOClient::kSuccess, NULL, 0);
    return;
  }

  ReadFile();
}

// Call sequence:
//                               fully read
// ReadFile() ---> OnFileRead() ------------> Done.
//     ^                |
//     | partially read |
//     |----------------|
void CdmFileIOImpl::ReadFile() {
  PP_DCHECK(state_ == STATE_READING);
  PP_DCHECK(!io_buffer_.empty());

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&CdmFileIOImpl::OnFileRead);
  CHECK_PP_OK_COMPLETIONPENDING(
      file_io_.Read(io_offset_, &io_buffer_[0], io_buffer_.size(), cb),
      READ_ERROR);
}

void CdmFileIOImpl::OnFileRead(int32_t bytes_read) {
  CDM_DLOG() << __FUNCTION__ << ": " << bytes_read;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_READING);

  // 0 |bytes_read| indicates end-of-file reached.
  if (bytes_read < PP_OK) {
    CDM_DLOG() << "Read file failed.";
    state_ = STATE_ERROR;
    OnError(READ_ERROR);
    return;
  }

  PP_DCHECK(static_cast<size_t>(bytes_read) <= io_buffer_.size());
  // Append |bytes_read| in |io_buffer_| to |cumulative_read_buffer_|.
  cumulative_read_buffer_.insert(cumulative_read_buffer_.end(),
                                 io_buffer_.begin(),
                                 io_buffer_.begin() + bytes_read);
  io_offset_ += bytes_read;

  // Not received end-of-file yet. Keep reading.
  if (bytes_read > 0) {
    ReadFile();
    return;
  }

  // We hit end-of-file. Return read data to the client.

  // Clear |cumulative_read_buffer_| in case OnReadComplete() calls Read() or
  // Write().
  std::vector<char> local_buffer;
  std::swap(cumulative_read_buffer_, local_buffer);

  const uint8_t* data = local_buffer.empty() ?
      NULL : reinterpret_cast<const uint8_t*>(&local_buffer[0]);

  // Call this before OnReadComplete() so that we always have the latest file
  // size before CDM fires errors.
  if (!first_file_read_reported_) {
    first_file_read_cb_.Run(local_buffer.size());
    first_file_read_reported_ = true;
  }

  Reset();

  state_ = STATE_FILE_SYSTEM_OPENED;
  client_->OnReadComplete(
      cdm::FileIOClient::kSuccess, data, local_buffer.size());
}

void CdmFileIOImpl::OpenTempFileForWrite() {
  PP_DCHECK(state_ == STATE_WRITING);

  PP_DCHECK(file_name_.size() > 1 && file_name_[0] == '/');
  // Temporary file name format: /_<requested_file_name>
  std::string temp_file_name = "/_" + file_name_.substr(1);

  PP_DCHECK(file_io_.is_null());
  PP_DCHECK(file_ref_.is_null());
  file_io_ = pp::FileIO(pp_instance_handle_);
  file_ref_ = pp::FileRef(file_system_, temp_file_name.c_str());

  // Create the file if it doesn't exist. Truncate the file to length 0 if it
  // exists.
  // TODO(xhwang): Find a good way to report to UMA cases where the temporary
  // file already exists (due to previous interruption or failure).
  int32_t file_open_flag = PP_FILEOPENFLAG_WRITE |
                           PP_FILEOPENFLAG_TRUNCATE |
                           PP_FILEOPENFLAG_CREATE;

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&CdmFileIOImpl::OnTempFileOpenedForWrite);
  CHECK_PP_OK_COMPLETIONPENDING(
      file_io_.Open(file_ref_, file_open_flag, cb), WRITE_ERROR);
}

void CdmFileIOImpl::OnTempFileOpenedForWrite(int32_t result) {
  CDM_DLOG() << __FUNCTION__ << ": " << result;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_WRITING);

  if (result != PP_OK) {
    CDM_DLOG() << "Open temporary file failed.";
    state_ = STATE_ERROR;
    OnError(WRITE_ERROR);
    return;
  }

  // We were told to write 0 bytes (to clear the file). In this case, there's
  // no need to write anything.
  if (io_buffer_.empty()) {
    RenameTempFile();
    return;
  }

  PP_DCHECK(io_offset_ == 0);
  io_offset_ = 0;
  WriteTempFile();
}

// Call sequence:
//                                         fully written
// WriteTempFile() -> OnTempFileWritten() ---------------> RenameTempFile().
//      ^                     |
//      |  partially written  |
//      |---------------------|
void CdmFileIOImpl::WriteTempFile() {
  PP_DCHECK(state_ == STATE_WRITING);
  PP_DCHECK(io_offset_ < io_buffer_.size());

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&CdmFileIOImpl::OnTempFileWritten);
  CHECK_PP_OK_COMPLETIONPENDING(file_io_.Write(io_offset_,
                                               &io_buffer_[io_offset_],
                                               io_buffer_.size() - io_offset_,
                                               cb),
                                WRITE_ERROR);
}

void CdmFileIOImpl::OnTempFileWritten(int32_t bytes_written) {
  CDM_DLOG() << __FUNCTION__ << ": " << bytes_written;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_WRITING);

  if (bytes_written <= PP_OK) {
    CDM_DLOG() << "Write temporary file failed.";
    state_ = STATE_ERROR;
    OnError(WRITE_ERROR);
    return;
  }

  io_offset_ += bytes_written;
  PP_DCHECK(io_offset_ <= io_buffer_.size());

  if (io_offset_ < io_buffer_.size()) {
    WriteTempFile();
    return;
  }

  // All data written. Now rename the temporary file to the real file.
  RenameTempFile();
}

void CdmFileIOImpl::RenameTempFile() {
  PP_DCHECK(state_ == STATE_WRITING);

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(&CdmFileIOImpl::OnTempFileRenamed);
  CHECK_PP_OK_COMPLETIONPENDING(
      file_ref_.Rename(pp::FileRef(file_system_, file_name_.c_str()), cb),
      WRITE_ERROR);
}

void CdmFileIOImpl::OnTempFileRenamed(int32_t result) {
  CDM_DLOG() << __FUNCTION__ << ": " << result;
  PP_DCHECK(IsMainThread());
  PP_DCHECK(state_ == STATE_WRITING);

  if (result != PP_OK) {
    CDM_DLOG() << "Rename temporary file failed.";
    state_ = STATE_ERROR;
    OnError(WRITE_ERROR);
    return;
  }

  Reset();

  state_ = STATE_FILE_SYSTEM_OPENED;
  client_->OnWriteComplete(cdm::FileIOClient::kSuccess);
}

void CdmFileIOImpl::Reset() {
  PP_DCHECK(IsMainThread());
  io_buffer_.clear();
  io_offset_ = 0;
  cumulative_read_buffer_.clear();
  file_io_.Close();
  file_io_ = pp::FileIO();
  file_ref_ = pp::FileRef();
}

void CdmFileIOImpl::OnError(ErrorType error_type) {
  // For *_WHILE_IN_USE errors, do not reset these values. Otherwise, the
  // existing read/write operation will fail.
  if (error_type == READ_ERROR || error_type == WRITE_ERROR)
    Reset();

  PostOnMain(callback_factory_.NewCallback(&CdmFileIOImpl::NotifyClientOfError,
                                           error_type));
}

void CdmFileIOImpl::NotifyClientOfError(int32_t result,
                                        ErrorType error_type) {
  PP_DCHECK(result == PP_OK);
  switch (error_type) {
    case OPEN_ERROR:
      client_->OnOpenComplete(cdm::FileIOClient::kError);
      break;
    case READ_ERROR:
      client_->OnReadComplete(cdm::FileIOClient::kError, NULL, 0);
      break;
    case WRITE_ERROR:
      client_->OnWriteComplete(cdm::FileIOClient::kError);
      break;
    case OPEN_WHILE_IN_USE:
      client_->OnOpenComplete(cdm::FileIOClient::kInUse);
      break;
    case READ_WHILE_IN_USE:
      client_->OnReadComplete(cdm::FileIOClient::kInUse, NULL, 0);
      break;
    case WRITE_WHILE_IN_USE:
      client_->OnWriteComplete(cdm::FileIOClient::kInUse);
      break;
  }
}

}  // namespace media
