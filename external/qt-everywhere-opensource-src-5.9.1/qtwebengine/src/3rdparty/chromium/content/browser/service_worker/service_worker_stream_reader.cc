// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_stream_reader.h"

#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "net/base/io_buffer.h"

namespace content {

ServiceWorkerStreamReader::ServiceWorkerStreamReader(
    ServiceWorkerURLRequestJob* owner,
    scoped_refptr<ServiceWorkerVersion> streaming_version)
    : owner_(owner),
      stream_pending_buffer_size_(0),
      streaming_version_(streaming_version) {
  streaming_version_->AddStreamingURLRequestJob(owner_);
}

ServiceWorkerStreamReader::~ServiceWorkerStreamReader() {
  if (streaming_version_) {
    streaming_version_->RemoveStreamingURLRequestJob(owner_);
    streaming_version_ = nullptr;
  }
  if (stream_) {
    stream_->RemoveReadObserver(this);
    stream_->Abort();
    stream_ = nullptr;
  }
  if (!waiting_stream_url_.is_empty()) {
    StreamRegistry* stream_registry =
        GetStreamContextForResourceContext(owner_->resource_context())
            ->registry();
    stream_registry->RemoveRegisterObserver(waiting_stream_url_);
    stream_registry->AbortPendingStream(waiting_stream_url_);
  }
}

void ServiceWorkerStreamReader::Start(const GURL& stream_url) {
  DCHECK(!stream_);
  DCHECK(waiting_stream_url_.is_empty());

  StreamContext* stream_context =
      GetStreamContextForResourceContext(owner_->resource_context());
  stream_ = stream_context->registry()->GetStream(stream_url);
  if (!stream_) {
    waiting_stream_url_ = stream_url;
    // Wait for StreamHostMsg_StartBuilding message from the ServiceWorker.
    stream_context->registry()->SetRegisterObserver(waiting_stream_url_, this);
    return;
  }
  stream_->SetReadObserver(this);
  owner_->OnResponseStarted();
}

int ServiceWorkerStreamReader::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(stream_);
  DCHECK(waiting_stream_url_.is_empty());

  int bytes_read = 0;
  switch (stream_->ReadRawData(buf, buf_size, &bytes_read)) {
    case Stream::STREAM_HAS_DATA:
      DCHECK_GT(bytes_read, 0);
      return bytes_read;
    case Stream::STREAM_COMPLETE:
      DCHECK_EQ(0, bytes_read);
      owner_->RecordResult(ServiceWorkerMetrics::REQUEST_JOB_STREAM_RESPONSE);
      return 0;
    case Stream::STREAM_EMPTY:
      stream_pending_buffer_ = buf;
      stream_pending_buffer_size_ = buf_size;
      return net::ERR_IO_PENDING;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      owner_->RecordResult(
          ServiceWorkerMetrics::REQUEST_JOB_ERROR_STREAM_ABORTED);
      return net::ERR_CONNECTION_RESET;
  }
  NOTREACHED();
  return net::ERR_FAILED;
}

void ServiceWorkerStreamReader::OnDataAvailable(Stream* stream) {
  // Do nothing if stream_pending_buffer_ is empty, i.e. there's no ReadRawData
  // operation waiting for IO completion.
  if (!stream_pending_buffer_)
    return;

  // stream_pending_buffer_ is set to the IOBuffer instance provided to
  // ReadRawData() by URLRequestJob.

  int result = 0;
  switch (stream_->ReadRawData(stream_pending_buffer_.get(),
                               stream_pending_buffer_size_, &result)) {
    case Stream::STREAM_HAS_DATA:
      DCHECK_GT(result, 0);
      break;
    case Stream::STREAM_COMPLETE:
      // Calling NotifyReadComplete with 0 signals completion.
      DCHECK(!result);
      owner_->RecordResult(ServiceWorkerMetrics::REQUEST_JOB_STREAM_RESPONSE);
      break;
    case Stream::STREAM_EMPTY:
      NOTREACHED();
      break;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      result = net::ERR_CONNECTION_RESET;
      owner_->RecordResult(
          ServiceWorkerMetrics::REQUEST_JOB_ERROR_STREAM_ABORTED);
      break;
  }

  // Clear the buffers before notifying the read is complete, so that it is
  // safe for the observer to read.
  stream_pending_buffer_ = nullptr;
  stream_pending_buffer_size_ = 0;
  owner_->OnReadRawDataComplete(result);
}

void ServiceWorkerStreamReader::OnStreamRegistered(Stream* stream) {
  StreamContext* stream_context =
      GetStreamContextForResourceContext(owner_->resource_context());
  stream_context->registry()->RemoveRegisterObserver(waiting_stream_url_);
  waiting_stream_url_ = GURL();
  stream_ = stream;
  stream_->SetReadObserver(this);
  owner_->OnResponseStarted();
}

}  // namespace content
