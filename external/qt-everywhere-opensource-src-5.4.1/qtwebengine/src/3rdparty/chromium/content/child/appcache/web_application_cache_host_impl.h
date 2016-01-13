// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
#define CONTENT_CHILD_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_

#include <string>

#include "third_party/WebKit/public/platform/WebApplicationCacheHost.h"
#include "third_party/WebKit/public/platform/WebApplicationCacheHostClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/gurl.h"
#include "webkit/common/appcache/appcache_interfaces.h"

namespace content {

class WebApplicationCacheHostImpl
    : NON_EXPORTED_BASE(public blink::WebApplicationCacheHost) {
 public:
  // Returns the host having given id or NULL if there is no such host.
  static WebApplicationCacheHostImpl* FromId(int id);

  WebApplicationCacheHostImpl(blink::WebApplicationCacheHostClient* client,
                              appcache::AppCacheBackend* backend);
  virtual ~WebApplicationCacheHostImpl();

  int host_id() const { return host_id_; }
  appcache::AppCacheBackend* backend() const { return backend_; }
  blink::WebApplicationCacheHostClient* client() const { return client_; }

  virtual void OnCacheSelected(const appcache::AppCacheInfo& info);
  void OnStatusChanged(appcache::AppCacheStatus);
  void OnEventRaised(appcache::AppCacheEventID);
  void OnProgressEventRaised(const GURL& url, int num_total, int num_complete);
  void OnErrorEventRaised(const appcache::AppCacheErrorDetails& details);
  virtual void OnLogMessage(appcache::AppCacheLogLevel log_level,
                            const std::string& message) {}
  virtual void OnContentBlocked(const GURL& manifest_url) {}

  // blink::WebApplicationCacheHost:
  virtual void willStartMainResourceRequest(
      blink::WebURLRequest&, const blink::WebApplicationCacheHost*);
  virtual void willStartSubResourceRequest(blink::WebURLRequest&);
  virtual void selectCacheWithoutManifest();
  virtual bool selectCacheWithManifest(const blink::WebURL& manifestURL);
  virtual void didReceiveResponseForMainResource(const blink::WebURLResponse&);
  virtual void didReceiveDataForMainResource(const char* data, int len);
  virtual void didFinishLoadingMainResource(bool success);
  virtual blink::WebApplicationCacheHost::Status status();
  virtual bool startUpdate();
  virtual bool swapCache();
  virtual void getResourceList(blink::WebVector<ResourceInfo>* resources);
  virtual void getAssociatedCacheInfo(CacheInfo* info);

 private:
  enum IsNewMasterEntry {
    MAYBE,
    YES,
    NO
  };

  blink::WebApplicationCacheHostClient* client_;
  appcache::AppCacheBackend* backend_;
  int host_id_;
  appcache::AppCacheStatus status_;
  blink::WebURLResponse document_response_;
  GURL document_url_;
  bool is_scheme_supported_;
  bool is_get_method_;
  IsNewMasterEntry is_new_master_entry_;
  appcache::AppCacheInfo cache_info_;
  GURL original_main_resource_url_;  // Used to detect redirection.
  bool was_select_cache_called_;
};

}  // namespace content

#endif  // CONTENT_CHILD_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
