// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_READ_FROM_CACHE_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_READ_FROM_CACHE_JOB_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/common/content_export.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerResponseReader;

// A URLRequestJob derivative used to retrieve script resources
// from the service workers script cache. It uses a response reader
// and pipes the response to the consumer of this url request job.
class CONTENT_EXPORT ServiceWorkerReadFromCacheJob
    : public net::URLRequestJob {
 public:
  ServiceWorkerReadFromCacheJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      base::WeakPtr<ServiceWorkerContextCore> context,
      int64 response_id);

 private:
  virtual ~ServiceWorkerReadFromCacheJob();

  // net::URLRequestJob overrides
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual net::LoadState GetLoadState() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  // Reader completion callbacks.
  void OnReadInfoComplete(int result);
  void OnReadComplete(int result);

  // Helpers
  const net::HttpResponseInfo* http_info() const;
  bool is_range_request() const { return range_requested_.IsValid(); }
  void SetupRangeResponse(int response_data_size);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  int64 response_id_;
  scoped_ptr<ServiceWorkerResponseReader> reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> http_info_io_buffer_;
  scoped_ptr<net::HttpResponseInfo> http_info_;
  net::HttpByteRange range_requested_;
  scoped_ptr<net::HttpResponseInfo> range_response_info_;
  bool has_been_killed_;
  base::WeakPtrFactory<ServiceWorkerReadFromCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerReadFromCacheJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_READ_FROM_CACHE_JOB_H_
