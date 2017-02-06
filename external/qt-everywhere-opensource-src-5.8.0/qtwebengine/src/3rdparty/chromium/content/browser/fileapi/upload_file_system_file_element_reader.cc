// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/upload_file_system_file_element_reader.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/net_errors.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace content {

UploadFileSystemFileElementReader::UploadFileSystemFileElementReader(
    storage::FileSystemContext* file_system_context,
    const GURL& url,
    uint64_t range_offset,
    uint64_t range_length,
    const base::Time& expected_modification_time)
    : file_system_context_(file_system_context),
      url_(url),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      stream_length_(0),
      position_(0),
      weak_ptr_factory_(this) {}

UploadFileSystemFileElementReader::~UploadFileSystemFileElementReader() {
}

int UploadFileSystemFileElementReader::Init(
    const net::CompletionCallback& callback) {
  // Reset states.
  weak_ptr_factory_.InvalidateWeakPtrs();
  stream_length_ = 0;
  position_ = 0;

  // Initialize the stream reader and the length.
  stream_reader_ = file_system_context_->CreateFileStreamReader(
      file_system_context_->CrackURL(url_), range_offset_,
      range_length_ == std::numeric_limits<uint64_t>::max()
          ? storage::kMaximumLength
          : base::checked_cast<int64_t>(range_length_),
      expected_modification_time_);
  DCHECK(stream_reader_);

  const int64_t result = stream_reader_->GetLength(
      base::Bind(&UploadFileSystemFileElementReader::OnGetLength,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  if (result >= 0) {
    stream_length_ = result;
    return net::OK;
  }

  // The error code can be casted to int.
  return static_cast<int>(result);
}

uint64_t UploadFileSystemFileElementReader::GetContentLength() const {
  return std::min(stream_length_, range_length_);
}

uint64_t UploadFileSystemFileElementReader::BytesRemaining() const {
  return GetContentLength() - position_;
}

int UploadFileSystemFileElementReader::Read(
    net::IOBuffer* buf,
    int buf_length,
    const net::CompletionCallback& callback) {
  DCHECK_LT(0, buf_length);
  DCHECK(stream_reader_);

  const uint64_t num_bytes_to_read =
      std::min(BytesRemaining(), static_cast<uint64_t>(buf_length));

  if (num_bytes_to_read == 0)
    return 0;

  const int result = stream_reader_->Read(
      buf, num_bytes_to_read,
      base::Bind(&UploadFileSystemFileElementReader::OnRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  if (result >= 0)
    OnRead(net::CompletionCallback(), result);
  return result;
}

void UploadFileSystemFileElementReader::OnGetLength(
    const net::CompletionCallback& callback,
    int64_t result) {
  if (result >= 0) {
    stream_length_ = result;
    callback.Run(net::OK);
    return;
  }
  callback.Run(result);
}

void UploadFileSystemFileElementReader::OnRead(
    const net::CompletionCallback& callback,
    int result) {
  if (result > 0) {
    position_ += result;
    DCHECK_LE(position_, GetContentLength());
  }
  if (!callback.is_null())
    callback.Run(result);
}

}  // namespace content
