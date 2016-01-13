// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
#define CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace blink {
class WebDataSource;
}

namespace content {

class DocumentState;

// Stores internal state per WebDataSource.
class InternalDocumentStateData : public base::SupportsUserData::Data {
 public:
  InternalDocumentStateData();

  static InternalDocumentStateData* FromDataSource(blink::WebDataSource* ds);
  static InternalDocumentStateData* FromDocumentState(DocumentState* ds);

  // Set to true once RenderViewImpl::didFirstVisuallyNonEmptyLayout() is
  // invoked.
  bool did_first_visually_non_empty_layout() const {
    return did_first_visually_non_empty_layout_;
  }
  void set_did_first_visually_non_empty_layout(bool value) {
    did_first_visually_non_empty_layout_ = value;
  }

  // Set to true once RenderViewImpl::DidFlushPaint() is inovked after
  // RenderViewImpl::didFirstVisuallyNonEmptyLayout(). In other words after the
  // page has painted something.
  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }
  void set_did_first_visually_non_empty_paint(bool value) {
    did_first_visually_non_empty_paint_ = value;
  }

  int http_status_code() const { return http_status_code_; }
  void set_http_status_code(int http_status_code) {
    http_status_code_ = http_status_code;
  }

  const GURL& searchable_form_url() const { return searchable_form_url_; }
  void set_searchable_form_url(const GURL& url) { searchable_form_url_ = url; }
  const std::string& searchable_form_encoding() const {
    return searchable_form_encoding_;
  }
  void set_searchable_form_encoding(const std::string& encoding) {
    searchable_form_encoding_ = encoding;
  }

  // True if an error page should be used, if the http status code also
  // indicates an error.
  bool use_error_page() const { return use_error_page_; }
  void set_use_error_page(bool use_error_page) {
    use_error_page_ = use_error_page;
  }

  // True if the user agent was overridden for this page.
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  void set_is_overriding_user_agent(bool state) {
    is_overriding_user_agent_ = state;
  }

  // True if we have to reset the scroll and scale state of the page
  // after the provisional load has been committed.
  bool must_reset_scroll_and_scale_state() const {
    return must_reset_scroll_and_scale_state_;
  }
  void set_must_reset_scroll_and_scale_state(bool state) {
    must_reset_scroll_and_scale_state_ = state;
  }

  // Sets the cache policy. The cache policy is only used if explicitly set and
  // by default is not set. You can mark a NavigationState as not having a cache
  // state by way of clear_cache_policy_override.
  void set_cache_policy_override(
      blink::WebURLRequest::CachePolicy cache_policy) {
    cache_policy_override_ = cache_policy;
    cache_policy_override_set_ = true;
  }
  blink::WebURLRequest::CachePolicy cache_policy_override() const {
    return cache_policy_override_;
  }
  void clear_cache_policy_override() {
    cache_policy_override_set_ = false;
    cache_policy_override_ = blink::WebURLRequest::UseProtocolCachePolicy;
  }
  bool is_cache_policy_override_set() const {
    return cache_policy_override_set_;
  }

 protected:
  virtual ~InternalDocumentStateData();

 private:
  bool did_first_visually_non_empty_layout_;
  bool did_first_visually_non_empty_paint_;
  int http_status_code_;
  GURL searchable_form_url_;
  std::string searchable_form_encoding_;
  bool use_error_page_;
  bool is_overriding_user_agent_;
  bool must_reset_scroll_and_scale_state_;
  bool cache_policy_override_set_;
  blink::WebURLRequest::CachePolicy cache_policy_override_;

  DISALLOW_COPY_AND_ASSIGN(InternalDocumentStateData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
