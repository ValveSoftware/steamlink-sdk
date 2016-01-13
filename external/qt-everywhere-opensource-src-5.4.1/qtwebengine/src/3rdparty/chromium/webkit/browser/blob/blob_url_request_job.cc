// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_url_request_job.h"

#include <limits>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util_proxy.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_status.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace webkit_blob {

namespace {

bool IsFileType(BlobData::Item::Type type) {
  switch (type) {
    case BlobData::Item::TYPE_FILE:
    case BlobData::Item::TYPE_FILE_FILESYSTEM:
      return true;
    default:
      return false;
  }
}

}  // namespace

BlobURLRequestJob::BlobURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    BlobData* blob_data,
    fileapi::FileSystemContext* file_system_context,
    base::MessageLoopProxy* file_thread_proxy)
    : net::URLRequestJob(request, network_delegate),
      blob_data_(blob_data),
      file_system_context_(file_system_context),
      file_thread_proxy_(file_thread_proxy),
      total_size_(0),
      remaining_bytes_(0),
      pending_get_file_info_count_(0),
      current_item_index_(0),
      current_item_offset_(0),
      error_(false),
      byte_range_set_(false),
      weak_factory_(this) {
  DCHECK(file_thread_proxy_.get());
}

void BlobURLRequestJob::Start() {
  // Continue asynchronously.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BlobURLRequestJob::DidStart, weak_factory_.GetWeakPtr()));
}

void BlobURLRequestJob::Kill() {
  DeleteCurrentFileReader();

  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool BlobURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                    int dest_size,
                                    int* bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  // Bail out immediately if we encounter an error.
  if (error_) {
    *bytes_read = 0;
    return true;
  }

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  // Keep track of the buffer.
  DCHECK(!read_buf_.get());
  read_buf_ = new net::DrainableIOBuffer(dest, dest_size);

  return ReadLoop(bytes_read);
}

bool BlobURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_info_)
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

void BlobURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

int BlobURLRequestJob::GetResponseCode() const {
  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

void BlobURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_set_ = true;
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(jianli): Support multipart byte range requests.
        NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      }
    }
  }
}

BlobURLRequestJob::~BlobURLRequestJob() {
  STLDeleteValues(&index_to_reader_);
}

void BlobURLRequestJob::DidStart() {
  error_ = false;

  // We only support GET request per the spec.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  // If the blob data is not present, bail out.
  if (!blob_data_.get()) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  CountSize();
}

bool BlobURLRequestJob::AddItemLength(size_t index, int64 item_length) {
  if (item_length > kint64max - total_size_) {
    NotifyFailure(net::ERR_FAILED);
    return false;
  }

  // Cache the size and add it to the total size.
  DCHECK_LT(index, item_length_list_.size());
  item_length_list_[index] = item_length;
  total_size_ += item_length;
  return true;
}

void BlobURLRequestJob::CountSize() {
  pending_get_file_info_count_ = 0;
  total_size_ = 0;
  item_length_list_.resize(blob_data_->items().size());

  for (size_t i = 0; i < blob_data_->items().size(); ++i) {
    const BlobData::Item& item = blob_data_->items().at(i);
    if (IsFileType(item.type())) {
      ++pending_get_file_info_count_;
      GetFileStreamReader(i)->GetLength(
          base::Bind(&BlobURLRequestJob::DidGetFileItemLength,
                     weak_factory_.GetWeakPtr(), i));
      continue;
    }

    if (!AddItemLength(i, item.length()))
      return;
  }

  if (pending_get_file_info_count_ == 0)
    DidCountSize(net::OK);
}

void BlobURLRequestJob::DidCountSize(int error) {
  DCHECK(!error_);

  // If an error occured, bail out.
  if (error != net::OK) {
    NotifyFailure(error);
    return;
  }

  // Apply the range requirement.
  if (!byte_range_.ComputeBounds(total_size_)) {
    NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  // Do the seek at the beginning of the request.
  if (byte_range_.first_byte_position())
    Seek(byte_range_.first_byte_position());

  NotifySuccess();
}

void BlobURLRequestJob::DidGetFileItemLength(size_t index, int64 result) {
  // Do nothing if we have encountered an error.
  if (error_)
    return;

  if (result == net::ERR_UPLOAD_FILE_CHANGED) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  } else if (result < 0) {
    NotifyFailure(result);
    return;
  }

  DCHECK_LT(index, blob_data_->items().size());
  const BlobData::Item& item = blob_data_->items().at(index);
  DCHECK(IsFileType(item.type()));

  uint64 file_length = result;
  uint64 item_offset = item.offset();
  uint64 item_length = item.length();

  if (item_offset > file_length) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  uint64 max_length = file_length - item_offset;

  // If item length is -1, we need to use the file size being resolved
  // in the real time.
  if (item_length == static_cast<uint64>(-1)) {
    item_length = max_length;
  } else if (item_length > max_length) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  if (!AddItemLength(index, item_length))
    return;

  if (--pending_get_file_info_count_ == 0)
    DidCountSize(net::OK);
}

