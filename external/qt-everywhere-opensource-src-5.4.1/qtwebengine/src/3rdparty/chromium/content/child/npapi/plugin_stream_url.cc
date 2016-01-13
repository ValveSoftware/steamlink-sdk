// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_stream_url.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/plugin_url_fetcher.h"
#include "content/child/npapi/webplugin.h"
#include "net/http/http_response_headers.h"

namespace content {

PluginStreamUrl::PluginStreamUrl(
    unsigned long resource_id,
    const GURL &url,
    PluginInstance *instance,
    bool notify_needed,
    void *notify_data)
    : PluginStream(instance, url.spec().c_str(), notify_needed, notify_data),
      url_(url),
      id_(resource_id) {
}

void PluginStreamUrl::SetPluginURLFetcher(PluginURLFetcher* fetcher) {
  plugin_url_fetcher_.reset(fetcher);
}

void PluginStreamUrl::URLRedirectResponse(bool allow) {
  if (plugin_url_fetcher_.get()) {
    plugin_url_fetcher_->URLRedirectResponse(allow);
  } else {
    instance()->webplugin()->URLRedirectResponse(allow, id_);
  }

  if (allow)
    UpdateUrl(pending_redirect_url_.c_str());
}

void PluginStreamUrl::FetchRange(const std::string& range) {
  PluginURLFetcher* range_fetcher = new PluginURLFetcher(
      this, url_, plugin_url_fetcher_->first_party_for_cookies(), "GET", NULL,
      0, plugin_url_fetcher_->referrer(), range, false, false,
      plugin_url_fetcher_->origin_pid(),
      plugin_url_fetcher_->render_frame_id(),
      plugin_url_fetcher_->render_view_id(), id_,
      plugin_url_fetcher_->copy_stream_data());
  range_request_fetchers_.push_back(range_fetcher);
}

bool PluginStreamUrl::Close(NPReason reason) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the destroy stream handler.
  scoped_refptr<PluginStream> protect(this);
  CancelRequest();
  bool result = PluginStream::Close(reason);
  instance()->RemoveStream(this);
  return result;
}

WebPluginResourceClient* PluginStreamUrl::AsResourceClient() {
  return static_cast<WebPluginResourceClient*>(this);
}

void PluginStreamUrl::CancelRequest() {
  if (id_ > 0) {
    if (plugin_url_fetcher_.get()) {
      plugin_url_fetcher_->Cancel();
    } else {
      if (instance()->webplugin()) {
        instance()->webplugin()->CancelResource(id_);
      }
    }
    id_ = 0;
  }
  if (instance()->webplugin()) {
    for (size_t i = 0; i < range_requests_.size(); ++i)
      instance()->webplugin()->CancelResource(range_requests_[i]);
  }

  range_requests_.clear();

  STLDeleteElements(&range_request_fetchers_);
}

void PluginStreamUrl::WillSendRequest(const GURL& url, int http_status_code) {
  if (notify_needed()) {
    // If the plugin participates in HTTP url redirect handling then notify it.
    if (net::HttpResponseHeaders::IsRedirectResponseCode(http_status_code) &&
        instance()->handles_url_redirects()) {
      pending_redirect_url_ = url.spec();
      instance()->NPP_URLRedirectNotify(url.spec().c_str(), http_status_code,
          notify_data());
      return;
    }
  }
  url_ = url;
  UpdateUrl(url.spec().c_str());
}

void PluginStreamUrl::DidReceiveResponse(const std::string& mime_type,
                                         const std::string& headers,
                                         uint32 expected_length,
                                         uint32 last_modified,
                                         bool request_is_seekable) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the new stream handler.
  scoped_refptr<PluginStream> protect(this);

  bool opened = Open(mime_type,
                     headers,
                     expected_length,
                     last_modified,
                     request_is_seekable);
  if (!opened) {
    CancelRequest();
    instance()->RemoveStream(this);
  } else {
    SetDeferLoading(false);
  }
}

void PluginStreamUrl::DidReceiveData(const char* buffer, int length,
                                     int data_offset) {
  if (!open())
    return;

  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the write handlers
  scoped_refptr<PluginStream> protect(this);

  if (length > 0) {
    // The PluginStreamUrl instance could get deleted if the plugin fails to
    // accept data in NPP_Write.
    if (Write(const_cast<char*>(buffer), length, data_offset) > 0) {
      SetDeferLoading(false);
    }
  }
}

void PluginStreamUrl::DidFinishLoading(unsigned long resource_id) {
  if (!seekable()) {
    Close(NPRES_DONE);
  } else {
    std::vector<unsigned long>::iterator it_resource = std::find(
        range_requests_.begin(),
        range_requests_.end(),
        resource_id);
    // Resource id must be known to us - either main resource id, or one
    // of the resources, created for range requests.
    DCHECK(resource_id == id_ || it_resource != range_requests_.end());
    // We should notify the plugin about failed/finished requests to ensure
    // that the number of active resource clients does not continue to grow.
    if (instance()->webplugin())
      instance()->webplugin()->CancelResource(resource_id);
    if (it_resource != range_requests_.end())
      range_requests_.erase(it_resource);
  }
}

void PluginStreamUrl::DidFail(unsigned long resource_id) {
  Close(NPRES_NETWORK_ERR);
}

bool PluginStreamUrl::IsMultiByteResponseExpected() {
  return seekable();
}

int PluginStreamUrl::ResourceId() {
  return id_;
}

PluginStreamUrl::~PluginStreamUrl() {
  if (!plugin_url_fetcher_.get() && instance() && instance()->webplugin()) {
    instance()->webplugin()->ResourceClientDeleted(AsResourceClient());
  }

  STLDeleteElements(&range_request_fetchers_);
}

void PluginStreamUrl::AddRangeRequestResourceId(unsigned long resource_id) {
  DCHECK_NE(resource_id, 0u);
  range_requests_.push_back(resource_id);
}

void PluginStreamUrl::SetDeferLoading(bool value) {
  // If we determined that the request had failed via the HTTP headers in the
  // response then we send out a failure notification to the plugin process, as
  // certain plugins don't handle HTTP failure codes correctly.
  if (plugin_url_fetcher_.get()) {
    if (!value && plugin_url_fetcher_->pending_failure_notification()) {
      // This object may be deleted now.
      DidFail(id_);
    }
    return;
  }
  if (id_ > 0)
    instance()->webplugin()->SetDeferResourceLoading(id_, value);
  for (size_t i = 0; i < range_requests_.size(); ++i)
    instance()->webplugin()->SetDeferResourceLoading(range_requests_[i],
                                                     value);
}

void PluginStreamUrl::UpdateUrl(const char* url) {
  DCHECK(!open());
  free(const_cast<char*>(stream()->url));
  stream()->url = base::strdup(url);
  pending_redirect_url_.clear();
}

}  // namespace content
