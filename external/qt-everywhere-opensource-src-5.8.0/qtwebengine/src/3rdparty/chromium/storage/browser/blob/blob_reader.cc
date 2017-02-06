// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/data_element.h"

namespace storage {
namespace {
bool IsFileType(DataElement::Type type) {
  switch (type) {
    case DataElement::TYPE_FILE:
    case DataElement::TYPE_FILE_FILESYSTEM:
      return true;
    default:
      return false;
  }
}

int ConvertBlobErrorToNetError(IPCBlobCreationCancelCode reason) {
  switch (reason) {
    case IPCBlobCreationCancelCode::UNKNOWN:
      return net::ERR_FAILED;
    case IPCBlobCreationCancelCode::OUT_OF_MEMORY:
      return net::ERR_OUT_OF_MEMORY;
    case IPCBlobCreationCancelCode::FILE_WRITE_FAILED:
      return net::ERR_FILE_NO_SPACE;
    case IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT:
      return net::ERR_UNEXPECTED;
    case IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING:
      return net::ERR_UNEXPECTED;
    case IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN:
      return net::ERR_INVALID_HANDLE;
  }
  NOTREACHED();
  return net::ERR_FAILED;
}
}  // namespace

BlobReader::FileStreamReaderProvider::~FileStreamReaderProvider() {}

BlobReader::BlobReader(
    const BlobDataHandle* blob_handle,
    std::unique_ptr<FileStreamReaderProvider> file_stream_provider,
    base::SequencedTaskRunner* file_task_runner)
    : file_stream_provider_(std::move(file_stream_provider)),
      file_task_runner_(file_task_runner),
      net_error_(net::OK),
      weak_factory_(this) {
  if (blob_handle && !blob_handle->IsBroken()) {
    blob_handle_.reset(new BlobDataHandle(*blob_handle));
  }
}

BlobReader::~BlobReader() {
  STLDeleteValues(&index_to_reader_);
}

BlobReader::Status BlobReader::CalculateSize(
    const net::CompletionCallback& done) {
  DCHECK(!total_size_calculated_);
  DCHECK(size_callback_.is_null());
  if (!blob_handle_.get() || blob_handle_->IsBroken()) {
    return ReportError(net::ERR_FILE_NOT_FOUND);
  }
  if (blob_handle_->IsBeingBuilt()) {
    blob_handle_->RunOnConstructionComplete(base::Bind(
        &BlobReader::AsyncCalculateSize, weak_factory_.GetWeakPtr(), done));
    return Status::IO_PENDING;
  }
  blob_data_ = blob_handle_->CreateSnapshot();
  return CalculateSizeImpl(done);
}

bool BlobReader::has_side_data() const {
  if (!blob_data_.get())
    return false;
  const auto& items = blob_data_->items();
  if (items.size() != 1)
    return false;
  const BlobDataItem& item = *items.at(0);
  if (item.type() != DataElement::TYPE_DISK_CACHE_ENTRY)
    return false;
  const int disk_cache_side_stream_index = item.disk_cache_side_stream_index();
  if (disk_cache_side_stream_index < 0)
    return false;
  return item.disk_cache_entry()->GetDataSize(disk_cache_side_stream_index) > 0;
}

BlobReader::Status BlobReader::ReadSideData(const StatusCallback& done) {
  if (!has_side_data())
    return ReportError(net::ERR_FILE_NOT_FOUND);
  const BlobDataItem* item = blob_data_->items()[0].get();
  const int disk_cache_side_stream_index = item->disk_cache_side_stream_index();
  const int side_data_size =
      item->disk_cache_entry()->GetDataSize(disk_cache_side_stream_index);
  side_data_ = new net::IOBufferWithSize(side_data_size);
  net_error_ = net::OK;
  const int result = item->disk_cache_entry()->ReadData(
      disk_cache_side_stream_index, 0, side_data_.get(), side_data_size,
      base::Bind(&BlobReader::DidReadDiskCacheEntrySideData,
                 weak_factory_.GetWeakPtr(), done, side_data_size));
  if (result >= 0) {
    DCHECK_EQ(side_data_size, result);
    return Status::DONE;
  }
  if (result == net::ERR_IO_PENDING)
    return Status::IO_PENDING;
  return ReportError(result);
}

void BlobReader::DidReadDiskCacheEntrySideData(const StatusCallback& done,
                                               int expected_size,
                                               int result) {
  if (result >= 0) {
    DCHECK_EQ(expected_size, result);
    done.Run(Status::DONE);
    return;
  }
  side_data_ = nullptr;
  done.Run(ReportError(result));
}

BlobReader::Status BlobReader::SetReadRange(uint64_t offset, uint64_t length) {
  if (!blob_handle_.get() || blob_handle_->IsBroken()) {
    return ReportError(net::ERR_FILE_NOT_FOUND);
  }
  if (!total_size_calculated_) {
    return ReportError(net::ERR_FAILED);
  }
  if (offset + length > total_size_) {
    return ReportError(net::ERR_FILE_NOT_FOUND);
  }
  // Skip the initial items that are not in the range.
  remaining_bytes_ = length;
  const auto& items = blob_data_->items();
  for (current_item_index_ = 0;
       current_item_index_ < items.size() &&
       offset >= item_length_list_[current_item_index_];
       ++current_item_index_) {
    offset -= item_length_list_[current_item_index_];
  }

  // Set the offset that need to jump to for the first item in the range.
  current_item_offset_ = offset;
  if (current_item_offset_ == 0)
    return Status::DONE;

  // Adjust the offset of the first stream if it is of file type.
  const BlobDataItem& item = *items.at(current_item_index_);
  if (IsFileType(item.type())) {
    SetFileReaderAtIndex(current_item_index_,
                         CreateFileStreamReader(item, offset));
  }
  return Status::DONE;
}

BlobReader::Status BlobReader::Read(net::IOBuffer* buffer,
                                    size_t dest_size,
                                    int* bytes_read,
                                    net::CompletionCallback done) {
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0ul);
  DCHECK(read_callback_.is_null());

