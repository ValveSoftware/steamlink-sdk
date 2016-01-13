// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains URLFetcher, a wrapper around URLRequest that handles
// low-level details like thread safety, ref counting, and incremental buffer
// reading.  This is useful for callers who simply want to get the data from a
// URL and don't care about all the nitty-gritty details.
//
// NOTE(willchan): Only one "IO" thread is supported for URLFetcher.  This is a
// temporary situation.  We will work on allowing support for multiple "io"
// threads per process.

#ifndef NET_URL_REQUEST_URL_FETCHER_IMPL_H_
#define NET_URL_REQUEST_URL_FETCHER_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/url_request/url_fetcher.h"

namespace net {
class URLFetcherCore;
class URLFetcherDelegate;
class URLFetcherFactory;

class NET_EXPORT_PRIVATE URLFetcherImpl : public URLFetcher {
 public:
  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  URLFetcherImpl(const GURL& url,
                 RequestType request_type,
                 URLFetcherDelegate* d);
  virtual ~URLFetcherImpl();

  // URLFetcher implementation:
  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) OVERRIDE;
  virtual void SetUploadFilePath(
      const std::string& upload_content_type,
      const base::FilePath& file_path,
      uint64 range_offset,
      uint64 range_length,
      scoped_refptr<base::TaskRunner> file_task_runner) OVERRIDE;
  virtual void SetChunkedUpload(
      const std::string& upload_content_type) OVERRIDE;
  virtual void AppendChunkToUpload(const std::string& data,
                                   bool is_last_chunk) OVERRIDE;
  virtual void SetLoadFlags(int load_flags) OVERRIDE;
  virtual int GetLoadFlags() const OVERRIDE;
  virtual void SetReferrer(const std::string& referrer) OVERRIDE;
  virtual void SetReferrerPolicy(
      URLRequest::ReferrerPolicy referrer_policy) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const std::string& extra_request_headers) OVERRIDE;
  virtual void AddExtraRequestHeader(const std::string& header_line) OVERRIDE;
  virtual void SetRequestContext(
      URLRequestContextGetter* request_context_getter) OVERRIDE;
  virtual void SetFirstPartyForCookies(
      const GURL& first_party_for_cookies) OVERRIDE;
  virtual void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) OVERRIDE;
  virtual void SetStopOnRedirect(bool stop_on_redirect) OVERRIDE;
  virtual void SetAutomaticallyRetryOn5xx(bool retry) OVERRIDE;
  virtual void SetMaxRetriesOn5xx(int max_retries) OVERRIDE;
  virtual int GetMaxRetriesOn5xx() const OVERRIDE;
  virtual base::TimeDelta GetBackoffDelay() const OVERRIDE;
  virtual void SetAutomaticallyRetryOnNetworkChanges(int max_retries) OVERRIDE;
  virtual void SaveResponseToFileAtPath(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) OVERRIDE;
  virtual void SaveResponseToTemporaryFile(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) OVERRIDE;
  virtual void SaveResponseWithWriter(
      scoped_ptr<URLFetcherResponseWriter> response_writer) OVERRIDE;
  virtual HttpResponseHeaders* GetResponseHeaders() const OVERRIDE;
  virtual HostPortPair GetSocketAddress() const OVERRIDE;
  virtual bool WasFetchedViaProxy() const OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual const GURL& GetOriginalURL() const OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const URLRequestStatus& GetStatus() const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual const ResponseCookies& GetCookies() const OVERRIDE;
  virtual void ReceivedContentWasMalformed() OVERRIDE;
  virtual bool GetResponseAsString(
      std::string* out_response_string) const OVERRIDE;
  virtual bool GetResponseAsFilePath(
      bool take_ownership,
      base::FilePath* out_response_path) const OVERRIDE;

  static void CancelAll();

  static void SetEnableInterceptionForTests(bool enabled);
  static void SetIgnoreCertificateRequests(bool ignored);

  // TODO(akalin): Make these private again once URLFetcher::Create()
  // is in net/.

  static URLFetcherFactory* factory();

  // Sets the factory used by the static method Create to create a URLFetcher.
  // URLFetcher does not take ownership of |factory|. A value of NULL results
  // in a URLFetcher being created directly.
  //
  // NOTE: for safety, this should only be used through ScopedURLFetcherFactory!
  static void set_factory(URLFetcherFactory* factory);

 protected:
  // Returns the delegate.
  URLFetcherDelegate* delegate() const;

 private:
  friend class URLFetcherTest;

  // Only used by URLFetcherTest, returns the number of URLFetcher::Core objects
  // actively running.
  static int GetNumFetcherCores();

  const scoped_refptr<URLFetcherCore> core_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherImpl);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_IMPL_H_
