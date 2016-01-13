// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerResponseWriter;
class ServiceWorkerVersions;

// A URLRequestJob derivative used to cache the main script
// and its imports during the initial install of a new version.
// Another separate URLRequest is started which will perform
// a network fetch. The response produced for that separate
// request is written to the service worker script cache and piped
// to the consumer of the ServiceWorkerWriteToCacheJob for delivery
// to the renderer process housing the worker.
class CONTENT_EXPORT ServiceWorkerWriteToCacheJob
    : public net::URLRequestJob,
      public net::URLRequest::Delegate {
 public:
  ServiceWorkerWriteToCacheJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerVersion* version,
      int64 response_id);

 private:
  virtual ~ServiceWorkerWriteToCacheJob();

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

  const net::HttpResponseInfo* http_info() const;

  // Methods to drive the net request forward and
  // write data to the disk cache.
  void InitNetRequest();
  void StartNetRequest();
  net::URLRequestStatus ReadNetData(
      net::IOBuffer* buf,
      int buf_size,
      int *bytes_read);
  void WriteHeadersToCache();
  void OnWriteHeadersComplete(int result);
  void WriteDataToCache(int bytes_to_write);
  void OnWriteDataComplete(int result);

  // net::URLRequest::Delegate overrides that observe the net request.
  virtual void OnReceivedRedirect(
      net::URLRequest* request,
      const GURL& new_url,
      bool* defer_redirect) OVERRIDE;
  virtual void OnAuthRequired(
      net::URLRequest* request,
      net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual void OnSSLCertificateError(
      net::URLRequest* request,
      const net::SSLInfo& ssl_info,
      bool fatal) OVERRIDE;
  virtual void OnBeforeNetworkStart(
      net::URLRequest* request,
      bool* defer) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(
      net::URLRequest* request,
      int bytes_read) OVERRIDE;

  void AsyncNotifyDoneHelper(const net::URLRequestStatus& status);

  scoped_refptr<net::IOBuffer> io_buffer_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  GURL url_;
  int64 response_id_;
  scoped_ptr<net::URLRequest> net_request_;
  scoped_ptr<net::HttpResponseInfo> http_info_;
  scoped_ptr<ServiceWorkerResponseWriter> writer_;
  scoped_refptr<ServiceWorkerVersion> version_;
  bool has_been_killed_;
  bool did_notify_started_;
  bool did_notify_finished_;
  base::WeakPtrFactory<ServiceWorkerWriteToCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerWriteToCacheJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_