  *bytes_read = 0;
  if (!blob_data_.get()) {
    return ReportError(net::ERR_FILE_NOT_FOUND);
  }
  if (!total_size_calculated_) {
    return ReportError(net::ERR_FAILED);
  }

  // Bail out immediately if we encountered an error.
  if (net_error_ != net::OK) {
    return Status::NET_ERROR;
  }

  DCHECK_GE(dest_size, 0ul);
  if (remaining_bytes_ < static_cast<uint64_t>(dest_size))
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return Status::DONE;
  }

  // Keep track of the buffer.
  DCHECK(!read_buf_.get());
  read_buf_ = new net::DrainableIOBuffer(buffer, dest_size);

  Status status = ReadLoop(bytes_read);
  if (status == Status::IO_PENDING)
    read_callback_ = done;
  return status;
}

void BlobReader::Kill() {
  DeleteCurrentFileReader();
  weak_factory_.InvalidateWeakPtrs();
}

bool BlobReader::IsInMemory() const {
  if (blob_handle_ && blob_handle_->IsBeingBuilt()) {
    return false;
  }
  if (!blob_data_.get()) {
    return true;
  }
  for (const auto& item : blob_data_->items()) {
    if (item->type() != DataElement::TYPE_BYTES) {
      return false;
    }
  }
  return true;
}

void BlobReader::InvalidateCallbacksAndDone(int net_error,
                                            net::CompletionCallback done) {
  net_error_ = net_error;
  weak_factory_.InvalidateWeakPtrs();
  size_callback_.Reset();
  read_callback_.Reset();
  read_buf_ = nullptr;
  done.Run(net_error);
}

BlobReader::Status BlobReader::ReportError(int net_error) {
  net_error_ = net_error;
  return Status::NET_ERROR;
}

void BlobReader::AsyncCalculateSize(const net::CompletionCallback& done,
                                    bool async_succeeded,
                                    IPCBlobCreationCancelCode reason) {
  if (!async_succeeded) {
    InvalidateCallbacksAndDone(ConvertBlobErrorToNetError(reason), done);
    return;
  }
  DCHECK(!blob_handle_->IsBroken()) << "Callback should have returned false.";
  blob_data_ = blob_handle_->CreateSnapshot();
  Status size_status = CalculateSizeImpl(done);
  switch (size_status) {
    case Status::NET_ERROR:
      InvalidateCallbacksAndDone(net_error_, done);
      return;
    case Status::DONE:
      done.Run(net::OK);
      return;
    case Status::IO_PENDING:
      return;
  }
}

