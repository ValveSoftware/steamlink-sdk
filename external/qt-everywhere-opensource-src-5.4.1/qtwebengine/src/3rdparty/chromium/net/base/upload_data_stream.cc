// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"

namespace net {

UploadDataStream::UploadDataStream(
    ScopedVector<UploadElementReader> element_readers,
    int64 identifier)
    : element_readers_(element_readers.Pass()),
      element_index_(0),
      total_size_(0),
      current_position_(0),
      identifier_(identifier),
      is_chunked_(false),
      last_chunk_appended_(false),
      read_failed_(false),
      initialized_successfully_(false),
      weak_ptr_factory_(this) {
}

UploadDataStream::UploadDataStream(Chunked /*chunked*/, int64 identifier)
    : element_index_(0),
      total_size_(0),
      current_position_(0),
      identifier_(identifier),
      is_chunked_(true),
      last_chunk_appended_(false),
      read_failed_(false),
      initialized_successfully_(false),
      weak_ptr_factory_(this) {
}

UploadDataStream::~UploadDataStream() {
}

UploadDataStream* UploadDataStream::CreateWithReader(
    scoped_ptr<UploadElementReader> reader,
    int64 identifier) {
  ScopedVector<UploadElementReader> readers;
  readers.push_back(reader.release());
  return new UploadDataStream(readers.Pass(), identifier);
}

int UploadDataStream::Init(const CompletionCallback& callback) {
  Reset();
  return InitInternal(0, callback);
}

int UploadDataStream::Read(IOBuffer* buf,
                           int buf_len,
                           const CompletionCallback& callback) {
  DCHECK(initialized_successfully_);
  DCHECK_GT(buf_len, 0);
  return ReadInternal(new DrainableIOBuffer(buf, buf_len), callback);
}

bool UploadDataStream::IsEOF() const {
  DCHECK(initialized_successfully_);
  if (!is_chunked_)
    return current_position_ == total_size_;

  // If the upload data is chunked, check if the last chunk is appended and all
  // elements are consumed.
  return element_index_ == element_readers_.size() && last_chunk_appended_;
}

bool UploadDataStream::IsInMemory() const {
  // Chunks are in memory, but UploadData does not have all the chunks at
  // once. Chunks are provided progressively with AppendChunk() as chunks
  // are ready. Check is_chunked_ here, rather than relying on the loop
  // below, as there is a case that is_chunked_ is set to true, but the
  // first chunk is not yet delivered.
  if (is_chunked_)
    return false;

  for (size_t i = 0; i < element_readers_.size(); ++i) {
    if (!element_readers_[i]->IsInMemory())
      return false;
  }
  return true;
}

void UploadDataStream::AppendChunk(const char* bytes,
                                   int bytes_len,
                                   bool is_last_chunk) {
  DCHECK(is_chunked_);
  DCHECK(!last_chunk_appended_);
  last_chunk_appended_ = is_last_chunk;

  // Initialize a reader for the newly appended chunk. We leave |total_size_| at
  // zero, since for chunked uploads, we may not know the total size.
  std::vector<char> data(bytes, bytes + bytes_len);
  UploadElementReader* reader = new UploadOwnedBytesElementReader(&data);
  const int rv = reader->Init(net::CompletionCallback());
  DCHECK_EQ(OK, rv);
  element_readers_.push_back(reader);

  // Resume pending read.
  if (!pending_chunked_read_callback_.is_null()) {
    base::Closure callback = pending_chunked_read_callback_;
    pending_chunked_read_callback_.Reset();
    callback.Run();
  }
}

void UploadDataStream::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_chunked_read_callback_.Reset();
  initialized_successfully_ = false;
  read_failed_ = false;
  current_position_ = 0;
  total_size_ = 0;
  element_index_ = 0;
}

