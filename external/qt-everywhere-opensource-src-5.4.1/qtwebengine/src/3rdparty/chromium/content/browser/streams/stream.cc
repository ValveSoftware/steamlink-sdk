// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "content/browser/streams/stream_handle_impl.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"

namespace {
// Start throttling the connection at about 1MB.
const size_t kDeferSizeThreshold = 40 * 32768;
}

namespace content {

Stream::Stream(StreamRegistry* registry,
               StreamWriteObserver* write_observer,
               const GURL& url)
    : can_add_data_(true),
      url_(url),
      data_length_(0),
      data_bytes_read_(0),
      last_total_buffered_bytes_(0),
      registry_(registry),
      read_observer_(NULL),
      write_observer_(write_observer),
      stream_handle_(NULL),
      weak_ptr_factory_(this) {
  CreateByteStream(base::MessageLoopProxy::current(),
                   base::MessageLoopProxy::current(),
                   kDeferSizeThreshold,
                   &writer_,
                   &reader_);

  // Setup callback for writing.
  writer_->RegisterCallback(base::Bind(&Stream::OnSpaceAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));
  reader_->RegisterCallback(base::Bind(&Stream::OnDataAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));

  registry_->RegisterStream(this);
}

Stream::~Stream() {
}

bool Stream::SetReadObserver(StreamReadObserver* observer) {
  if (read_observer_)
    return false;
  read_observer_ = observer;
  return true;
}

void Stream::RemoveReadObserver(StreamReadObserver* observer) {
  DCHECK(observer == read_observer_);
  read_observer_ = NULL;
}

void Stream::RemoveWriteObserver(StreamWriteObserver* observer) {
  DCHECK(observer == write_observer_);
  write_observer_ = NULL;
}

void Stream::Abort() {
  // Clear all buffer. It's safe to clear reader_ here since the same thread
  // is used for both input and output operation.
  writer_.reset();
  reader_.reset();
  ClearBuffer();
  can_add_data_ = false;
  registry_->UnregisterStream(url());
}

void Stream::AddData(scoped_refptr<net::IOBuffer> buffer, size_t size) {
  if (!writer_.get())
    return;

  size_t current_buffered_bytes = writer_->GetTotalBufferedBytes();
  if (!registry_->UpdateMemoryUsage(url(), current_buffered_bytes, size)) {
    Abort();
    return;
  }

  // Now it's guaranteed that this doesn't overflow. This must be done before
  // Write() since GetTotalBufferedBytes() may return different value after
  // Write() call, so if we use the new value, information in this instance and
  // one in |registry_| become inconsistent.
  last_total_buffered_bytes_ = current_buffered_bytes + size;

  can_add_data_ = writer_->Write(buffer, size);
}

void Stream::AddData(const char* data, size_t size) {
  if (!writer_.get())
    return;

  scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(size));
  memcpy(io_buffer->data(), data, size);
  AddData(io_buffer, size);
}

void Stream::Finalize() {
  if (!writer_.get())
    return;

  writer_->Close(0);
  writer_.reset();

  // Continue asynchronously.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&Stream::OnDataAvailable, weak_ptr_factory_.GetWeakPtr()));
}

Stream::StreamState Stream::ReadRawData(net::IOBuffer* buf,
                                        int buf_size,
                                        int* bytes_read) {
  DCHECK(buf);
  DCHECK(bytes_read);

  *bytes_read = 0;
  if (!data_.get()) {
    DCHECK(!data_length_);
    DCHECK(!data_bytes_read_);

    if (!reader_.get())
      return STREAM_ABORTED;

    ByteStreamReader::StreamState state = reader_->Read(&data_, &data_length_);
    switch (state) {
      case ByteStreamReader::STREAM_HAS_DATA:
        break;
      case ByteStreamReader::STREAM_COMPLETE:
        registry_->UnregisterStream(url());
        return STREAM_COMPLETE;
      case ByteStreamReader::STREAM_EMPTY:
        return STREAM_EMPTY;
    }
  }

  const size_t remaining_bytes = data_length_ - data_bytes_read_;
  size_t to_read =
      static_cast<size_t>(buf_size) < remaining_bytes ?
      buf_size : remaining_bytes;
  memcpy(buf->data(), data_->data() + data_bytes_read_, to_read);
  data_bytes_read_ += to_read;
  if (data_bytes_read_ >= data_length_)
    ClearBuffer();

  *bytes_read = to_read;
  return STREAM_HAS_DATA;
}

scoped_ptr<StreamHandle> Stream::CreateHandle(
    const GURL& original_url,
    const std::string& mime_type,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  CHECK(!stream_handle_);
  stream_handle_ = new StreamHandleImpl(weak_ptr_factory_.GetWeakPtr(),
                                        original_url,
                                        mime_type,
                                        response_headers);
  return scoped_ptr<StreamHandle>(stream_handle_).Pass();
}

void Stream::CloseHandle() {
  // Prevent deletion until this function ends.
  scoped_refptr<Stream> ref(this);

  CHECK(stream_handle_);
  stream_handle_ = NULL;
  registry_->UnregisterStream(url());
  if (write_observer_)
    write_observer_->OnClose(this);
}

void Stream::OnSpaceAvailable() {
  can_add_data_ = true;
  if (write_observer_)
    write_observer_->OnSpaceAvailable(this);
}

void Stream::OnDataAvailable() {
  if (read_observer_)
    read_observer_->OnDataAvailable(this);
}

void Stream::ClearBuffer() {
  data_ = NULL;
  data_length_ = 0;
  data_bytes_read_ = 0;
}

}  // namespace content
