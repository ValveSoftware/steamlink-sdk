// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// In tests, this value is used to override the return value of
// UploadFileElementReader::GetContentLength() when set to non-zero.
uint64 overriding_content_length = 0;

}  // namespace

UploadFileElementReader::UploadFileElementReader(
    base::TaskRunner* task_runner,
    const base::FilePath& path,
    uint64 range_offset,
    uint64 range_length,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      content_length_(0),
      bytes_remaining_(0),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_.get());
}

UploadFileElementReader::~UploadFileElementReader() {
}

const UploadFileElementReader* UploadFileElementReader::AsFileReader() const {
  return this;
}

int UploadFileElementReader::Init(const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  Reset();

  file_stream_.reset(new FileStream(task_runner_.get()));
  int result = file_stream_->Open(
      path_,
      base::File::FLAG_OPEN | base::File::FLAG_READ |
      base::File::FLAG_ASYNC,
      base::Bind(&UploadFileElementReader::OnOpenCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  DCHECK_GT(0, result);
  return result;
}

uint64 UploadFileElementReader::GetContentLength() const {
  if (overriding_content_length)
    return overriding_content_length;
  return content_length_;
}

uint64 UploadFileElementReader::BytesRemaining() const {
  return bytes_remaining_;
}

int UploadFileElementReader::Read(IOBuffer* buf,
                                  int buf_length,
                                  const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  uint64 num_bytes_to_read =
      std::min(BytesRemaining(), static_cast<uint64>(buf_length));
  if (num_bytes_to_read == 0)
    return 0;

  int result = file_stream_->Read(
      buf, num_bytes_to_read,
      base::Bind(base::IgnoreResult(&UploadFileElementReader::OnReadCompleted),
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  // Even in async mode, FileStream::Read() may return the result synchronously.
  if (result != ERR_IO_PENDING)
    return OnReadCompleted(CompletionCallback(), result);
  return ERR_IO_PENDING;
}

void UploadFileElementReader::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  bytes_remaining_ = 0;
  content_length_ = 0;
  file_stream_.reset();
}

void UploadFileElementReader::OnOpenCompleted(
    const CompletionCallback& callback,
    int result) {
  DCHECK(!callback.is_null());

  if (result < 0) {
    DLOG(WARNING) << "Failed to open \"" << path_.value()
                  << "\" for reading: " << result;
    callback.Run(result);
    return;
  }

  if (range_offset_) {
    int result = file_stream_->Seek(
        FROM_BEGIN, range_offset_,
        base::Bind(&UploadFileElementReader::OnSeekCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
    DCHECK_GT(0, result);
    if (result != ERR_IO_PENDING)
      callback.Run(result);
  } else {
    OnSeekCompleted(callback, OK);
  }
}

void UploadFileElementReader::OnSeekCompleted(
    const CompletionCallback& callback,
    int64 result) {
  DCHECK(!callback.is_null());

  if (result < 0) {
    DLOG(WARNING) << "Failed to seek \"" << path_.value()
                  << "\" to offset: " << range_offset_ << " (" << result << ")";
    callback.Run(result);
    return;
  }

  base::File::Info* file_info = new base::File::Info;
  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_,
      FROM_HERE,
      base::Bind(&base::GetFileInfo,
                 path_,
                 file_info),
      base::Bind(&UploadFileElementReader::OnGetFileInfoCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(file_info)));
  DCHECK(posted);
}

void UploadFileElementReader::OnGetFileInfoCompleted(
    const CompletionCallback& callback,
    base::File::Info* file_info,
    bool result) {
  DCHECK(!callback.is_null());
  if (!result) {
    DLOG(WARNING) << "Failed to get file info of \"" << path_.value() << "\"";
    callback.Run(ERR_FILE_NOT_FOUND);
    return;
  }

  int64 length = file_info->size;
  if (range_offset_ < static_cast<uint64>(length)) {
    // Compensate for the offset.
    length = std::min(length - range_offset_, range_length_);
  }

  // If the underlying file has been changed and the expected file modification
  // time is set, treat it as error. Note that the expected modification time
  // from WebKit is based on time_t precision. So we have to convert both to
  // time_t to compare. This check is used for sliced files.
  if (!expected_modification_time_.is_null() &&
      expected_modification_time_.ToTimeT() !=
      file_info->last_modified.ToTimeT()) {
    callback.Run(ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  content_length_ = length;
  bytes_remaining_ = GetContentLength();
  callback.Run(OK);
}

int UploadFileElementReader::OnReadCompleted(
    const CompletionCallback& callback,
    int result) {
  if (result == 0)  // Reached end-of-file earlier than expected.
    result = ERR_UPLOAD_FILE_CHANGED;

  if (result > 0) {
    DCHECK_GE(bytes_remaining_, static_cast<uint64>(result));
    bytes_remaining_ -= result;
  }

  if (!callback.is_null())
    callback.Run(result);
  return result;
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
ScopedOverridingContentLengthForTests(uint64 value) {
  overriding_content_length = value;
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
~ScopedOverridingContentLengthForTests() {
  overriding_content_length = 0;
}

}  // namespace net
