// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include <utility>

#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/resource_request_body_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): Remove the blink code that sets the caching flags when
// PlzNavigate launches.
void UpdateLoadFlagsWithCacheFlags(
    int* load_flags,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool is_post) {
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_MAIN_RESOURCE:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE:
      *load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE:
      *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      *load_flags |=
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::NORMAL:
      if (is_post)
        *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    default:
      break;
  }
}

// This is based on SecurityOrigin::isPotentiallyTrustworthy.
// TODO(clamy): This should be function in url::Origin.
bool IsPotentiallyTrustworthyOrigin(const url::Origin& origin) {
  if (origin.unique())
    return false;

  if (origin.scheme() == url::kHttpsScheme ||
      origin.scheme() == url::kAboutScheme ||
      origin.scheme() == url::kDataScheme ||
      origin.scheme() == url::kWssScheme ||
      origin.scheme() == url::kFileScheme) {
    return true;
  }

  if (net::IsLocalhost(origin.host()))
    return true;

  // TODO(clamy): Check for whitelisted origins.
  return false;
}

// TODO(clamy): This should be function in FrameTreeNode.
bool IsSecureFrame(FrameTreeNode* frame) {
  while (frame) {
    if (!IsPotentiallyTrustworthyOrigin(frame->current_origin()))
      return false;
    frame = frame->parent();
  }
  return true;
}

// TODO(clamy): This should match what's happening in
// blink::FrameFetchContext::addAdditionalRequestHeaders.
void AddAdditionalRequestHeaders(net::HttpRequestHeaders* headers,
                                 const GURL& url,
                                 FrameMsg_Navigate_Type::Value navigation_type,
                                 BrowserContext* browser_context) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  bool is_reload =
      navigation_type == FrameMsg_Navigate_Type::RELOAD ||
      navigation_type == FrameMsg_Navigate_Type::RELOAD_MAIN_RESOURCE ||
      navigation_type == FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE ||
      navigation_type == FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
  if (is_reload)
    headers->RemoveHeader("Save-Data");

  if (GetContentClient()->browser()->IsDataSaverEnabled(browser_context))
    headers->SetHeaderIfMissing("Save-Data", "on");

  headers->SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                              GetContentClient()->GetUserAgent());

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  headers->AddHeaderFromString("Upgrade-Insecure-Requests: 1");
}

}  // namespace

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    const GURL& dest_url,
    const Referrer& dest_referrer,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    FrameMsg_Navigate_Type::Value navigation_type,
    LoFiState lofi_state,
    bool is_same_document_history_load,
    bool is_history_navigation_in_new_child,
    const base::TimeTicks& navigation_start,
    NavigationControllerImpl* controller) {
  // Fill POST data in the request body.
  scoped_refptr<ResourceRequestBodyImpl> request_body;
  if (frame_entry.method() == "POST")
    request_body = frame_entry.GetPostData();

  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, entry.ConstructCommonNavigationParams(
                           frame_entry, request_body, dest_url, dest_referrer,
                           navigation_type, lofi_state, navigation_start),
      BeginNavigationParams(entry.extra_headers(), net::LOAD_NORMAL,
                            false,  // has_user_gestures
                            false,  // skip_service_worker
                            REQUEST_CONTEXT_TYPE_LOCATION),
      entry.ConstructRequestNavigationParams(
          frame_entry, is_same_document_history_load,
          is_history_navigation_in_new_child,
          entry.GetSubframeUniqueNames(frame_tree_node),
          frame_tree_node->has_committed_real_load(),
          controller->GetPendingEntryIndex() == -1,
          controller->GetIndexOfEntry(&entry),
          controller->GetLastCommittedEntryIndex(),
          controller->GetEntryCount()),
      true, &frame_entry, &entry));
  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    int current_history_list_offset,
    int current_history_list_length) {
  // TODO(clamy): Check if some PageState should be provided here.
  // TODO(clamy): See how we should handle override of the user agent when the
  // navigation may start in a renderer and commit in another one.
  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  // TODO(clamy): The pending history list offset should be properly set.
  RequestNavigationParams request_params(
      false,                   // is_overriding_user_agent
      std::vector<GURL>(),     // redirects
      false,                   // can_load_local_resources
      PageState(),             // page_state
      0,                       // nav_entry_id
      false,                   // is_same_document_history_load
      false,                   // is_history_navigation_in_new_child
      std::map<std::string, bool>(), // subframe_unique_names
      frame_tree_node->has_committed_real_load(),
      false,                   // intended_as_new_entry
      -1,                      // pending_history_list_offset
      current_history_list_offset, current_history_list_length,
      false,                   // is_view_source
      false,                   // should_clear_history_list
      begin_params.has_user_gesture);
  std::unique_ptr<NavigationRequest> navigation_request(
      new NavigationRequest(frame_tree_node, common_params, begin_params,
                            request_params, false, nullptr, nullptr));
  return navigation_request;
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const RequestNavigationParams& request_params,
    bool browser_initiated,
    const FrameNavigationEntry* frame_entry,
    const NavigationEntryImpl* entry)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      begin_params_(begin_params),
      request_params_(request_params),
      browser_initiated_(browser_initiated),
      state_(NOT_STARTED),
      restore_type_(RestoreType::NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings),
      associated_site_instance_type_(AssociatedSiteInstanceType::NONE) {
  DCHECK(!browser_initiated || (entry != nullptr && frame_entry != nullptr));
  if (browser_initiated) {
    FrameNavigationEntry* frame_entry = entry->GetFrameEntry(frame_tree_node);
    if (frame_entry) {
      source_site_instance_ = frame_entry->source_site_instance();
      dest_site_instance_ = frame_entry->site_instance();
    }

    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  } else {
    // This is needed to have about:blank and data URLs commit in the same
    // SiteInstance as the initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();
  }

  // Update the load flags with cache information.
  UpdateLoadFlagsWithCacheFlags(&begin_params_.load_flags,
                                common_params_.navigation_type,
                                common_params_.method == "POST");

  // Add necessary headers that may not be present in the BeginNavigationParams.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_.headers);
  AddAdditionalRequestHeaders(
      &headers, common_params_.url, common_params_.navigation_type,
      frame_tree_node_->navigator()->GetController()->GetBrowserContext());
  begin_params_.headers = headers.ToString();
}

