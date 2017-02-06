// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_
#define CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/http/http_response_info.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "url/gurl.h"

namespace content {

class NavigationState;

// The RenderView stores an instance of this class in the "extra data" of each
// WebDataSource (see RenderView::DidCreateDataSource).
class CONTENT_EXPORT DocumentState
    : NON_EXPORTED_BASE(public blink::WebDataSource::ExtraData),
      public base::SupportsUserData {
 public:
  // The exact values of this enum are used in histograms, so new values must be
  // added to the end.
  enum LoadType {
    UNDEFINED_LOAD,            // Not yet initialized.
    RELOAD,                    // User pressed reload.
    HISTORY_LOAD,              // Back or forward.
    NORMAL_LOAD,               // User entered URL, or omnibox search.
    LINK_LOAD,                 // (deprecated) Included next 4 categories.
    LINK_LOAD_NORMAL,          // Commonly following of link.
    LINK_LOAD_RELOAD,          // JS/link directed reload.
    LINK_LOAD_CACHE_STALE_OK,  // back/forward or encoding change.
    LINK_LOAD_CACHE_ONLY,      // Allow stale data (avoid doing a re-post)
    kLoadTypeMax               // Bounding value for this enum.
  };

  DocumentState();
  ~DocumentState() override;

  static DocumentState* FromDataSource(blink::WebDataSource* ds) {
    return static_cast<DocumentState*>(ds->getExtraData());
  }

  // The time that this navigation was requested.
  const base::Time& request_time() const {
    return request_time_;
  }
  void set_request_time(const base::Time& value) {
    DCHECK(start_load_time_.is_null());
    request_time_ = value;
  }

  // The time that the document load started.
  const base::Time& start_load_time() const {
    return start_load_time_;
  }
  void set_start_load_time(const base::Time& value) {
    // TODO(jar): This should not be set twice.
    // DCHECK(!start_load_time_.is_null());
    DCHECK(finish_document_load_time_.is_null());
    start_load_time_ = value;
  }

  // The time that the document load was committed.
  const base::Time& commit_load_time() const {
    return commit_load_time_;
  }
  void set_commit_load_time(const base::Time& value) {
    commit_load_time_ = value;
  }

  // The time that the document finished loading.
  const base::Time& finish_document_load_time() const {
    return finish_document_load_time_;
  }
  void set_finish_document_load_time(const base::Time& value) {
    // TODO(jar): Some unittests break the following DCHECK, and don't have
    // DCHECK(!start_load_time_.is_null());
    DCHECK(!value.is_null());
    // TODO(jar): Double setting does happen, but probably shouldn't.
    // DCHECK(finish_document_load_time_.is_null());
    // TODO(jar): We should guarantee this order :-(.
    // DCHECK(finish_load_time_.is_null());
    finish_document_load_time_ = value;
  }

  // The time that the document and all subresources finished loading.
  const base::Time& finish_load_time() const { return finish_load_time_; }
  void set_finish_load_time(const base::Time& value) {
    DCHECK(!value.is_null());
    DCHECK(finish_load_time_.is_null());
    // The following is not already set in all cases :-(
    // DCHECK(!finish_document_load_time_.is_null());
    finish_load_time_ = value;
  }

  // The time that painting first happened after a new navigation.
  const base::Time& first_paint_time() const { return first_paint_time_; }
  void set_first_paint_time(const base::Time& value) {
    first_paint_time_ = value;
  }

  // The time that painting first happened after the document loaded.
  const base::Time& first_paint_after_load_time() const {
    return first_paint_after_load_time_;
  }
  void set_first_paint_after_load_time(const base::Time& value) {
    first_paint_after_load_time_ = value;
  }

  // True iff the histograms for the associated frame have been dumped.
  bool load_histograms_recorded() const { return load_histograms_recorded_; }
  void set_load_histograms_recorded(bool value) {
    load_histograms_recorded_ = value;
  }

  bool web_timing_histograms_recorded() const {
    return web_timing_histograms_recorded_;
  }
  void set_web_timing_histograms_recorded(bool value) {
    web_timing_histograms_recorded_ = value;
  }

  // Indicator if SPDY was used as part of this page load.
  bool was_fetched_via_spdy() const { return was_fetched_via_spdy_; }
  void set_was_fetched_via_spdy(bool value) { was_fetched_via_spdy_ = value; }

  bool was_npn_negotiated() const { return was_npn_negotiated_; }
  void set_was_npn_negotiated(bool value) { was_npn_negotiated_ = value; }

  const std::string& npn_negotiated_protocol() const {
    return npn_negotiated_protocol_;
  }
  void set_npn_negotiated_protocol(const std::string& value) {
    npn_negotiated_protocol_ = value;
  }

  bool was_alternate_protocol_available() const {
    return was_alternate_protocol_available_;
  }
  void set_was_alternate_protocol_available(bool value) {
    was_alternate_protocol_available_ = value;
  }

  net::HttpResponseInfo::ConnectionInfo connection_info() const {
    return connection_info_;
  }
  void set_connection_info(
      net::HttpResponseInfo::ConnectionInfo connection_info) {
    connection_info_ = connection_info;
  }

  bool was_fetched_via_proxy() const { return was_fetched_via_proxy_; }
  void set_was_fetched_via_proxy(bool value) {
    was_fetched_via_proxy_ = value;
  }

  const net::HostPortPair& proxy_server() const { return proxy_server_; }
  void set_proxy_server(const net::HostPortPair& proxy_server) {
    proxy_server_ = proxy_server;
  }

  void set_was_prefetcher(bool value) { was_prefetcher_ = value; }
  bool was_prefetcher() const { return was_prefetcher_; }

  void set_was_referred_by_prefetcher(bool value) {
    was_referred_by_prefetcher_ = value;
  }
  bool was_referred_by_prefetcher() const {
    return was_referred_by_prefetcher_;
  }

  void set_was_after_preconnect_request(bool value) {
    was_after_preconnect_request_ = value;
  }
  bool was_after_preconnect_request() { return was_after_preconnect_request_; }

  // For LoadDataWithBaseURL navigations, |was_load_data_with_base_url_request_|
  // is set to true and |data_url_| is set to the data URL of the navigation.
  // Otherwise, |was_load_data_with_base_url_request_| is false and |data_url_|
  // is empty.
  void set_was_load_data_with_base_url_request(bool value) {
    was_load_data_with_base_url_request_ = value;
  }
  bool was_load_data_with_base_url_request() const {
    return was_load_data_with_base_url_request_;
  }
  const GURL& data_url() const {
    return data_url_;
  }
  void set_data_url(const GURL& data_url) {
    data_url_ = data_url;
  }

  // Record the nature of this load, for use when histogramming page load times.
  LoadType load_type() const { return load_type_; }
  void set_load_type(LoadType load_type) { load_type_ = load_type; }

  NavigationState* navigation_state() { return navigation_state_.get(); }
  void set_navigation_state(NavigationState* navigation_state);

  bool can_load_local_resources() const { return can_load_local_resources_; }
  void set_can_load_local_resources(bool can_load) {
    can_load_local_resources_ = can_load;
  }

 private:
  base::Time request_time_;
  base::Time start_load_time_;
  base::Time commit_load_time_;
  base::Time finish_document_load_time_;
  base::Time finish_load_time_;
  base::Time first_paint_time_;
  base::Time first_paint_after_load_time_;
  bool load_histograms_recorded_;
  bool web_timing_histograms_recorded_;
  bool was_fetched_via_spdy_;
  bool was_npn_negotiated_;
  std::string npn_negotiated_protocol_;
  bool was_alternate_protocol_available_;
  net::HttpResponseInfo::ConnectionInfo connection_info_;
  bool was_fetched_via_proxy_;
  net::HostPortPair proxy_server_;

  // A prefetcher is a page that contains link rel=prefetch elements.
  bool was_prefetcher_;
  bool was_referred_by_prefetcher_;
  bool was_after_preconnect_request_;

  bool was_load_data_with_base_url_request_;
  GURL data_url_;

  LoadType load_type_;

  std::unique_ptr<NavigationState> navigation_state_;

  bool can_load_local_resources_;
};

#endif  // CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_

}  // namespace content