int UploadDataStream::InitInternal(int start_index,
                                   const CompletionCallback& callback) {
  DCHECK(!initialized_successfully_);

  // Call Init() for all elements.
  for (size_t i = start_index; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    // When new_result is ERR_IO_PENDING, InitInternal() will be called
    // with start_index == i + 1 when reader->Init() finishes.
    const int result = reader->Init(
        base::Bind(&UploadDataStream::ResumePendingInit,
                   weak_ptr_factory_.GetWeakPtr(),
                   i + 1,
                   callback));
    if (result != OK) {
      DCHECK(result != ERR_IO_PENDING || !callback.is_null());
      return result;
    }
  }

  // Finalize initialization.
  if (!is_chunked_) {
    uint64 total_size = 0;
    for (size_t i = 0; i < element_readers_.size(); ++i) {
      UploadElementReader* reader = element_readers_[i];
      total_size += reader->GetContentLength();
    }
    total_size_ = total_size;
  }
  initialized_successfully_ = true;
  return OK;
}

void UploadDataStream::ResumePendingInit(int start_index,
                                         const CompletionCallback& callback,
                                         int previous_result) {
  DCHECK(!initialized_successfully_);
  DCHECK(!callback.is_null());
  DCHECK_NE(ERR_IO_PENDING, previous_result);

  // Check the last result.
  if (previous_result != OK) {
    callback.Run(previous_result);
    return;
  }

  const int result = InitInternal(start_index, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

int UploadDataStream::ReadInternal(scoped_refptr<DrainableIOBuffer> buf,
                                   const CompletionCallback& callback) {
  DCHECK(initialized_successfully_);

  while (!read_failed_ && element_index_ < element_readers_.size()) {
    UploadElementReader* reader = element_readers_[element_index_];

    if (reader->BytesRemaining() == 0) {
      ++element_index_;
      continue;
    }

    if (buf->BytesRemaining() == 0)
      break;

    int result = reader->Read(
        buf.get(),
        buf->BytesRemaining(),
        base::Bind(base::IgnoreResult(&UploadDataStream::ResumePendingRead),
                   weak_ptr_factory_.GetWeakPtr(),
                   buf,
                   callback));
    if (result == ERR_IO_PENDING) {
      DCHECK(!callback.is_null());
      return ERR_IO_PENDING;
    }
    ProcessReadResult(buf, result);
  }

  if (read_failed_) {
    // Chunked transfers may only contain byte readers, so cannot have read
    // failures.
    DCHECK(!is_chunked_);

    // If an error occured during read operation, then pad with zero.
    // Otherwise the server will hang waiting for the rest of the data.
    const int num_bytes_to_fill =
        std::min(static_cast<uint64>(buf->BytesRemaining()),
                 size() - position() - buf->BytesConsumed());
    DCHECK_LE(0, num_bytes_to_fill);
    memset(buf->data(), 0, num_bytes_to_fill);
    buf->DidConsume(num_bytes_to_fill);
  }

  const int bytes_copied = buf->BytesConsumed();
  current_position_ += bytes_copied;
  DCHECK(is_chunked_ || total_size_ >= current_position_);

  if (is_chunked_ && !IsEOF() && bytes_copied == 0) {
    DCHECK(!callback.is_null());
    DCHECK(pending_chunked_read_callback_.is_null());
    pending_chunked_read_callback_ =
        base::Bind(&UploadDataStream::ResumePendingRead,
                   weak_ptr_factory_.GetWeakPtr(),
                   buf,
                   callback,
                   OK);
    return ERR_IO_PENDING;
  }

  // Returning 0 is allowed only when IsEOF() == true.
  DCHECK(bytes_copied != 0 || IsEOF());
  return bytes_copied;
}

void UploadDataStream::ResumePendingRead(scoped_refptr<DrainableIOBuffer> buf,
                                         const CompletionCallback& callback,
                                         int previous_result) {
  DCHECK(!callback.is_null());

  ProcessReadResult(buf, previous_result);

  const int result = ReadInternal(buf, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

void UploadDataStream::ProcessReadResult(scoped_refptr<DrainableIOBuffer> buf,
                                         int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!read_failed_);

  if (result >= 0)
    buf->DidConsume(result);
  else
    read_failed_ = true;
}

}  // namespace net
