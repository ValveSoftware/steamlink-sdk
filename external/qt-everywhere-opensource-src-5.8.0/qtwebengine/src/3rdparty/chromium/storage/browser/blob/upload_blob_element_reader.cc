// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/upload_blob_element_reader.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace storage {

UploadBlobElementReader::UploadBlobElementReader(
    std::unique_ptr<BlobDataHandle> handle,
    FileSystemContext* file_system_context,
    base::SingleThreadTaskRunner* file_task_runner)
    : handle_(std::move(handle)),
      file_system_context_(file_system_context),
      file_runner_(file_task_runner) {}

UploadBlobElementReader::~UploadBlobElementReader() {}

int UploadBlobElementReader::Init(const net::CompletionCallback& callback) {
  reader_ =
      handle_->CreateReader(file_system_context_.get(), file_runner_.get());
  BlobReader::Status status = reader_->CalculateSize(callback);
  switch (status) {
    case BlobReader::Status::NET_ERROR:
      return reader_->net_error();
    case BlobReader::Status::IO_PENDING:
      return net::ERR_IO_PENDING;
    case BlobReader::Status::DONE:
      return net::OK;
  }
  NOTREACHED();
  return net::ERR_FAILED;
}

uint64_t UploadBlobElementReader::GetContentLength() const {
  return reader_->total_size();
}

uint64_t UploadBlobElementReader::BytesRemaining() const {
  return reader_->remaining_bytes();
}

bool UploadBlobElementReader::IsInMemory() const {
  return reader_->IsInMemory();
}

int UploadBlobElementReader::Read(net::IOBuffer* buf,
                                  int buf_length,
                                  const net::CompletionCallback& callback) {
  int length = 0;
  BlobReader::Status status = reader_->Read(buf, buf_length, &length, callback);
  switch (status) {
    case BlobReader::Status::NET_ERROR:
      return reader_->net_error();
    case BlobReader::Status::IO_PENDING:
      return net::ERR_IO_PENDING;
    case BlobReader::Status::DONE:
      return length;
  }
  NOTREACHED();
  return net::ERR_FAILED;
}

const std::string& UploadBlobElementReader::uuid() const {
  return handle_->uuid();
}

}  // namespace storage