BlobReader::Status BlobReader::CalculateSizeImpl(
    const net::CompletionCallback& done) {
  DCHECK(!total_size_calculated_);
  DCHECK(size_callback_.is_null());

  net_error_ = net::OK;
  total_size_ = 0;
  const auto& items = blob_data_->items();
  item_length_list_.resize(items.size());
  pending_get_file_info_count_ = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    const BlobDataItem& item = *items.at(i);
    if (IsFileType(item.type())) {
      ++pending_get_file_info_count_;
      storage::FileStreamReader* const reader = GetOrCreateFileReaderAtIndex(i);
      if (!reader) {
        return ReportError(net::ERR_FAILED);
      }
      int64_t length_output = reader->GetLength(base::Bind(
          &BlobReader::DidGetFileItemLength, weak_factory_.GetWeakPtr(), i));
      if (length_output == net::ERR_IO_PENDING) {
        continue;
      }
      if (length_output < 0) {
        return ReportError(length_output);
      }
      // We got the length right away
      --pending_get_file_info_count_;
      uint64_t resolved_length;
      if (!ResolveFileItemLength(item, length_output, &resolved_length)) {
        return ReportError(net::ERR_FILE_NOT_FOUND);
      }
      if (!AddItemLength(i, resolved_length)) {
        return ReportError(net::ERR_FAILED);
      }
      continue;
    }

    if (!AddItemLength(i, item.length()))
      return ReportError(net::ERR_FAILED);
  }

  if (pending_get_file_info_count_ == 0) {
    DidCountSize();
    return Status::DONE;
  }
  // Note: We only set the callback if we know that we're an async operation.
  size_callback_ = done;
  return Status::IO_PENDING;
}

bool BlobReader::AddItemLength(size_t index, uint64_t item_length) {
  if (item_length > std::numeric_limits<uint64_t>::max() - total_size_) {
    return false;
  }

  // Cache the size and add it to the total size.
  DCHECK_LT(index, item_length_list_.size());
  item_length_list_[index] = item_length;
  total_size_ += item_length;
  return true;
}

bool BlobReader::ResolveFileItemLength(const BlobDataItem& item,
                                       int64_t total_length,
                                       uint64_t* output_length) {
  DCHECK(IsFileType(item.type()));
  DCHECK(output_length);
  uint64_t file_length = total_length;
  uint64_t item_offset = item.offset();
  uint64_t item_length = item.length();
  if (item_offset > file_length) {
    return false;
  }

  uint64_t max_length = file_length - item_offset;

  // If item length is undefined, then we need to use the file size being
  // resolved in the real time.
  if (item_length == std::numeric_limits<uint64_t>::max()) {
    item_length = max_length;
  } else if (item_length > max_length) {
    return false;
  }

  *output_length = item_length;
  return true;
}

void BlobReader::DidGetFileItemLength(size_t index, int64_t result) {
  // Do nothing if we have encountered an error.
  if (net_error_)
    return;

  if (result == net::ERR_UPLOAD_FILE_CHANGED)
    result = net::ERR_FILE_NOT_FOUND;
  if (result < 0) {
    InvalidateCallbacksAndDone(result, size_callback_);
    return;
  }

  const auto& items = blob_data_->items();
  DCHECK_LT(index, items.size());
  const BlobDataItem& item = *items.at(index);
  uint64_t length;
  if (!ResolveFileItemLength(item, result, &length)) {
    InvalidateCallbacksAndDone(net::ERR_FILE_NOT_FOUND, size_callback_);
    return;
  }
  if (!AddItemLength(index, length)) {
    InvalidateCallbacksAndDone(net::ERR_FAILED, size_callback_);
    return;
  }

  if (--pending_get_file_info_count_ == 0)
    DidCountSize();
}

void BlobReader::DidCountSize() {
  DCHECK(!net_error_);
  total_size_calculated_ = true;
  remaining_bytes_ = total_size_;
  // This is set only if we're async.
  if (!size_callback_.is_null()) {
    net::CompletionCallback done = size_callback_;
    size_callback_.Reset();
    done.Run(net::OK);
  }
}

BlobReader::Status BlobReader::ReadLoop(int* bytes_read) {
  // Read until we encounter an error or could not get the data immediately.
  while (remaining_bytes_ > 0 && read_buf_->BytesRemaining() > 0) {
    Status read_status = ReadItem();
    if (read_status == Status::DONE) {
      continue;
    }
    return read_status;
  }

  *bytes_read = BytesReadCompleted();
  return Status::DONE;
}

