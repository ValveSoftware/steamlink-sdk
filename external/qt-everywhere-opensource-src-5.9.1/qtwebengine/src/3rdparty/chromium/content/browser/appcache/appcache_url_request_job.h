// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_executable_handler.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace net {
class GrowableIOBuffer;
};

namespace content {
class AppCacheHost;
class AppCacheRequestHandlerTest;
class AppCacheURLRequestJobTest;

// A net::URLRequestJob derivative that knows how to return a response stored
// in the appcache.
class CONTENT_EXPORT AppCacheURLRequestJob
    : public net::URLRequestJob,
      public AppCacheStorage::Delegate {
 public:
  // Callback that will be invoked before the request is restarted. The caller
  // can use this opportunity to grab state from the AppCacheURLRequestJob to
  // determine how it should behave when the request is restarted.
  using OnPrepareToRestartCallback = base::Closure;

  AppCacheURLRequestJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        AppCacheStorage* storage,
                        AppCacheHost* host,
                        bool is_main_resource,
                        const OnPrepareToRestartCallback& restart_callback_);

  ~AppCacheURLRequestJob() override;

  // Informs the job of what response it should deliver. Only one of these
  // methods should be called, and only once per job. A job will sit idle and
  // wait indefinitely until one of the deliver methods is called.
  void DeliverAppCachedResponse(const GURL& manifest_url,
                                int64_t cache_id,
                                const AppCacheEntry& entry,
                                bool is_fallback);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  bool is_waiting() const {
    return delivery_type_ == AWAITING_DELIVERY_ORDERS;
  }

  bool is_delivering_appcache_response() const {
    return delivery_type_ == APPCACHED_DELIVERY;
  }

  bool is_delivering_network_response() const {
    return delivery_type_ == NETWORK_DELIVERY;
  }

  bool is_delivering_error_response() const {
    return delivery_type_ == ERROR_DELIVERY;
  }

  // Accessors for the info about the appcached response, if any,
  // that this job has been instructed to deliver. These are only
  // valid to call if is_delivering_appcache_response.
  const GURL& manifest_url() const { return manifest_url_; }
  int64_t cache_id() const { return cache_id_; }
  const AppCacheEntry& entry() const { return entry_; }

  // net::URLRequestJob's Kill method is made public so the users of this
  // class in the appcache namespace can call it.
  void Kill() override;

  // Returns true if the job has been started by the net library.
  bool has_been_started() const {
    return has_been_started_;
  }

  // Returns true if the job has been killed.
  bool has_been_killed() const {
    return has_been_killed_;
  }

  // Returns true if the cache entry was not found in the disk cache.
  bool cache_entry_not_found() const {
    return cache_entry_not_found_;
  }

 private:
  friend class AppCacheRequestHandlerTest;
  friend class AppCacheURLRequestJobTest;

  // Friend so it can get a weak pointer.
  friend class AppCacheRequestHandler;

  enum DeliveryType {
    AWAITING_DELIVERY_ORDERS,
    APPCACHED_DELIVERY,
    NETWORK_DELIVERY,
    ERROR_DELIVERY
  };

  base::WeakPtr<AppCacheURLRequestJob> GetWeakPtr();

  // Returns true if one of the Deliver methods has been called.
  bool has_delivery_orders() const {
    return !is_waiting();
  }

  void MaybeBeginDelivery();
  void BeginDelivery();

  // For executable response handling.
  void BeginExecutableHandlerDelivery();
  void OnExecutableSourceLoaded(int result);
  void InvokeExecutableHandler(AppCacheExecutableHandler* handler);
  void OnExecutableResponseCallback(
      const AppCacheExecutableHandler::Response& response);
  void BeginErrorDelivery(const char* message);

  // AppCacheStorage::Delegate methods
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;
  void OnCacheLoaded(AppCache* cache, int64_t cache_id) override;

  const net::HttpResponseInfo* http_info() const;
  bool is_range_request() const { return range_requested_.IsValid(); }
  void SetupRangeResponse();

  // AppCacheResponseReader completion callback
  void OnReadComplete(int result);

  // net::URLRequestJob methods, see url_request_job.h for doc comments
  void Start() override;
  net::LoadState GetLoadState() const override;
  bool GetCharset(std::string* charset) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  net::HostPortPair GetSocketAddress() const override;

  // Sets extra request headers for Job types that support request headers.
  // This is how we get informed of range-requests.
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

  // FilterContext methods
  bool GetMimeType(std::string* mime_type) const override;
  int GetResponseCode() const override;

  // Invokes |prepare_to_restart_callback_| and then calls
  // net::URLRequestJob::NotifyRestartRequired.
  void NotifyRestartRequired();

  AppCacheHost* host_;
  AppCacheStorage* storage_;
  base::TimeTicks start_time_tick_;
  bool has_been_started_;
  bool has_been_killed_;
  DeliveryType delivery_type_;
  GURL manifest_url_;
  int64_t cache_id_;
  AppCacheEntry entry_;
  bool is_fallback_;
  bool is_main_resource_;  // Used for histogram logging.
  bool cache_entry_not_found_;
  scoped_refptr<AppCacheResponseInfo> info_;
  scoped_refptr<net::GrowableIOBuffer> handler_source_buffer_;
  std::unique_ptr<AppCacheResponseReader> handler_source_reader_;
  net::HttpByteRange range_requested_;
  std::unique_ptr<net::HttpResponseInfo> range_response_info_;
  std::unique_ptr<AppCacheResponseReader> reader_;
  scoped_refptr<AppCache> cache_;
  scoped_refptr<AppCacheGroup> group_;
  const OnPrepareToRestartCallback on_prepare_to_restart_callback_;
  base::WeakPtrFactory<AppCacheURLRequestJob> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_