void BlobURLRequestJob::Seek(int64 offset) {
  // Skip the initial items that are not in the range.
  for (current_item_index_ = 0;
       current_item_index_ < blob_data_->items().size() &&
           offset >= item_length_list_[current_item_index_];
       ++current_item_index_) {
    offset -= item_length_list_[current_item_index_];
  }

  // Set the offset that need to jump to for the first item in the range.
  current_item_offset_ = offset;

  if (offset == 0)
    return;

  // Adjust the offset of the first stream if it is of file type.
  const BlobData::Item& item = blob_data_->items().at(current_item_index_);
  if (IsFileType(item.type())) {
    DeleteCurrentFileReader();
    CreateFileStreamReader(current_item_index_, offset);
  }
}

bool BlobURLRequestJob::ReadItem() {
  // Are we done with reading all the blob data?
  if (remaining_bytes_ == 0)
    return true;

  // If we get to the last item but still expect something to read, bail out
  // since something is wrong.
  if (current_item_index_ >= blob_data_->items().size()) {
    NotifyFailure(net::ERR_FAILED);
    return false;
  }

  // Compute the bytes to read for current item.
  int bytes_to_read = ComputeBytesToRead();

  // If nothing to read for current item, advance to next item.
  if (bytes_to_read == 0) {
    AdvanceItem();
    return ReadItem();
  }

  // Do the reading.
  const BlobData::Item& item = blob_data_->items().at(current_item_index_);
  if (item.type() == BlobData::Item::TYPE_BYTES)
    return ReadBytesItem(item, bytes_to_read);
  if (IsFileType(item.type())) {
    return ReadFileItem(GetFileStreamReader(current_item_index_),
                        bytes_to_read);
  }
  NOTREACHED();
  return false;
}

void BlobURLRequestJob::AdvanceItem() {
  // Close the file if the current item is a file.
  DeleteCurrentFileReader();

  // Advance to the next item.
  current_item_index_++;
  current_item_offset_ = 0;
}

void BlobURLRequestJob::AdvanceBytesRead(int result) {
  DCHECK_GT(result, 0);

  // Do we finish reading the current item?
  current_item_offset_ += result;
  if (current_item_offset_ == item_length_list_[current_item_index_])
    AdvanceItem();

  // Subtract the remaining bytes.
  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  // Adjust the read buffer.
  read_buf_->DidConsume(result);
  DCHECK_GE(read_buf_->BytesRemaining(), 0);
}

bool BlobURLRequestJob::ReadBytesItem(const BlobData::Item& item,
                                      int bytes_to_read) {
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  memcpy(read_buf_->data(),
         item.bytes() + item.offset() + current_item_offset_,
         bytes_to_read);

  AdvanceBytesRead(bytes_to_read);
  return true;
}

bool BlobURLRequestJob::ReadFileItem(FileStreamReader* reader,
                                     int bytes_to_read) {
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);
  DCHECK(reader);
  const int result = reader->Read(
      read_buf_.get(),
      bytes_to_read,
      base::Bind(&BlobURLRequestJob::DidReadFile, base::Unretained(this)));
  if (result >= 0) {
    // Data is immediately available.
    if (GetStatus().is_io_pending())
      DidReadFile(result);
    else
      AdvanceBytesRead(result);
    return true;
  }
  if (result == net::ERR_IO_PENDING)
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  else
    NotifyFailure(result);
  return false;
}

void BlobURLRequestJob::DidReadFile(int result) {
  if (result <= 0) {
    NotifyFailure(net::ERR_FAILED);
    return;
  }
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status

  AdvanceBytesRead(result);

  // If the read buffer is completely filled, we're done.
  if (!read_buf_->BytesRemaining()) {
    int bytes_read = BytesReadCompleted();
    NotifyReadComplete(bytes_read);
    return;
  }

  // Otherwise, continue the reading.
  int bytes_read = 0;
  if (ReadLoop(&bytes_read))
    NotifyReadComplete(bytes_read);
}

void BlobURLRequestJob::DeleteCurrentFileReader() {
  IndexToReaderMap::iterator found = index_to_reader_.find(current_item_index_);
  if (found != index_to_reader_.end() && found->second) {
    delete found->second;
    index_to_reader_.erase(found);
  }
}

int BlobURLRequestJob::BytesReadCompleted() {
  int bytes_read = read_buf_->BytesConsumed();
  read_buf_ = NULL;
  return bytes_read;
}

int BlobURLRequestJob::ComputeBytesToRead() const {
  int64 current_item_length = item_length_list_[current_item_index_];

  int64 item_remaining = current_item_length - current_item_offset_;
  int64 buf_remaining = read_buf_->BytesRemaining();
  int64 max_remaining = std::numeric_limits<int>::max();

  int64 min = std::min(std::min(std::min(item_remaining,
                                         buf_remaining),
                                         remaining_bytes_),
                                         max_remaining);

  return static_cast<int>(min);
}

