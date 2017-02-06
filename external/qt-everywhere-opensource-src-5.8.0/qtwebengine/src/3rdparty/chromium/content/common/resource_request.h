// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESOURCE_REQUEST_H_
#define CONTENT_COMMON_RESOURCE_REQUEST_H_

#include <stdint.h>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "net/base/request_priority.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT ResourceRequest {
  ResourceRequest();
  ResourceRequest(const ResourceRequest& request);
  ~ResourceRequest();

  // The request method: GET, POST, etc.
  std::string method;

  // The requested URL.
  GURL url;

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy. Leaving it empty may
  // lead to undesired cookie blocking. Third-party cookie blocking can be
  // bypassed by setting first_party_for_cookies = url, but this should ideally
  // only be done if there really is no way to determine the correct value.
  GURL first_party_for_cookies;

  // The origin of the context which initiated the request, which will be used
  // for cookie checks like 'First-Party-Only'.
  url::Origin request_initiator;

  // The referrer to use (may be empty).
  GURL referrer;

  // The referrer policy to use.
  blink::WebReferrerPolicy referrer_policy = blink::WebReferrerPolicyAlways;

  // The frame's visiblity state.
  blink::WebPageVisibilityState visiblity_state =
      blink::WebPageVisibilityStateVisible;

  // Additional HTTP request headers.
  std::string headers;

  // net::URLRequest load flags (0 by default).
  int load_flags = 0;

  // Process ID from which this request originated, or zero if it originated
  // in the renderer itself.
  int origin_pid = 0;

  // What this resource load is for (main frame, sub-frame, sub-resource,
  // object).
  ResourceType resource_type = RESOURCE_TYPE_MAIN_FRAME;

  // The priority of this request.
  net::RequestPriority priority = net::IDLE;

  // Used by plugin->browser requests to get the correct net::URLRequestContext.
  uint32_t request_context = 0;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kAppCacheNoHostId.
  int appcache_host_id = kAppCacheNoHostId;

  // True if corresponding AppCache group should be resetted.
  bool should_reset_appcache = false;

  // Indicates which frame (or worker context) the request is being loaded into,
  // or kInvalidServiceWorkerProviderId.
  int service_worker_provider_id = kInvalidServiceWorkerProviderId;

  // True if the request originated from a Service Worker, e.g. due to a
  // fetch() in the Service Worker script.
  bool originated_from_service_worker = false;

  // Indicates which types of ServiceWorkers should skip handling this request.
  SkipServiceWorker skip_service_worker = SkipServiceWorker::NONE;

  // The request mode passed to the ServiceWorker.
  FetchRequestMode fetch_request_mode = FETCH_REQUEST_MODE_SAME_ORIGIN;

  // The credentials mode passed to the ServiceWorker.
  FetchCredentialsMode fetch_credentials_mode = FETCH_CREDENTIALS_MODE_OMIT;

  // The redirect mode used in Fetch API.
  FetchRedirectMode fetch_redirect_mode = FetchRedirectMode::FOLLOW_MODE;

  // The request context passed to the ServiceWorker.
  RequestContextType fetch_request_context_type =
      REQUEST_CONTEXT_TYPE_UNSPECIFIED;

  // The frame type passed to the ServiceWorker.
  RequestContextFrameType fetch_frame_type =
      REQUEST_CONTEXT_FRAME_TYPE_AUXILIARY;

  // Optional resource request body (may be null).
  scoped_refptr<ResourceRequestBodyImpl> request_body;

  bool download_to_file = false;

  // True if the request was user initiated.
  bool has_user_gesture = false;

  // True if load timing data should be collected for request.
  bool enable_load_timing = false;

  // True if upload progress should be available for request.
  bool enable_upload_progress = false;

  // True if login prompts for this request should be supressed.
  bool do_not_prompt_for_login = false;

  // The routing id of the RenderFrame.
  int render_frame_id = 0;

  // True if |frame_id| is the main frame of a RenderView.
  bool is_main_frame = false;

  // True if |parent_render_frame_id| is the main frame of a RenderView.
  bool parent_is_main_frame = false;

  // Identifies the parent frame of the frame that sent the request.
  // -1 if unknown / invalid.
  int parent_render_frame_id = -1;

  ui::PageTransition transition_type = ui::PAGE_TRANSITION_LINK;

  // For navigations, whether this navigation should replace the current session
  // history entry on commit.
  bool should_replace_current_entry = false;

  // The following two members identify a previous request that has been
  // created before this navigation has been transferred to a new process.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  int transferred_request_child_id = -1;
  int transferred_request_request_id = -1;

  // Whether or not we should allow the URL to download.
  bool allow_download = false;

  // Whether to intercept headers to pass back to the renderer.
  bool report_raw_headers = false;

  // Whether or not to request a LoFi version of the resource or let the browser
  // decide.
  LoFiState lofi_state = LOFI_UNSPECIFIED;

  // PlzNavigate: the stream url associated with a navigation. Used to get
  // access to the body of the response that has already been fetched by the
  // browser.
  GURL resource_body_stream_url;

  // Wether or not the initiator of this request is a secure context.
  bool initiated_in_secure_context = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_RESOURCE_REQUEST_H_