BlobReader::Status BlobReader::ReadItem() {
  // Are we done with reading all the blob data?
  if (remaining_bytes_ == 0)
    return Status::DONE;

  const auto& items = blob_data_->items();
  // If we get to the last item but still expect something to read, bail out
  // since something is wrong.
  if (current_item_index_ >= items.size()) {
    return ReportError(net::ERR_FAILED);
  }

  // Compute the bytes to read for current item.
  int bytes_to_read = ComputeBytesToRead();

  // If nothing to read for current item, advance to next item.
  if (bytes_to_read == 0) {
    AdvanceItem();
    return Status::DONE;
  }

  // Do the reading.
  const BlobDataItem& item = *items.at(current_item_index_);
  if (item.type() == DataElement::TYPE_BYTES) {
    ReadBytesItem(item, bytes_to_read);
    return Status::DONE;
  }
  if (item.type() == DataElement::TYPE_DISK_CACHE_ENTRY)
    return ReadDiskCacheEntryItem(item, bytes_to_read);
  if (!IsFileType(item.type())) {
    NOTREACHED();
    return ReportError(net::ERR_FAILED);
  }
  storage::FileStreamReader* const reader =
      GetOrCreateFileReaderAtIndex(current_item_index_);
  if (!reader) {
    return ReportError(net::ERR_FAILED);
  }

  return ReadFileItem(reader, bytes_to_read);
}

void BlobReader::AdvanceItem() {
  // Close the file if the current item is a file.
  DeleteCurrentFileReader();

  // Advance to the next item.
  current_item_index_++;
  current_item_offset_ = 0;
}

void BlobReader::AdvanceBytesRead(int result) {
  DCHECK_GT(result, 0);

  // Do we finish reading the current item?
  current_item_offset_ += result;
  if (current_item_offset_ == item_length_list_[current_item_index_])
    AdvanceItem();

  // Subtract the remaining bytes.
  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0ul);

  // Adjust the read buffer.
  read_buf_->DidConsume(result);
  DCHECK_GE(read_buf_->BytesRemaining(), 0);
}

void BlobReader::ReadBytesItem(const BlobDataItem& item, int bytes_to_read) {
  TRACE_EVENT1("Blob", "BlobReader::ReadBytesItem", "uuid", blob_data_->uuid());
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  memcpy(read_buf_->data(), item.bytes() + item.offset() + current_item_offset_,
         bytes_to_read);

  AdvanceBytesRead(bytes_to_read);
}

BlobReader::Status BlobReader::ReadFileItem(FileStreamReader* reader,
                                            int bytes_to_read) {
  DCHECK(!io_pending_)
      << "Can't begin IO while another IO operation is pending.";
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);
  DCHECK(reader);
  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobRequest::ReadFileItem", this, "uuid",
                           blob_data_->uuid());
  const int result = reader->Read(
      read_buf_.get(), bytes_to_read,
      base::Bind(&BlobReader::DidReadFile, weak_factory_.GetWeakPtr()));
  if (result >= 0) {
    AdvanceBytesRead(result);
    return Status::DONE;
  }
  if (result == net::ERR_IO_PENDING) {
    io_pending_ = true;
    return Status::IO_PENDING;
  }
  return ReportError(result);
}

void BlobReader::DidReadFile(int result) {
  TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::ReadFileItem", this, "uuid",
                         blob_data_->uuid());
  DidReadItem(result);
}

void BlobReader::ContinueAsyncReadLoop() {
  int bytes_read = 0;
  Status read_status = ReadLoop(&bytes_read);
  switch (read_status) {
    case Status::DONE: {
      net::CompletionCallback done = read_callback_;
      read_callback_.Reset();
      done.Run(bytes_read);
      return;
    }
    case Status::NET_ERROR:
      InvalidateCallbacksAndDone(net_error_, read_callback_);
      return;
    case Status::IO_PENDING:
      return;
  }
}

void BlobReader::DeleteCurrentFileReader() {
  SetFileReaderAtIndex(current_item_index_,
                       std::unique_ptr<FileStreamReader>());
}

