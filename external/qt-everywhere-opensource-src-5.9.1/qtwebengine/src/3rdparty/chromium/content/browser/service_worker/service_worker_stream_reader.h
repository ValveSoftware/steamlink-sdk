// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STREAM_READER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STREAM_READER_H_

#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_register_observer.h"
#include "net/url_request/url_request.h"

namespace net {
class IOBuffer;
}

namespace content {

class ServiceWorkerVersion;

// Reads a stream response for ServiceWorkerURLRequestJob.
// Owned by ServiceWorkerURLRequestJob.
class ServiceWorkerStreamReader : public StreamReadObserver,
                                  public StreamRegisterObserver {
 public:
  // |streaming_version| is the ServiceWorkerVersion that must be kept alive
  // while the response is being read.
  ServiceWorkerStreamReader(
      ServiceWorkerURLRequestJob* owner,
      scoped_refptr<ServiceWorkerVersion> streaming_version);
  ~ServiceWorkerStreamReader() override;

  // Starts reading the stream. owner_->OnResponseStarted will be called when
  // the response starts.
  void Start(const GURL& stream_url);

  // Same as URLRequestJob::ReadRawData. If ERR_IO_PENDING is returned,
  // owner_->OnReadRawDataComplete will be called when the read completes.
  int ReadRawData(net::IOBuffer* buf, int buf_size);

  // StreamObserver override:
  void OnDataAvailable(Stream* stream) override;

  // StreamRegisterObserver override:
  void OnStreamRegistered(Stream* stream) override;

 private:
  ServiceWorkerURLRequestJob* owner_;

  scoped_refptr<Stream> stream_;
  GURL waiting_stream_url_;
  scoped_refptr<net::IOBuffer> stream_pending_buffer_;
  int stream_pending_buffer_size_;

  scoped_refptr<ServiceWorkerVersion> streaming_version_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STREAM_READER_H_
