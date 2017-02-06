// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

// PlzNavigate
bool ShouldMakeNetworkRequestForURL(const GURL& url) {
  CHECK(IsBrowserSideNavigationEnabled());

  // Data URLs, Javascript URLs, about:blank, srcdoc should not send a request
  // to the network stack.
  // TODO(clamy): same document navigations should not send requests to the
  // network stack. Neither should pushState/popState.
  return !url.SchemeIs(url::kDataScheme) && url != GURL(url::kAboutBlankURL) &&
         !url.SchemeIs(url::kJavaScriptScheme) && !url.is_empty() &&
         !url.SchemeIs(url::kContentIDScheme) &&
         url != GURL(content::kAboutSrcDocURL);
}

CommonNavigationParams::CommonNavigationParams()
    : transition(ui::PAGE_TRANSITION_LINK),
      navigation_type(FrameMsg_Navigate_Type::NORMAL),
      allow_download(true),
      should_replace_current_entry(false),
      report_type(FrameMsg_UILoadMetricsReportType::NO_REPORT),
      lofi_state(LOFI_UNSPECIFIED),
      navigation_start(base::TimeTicks::Now()),
      method("GET") {}

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool allow_download,
    bool should_replace_current_entry,
    base::TimeTicks ui_timestamp,
    FrameMsg_UILoadMetricsReportType::Value report_type,
    const GURL& base_url_for_data_url,
    const GURL& history_url_for_data_url,
    LoFiState lofi_state,
    const base::TimeTicks& navigation_start,
    std::string method,
    const scoped_refptr<ResourceRequestBodyImpl>& post_data)
    : url(url),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      allow_download(allow_download),
      should_replace_current_entry(should_replace_current_entry),
      ui_timestamp(ui_timestamp),
      report_type(report_type),
      base_url_for_data_url(base_url_for_data_url),
      history_url_for_data_url(history_url_for_data_url),
      lofi_state(lofi_state),
      navigation_start(navigation_start),
      method(method),
      post_data(post_data) {
  // |method != "POST"| should imply absence of |post_data|.
  if (method != "POST" && post_data) {
    NOTREACHED();
    this->post_data = nullptr;
  }
}

CommonNavigationParams::CommonNavigationParams(
    const CommonNavigationParams& other) = default;

CommonNavigationParams::~CommonNavigationParams() {
}

BeginNavigationParams::BeginNavigationParams()
    : load_flags(0),
      has_user_gesture(false),
      skip_service_worker(false),
      request_context_type(REQUEST_CONTEXT_TYPE_LOCATION) {}

BeginNavigationParams::BeginNavigationParams(
    std::string headers,
    int load_flags,
    bool has_user_gesture,
    bool skip_service_worker,
    RequestContextType request_context_type)
    : headers(headers),
      load_flags(load_flags),
      has_user_gesture(has_user_gesture),
      skip_service_worker(skip_service_worker),
      request_context_type(request_context_type) {}

BeginNavigationParams::BeginNavigationParams(
    const BeginNavigationParams& other) = default;

StartNavigationParams::StartNavigationParams()
    :
#if defined(OS_ANDROID)
      has_user_gesture(false),
#endif
      transferred_request_child_id(-1),
      transferred_request_request_id(-1) {
}

StartNavigationParams::StartNavigationParams(
    const std::string& extra_headers,
#if defined(OS_ANDROID)
    bool has_user_gesture,
#endif
    int transferred_request_child_id,
    int transferred_request_request_id)
    : extra_headers(extra_headers),
#if defined(OS_ANDROID)
      has_user_gesture(has_user_gesture),
#endif
      transferred_request_child_id(transferred_request_child_id),
      transferred_request_request_id(transferred_request_request_id) {
}

StartNavigationParams::StartNavigationParams(
    const StartNavigationParams& other) = default;

StartNavigationParams::~StartNavigationParams() {
}

RequestNavigationParams::RequestNavigationParams()
    : is_overriding_user_agent(false),
      can_load_local_resources(false),
      request_time(base::Time::Now()),
      page_id(-1),
      nav_entry_id(0),
      is_same_document_history_load(false),
      has_committed_real_load(false),
      intended_as_new_entry(false),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(0),
      is_view_source(false),
      should_clear_history_list(false),
      should_create_service_worker(false) {}

RequestNavigationParams::RequestNavigationParams(
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    bool can_load_local_resources,
    base::Time request_time,
    const PageState& page_state,
    int32_t page_id,
    int nav_entry_id,
    bool is_same_document_history_load,
    bool has_committed_real_load,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    bool is_view_source,
    bool should_clear_history_list)
    : is_overriding_user_agent(is_overriding_user_agent),
      redirects(redirects),
      can_load_local_resources(can_load_local_resources),
      request_time(request_time),
      page_state(page_state),
      page_id(page_id),
      nav_entry_id(nav_entry_id),
      is_same_document_history_load(is_same_document_history_load),
      has_committed_real_load(has_committed_real_load),
      intended_as_new_entry(intended_as_new_entry),
      pending_history_list_offset(pending_history_list_offset),
      current_history_list_offset(current_history_list_offset),
      current_history_list_length(current_history_list_length),
      is_view_source(is_view_source),
      should_clear_history_list(should_clear_history_list),
      should_create_service_worker(false) {}

RequestNavigationParams::RequestNavigationParams(
    const RequestNavigationParams& other) = default;

RequestNavigationParams::~RequestNavigationParams() {
}

NavigationParams::NavigationParams(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params)
    : common_params(common_params),
      start_params(start_params),
      request_params(request_params) {
}

NavigationParams::~NavigationParams() {
}

}  // namespace content