NavigationRequest::~NavigationRequest() {
}

void NavigationRequest::BeginNavigation() {
  DCHECK(!loader_);
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  state_ = STARTED;
  RenderFrameDevToolsAgentHost::OnBeforeNavigation(navigation_handle_.get());

  if (ShouldMakeNetworkRequestForURL(common_params_.url)) {
    // It's safe to use base::Unretained because this NavigationRequest owns
    // the NavigationHandle where the callback will be stored.
    // TODO(clamy): pass the real value for |is_external_protocol| if needed.
    // TODO(clamy): pass the method to the NavigationHandle instead of a
    // boolean.
    navigation_handle_->WillStartRequest(
        common_params_.method, common_params_.post_data,
        Referrer::SanitizeForRequest(common_params_.url,
                                     common_params_.referrer),
        begin_params_.has_user_gesture, common_params_.transition, false,
        begin_params_.request_context_type,
        base::Bind(&NavigationRequest::OnStartChecksComplete,
                   base::Unretained(this)));
    return;
  }

  // There is no need to make a network request for this navigation, so commit
  // it immediately.
  state_ = RESPONSE_STARTED;

  // Select an appropriate RenderFrameHost.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
  NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(render_frame_host,
                                                           common_params_.url);

  // Inform the NavigationHandle that the navigation will commit.
  navigation_handle_->ReadyToCommitNavigation(render_frame_host);

  CommitNavigation();
}

void NavigationRequest::CreateNavigationHandle(int pending_nav_entry_id) {
  // TODO(nasko): Update the NavigationHandle creation to ensure that the
  // proper values are specified for is_same_page and is_srcdoc.
  navigation_handle_ = NavigationHandleImpl::Create(
      common_params_.url, frame_tree_node_, !browser_initiated_,
      false,  // is_same_page
      false,  // is_srcdoc
      common_params_.navigation_start, pending_nav_entry_id,
      false);  // started_in_context_menu

  if (!begin_params_.searchable_form_url.is_empty()) {
    navigation_handle_->set_searchable_form_url(
        begin_params_.searchable_form_url);
    navigation_handle_->set_searchable_form_encoding(
        begin_params_.searchable_form_encoding);
  }
}