BlobReader::Status BlobReader::ReadDiskCacheEntryItem(const BlobDataItem& item,
                                                      int bytes_to_read) {
  DCHECK(!io_pending_)
      << "Can't begin IO while another IO operation is pending.";
  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobRequest::ReadDiskCacheItem", this,
                           "uuid", blob_data_->uuid());
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  const int result = item.disk_cache_entry()->ReadData(
      item.disk_cache_stream_index(), item.offset() + current_item_offset_,
      read_buf_.get(), bytes_to_read,
      base::Bind(&BlobReader::DidReadDiskCacheEntry,
                 weak_factory_.GetWeakPtr()));
  if (result >= 0) {
    AdvanceBytesRead(result);
    return Status::DONE;
  }
  if (result == net::ERR_IO_PENDING) {
    io_pending_ = true;
    return Status::IO_PENDING;
  }
  return ReportError(result);
}

void BlobReader::DidReadDiskCacheEntry(int result) {
  TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::ReadDiskCacheItem", this, "uuid",
                         blob_data_->uuid());
  DidReadItem(result);
}

void BlobReader::DidReadItem(int result) {
  DCHECK(io_pending_) << "Asynchronous IO completed while IO wasn't pending?";
  io_pending_ = false;
  if (result <= 0) {
    InvalidateCallbacksAndDone(result, read_callback_);
    return;
  }
  AdvanceBytesRead(result);
  ContinueAsyncReadLoop();
}

int BlobReader::BytesReadCompleted() {
  int bytes_read = read_buf_->BytesConsumed();
  read_buf_ = nullptr;
  return bytes_read;
}

int BlobReader::ComputeBytesToRead() const {
  uint64_t current_item_length = item_length_list_[current_item_index_];

  uint64_t item_remaining = current_item_length - current_item_offset_;
  uint64_t buf_remaining = read_buf_->BytesRemaining();
  uint64_t max_int_value = std::numeric_limits<int>::max();
  // Here we make sure we don't overflow 'max int'.
  uint64_t min = std::min(
      std::min(std::min(item_remaining, buf_remaining), remaining_bytes_),
      max_int_value);

  return static_cast<int>(min);
}

FileStreamReader* BlobReader::GetOrCreateFileReaderAtIndex(size_t index) {
  const auto& items = blob_data_->items();
  DCHECK_LT(index, items.size());
  const BlobDataItem& item = *items.at(index);
  if (!IsFileType(item.type()))
    return nullptr;
  auto it = index_to_reader_.find(index);
  if (it != index_to_reader_.end()) {
    DCHECK(it->second);
    return it->second;
  }
  std::unique_ptr<FileStreamReader> reader = CreateFileStreamReader(item, 0);
  FileStreamReader* ret_value = reader.get();
  if (!ret_value)
    return nullptr;
  index_to_reader_[index] = reader.release();
  return ret_value;
}

std::unique_ptr<FileStreamReader> BlobReader::CreateFileStreamReader(
    const BlobDataItem& item,
    uint64_t additional_offset) {
  DCHECK(IsFileType(item.type()));

  switch (item.type()) {
    case DataElement::TYPE_FILE:
      return file_stream_provider_->CreateForLocalFile(
          file_task_runner_.get(), item.path(),
          item.offset() + additional_offset, item.expected_modification_time());
    case DataElement::TYPE_FILE_FILESYSTEM:
      return file_stream_provider_->CreateFileStreamReader(
          item.filesystem_url(), item.offset() + additional_offset,
          item.length() == std::numeric_limits<uint64_t>::max()
              ? storage::kMaximumLength
              : item.length() - additional_offset,
          item.expected_modification_time());
    case DataElement::TYPE_BLOB:
    case DataElement::TYPE_BYTES:
    case DataElement::TYPE_BYTES_DESCRIPTION:
    case DataElement::TYPE_DISK_CACHE_ENTRY:
    case DataElement::TYPE_UNKNOWN:
      break;
  }

  NOTREACHED();
  return nullptr;
}

void BlobReader::SetFileReaderAtIndex(
    size_t index,
    std::unique_ptr<FileStreamReader> reader) {
  auto found = index_to_reader_.find(current_item_index_);
  if (found != index_to_reader_.end()) {
    if (found->second) {
      delete found->second;
    }
    if (!reader.get()) {
      index_to_reader_.erase(found);
      return;
    }
    found->second = reader.release();
  } else if (reader.get()) {
    index_to_reader_[current_item_index_] = reader.release();
  }
}

}  // namespace storage