bool BlobURLRequestJob::ReadLoop(int* bytes_read) {
  // Read until we encounter an error or could not get the data immediately.
  while (remaining_bytes_ > 0 && read_buf_->BytesRemaining() > 0) {
    if (!ReadItem())
      return false;
  }

  *bytes_read = BytesReadCompleted();
  return true;
}

void BlobURLRequestJob::NotifySuccess() {
  net::HttpStatusCode status_code = net::HTTP_OK;
  if (byte_range_set_ && byte_range_.IsValid())
    status_code = net::HTTP_PARTIAL_CONTENT;
  HeadersCompleted(status_code);
}

void BlobURLRequestJob::NotifyFailure(int error_code) {
  error_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  if (response_info_) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     error_code));
    return;
  }

  net::HttpStatusCode status_code = net::HTTP_INTERNAL_SERVER_ERROR;
  switch (error_code) {
    case net::ERR_ACCESS_DENIED:
      status_code = net::HTTP_FORBIDDEN;
      break;
    case net::ERR_FILE_NOT_FOUND:
      status_code = net::HTTP_NOT_FOUND;
      break;
    case net::ERR_METHOD_NOT_SUPPORTED:
      status_code = net::HTTP_METHOD_NOT_ALLOWED;
      break;
    case net::ERR_REQUEST_RANGE_NOT_SATISFIABLE:
      status_code = net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE;
      break;
    case net::ERR_FAILED:
      break;
    default:
      DCHECK(false);
      break;
  }
  HeadersCompleted(status_code);
}

void BlobURLRequestJob::HeadersCompleted(net::HttpStatusCode status_code) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(status_code));
  status.append("\0\0", 2);
  net::HttpResponseHeaders* headers = new net::HttpResponseHeaders(status);

  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT) {
    std::string content_length_header(net::HttpRequestHeaders::kContentLength);
    content_length_header.append(": ");
    content_length_header.append(base::Int64ToString(remaining_bytes_));
    headers->AddHeader(content_length_header);
    if (status_code == net::HTTP_PARTIAL_CONTENT) {
      DCHECK(byte_range_set_);
      DCHECK(byte_range_.IsValid());
      std::string content_range_header(net::HttpResponseHeaders::kContentRange);
      content_range_header.append(": bytes ");
      content_range_header.append(base::StringPrintf(
          "%" PRId64 "-%" PRId64,
          byte_range_.first_byte_position(), byte_range_.last_byte_position()));
      content_range_header.append("/");
      content_range_header.append(base::StringPrintf("%" PRId64, total_size_));
      headers->AddHeader(content_range_header);
    }
    if (!blob_data_->content_type().empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(blob_data_->content_type());
      headers->AddHeader(content_type_header);
    }
    if (!blob_data_->content_disposition().empty()) {
      std::string content_disposition_header("Content-Disposition: ");
      content_disposition_header.append(blob_data_->content_disposition());
      headers->AddHeader(content_disposition_header);
    }
  }

  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = headers;

  set_expected_content_size(remaining_bytes_);

  NotifyHeadersComplete();
}

FileStreamReader* BlobURLRequestJob::GetFileStreamReader(size_t index) {
  DCHECK_LT(index, blob_data_->items().size());
  const BlobData::Item& item = blob_data_->items().at(index);
  if (!IsFileType(item.type()))
    return NULL;
  if (index_to_reader_.find(index) == index_to_reader_.end())
    CreateFileStreamReader(index, 0);
  DCHECK(index_to_reader_[index]);
  return index_to_reader_[index];
}

void BlobURLRequestJob::CreateFileStreamReader(size_t index,
                                               int64 additional_offset) {
  DCHECK_LT(index, blob_data_->items().size());
  const BlobData::Item& item = blob_data_->items().at(index);
  DCHECK(IsFileType(item.type()));
  DCHECK_EQ(0U, index_to_reader_.count(index));

  FileStreamReader* reader = NULL;
  switch (item.type()) {
    case BlobData::Item::TYPE_FILE:
      reader = FileStreamReader::CreateForLocalFile(
          file_thread_proxy_.get(),
          item.path(),
          item.offset() + additional_offset,
          item.expected_modification_time());
      break;
    case BlobData::Item::TYPE_FILE_FILESYSTEM:
      reader = file_system_context_->CreateFileStreamReader(
          fileapi::FileSystemURL(
              file_system_context_->CrackURL(item.filesystem_url())),
          item.offset() + additional_offset,
          item.expected_modification_time()).release();
      break;
    default:
      NOTREACHED();
  }
  DCHECK(reader);
  index_to_reader_[index] = reader;
}

}  // namespace webkit_blob