void NavigationRequest::TransferNavigationHandleOwnership(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host->SetNavigationHandle(std::move(navigation_handle_));
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  // If the navigation is no longer a POST, the POST data should be reset.
  if (redirect_info.new_method != "POST")
    common_params_.post_data = nullptr;

  // Mark time for the Navigation Timing API.
  if (request_params_.navigation_timing.redirect_start.is_null()) {
    request_params_.navigation_timing.redirect_start =
        request_params_.navigation_timing.fetch_start;
  }
  request_params_.navigation_timing.redirect_end = base::TimeTicks::Now();
  request_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  request_params_.redirect_response.push_back(response->head);

  request_params_.redirects.push_back(common_params_.url);
  common_params_.url = redirect_info.new_url;
  common_params_.method = redirect_info.new_method;
  common_params_.referrer.url = GURL(redirect_info.new_referrer);

  // For non browser initiated navigations we need to check if the source has
  // access to the URL. We always allow browser initiated requests.
  // TODO(clamy): Kill the renderer if FilterURL fails?
  GURL url = common_params_.url;
  if (!browser_initiated_ && source_site_instance()) {
    source_site_instance()->GetProcess()->FilterURL(false, &url);
    // FilterURL sets the URL to about:blank if the CSP checks prevent the
    // renderer from accessing it.
    if ((url == url::kAboutBlankURL) && (url != common_params_.url)) {
      frame_tree_node_->ResetNavigationRequest(false);
      return;
    }
  }

  // It's safe to use base::Unretained because this NavigationRequest owns the
  // NavigationHandle where the callback will be stored.
  // TODO(clamy): pass the real value for |is_external_protocol| if needed.
  navigation_handle_->WillRedirectRequest(
      common_params_.url, common_params_.method, common_params_.referrer.url,
      false, response->head.headers, response->head.connection_info,
      base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    std::unique_ptr<StreamHandle> body,
    const SSLStatus& ssl_status,
    std::unique_ptr<NavigationData> navigation_data) {
  DCHECK(state_ == STARTED);
  state_ = RESPONSE_STARTED;

  // HTTP 204 (No Content) and HTTP 205 (Reset Content) responses should not
  // commit; they leave the frame showing the previous page.
  DCHECK(response);
  if (response->head.headers.get() &&
      (response->head.headers->response_code() == 204 ||
       response->head.headers->response_code() == 205)) {
    frame_tree_node_->navigator()->DiscardPendingEntryIfNeeded(
        navigation_handle_.get());
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  // Update the service worker params of the request params.
  bool did_create_service_worker_host =
      navigation_handle_->service_worker_handle() &&
      navigation_handle_->service_worker_handle()
              ->service_worker_provider_host_id() !=
          kInvalidServiceWorkerProviderId;
  request_params_.service_worker_provider_id =
      did_create_service_worker_host
          ? navigation_handle_->service_worker_handle()
                ->service_worker_provider_host_id()
          : kInvalidServiceWorkerProviderId;

  // Update the lofi state of the request.
  if (response->head.is_using_lofi)
    common_params_.lofi_state = LOFI_ON;
  else
    common_params_.lofi_state = LOFI_OFF;

  // Select an appropriate renderer to commit the navigation.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
  NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(render_frame_host,
                                                           common_params_.url);

  // For renderer-initiated navigations that are set to commit in a different
  // renderer, allow the embedder to cancel the transfer.
  if (!browser_initiated_ &&
      render_frame_host != frame_tree_node_->current_frame_host() &&
      !frame_tree_node_->navigator()->GetDelegate()->ShouldTransferNavigation(
          frame_tree_node_->IsMainFrame())) {
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  if (navigation_data)
    navigation_handle_->set_navigation_data(std::move(navigation_data));

  // Store the response and the StreamHandle until checks have been processed.
  response_ = response;
  body_ = std::move(body);

  // Check if the navigation should be allowed to proceed.
  // TODO(clamy): pass the right values for request_id, is_download and
  // is_stream.
  navigation_handle_->WillProcessResponse(
      render_frame_host, response->head.headers.get(),
      response->head.connection_info, ssl_status, GlobalRequestID(),
      common_params_.should_replace_current_entry, false, false,
      base::Closure(),
      base::Bind(&NavigationRequest::OnWillProcessResponseChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnRequestFailed(bool has_stale_copy_in_cache,
                                        int net_error) {
  DCHECK(state_ == STARTED);
  state_ = FAILED;
  navigation_handle_->set_net_error_code(static_cast<net::Error>(net_error));
  frame_tree_node_->navigator()->FailedNavigation(
      frame_tree_node_, has_stale_copy_in_cache, net_error);
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  if (frame_tree_node_->IsMainFrame()) {
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "Navigation timeToNetworkStack", navigation_handle_.get(),
        timestamp);
  }

  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  CHECK(result != NavigationThrottle::DEFER);

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result == NavigationThrottle::CANCEL_AND_IGNORE ||
      result == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  if (result == NavigationThrottle::BLOCK_REQUEST) {
    OnRequestFailed(false, net::ERR_BLOCKED_BY_CLIENT);
    return;
  }

  // Use the SiteInstance of the navigating RenderFrameHost to get access to
  // the StoragePartition. Using the url of the navigation will result in a
  // wrong StoragePartition being picked when a WebView is navigating.
  DCHECK_NE(AssociatedSiteInstanceType::NONE, associated_site_instance_type_);
  RenderFrameHostImpl* navigating_frame_host =
      associated_site_instance_type_ == AssociatedSiteInstanceType::SPECULATIVE
          ? frame_tree_node_->render_manager()->speculative_frame_host()
          : frame_tree_node_->current_frame_host();
  DCHECK(navigating_frame_host);

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      browser_context, navigating_frame_host->GetSiteInstance());
  DCHECK(partition);

  // Only initialize the ServiceWorkerNavigationHandle if it can be created for
  // this frame.
  bool can_create_service_worker =
      (frame_tree_node_->pending_sandbox_flags() &
       blink::WebSandboxFlags::Origin) != blink::WebSandboxFlags::Origin;
  request_params_.should_create_service_worker = can_create_service_worker;
  if (can_create_service_worker) {
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());
    navigation_handle_->InitServiceWorkerHandle(service_worker_context);
  }

  // Mark the fetch_start (Navigation Timing API).
  request_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  // TODO(mkwst): This is incorrect. It ought to use the definition from
  // 'Document::firstPartyForCookies()' in Blink, which walks the ancestor tree
  // and verifies that all origins are PSL-matches (and special-cases extension
  // URLs).
  const GURL& first_party_for_cookies =
      frame_tree_node_->IsMainFrame()
          ? common_params_.url
          : frame_tree_node_->frame_tree()->root()->current_url();
  bool parent_is_main_frame = !frame_tree_node_->parent()
                                  ? false
                                  : frame_tree_node_->parent()->IsMainFrame();

  std::unique_ptr<NavigationUIData> navigation_ui_data;
  if (navigation_handle_->navigation_ui_data())
    navigation_ui_data = navigation_handle_->navigation_ui_data()->Clone();

  bool is_for_guests_only =
      navigation_handle_->GetStartingSiteInstance()->GetSiteURL().
          SchemeIs(kGuestScheme);

  bool report_raw_headers =
      RenderFrameDevToolsAgentHost::IsNetworkHandlerEnabled(frame_tree_node_);

  loader_ = NavigationURLLoader::Create(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      base::MakeUnique<NavigationRequestInfo>(
          common_params_, begin_params_, first_party_for_cookies,
          frame_tree_node_->current_origin(), frame_tree_node_->IsMainFrame(),
          parent_is_main_frame, IsSecureFrame(frame_tree_node_->parent()),
          frame_tree_node_->frame_tree_node_id(), is_for_guests_only,
          report_raw_headers),
      std::move(navigation_ui_data),
      navigation_handle_->service_worker_handle(), this);
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  CHECK(result != NavigationThrottle::DEFER);

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result == NavigationThrottle::CANCEL_AND_IGNORE ||
      result == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  loader_->FollowRedirect();
}

void NavigationRequest::OnWillProcessResponseChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  CHECK(result != NavigationThrottle::DEFER);

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result == NavigationThrottle::CANCEL_AND_IGNORE ||
      result == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  // Have the processing of the response resume in the network stack.
  loader_->ProceedWithResponse();

  CommitNavigation();

  // DO NOT ADD CODE after this. The previous call to CommitNavigation caused
  // the destruction of the NavigationRequest.
}

void NavigationRequest::CommitNavigation() {
  DCHECK(response_ || !ShouldMakeNetworkRequestForURL(common_params_.url));
  DCHECK(!common_params_.url.SchemeIs(url::kJavaScriptScheme));

  // Retrieve the RenderFrameHost that needs to commit the navigation.
  RenderFrameHostImpl* render_frame_host =
      navigation_handle_->GetRenderFrameHost();
  DCHECK(render_frame_host ==
             frame_tree_node_->render_manager()->current_frame_host() ||
         render_frame_host ==
             frame_tree_node_->render_manager()->speculative_frame_host());

  TransferNavigationHandleOwnership(render_frame_host);

  DCHECK_EQ(request_params_.has_user_gesture, begin_params_.has_user_gesture);

  render_frame_host->CommitNavigation(response_.get(), std::move(body_),
                                      common_params_, request_params_,
                                      is_view_source_);

  frame_tree_node_->ResetNavigationRequest(true);
}

}  // namespace content
