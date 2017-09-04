// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/browser/loader/resource_dispatcher_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/timer/timer.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/async_resource_handler.h"
#include "content/browser/loader/async_revalidation_manager.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/intercepting_resource_handler.h"
#include "content/browser/loader/loader_delegate.h"
#include "content/browser/loader/mime_sniffing_resource_handler.h"
#include "content/browser/loader/mojo_async_resource_handler.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/navigation_resource_throttle.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/loader/power_save_block_resource_throttle.h"
#include "content/browser/loader/redirect_to_file_resource_handler.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/browser/loader/stream_resource_handler.h"
#include "content/browser/loader/sync_resource_handler.h"
#include "content/browser/loader/throttling_resource_handler.h"
#include "content/browser/loader/upload_data_stream_builder.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/foreign_fetch_request_handler.h"
#include "content/browser/service_worker/link_header_support.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/navigation_params.h"
#include "content/common/net/url_request_service_worker_data.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/resource_request_completion_status.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "net/base/auth.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using storage::ShareableFileReference;
using SyncLoadResultCallback =
    content::ResourceDispatcherHostImpl::SyncLoadResultCallback;

// ----------------------------------------------------------------------------

namespace content {

namespace {

static ResourceDispatcherHostImpl* g_resource_dispatcher_host;

// The interval for calls to ResourceDispatcherHostImpl::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 250;

// Maximum byte "cost" of all the outstanding requests for a renderer.
// See delcaration of |max_outstanding_requests_cost_per_process_| for details.
// This bound is 25MB, which allows for around 6000 outstanding requests.
const int kMaxOutstandingRequestsCostPerProcess = 26214400;

// The number of milliseconds after noting a user gesture that we will
// tag newly-created URLRequest objects with the
// net::LOAD_MAYBE_USER_GESTURE load flag. This is a fairly arbitrary
// guess at how long to expect direct impact from a user gesture, but
// this should be OK as the load flag is a best-effort thing only,
// rather than being intended as fully accurate.
const int kUserGestureWindowMs = 3500;

// Ratio of |max_num_in_flight_requests_| that any one renderer is allowed to
// use. Arbitrarily chosen.
const double kMaxRequestsPerProcessRatio = 0.45;

// TODO(jkarlin): The value is high to reduce the chance of the detachable
// request timing out, forcing a blocked second request to open a new connection
// and start over. Reduce this value once we have a better idea of what it
// should be and once we stop blocking multiple simultaneous requests for the
// same resource (see bugs 46104 and 31014).
const int kDefaultDetachableCancelDelayMs = 30000;

enum SHA1HistogramTypes {
  // SHA-1 is not present in the certificate chain.
  SHA1_NOT_PRESENT = 0,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after January 1, 2017.
  SHA1_EXPIRES_AFTER_JANUARY_2017 = 1,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after June 1, 2016.
  SHA1_EXPIRES_AFTER_JUNE_2016 = 2,
  // SHA-1 is present in the certificate chain, and the leaf expires on or
  // after January 1, 2016.
  SHA1_EXPIRES_AFTER_JANUARY_2016 = 3,
  // SHA-1 is present in the certificate chain, but the leaf expires before
  // January 1, 2016
  SHA1_PRESENT = 4,
  // Always keep this at the end.
  SHA1_HISTOGRAM_TYPES_MAX,
};

void RecordCertificateHistograms(const net::SSLInfo& ssl_info,
                                 ResourceType resource_type) {
  // The internal representation of the dates for UI treatment of SHA-1.
  // See http://crbug.com/401365 for details
  static const int64_t kJanuary2017 = INT64_C(13127702400000000);
  static const int64_t kJune2016 = INT64_C(13109213000000000);
  static const int64_t kJanuary2016 = INT64_C(13096080000000000);

  SHA1HistogramTypes sha1_histogram = SHA1_NOT_PRESENT;
  if (ssl_info.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT) {
    DCHECK(ssl_info.cert.get());
    if (ssl_info.cert->valid_expiry() >=
        base::Time::FromInternalValue(kJanuary2017)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JANUARY_2017;
    } else if (ssl_info.cert->valid_expiry() >=
               base::Time::FromInternalValue(kJune2016)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JUNE_2016;
    } else if (ssl_info.cert->valid_expiry() >=
               base::Time::FromInternalValue(kJanuary2016)) {
      sha1_histogram = SHA1_EXPIRES_AFTER_JANUARY_2016;
    } else {
      sha1_histogram = SHA1_PRESENT;
    }
  }
  if (resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    UMA_HISTOGRAM_ENUMERATION("Net.Certificate.SHA1.MainFrame",
                              sha1_histogram,
                              SHA1_HISTOGRAM_TYPES_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.Certificate.SHA1.Subresource",
                              sha1_histogram,
                              SHA1_HISTOGRAM_TYPES_MAX);
  }
}

bool IsDetachableResourceType(ResourceType type) {
  switch (type) {
    case RESOURCE_TYPE_PREFETCH:
    case RESOURCE_TYPE_PING:
    case RESOURCE_TYPE_CSP_REPORT:
      return true;
    default:
      return false;
  }
}

// Aborts a request before an URLRequest has actually been created.
void AbortRequestBeforeItStarts(
    ResourceMessageFilter* filter,
    const SyncLoadResultCallback& sync_result_handler,
    int request_id,
    mojom::URLLoaderClientAssociatedPtr url_loader_client) {
  if (sync_result_handler) {
    SyncLoadResult result;
    result.error_code = net::ERR_ABORTED;
    sync_result_handler.Run(&result);
  } else {
    // Tell the renderer that this request was disallowed.
    ResourceRequestCompletionStatus request_complete_data;
    request_complete_data.error_code = net::ERR_ABORTED;
    request_complete_data.was_ignored_by_handler = false;
    request_complete_data.exists_in_cache = false;
    // No security info needed, connection not established.
    request_complete_data.completion_time = base::TimeTicks();
    request_complete_data.encoded_data_length = 0;
    if (url_loader_client) {
      url_loader_client->OnComplete(request_complete_data);
    } else {
      filter->Send(
          new ResourceMsg_RequestComplete(request_id, request_complete_data));
    }
  }
}

void SetReferrerForRequest(net::URLRequest* request, const Referrer& referrer) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!referrer.url.is_valid() ||
      command_line->HasSwitch(switches::kNoReferrers)) {
    request->SetReferrer(std::string());
  } else {
    request->SetReferrer(referrer.url.spec());
  }

  net::URLRequest::ReferrerPolicy net_referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  switch (referrer.policy) {
    case blink::WebReferrerPolicyAlways:
      net_referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
      break;
    case blink::WebReferrerPolicyNever:
      net_referrer_policy = net::URLRequest::NO_REFERRER;
      break;
    case blink::WebReferrerPolicyOrigin:
      net_referrer_policy = net::URLRequest::ORIGIN;
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      net_referrer_policy =
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      net_referrer_policy =
          net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      break;
    case blink::WebReferrerPolicyDefault:
      net_referrer_policy =
          command_line->HasSwitch(switches::kReducedReferrerGranularity)
              ? net::URLRequest::
                    REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN
              : net::URLRequest::
                    CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      net_referrer_policy = net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      break;
  }
  request->set_referrer_policy(net_referrer_policy);
}

void RemoveDownloadFileFromChildSecurityPolicy(int child_id,
                                               const base::FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      child_id, path);
}

bool IsValidatedSCT(
    const net::SignedCertificateTimestampAndStatus& sct_status) {
  return sct_status.status == net::ct::SCT_STATUS_OK;
}

storage::BlobStorageContext* GetBlobStorageContext(
    ChromeBlobStorageContext* blob_storage_context) {
  if (!blob_storage_context)
    return NULL;
  return blob_storage_context->context();
}

void AttachRequestBodyBlobDataHandles(
    ResourceRequestBodyImpl* body,
    storage::BlobStorageContext* blob_context) {
  DCHECK(blob_context);
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const ResourceRequestBodyImpl::Element& element = (*body->elements())[i];
    if (element.type() != ResourceRequestBodyImpl::Element::TYPE_BLOB)
      continue;
    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_context->GetBlobDataFromUUID(element.blob_uuid());
    DCHECK(handle);
    if (!handle)
      continue;
    // Ensure the blob and any attached shareable files survive until
    // upload completion. The |body| takes ownership of |handle|.
    const void* key = handle.get();
    body->SetUserData(key, handle.release());
  }
}

// PlzNavigate
// This method is called in the UI thread to send the timestamp of a resource
// request to the respective Navigator (for an UMA histogram).
void LogResourceRequestTimeOnUI(
    base::TimeTicks timestamp,
    int render_process_id,
    int render_frame_id,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (host != nullptr) {
    DCHECK(host->frame_tree_node()->IsMainFrame());
    host->frame_tree_node()->navigator()->LogResourceRequestTime(
        timestamp, url);
  }
}

bool IsUsingLoFi(LoFiState lofi_state,
                 ResourceDispatcherHostDelegate* delegate,
                 const net::URLRequest& request,
                 ResourceContext* resource_context,
                 bool is_main_frame) {
  if (lofi_state == LOFI_UNSPECIFIED && delegate && is_main_frame)
    return delegate->ShouldEnableLoFiMode(request, resource_context);
  return lofi_state == LOFI_ON;
}

// The following functions simplify code paths where the UI thread notifies the
// ResourceDispatcherHostImpl of information pertaining to loading behavior of
// frame hosts.
void NotifyForRouteOnIO(
    base::Callback<void(ResourceDispatcherHostImpl*,
                        const GlobalFrameRoutingId&)> frame_callback,
    const GlobalFrameRoutingId& global_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  if (rdh)
    frame_callback.Run(rdh, global_routing_id);
}

void NotifyForRouteFromUI(
    const GlobalFrameRoutingId& global_routing_id,
    base::Callback<void(ResourceDispatcherHostImpl*,
                        const GlobalFrameRoutingId&)> frame_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyForRouteOnIO, frame_callback, global_routing_id));
}

void NotifyForRouteSetOnIO(
    base::Callback<void(ResourceDispatcherHostImpl*,
                        const GlobalFrameRoutingId&)> frame_callback,
    std::unique_ptr<std::set<GlobalFrameRoutingId>> routing_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& routing_id : *routing_ids)
    NotifyForRouteOnIO(frame_callback, routing_id);
}

void NotifyForEachFrameFromUI(
    RenderFrameHost* root_frame_host,
    base::Callback<void(ResourceDispatcherHostImpl*,
                        const GlobalFrameRoutingId&)> frame_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTree* frame_tree = static_cast<RenderFrameHostImpl*>(root_frame_host)
                              ->frame_tree_node()
                              ->frame_tree();
  DCHECK_EQ(root_frame_host, frame_tree->GetMainFrame());
  std::unique_ptr<std::set<GlobalFrameRoutingId>> routing_ids(
      new std::set<GlobalFrameRoutingId>());
  for (FrameTreeNode* node : frame_tree->Nodes()) {
    RenderFrameHostImpl* frame_host = node->current_frame_host();
    RenderFrameHostImpl* pending_frame_host =
        IsBrowserSideNavigationEnabled()
            ? node->render_manager()->speculative_frame_host()
            : node->render_manager()->pending_frame_host();
    if (frame_host)
      routing_ids->insert(frame_host->GetGlobalFrameRoutingId());
    if (pending_frame_host)
      routing_ids->insert(pending_frame_host->GetGlobalFrameRoutingId());
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&NotifyForRouteSetOnIO, frame_callback,
                                     base::Passed(std::move(routing_ids))));
}

// Sends back the result of a synchronous loading result to the renderer through
// Chrome IPC.
void HandleSyncLoadResult(base::WeakPtr<ResourceMessageFilter> filter,
                          std::unique_ptr<IPC::Message> sync_result,
                          const SyncLoadResult* result) {
  if (!filter)
    return;

  if (result) {
    ResourceHostMsg_SyncLoad::WriteReplyParams(sync_result.get(), *result);
  } else {
    sync_result->set_reply_error();
  }
  filter->Send(sync_result.release());
}

}  // namespace

ResourceDispatcherHostImpl::LoadInfo::LoadInfo() {}
ResourceDispatcherHostImpl::LoadInfo::LoadInfo(const LoadInfo& other) = default;
ResourceDispatcherHostImpl::LoadInfo::~LoadInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::HeaderInterceptorInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::~HeaderInterceptorInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::HeaderInterceptorInfo(
    const HeaderInterceptorInfo& other) {}

// static
ResourceDispatcherHost* ResourceDispatcherHost::Get() {
  return g_resource_dispatcher_host;
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl(
    CreateDownloadHandlerIntercept download_handler_intercept)
    : request_id_(-1),
      is_shutdown_(false),
      num_in_flight_requests_(0),
      max_num_in_flight_requests_(base::SharedMemory::GetHandleLimit()),
      max_num_in_flight_requests_per_process_(static_cast<int>(
          max_num_in_flight_requests_ * kMaxRequestsPerProcessRatio)),
      max_outstanding_requests_cost_per_process_(
          kMaxOutstandingRequestsCostPerProcess),
      filter_(nullptr),
      delegate_(nullptr),
      loader_delegate_(nullptr),
      allow_cross_origin_auth_prompt_(false),
      create_download_handler_intercept_(download_handler_intercept) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_resource_dispatcher_host);
  g_resource_dispatcher_host = this;

  ANNOTATE_BENIGN_RACE(
      &last_user_gesture_time_,
      "We don't care about the precise value, see http://crbug.com/92889");

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnInit,
                                     base::Unretained(this)));

  update_load_states_timer_.reset(new base::RepeatingTimer());

  // stale-while-revalidate currently doesn't work with browser-side navigation.
  // Only enable stale-while-revalidate if browser navigation is not enabled.
  //
  // TODO(ricea): Make stale-while-revalidate and browser-side navigation work
  // together. Or disable stale-while-revalidate completely before browser-side
  // navigation becomes the default. crbug.com/561610
  if (!IsBrowserSideNavigationEnabled() &&
      base::FeatureList::IsEnabled(features::kStaleWhileRevalidate)) {
    async_revalidation_manager_.reset(new AsyncRevalidationManager);
  }
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl()
    : ResourceDispatcherHostImpl(CreateDownloadHandlerIntercept()) {}

ResourceDispatcherHostImpl::~ResourceDispatcherHostImpl() {
  DCHECK(outstanding_requests_stats_map_.empty());
  DCHECK(g_resource_dispatcher_host);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_resource_dispatcher_host = NULL;
}

// static
ResourceDispatcherHostImpl* ResourceDispatcherHostImpl::Get() {
  return g_resource_dispatcher_host;
}

// static
void ResourceDispatcherHostImpl::ResumeBlockedRequestsForRouteFromUI(
    const GlobalFrameRoutingId& global_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyForRouteFromUI(
      global_routing_id,
      base::Bind(&ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute));
}

// static
void ResourceDispatcherHostImpl::BlockRequestsForFrameFromUI(
    RenderFrameHost* root_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyForEachFrameFromUI(
      root_frame_host,
      base::Bind(&ResourceDispatcherHostImpl::BlockRequestsForRoute));
}

// static
void ResourceDispatcherHostImpl::ResumeBlockedRequestsForFrameFromUI(
    RenderFrameHost* root_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyForEachFrameFromUI(
      root_frame_host,
      base::Bind(&ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute));
}

// static
void ResourceDispatcherHostImpl::CancelBlockedRequestsForFrameFromUI(
    RenderFrameHostImpl* root_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyForEachFrameFromUI(
      root_frame_host,
      base::Bind(&ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute));
}

void ResourceDispatcherHostImpl::SetDelegate(
    ResourceDispatcherHostDelegate* delegate) {
  delegate_ = delegate;
}

void ResourceDispatcherHostImpl::SetAllowCrossOriginAuthPrompt(bool value) {
  allow_cross_origin_auth_prompt_ = value;
}

void ResourceDispatcherHostImpl::CancelRequestsForContext(
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context);

  // Note that request cancellation has side effects. Therefore, we gather all
  // the requests to cancel first, and then we start cancelling. We assert at
  // the end that there are no more to cancel since the context is about to go
  // away.
  typedef std::vector<std::unique_ptr<ResourceLoader>> LoaderList;
  LoaderList loaders_to_cancel;

  for (LoaderMap::iterator i = pending_loaders_.begin();
       i != pending_loaders_.end();) {
    ResourceLoader* loader = i->second.get();
    if (loader->GetRequestInfo()->GetContext() == context) {
      loaders_to_cancel.push_back(std::move(i->second));
      IncrementOutstandingRequestsMemory(-1, *loader->GetRequestInfo());
      pending_loaders_.erase(i++);
    } else {
      ++i;
    }
  }

  for (BlockedLoadersMap::iterator i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end();) {
    BlockedLoadersList* loaders = i->second.get();
    if (loaders->empty()) {
      // This can happen if BlockRequestsForRoute() has been called for a route,
      // but we haven't blocked any matching requests yet.
      ++i;
      continue;
    }
    ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
    if (info->GetContext() == context) {
      std::unique_ptr<BlockedLoadersList> deleter(std::move(i->second));
      blocked_loaders_map_.erase(i++);
      for (auto& loader : *loaders) {
        info = loader->GetRequestInfo();
        // We make the assumption that all requests on the list have the same
        // ResourceContext.
        DCHECK_EQ(context, info->GetContext());
        IncrementOutstandingRequestsMemory(-1, *info);
        loaders_to_cancel.push_back(std::move(loader));
      }
    } else {
      ++i;
    }
  }

#ifndef NDEBUG
  for (const auto& loader : loaders_to_cancel) {
    // There is no strict requirement that this be the case, but currently
    // downloads, streams, detachable requests, transferred requests, and
    // browser-owned requests are the only requests that aren't cancelled when
    // the associated processes go away. It may be OK for this invariant to
    // change in the future, but if this assertion fires without the invariant
    // changing, then it's indicative of a leak.
    DCHECK(loader->GetRequestInfo()->IsDownload() ||
           loader->GetRequestInfo()->is_stream() ||
           (loader->GetRequestInfo()->detachable_handler() &&
            loader->GetRequestInfo()->detachable_handler()->is_detached()) ||
           loader->GetRequestInfo()->GetProcessType() == PROCESS_TYPE_BROWSER ||
           loader->is_transferring());
  }
#endif

  loaders_to_cancel.clear();

  if (async_revalidation_manager_) {
    // Cancelling async revalidations should not result in the creation of new
    // requests. Do it before the CHECKs to ensure this does not happen.
    async_revalidation_manager_->CancelAsyncRevalidationsForResourceContext(
        context);
  }
}

void ResourceDispatcherHostImpl::ClearLoginDelegateForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  if (info) {
    ResourceLoader* loader = GetLoader(info->GetGlobalRequestID());
    if (loader)
      loader->ClearLoginDelegate();
  }
}

void ResourceDispatcherHostImpl::RegisterInterceptor(
    const std::string& http_header,
    const std::string& starts_with,
    const InterceptorCallback& interceptor) {
  DCHECK(!http_header.empty());
  DCHECK(interceptor);
  // Only one interceptor per header is supported.
  DCHECK(http_header_interceptor_map_.find(http_header) ==
         http_header_interceptor_map_.end());

  HeaderInterceptorInfo interceptor_info;
  interceptor_info.starts_with = starts_with;
  interceptor_info.interceptor = interceptor;

  http_header_interceptor_map_[http_header] = interceptor_info;
}

void ResourceDispatcherHostImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&ResourceDispatcherHostImpl::OnShutdown,
                                     base::Unretained(this)));
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandlerForDownload(
    net::URLRequest* request,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request) {
  DCHECK(!create_download_handler_intercept_.is_null());
  // TODO(ananta)
  // Find a better way to create the download handler and notifying the
  // delegate of the download start.
  std::unique_ptr<ResourceHandler> handler =
      create_download_handler_intercept_.Run(request);
  handler =
      HandleDownloadStarted(request, std::move(handler), is_content_initiated,
                            must_download, is_new_request);
  return handler;
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::MaybeInterceptAsStream(
    const base::FilePath& plugin_path,
    net::URLRequest* request,
    ResourceResponse* response,
    std::string* payload) {
  payload->clear();
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  const std::string& mime_type = response->head.mime_type;

  GURL origin;
  if (!delegate_ ||
      !delegate_->ShouldInterceptResourceAsStream(
          request, plugin_path, mime_type, &origin, payload)) {
    return std::unique_ptr<ResourceHandler>();
  }

  StreamContext* stream_context =
      GetStreamContextForResourceContext(info->GetContext());

  std::unique_ptr<StreamResourceHandler> handler(
      new StreamResourceHandler(request, stream_context->registry(), origin));

  info->set_is_stream(true);
  std::unique_ptr<StreamInfo> stream_info(new StreamInfo);
  stream_info->handle = handler->stream()->CreateHandle();
  stream_info->original_url = request->url();
  stream_info->mime_type = mime_type;
  // Make a copy of the response headers so it is safe to pass across threads;
  // the old handler (AsyncResourceHandler) may modify it in parallel via the
  // ResourceDispatcherHostDelegate.
  if (response->head.headers.get()) {
    stream_info->response_headers =
        new net::HttpResponseHeaders(response->head.headers->raw_headers());
  }
  delegate_->OnStreamCreated(request, std::move(stream_info));
  return std::move(handler);
}

ResourceDispatcherHostLoginDelegate*
ResourceDispatcherHostImpl::CreateLoginDelegate(
    ResourceLoader* loader,
    net::AuthChallengeInfo* auth_info) {
  if (!delegate_)
    return NULL;

  return delegate_->CreateLoginDelegate(auth_info, loader->request());
}

bool ResourceDispatcherHostImpl::HandleExternalProtocol(ResourceLoader* loader,
                                                        const GURL& url) {
  if (!delegate_)
    return false;

  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  if (!IsResourceTypeFrame(info->GetResourceType()))
    return false;

  const net::URLRequestJobFactory* job_factory =
      info->GetContext()->GetRequestContext()->job_factory();
  if (job_factory->IsHandledURL(url))
    return false;

  return delegate_->HandleExternalProtocol(url, info);
}

void ResourceDispatcherHostImpl::DidStartRequest(ResourceLoader* loader) {
  // Make sure we have the load state monitor running.
  if (!update_load_states_timer_->IsRunning() &&
      scheduler_->HasLoadingClients()) {
    update_load_states_timer_->Start(
        FROM_HERE, TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHostImpl::UpdateLoadInfo);
  }
}

void ResourceDispatcherHostImpl::DidReceiveRedirect(
    ResourceLoader* loader,
    const GURL& new_url,
    ResourceResponse* response) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  if (delegate_) {
    delegate_->OnRequestRedirected(
        new_url, loader->request(), info->GetContext(), response);
  }

  net::URLRequest* request = loader->request();
  if (request->response_info().async_revalidation_required) {
    // Async revalidation is only supported for the first redirect leg.
    DCHECK_EQ(request->url_chain().size(), 1u);
    DCHECK(async_revalidation_manager_);

    async_revalidation_manager_->BeginAsyncRevalidation(*request,
                                                        scheduler_.get());
  }

  // Remove the LOAD_SUPPORT_ASYNC_REVALIDATION flag if it is present.
  // It is difficult to create a URLRequest with the correct flags and headers
  // for redirect legs other than the first one. Since stale-while-revalidate in
  // combination with redirects isn't needed for experimental use, punt on it
  // for now.
  // TODO(ricea): Fix this before launching the feature.
  if (request->load_flags() & net::LOAD_SUPPORT_ASYNC_REVALIDATION) {
    int new_load_flags =
        request->load_flags() & ~net::LOAD_SUPPORT_ASYNC_REVALIDATION;
    request->SetLoadFlags(new_load_flags);
  }

  // Don't notify WebContents observers for requests known to be
  // downloads; they aren't really associated with the Webcontents.
  // Note that not all downloads are known before content sniffing.
  if (info->IsDownload())
    return;

  // Notify the observers on the UI thread.
  std::unique_ptr<ResourceRedirectDetails> detail(new ResourceRedirectDetails(
      loader->request(),
      !!request->ssl_info().cert,
      new_url));
  loader_delegate_->DidGetRedirectForResourceRequest(
      info->GetWebContentsGetterForRequest(), std::move(detail));
}

void ResourceDispatcherHostImpl::DidReceiveResponse(ResourceLoader* loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  net::URLRequest* request = loader->request();
  if (request->was_fetched_via_proxy() &&
      request->was_fetched_via_spdy() &&
      request->url().SchemeIs(url::kHttpScheme)) {
    scheduler_->OnReceivedSpdyProxiedHttpResponse(
        info->GetChildID(), info->GetRouteID());
  }

  if (request->response_info().async_revalidation_required) {
    DCHECK(async_revalidation_manager_);
    async_revalidation_manager_->BeginAsyncRevalidation(*request,
                                                        scheduler_.get());
  }

  ProcessRequestForLinkHeaders(request);

  int render_process_id, render_frame_host;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_host))
    return;

  // Don't notify WebContents observers for requests known to be
  // downloads; they aren't really associated with the Webcontents.
  // Note that not all downloads are known before content sniffing.
  if (info->IsDownload())
    return;

  // Notify the observers on the UI thread.
  std::unique_ptr<ResourceRequestDetails> detail(new ResourceRequestDetails(
      request, !!request->ssl_info().cert));
  loader_delegate_->DidGetResourceResponseStart(
      info->GetWebContentsGetterForRequest(), std::move(detail));
}

void ResourceDispatcherHostImpl::DidFinishLoading(ResourceLoader* loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  // Record final result of all resource loads.
  if (info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME) {
    // This enumeration has "3" appended to its name to distinguish it from
    // older versions.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.ErrorCodesForMainFrame3",
        -loader->request()->status().error());

    // Record time to success and error for the most common errors, and for
    // the aggregate remainder errors.
    base::TimeDelta request_loading_time(
        base::TimeTicks::Now() - loader->request()->creation_time());
    switch (loader->request()->status().error()) {
      case net::OK:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.Success", request_loading_time);
        break;
      case net::ERR_ABORTED:
        UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ErrAborted.SentBytes",
                                    loader->request()->GetTotalSentBytes(), 1,
                                    50000000, 50);
        UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ErrAborted.ReceivedBytes",
                                    loader->request()->GetTotalReceivedBytes(),
                                    1, 50000000, 50);
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrAborted", request_loading_time);

        if (loader->request()->url().SchemeIsHTTPOrHTTPS()) {
          UMA_HISTOGRAM_LONG_TIMES("Net.RequestTime2.ErrAborted.HttpScheme",
                                   request_loading_time);
        } else {
          UMA_HISTOGRAM_LONG_TIMES("Net.RequestTime2.ErrAborted.NonHttpScheme",
                                   request_loading_time);
        }

        if (loader->request()->GetTotalReceivedBytes() > 0) {
          UMA_HISTOGRAM_LONG_TIMES("Net.RequestTime2.ErrAborted.NetworkContent",
                                   request_loading_time);
        } else if (loader->request()->received_response_content_length() > 0) {
          UMA_HISTOGRAM_LONG_TIMES(
              "Net.RequestTime2.ErrAborted.NoNetworkContent.CachedContent",
              request_loading_time);
        } else {
          UMA_HISTOGRAM_LONG_TIMES(
              "Net.RequestTime2.ErrAborted.NoBytesRead",
              request_loading_time);
        }

        if (delegate_) {
          delegate_->OnAbortedFrameLoad(loader->request()->url(),
                                        request_loading_time);
        }
        break;
      case net::ERR_CONNECTION_RESET:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrConnectionReset", request_loading_time);
        break;
      case net::ERR_CONNECTION_TIMED_OUT:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrConnectionTimedOut", request_loading_time);
        break;
      case net::ERR_INTERNET_DISCONNECTED:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrInternetDisconnected", request_loading_time);
        break;
      case net::ERR_NAME_NOT_RESOLVED:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrNameNotResolved", request_loading_time);
        break;
      case net::ERR_TIMED_OUT:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.ErrTimedOut", request_loading_time);
        break;
      default:
        UMA_HISTOGRAM_LONG_TIMES(
            "Net.RequestTime2.MiscError", request_loading_time);
        break;
    }

    if (loader->request()->url().SchemeIsCryptographic()) {
      if (loader->request()->url().host_piece() == "www.google.com") {
        UMA_HISTOGRAM_SPARSE_SLOWLY("Net.ErrorCodesForHTTPSGoogleMainFrame2",
                                    -loader->request()->status().error());
      }

      int num_valid_scts = std::count_if(
          loader->request()->ssl_info().signed_certificate_timestamps.begin(),
          loader->request()->ssl_info().signed_certificate_timestamps.end(),
          IsValidatedSCT);
      UMA_HISTOGRAM_COUNTS_100(
          "Net.CertificateTransparency.MainFrameValidSCTCount", num_valid_scts);
    }
  } else {
    if (info->GetResourceType() == RESOURCE_TYPE_IMAGE) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.ErrorCodesForImages",
          -loader->request()->status().error());
    }
    // This enumeration has "2" appended to distinguish it from older versions.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.ErrorCodesForSubresources2",
        -loader->request()->status().error());
  }

  if (loader->request()->url().SchemeIsCryptographic()) {
    RecordCertificateHistograms(loader->request()->ssl_info(),
                                info->GetResourceType());
  }

  if (delegate_)
    delegate_->RequestComplete(loader->request());

  // Destroy the ResourceLoader.
  RemovePendingRequest(info->GetChildID(), info->GetRequestID());
}

std::unique_ptr<net::ClientCertStore>
    ResourceDispatcherHostImpl::CreateClientCertStore(ResourceLoader* loader) {
  return delegate_->CreateClientCertStore(
      loader->GetRequestInfo()->GetContext());
}

void ResourceDispatcherHostImpl::OnInit() {
  scheduler_.reset(new ResourceScheduler);
}

void ResourceDispatcherHostImpl::OnShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  is_shutdown_ = true;
  pending_loaders_.clear();

  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_states_timer_.reset();

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_loaders_map_, as it modifies it.
  std::set<GlobalFrameRoutingId> ids;
  for (const auto& blocked_loaders : blocked_loaders_map_) {
    std::pair<std::set<GlobalFrameRoutingId>::iterator, bool> result =
        ids.insert(blocked_loaders.first);
    // We should not have duplicates.
    DCHECK(result.second);
  }
  for (const auto& routing_id : ids) {
    CancelBlockedRequestsForRoute(routing_id);
  }

  scheduler_.reset();
}

bool ResourceDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& message,
    ResourceMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  filter_ = filter;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcherHostImpl, message)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ResourceHostMsg_SyncLoad, OnSyncLoad)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_ReleaseDownloadedFile,
                        OnReleaseDownloadedFile)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DidChangePriority, OnDidChangePriority)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && IPC_MESSAGE_ID_CLASS(message.type()) == ResourceMsgStart) {
    base::PickleIterator iter(message);
    int request_id = -1;
    bool ok = iter.ReadInt(&request_id);
    DCHECK(ok);
    GlobalRequestID id(filter_->child_id(), request_id);
    DelegateMap::iterator it = delegate_map_.find(id);
    if (it != delegate_map_.end()) {
      for (auto& delegate : *it->second) {
        if (delegate.OnMessageReceived(message)) {
          handled = true;
          break;
        }
      }
    }

    // As the unhandled resource message effectively has no consumer, mark it as
    // handled to prevent needless propagation through the filter pipeline.
    handled = true;
  }

  filter_ = NULL;
  return handled;
}

void ResourceDispatcherHostImpl::OnRequestResource(
    int routing_id,
    int request_id,
    const ResourceRequest& request_data) {
  OnRequestResourceInternal(routing_id, request_id, request_data, nullptr,
                            nullptr);
}

void ResourceDispatcherHostImpl::OnRequestResourceInternal(
    int routing_id,
    int request_id,
    const ResourceRequest& request_data,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ResourceDispatcherHostImpl::OnRequestResource"));
  // When logging time-to-network only care about main frame and non-transfer
  // navigations.
  // PlzNavigate: this log happens from NavigationRequest::OnRequestStarted
  // instead.
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME &&
      request_data.transferred_request_request_id == -1 &&
      !IsBrowserSideNavigationEnabled()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LogResourceRequestTimeOnUI,
                   TimeTicks::Now(),
                   filter_->child_id(),
                   request_data.render_frame_id,
                   request_data.url));
  }
  BeginRequest(request_id, request_data, SyncLoadResultCallback(), routing_id,
               std::move(mojo_request), std::move(url_loader_client));
}

// Begins a resource request with the given params on behalf of the specified
// child process.  Responses will be dispatched through the given receiver. The
// process ID is used to lookup WebContentsImpl from routing_id's in the case of
// a request from a renderer.  request_context is the cookie/cache context to be
// used for this request.
//
// If sync_result is non-null, then a SyncLoad reply will be generated, else
// a normal asynchronous set of response messages will be generated.
void ResourceDispatcherHostImpl::OnSyncLoad(int request_id,
                                            const ResourceRequest& request_data,
                                            IPC::Message* sync_result) {
  SyncLoadResultCallback callback = base::Bind(
      &HandleSyncLoadResult, filter_->GetWeakPtr(),
      base::Passed(WrapUnique(sync_result)));
  BeginRequest(request_id, request_data, callback, sync_result->routing_id(),
               nullptr, nullptr);
}

bool ResourceDispatcherHostImpl::IsRequestIDInUse(
    const GlobalRequestID& id) const {
  if (pending_loaders_.find(id) != pending_loaders_.end())
    return true;
  for (const auto& blocked_loaders : blocked_loaders_map_) {
    for (const auto& loader : *blocked_loaders.second.get()) {
      ResourceRequestInfoImpl* info = loader->GetRequestInfo();
      if (info->GetGlobalRequestID() == id)
        return true;
    }
  }
  return false;
}

void ResourceDispatcherHostImpl::UpdateRequestForTransfer(
    int child_id,
    int route_id,
    int request_id,
    const ResourceRequest& request_data,
    LoaderMap::iterator iter) {
  ResourceRequestInfoImpl* info = iter->second->GetRequestInfo();
  GlobalFrameRoutingId old_routing_id(request_data.transferred_request_child_id,
                                      info->GetRenderFrameID());
  GlobalRequestID old_request_id(request_data.transferred_request_child_id,
                                 request_data.transferred_request_request_id);
  GlobalFrameRoutingId new_routing_id(child_id, request_data.render_frame_id);
  GlobalRequestID new_request_id(child_id, request_id);

  // Clear out data that depends on |info| before updating it.
  // We always need to move the memory stats to the new process.  In contrast,
  // stats.num_requests is only tracked for some requests (those that require
  // file descriptors for their shared memory buffer).
  IncrementOutstandingRequestsMemory(-1, *info);
  bool should_update_count = info->counted_as_in_flight_request();
  if (should_update_count)
    IncrementOutstandingRequestsCount(-1, info);

  DCHECK(pending_loaders_.find(old_request_id) == iter);
  std::unique_ptr<ResourceLoader> loader = std::move(iter->second);
  ResourceLoader* loader_ptr = loader.get();
  pending_loaders_.erase(iter);

  // ResourceHandlers should always get state related to the request from the
  // ResourceRequestInfo rather than caching it locally.  This lets us update
  // the info object when a transfer occurs.
  info->UpdateForTransfer(child_id, route_id, request_data.render_frame_id,
                          request_data.origin_pid, request_id,
                          filter_->GetWeakPtr());

  // Update maps that used the old IDs, if necessary.  Some transfers in tests
  // do not actually use a different ID, so not all maps need to be updated.
  pending_loaders_[new_request_id] = std::move(loader);
  IncrementOutstandingRequestsMemory(1, *info);
  if (should_update_count)
    IncrementOutstandingRequestsCount(1, info);
  if (old_routing_id != new_routing_id) {
    if (blocked_loaders_map_.find(old_routing_id) !=
            blocked_loaders_map_.end()) {
      blocked_loaders_map_[new_routing_id] =
          std::move(blocked_loaders_map_[old_routing_id]);
      blocked_loaders_map_.erase(old_routing_id);
    }
  }
  if (old_request_id != new_request_id) {
    DelegateMap::iterator it = delegate_map_.find(old_request_id);
    if (it != delegate_map_.end()) {
      // Tell each delegate that the request ID has changed.
      for (auto& delegate : *it->second)
        delegate.set_request_id(new_request_id);
      // Now store the observer list under the new request ID.
      delegate_map_[new_request_id] = delegate_map_[old_request_id];
      delegate_map_.erase(old_request_id);
    }
  }

  AppCacheInterceptor::CompleteCrossSiteTransfer(
      loader_ptr->request(),
      child_id,
      request_data.appcache_host_id,
      filter_);

  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(loader_ptr->request());
  if (handler) {
    if (!handler->SanityCheckIsSameContext(filter_->service_worker_context())) {
      bad_message::ReceivedBadMessage(
          filter_, bad_message::RDHI_WRONG_STORAGE_PARTITION);
    } else {
      handler->CompleteCrossSiteTransfer(
          child_id, request_data.service_worker_provider_id);
    }
  }
}

void ResourceDispatcherHostImpl::CompleteTransfer(
    int request_id,
    const ResourceRequest& request_data,
    int route_id) {
  // Caller should ensure that |request_data| is associated with a transfer.
  DCHECK(request_data.transferred_request_child_id != -1 ||
         request_data.transferred_request_request_id != -1);

  bool is_navigational_request =
      request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME ||
      request_data.resource_type == RESOURCE_TYPE_SUB_FRAME;
  if (!is_navigational_request) {
    // Transfers apply only to navigational requests - the renderer seems to
    // have sent bogus IPC data.
    bad_message::ReceivedBadMessage(
        filter_, bad_message::RDH_TRANSFERRING_NONNAVIGATIONAL_REQUEST);
    return;
  }

  // Attempt to find a loader associated with the deferred transfer request.
  LoaderMap::iterator it = pending_loaders_.find(
      GlobalRequestID(request_data.transferred_request_child_id,
                      request_data.transferred_request_request_id));
  if (it == pending_loaders_.end()) {
    // Renderer sent transferred_request_request_id and/or
    // transferred_request_child_id that doesn't have a corresponding entry on
    // the browser side.
    // TODO(lukasza): https://crbug.com/659613: Need to understand the scenario
    // that can lead here (and then attempt to reintroduce a renderer kill
    // below).
    return;
  }
  ResourceLoader* pending_loader = it->second.get();

  if (!pending_loader->is_transferring()) {
    // Renderer sent transferred_request_request_id and/or
    // transferred_request_child_id that doesn't correspond to an actually
    // transferring loader on the browser side.
    base::debug::Alias(pending_loader);
    bad_message::ReceivedBadMessage(filter_,
                                    bad_message::RDH_REQUEST_NOT_TRANSFERRING);
    return;
  }

  // If the request is transferring to a new process, we can update our
  // state and let it resume with its existing ResourceHandlers.
  UpdateRequestForTransfer(filter_->child_id(), route_id, request_id,
                           request_data, it);
  pending_loader->CompleteTransfer();
}

void ResourceDispatcherHostImpl::BeginRequest(
    int request_id,
    const ResourceRequest& request_data,
    const SyncLoadResultCallback& sync_result_handler,  // only valid for sync
    int route_id,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client) {
  int process_type = filter_->process_type();
  int child_id = filter_->child_id();

  // Reject request id that's currently in use.
  if (IsRequestIDInUse(GlobalRequestID(child_id, request_id))) {
    bad_message::ReceivedBadMessage(filter_,
                                    bad_message::RDH_INVALID_REQUEST_ID);
    return;
  }

  // PlzNavigate: reject invalid renderer main resource request.
  bool is_navigation_stream_request =
      IsBrowserSideNavigationEnabled() &&
      IsResourceTypeFrame(request_data.resource_type);
  // The process_type check is to ensure that unittests are not blocked from
  // issuing http requests.
  if ((process_type == PROCESS_TYPE_RENDERER) && is_navigation_stream_request
       && !request_data.resource_body_stream_url.SchemeIs(url::kBlobScheme)) {
    bad_message::ReceivedBadMessage(filter_, bad_message::RDH_INVALID_URL);
    return;
  }

  // Reject invalid priority.
  if (request_data.priority < net::MINIMUM_PRIORITY ||
      request_data.priority > net::MAXIMUM_PRIORITY) {
    bad_message::ReceivedBadMessage(filter_, bad_message::RDH_INVALID_PRIORITY);
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/91398
  char url_buf[128];
  base::strlcpy(url_buf, request_data.url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  // If the request that's coming in is being transferred from another process,
  // we want to reuse and resume the old loader rather than start a new one.
  if (request_data.transferred_request_child_id != -1 ||
      request_data.transferred_request_request_id != -1) {
    // TODO(yhirano): Make mojo work for this case.
    DCHECK(!url_loader_client);

    CompleteTransfer(request_id, request_data, route_id);
    return;
  }

  ResourceContext* resource_context = NULL;
  net::URLRequestContext* request_context = NULL;
  filter_->GetContexts(request_data.resource_type, &resource_context,
                       &request_context);

  // Parse the headers before calling ShouldServiceRequest, so that they are
  // available to be validated.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(request_data.headers);

  if (is_shutdown_ ||
      !ShouldServiceRequest(process_type, child_id, request_data, headers,
                            filter_, resource_context)) {
    AbortRequestBeforeItStarts(filter_, sync_result_handler, request_id,
                               std::move(url_loader_client));
    return;
  }
  // Check if we have a registered interceptor for the headers passed in. If
  // yes then we need to mark the current request as pending and wait for the
  // interceptor to invoke the callback with a status code indicating whether
  // the request needs to be aborted or continued.
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    HeaderInterceptorMap::iterator index =
        http_header_interceptor_map_.find(it.name());
    if (index != http_header_interceptor_map_.end()) {
      HeaderInterceptorInfo& interceptor_info = index->second;

      bool call_interceptor = true;
      if (!interceptor_info.starts_with.empty()) {
        call_interceptor =
            base::StartsWith(it.value(), interceptor_info.starts_with,
                             base::CompareCase::INSENSITIVE_ASCII);
      }
      if (call_interceptor) {
        interceptor_info.interceptor.Run(
            it.name(), it.value(), child_id, resource_context,
            base::Bind(&ResourceDispatcherHostImpl::ContinuePendingBeginRequest,
                       base::Unretained(this), request_id, request_data,
                       sync_result_handler, route_id, headers,
                       base::Passed(std::move(mojo_request)),
                       base::Passed(std::move(url_loader_client))));
        return;
      }
    }
  }
  ContinuePendingBeginRequest(request_id, request_data, sync_result_handler,
                              route_id, headers, std::move(mojo_request),
                              std::move(url_loader_client), true, 0);
}

void ResourceDispatcherHostImpl::ContinuePendingBeginRequest(
    int request_id,
    const ResourceRequest& request_data,
    const SyncLoadResultCallback& sync_result_handler,  // only valid for sync
    int route_id,
    const net::HttpRequestHeaders& headers,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client,
    bool continue_request,
    int error_code) {
  if (!continue_request) {
    // TODO(ananta): Find a way to specify the right error code here.  Passing
    // in a non-content error code is not safe.
    bad_message::ReceivedBadMessage(filter_, bad_message::RDH_ILLEGAL_ORIGIN);
    AbortRequestBeforeItStarts(filter_, sync_result_handler, request_id,
                               std::move(url_loader_client));
    return;
  }

  int process_type = filter_->process_type();
  int child_id = filter_->child_id();

  bool is_navigation_stream_request =
      IsBrowserSideNavigationEnabled() &&
      IsResourceTypeFrame(request_data.resource_type) &&
      process_type == PROCESS_TYPE_RENDERER;

  ResourceContext* resource_context = NULL;
  net::URLRequestContext* request_context = NULL;
  filter_->GetContexts(request_data.resource_type, &resource_context,
                       &request_context);

  // Allow the observer to block/handle the request.
  if (delegate_ && !delegate_->ShouldBeginRequest(request_data.method,
                                                  request_data.url,
                                                  request_data.resource_type,
                                                  resource_context)) {
    AbortRequestBeforeItStarts(filter_, sync_result_handler, request_id,
                               std::move(url_loader_client));
    return;
  }

  // Construct the request.
  std::unique_ptr<net::URLRequest> new_request = request_context->CreateRequest(
      is_navigation_stream_request ? request_data.resource_body_stream_url
                                   : request_data.url,
      request_data.priority, nullptr);

  // PlzNavigate: Always set the method to GET when gaining access to the
  // stream that contains the response body of a navigation. Otherwise the data
  // that was already fetched by the browser will not be transmitted to the
  // renderer.
  if (is_navigation_stream_request)
    new_request->set_method("GET");
  else
    new_request->set_method(request_data.method);

  new_request->set_first_party_for_cookies(
      request_data.first_party_for_cookies);

  // The initiator should normally be present, unless this is a navigation in a
  // top-level frame. It may be null for some top-level navigations (eg:
  // browser-initiated ones).
  DCHECK(request_data.request_initiator.has_value() ||
         request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME);
  new_request->set_initiator(request_data.request_initiator);

  if (request_data.originated_from_service_worker) {
    new_request->SetUserData(URLRequestServiceWorkerData::kUserDataKey,
                             new URLRequestServiceWorkerData());
  }

  // If the request is a MAIN_FRAME request, the first-party URL gets updated on
  // redirects.
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    new_request->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  // For PlzNavigate, this request has already been made and the referrer was
  // checked previously. So don't set the referrer for this stream request, or
  // else it will fail for SSL redirects since net/ will think the blob:https
  // for the stream is not a secure scheme (specifically, in the call to
  // ComputeReferrerForRedirect).
  if (!is_navigation_stream_request) {
    const Referrer referrer(
        request_data.referrer, request_data.referrer_policy);
    SetReferrerForRequest(new_request.get(), referrer);
  }

  new_request->SetExtraRequestHeaders(headers);

  storage::BlobStorageContext* blob_context =
      GetBlobStorageContext(filter_->blob_storage_context());
  // Resolve elements from request_body and prepare upload data.
  if (request_data.request_body.get()) {
    // |blob_context| could be null when the request is from the plugins because
    // ResourceMessageFilters created in PluginProcessHost don't have the blob
    // context.
    if (blob_context) {
      // Attaches the BlobDataHandles to request_body not to free the blobs and
      // any attached shareable files until upload completion. These data will
      // be used in UploadDataStream and ServiceWorkerURLRequestJob.
      AttachRequestBodyBlobDataHandles(
          request_data.request_body.get(),
          blob_context);
    }
    new_request->set_upload(UploadDataStreamBuilder::Build(
        request_data.request_body.get(), blob_context,
        filter_->file_system_context(),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get()));
  }

  bool allow_download = request_data.allow_download &&
      IsResourceTypeFrame(request_data.resource_type);
  bool do_not_prompt_for_login = request_data.do_not_prompt_for_login;
  bool is_sync_load = !!sync_result_handler;

  // Raw headers are sensitive, as they include Cookie/Set-Cookie, so only
  // allow requesting them if requester has ReadRawCookies permission.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  bool report_raw_headers = request_data.report_raw_headers;
  if (report_raw_headers && !policy->CanReadRawCookies(child_id)) {
    // TODO: crbug.com/523063 can we call bad_message::ReceivedBadMessage here?
    VLOG(1) << "Denied unauthorized request for raw headers";
    report_raw_headers = false;
  }
  int load_flags =
      BuildLoadFlagsForRequest(request_data, child_id, is_sync_load);
  if (request_data.resource_type == RESOURCE_TYPE_PREFETCH ||
      request_data.resource_type == RESOURCE_TYPE_FAVICON) {
    do_not_prompt_for_login = true;
  }
  if (request_data.resource_type == RESOURCE_TYPE_IMAGE &&
      HTTP_AUTH_RELATION_BLOCKED_CROSS ==
          HttpAuthRelationTypeOf(request_data.url,
                                 request_data.first_party_for_cookies)) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the user.
    // Only block image loads, as the attack applies largely to the "src"
    // property of the <img> tag. It is common for web properties to allow
    // untrusted values for <img src>; this is considered a fair thing for an
    // HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    do_not_prompt_for_login = true;
    load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  bool support_async_revalidation =
      !is_sync_load && async_revalidation_manager_ &&
      AsyncRevalidationManager::QualifiesForAsyncRevalidation(request_data);

  if (support_async_revalidation)
    load_flags |= net::LOAD_SUPPORT_ASYNC_REVALIDATION;

  // Sync loads should have maximum priority and should be the only
  // requets that have the ignore limits flag set.
  if (is_sync_load) {
    DCHECK_EQ(request_data.priority, net::MAXIMUM_PRIORITY);
    DCHECK_NE(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  } else {
    DCHECK_EQ(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  }
  new_request->SetLoadFlags(load_flags);

  // Make extra info and read footer (contains request ID).
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      process_type, child_id, route_id,
      -1,  // frame_tree_node_id
      request_data.origin_pid, request_id, request_data.render_frame_id,
      request_data.is_main_frame, request_data.parent_is_main_frame,
      request_data.resource_type, request_data.transition_type,
      request_data.should_replace_current_entry,
      false,  // is download
      false,  // is stream
      allow_download, request_data.has_user_gesture,
      request_data.enable_load_timing, request_data.enable_upload_progress,
      do_not_prompt_for_login, request_data.referrer_policy,
      request_data.visibility_state, resource_context, filter_->GetWeakPtr(),
      report_raw_headers, !is_sync_load,
      IsUsingLoFi(request_data.lofi_state, delegate_, *new_request,
                  resource_context,
                  request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME),
      support_async_revalidation ? request_data.headers : std::string(),
      request_data.request_body, request_data.initiated_in_secure_context);
  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(),
        filter_->blob_storage_context()->context()->GetBlobDataFromPublicURL(
            new_request->url()));
  }

  // Initialize the service worker handler for the request. We don't use
  // ServiceWorker for synchronous loads to avoid renderer deadlocks.
  const SkipServiceWorker should_skip_service_worker =
      is_sync_load ? SkipServiceWorker::ALL : request_data.skip_service_worker;
  ServiceWorkerRequestHandler::InitializeHandler(
      new_request.get(), filter_->service_worker_context(), blob_context,
      child_id, request_data.service_worker_provider_id,
      should_skip_service_worker != SkipServiceWorker::NONE,
      request_data.fetch_request_mode, request_data.fetch_credentials_mode,
      request_data.fetch_redirect_mode, request_data.resource_type,
      request_data.fetch_request_context_type, request_data.fetch_frame_type,
      request_data.request_body);

  ForeignFetchRequestHandler::InitializeHandler(
      new_request.get(), filter_->service_worker_context(), blob_context,
      child_id, request_data.service_worker_provider_id,
      should_skip_service_worker, request_data.fetch_request_mode,
      request_data.fetch_credentials_mode, request_data.fetch_redirect_mode,
      request_data.resource_type, request_data.fetch_request_context_type,
      request_data.fetch_frame_type, request_data.request_body,
      request_data.initiated_in_secure_context);

  // Have the appcache associate its extra info with the request.
  AppCacheInterceptor::SetExtraRequestInfo(
      new_request.get(), filter_->appcache_service(), child_id,
      request_data.appcache_host_id, request_data.resource_type,
      request_data.should_reset_appcache);

  std::unique_ptr<ResourceHandler> handler(CreateResourceHandler(
      new_request.get(), request_data, sync_result_handler, route_id,
      process_type, child_id, resource_context, std::move(mojo_request),
      std::move(url_loader_client)));

  if (handler)
    BeginRequestInternal(std::move(new_request), std::move(handler));
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandler(
    net::URLRequest* request,
    const ResourceRequest& request_data,
    const SyncLoadResultCallback& sync_result_handler,
    int route_id,
    int process_type,
    int child_id,
    ResourceContext* resource_context,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/456331 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456331 ResourceDispatcherHostImpl::CreateResourceHandler"));
  // Construct the IPC resource handler.
  std::unique_ptr<ResourceHandler> handler;
  if (sync_result_handler) {
    // download_to_file is not supported for synchronous requests.
    if (request_data.download_to_file) {
      bad_message::ReceivedBadMessage(filter_, bad_message::RDH_BAD_DOWNLOAD);
      return std::unique_ptr<ResourceHandler>();
    }

    DCHECK(!mojo_request.is_pending());
    DCHECK(!url_loader_client);
    handler.reset(new SyncResourceHandler(request, sync_result_handler, this));
  } else {
    if (mojo_request.is_pending()) {
      handler.reset(new MojoAsyncResourceHandler(request, this,
                                                 std::move(mojo_request),
                                                 std::move(url_loader_client)));
    } else {
      handler.reset(new AsyncResourceHandler(request, this));
    }

    // The RedirectToFileResourceHandler depends on being next in the chain.
    if (request_data.download_to_file) {
      handler.reset(
          new RedirectToFileResourceHandler(std::move(handler), request));
    }
  }

  bool start_detached = request_data.download_to_network_cache_only;

  // Prefetches and <a ping> requests outlive their child process.
  if (!sync_result_handler &&
      (start_detached ||
       IsDetachableResourceType(request_data.resource_type))) {
    std::unique_ptr<DetachableResourceHandler> detachable_handler =
        base::MakeUnique<DetachableResourceHandler>(
            request,
            base::TimeDelta::FromMilliseconds(kDefaultDetachableCancelDelayMs),
            std::move(handler));
    if (start_detached)
      detachable_handler->Detach();
    handler = std::move(detachable_handler);
  }

  return AddStandardHandlers(
      request, request_data.resource_type, resource_context,
      request_data.fetch_request_context_type, filter_->appcache_service(),
      child_id, route_id, std::move(handler));
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::AddStandardHandlers(
    net::URLRequest* request,
    ResourceType resource_type,
    ResourceContext* resource_context,
    RequestContextType fetch_request_context_type,
    AppCacheService* appcache_service,
    int child_id,
    int route_id,
    std::unique_ptr<ResourceHandler> handler) {
  // PlzNavigate: do not add ResourceThrottles for main resource requests from
  // the renderer.  Decisions about the navigation should have been done in the
  // initial request.
  bool is_renderer =
      filter_ ? (filter_->process_type() == PROCESS_TYPE_RENDERER) : false;
  if (is_renderer && IsBrowserSideNavigationEnabled() &&
      IsResourceTypeFrame(resource_type)) {
    DCHECK(request->url().SchemeIs(url::kBlobScheme));
    return handler;
  }

  // The InterceptingResourceHandler will replace its next handler with an
  // appropriate one based on the MIME type of the response if needed. It
  // should be placed at the end of the chain, just before |handler|.
  handler.reset(new InterceptingResourceHandler(std::move(handler), request));
  InterceptingResourceHandler* intercepting_handler =
      static_cast<InterceptingResourceHandler*>(handler.get());

  ScopedVector<ResourceThrottle> throttles;

  // Add a NavigationResourceThrottle for navigations.
  // PlzNavigate: the throttle is unnecessary as communication with the UI
  // thread is handled by the NavigationURLloader.
  if (!IsBrowserSideNavigationEnabled() && IsResourceTypeFrame(resource_type)) {
    throttles.push_back(new NavigationResourceThrottle(
        request, delegate_, fetch_request_context_type));
  }

  if (delegate_) {
    delegate_->RequestBeginning(request,
                                resource_context,
                                appcache_service,
                                resource_type,
                                &throttles);
  }

  if (request->has_upload()) {
    // Block power save while uploading data.
    throttles.push_back(new PowerSaveBlockResourceThrottle(
        request->url().host(),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE)));
  }

  // TODO(ricea): Stop looking this up so much.
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  throttles.push_back(scheduler_->ScheduleRequest(child_id, route_id,
                                                  info->IsAsync(), request));

  // Split the handler in two groups: the ones that need to execute
  // WillProcessResponse before mime sniffing and the others.
  // TODO(clamy): ScopedVector is deprecated. The interface should be changed
  // to use vectors instead.
  ScopedVector<ResourceThrottle> pre_mime_sniffing_throttles;
  ScopedVector<ResourceThrottle> post_mime_sniffing_throttles;
  for (auto throttle : throttles) {
    if (throttle->MustProcessResponseBeforeReadingBody()) {
      pre_mime_sniffing_throttles.push_back(throttle);
    } else {
      post_mime_sniffing_throttles.push_back(throttle);
    }
  }
  throttles.weak_clear();

  // Add the post mime sniffing throttles.
  handler.reset(new ThrottlingResourceHandler(
      std::move(handler), request, std::move(post_mime_sniffing_throttles)));

  PluginService* plugin_service = nullptr;
#if defined(ENABLE_PLUGINS)
  plugin_service = PluginService::GetInstance();
#endif

  // Insert a buffered event handler to sniff the mime type.
  // Note: all ResourceHandler following the MimeSniffingResourceHandler
  // should expect OnWillRead to be called *before* OnResponseStarted as
  // part of the mime sniffing process.
  handler.reset(new MimeSniffingResourceHandler(
      std::move(handler), this, plugin_service, intercepting_handler, request,
      fetch_request_context_type));

  // Add the pre mime sniffing throttles.
  handler.reset(new ThrottlingResourceHandler(
      std::move(handler), request, std::move(pre_mime_sniffing_throttles)));

  return handler;
}

void ResourceDispatcherHostImpl::OnReleaseDownloadedFile(int request_id) {
  UnregisterDownloadedTempFile(filter_->child_id(), request_id);
}

void ResourceDispatcherHostImpl::OnDidChangePriority(
    int request_id,
    net::RequestPriority new_priority,
    int intra_priority_value) {
  ResourceLoader* loader = GetLoader(filter_->child_id(), request_id);
  // The request may go away before processing this message, so |loader| can
  // legitimately be null.
  if (!loader)
    return;

  scheduler_->ReprioritizeRequest(loader->request(), new_priority,
                                  intra_priority_value);
}

void ResourceDispatcherHostImpl::RegisterDownloadedTempFile(
    int child_id, int request_id, const base::FilePath& file_path) {
  scoped_refptr<ShareableFileReference> reference =
      ShareableFileReference::Get(file_path);
  DCHECK(reference.get());

  registered_temp_files_[child_id][request_id] = reference;
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      child_id, reference->path());

  // When the temp file is deleted, revoke permissions that the renderer has
  // to that file. This covers an edge case where the file is deleted and then
  // the same name is re-used for some other purpose, we don't want the old
  // renderer to still have access to it.
  //
  // We do this when the file is deleted because the renderer can take a blob
  // reference to the temp file that outlives the url loaded that it was
  // loaded with to keep the file (and permissions) alive.
  reference->AddFinalReleaseCallback(
      base::Bind(&RemoveDownloadFileFromChildSecurityPolicy,
                 child_id));
}

void ResourceDispatcherHostImpl::UnregisterDownloadedTempFile(
    int child_id, int request_id) {
  DeletableFilesMap& map = registered_temp_files_[child_id];
  DeletableFilesMap::iterator found = map.find(request_id);
  if (found == map.end())
    return;

  map.erase(found);

  // Note that we don't remove the security bits here. This will be done
  // when all file refs are deleted (see RegisterDownloadedTempFile).
}

bool ResourceDispatcherHostImpl::Send(IPC::Message* message) {
  delete message;
  return false;
}

void ResourceDispatcherHostImpl::OnCancelRequest(int request_id) {
  CancelRequestFromRenderer(GlobalRequestID(filter_->child_id(), request_id));
}

ResourceRequestInfoImpl* ResourceDispatcherHostImpl::CreateRequestInfo(
    int child_id,
    int render_view_route_id,
    int render_frame_route_id,
    bool download,
    ResourceContext* context) {
  return new ResourceRequestInfoImpl(
      PROCESS_TYPE_RENDERER, child_id, render_view_route_id,
      -1,  // frame_tree_node_id
      0, MakeRequestID(), render_frame_route_id,
      false,  // is_main_frame
      false,  // parent_is_main_frame
      RESOURCE_TYPE_SUB_RESOURCE, ui::PAGE_TRANSITION_LINK,
      false,     // should_replace_current_entry
      download,  // is_download
      false,     // is_stream
      download,  // allow_download
      false,     // has_user_gesture
      false,     // enable_load_timing
      false,     // enable_upload_progress
      false,     // do_not_prompt_for_login
      blink::WebReferrerPolicyDefault, blink::WebPageVisibilityStateVisible,
      context,
      base::WeakPtr<ResourceMessageFilter>(),  // filter
      false,                                   // report_raw_headers
      true,                                    // is_async
      false,                                   // is_using_lofi
      std::string(),                           // original_headers
      nullptr,                                 // body
      false);                                  // initiated_in_secure_context
}

void ResourceDispatcherHostImpl::OnRenderViewHostCreated(int child_id,
                                                         int route_id) {
  scheduler_->OnClientCreated(child_id, route_id);
}

void ResourceDispatcherHostImpl::OnRenderViewHostDeleted(int child_id,
                                                         int route_id) {
  scheduler_->OnClientDeleted(child_id, route_id);
}

void ResourceDispatcherHostImpl::OnRenderViewHostSetIsLoading(int child_id,
                                                              int route_id,
                                                              bool is_loading) {
  scheduler_->OnLoadingStateChanged(child_id, route_id, !is_loading);
}

void ResourceDispatcherHostImpl::MarkAsTransferredNavigation(
    const GlobalRequestID& id,
    const base::Closure& on_transfer_complete_callback) {
  GetLoader(id)->MarkAsTransferring(on_transfer_complete_callback);
}

void ResourceDispatcherHostImpl::CancelTransferringNavigation(
    const GlobalRequestID& id) {
  // Request should still exist and be in the middle of a transfer.
  DCHECK(IsTransferredNavigation(id));
  RemovePendingRequest(id.child_id, id.request_id);
}

void ResourceDispatcherHostImpl::ResumeDeferredNavigation(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  // The response we were meant to resume could have already been canceled.
  if (loader)
    loader->CompleteTransfer();
}

// The object died, so cancel and detach all requests associated with it except
// for downloads and detachable resources, which belong to the browser process
// even if initiated via a renderer.
void ResourceDispatcherHostImpl::CancelRequestsForProcess(int child_id) {
  CancelRequestsForRoute(
      GlobalFrameRoutingId(child_id, MSG_ROUTING_NONE /* cancel all */));
  registered_temp_files_.erase(child_id);
}

void ResourceDispatcherHostImpl::CancelRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  int child_id = global_routing_id.child_id;
  int route_id = global_routing_id.frame_routing_id;
  bool cancel_all_routes = (route_id == MSG_ROUTING_NONE);

  bool any_requests_transferring = false;
  std::vector<GlobalRequestID> matching_requests;
  for (const auto& loader : pending_loaders_) {
    if (loader.first.child_id != child_id)
      continue;

    ResourceRequestInfoImpl* info = loader.second->GetRequestInfo();

    GlobalRequestID id(child_id, loader.first.request_id);
    DCHECK(id == loader.first);
    // Don't cancel navigations that are expected to live beyond this process.
    if (IsTransferredNavigation(id))
      any_requests_transferring = true;
    if (info->detachable_handler()) {
      info->detachable_handler()->Detach();
    } else if (!info->IsDownload() && !info->is_stream() &&
               !IsTransferredNavigation(id) &&
               (cancel_all_routes || route_id == info->GetRenderFrameID())) {
      matching_requests.push_back(id);
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    LoaderMap::iterator iter = pending_loaders_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a net::URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // net::URLRequest that needs exclusive access to the same cache entry, and
    // that net::URLRequest may complete and remove itself from
    // pending_requests_. So we need to check that iter is not equal to
    // pending_requests_.end().
    if (iter != pending_loaders_.end())
      RemovePendingLoader(iter);
  }

  // Don't clear the blocked loaders or offline policy maps if any of the
  // requests in route_id are being transferred to a new process, since those
  // maps will be updated with the new route_id after the transfer.  Otherwise
  // we will lose track of this info when the old route goes away, before the
  // new one is created.
  if (any_requests_transferring)
    return;

  // Now deal with blocked requests if any.
  if (!cancel_all_routes) {
    if (blocked_loaders_map_.find(global_routing_id) !=
        blocked_loaders_map_.end()) {
      CancelBlockedRequestsForRoute(global_routing_id);
    }
  } else {
    // We have to do all render frames for the process |child_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_loaders_map_, as blocking requests modifies the map.
    std::set<GlobalFrameRoutingId> routing_ids;
    for (const auto& blocked_loaders : blocked_loaders_map_) {
      if (blocked_loaders.first.child_id == child_id)
        routing_ids.insert(blocked_loaders.first);
    }
    for (const GlobalFrameRoutingId& route_id : routing_ids) {
      CancelBlockedRequestsForRoute(route_id);
    }
  }
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHostImpl::RemovePendingRequest(int child_id,
                                                      int request_id) {
  LoaderMap::iterator i = pending_loaders_.find(
      GlobalRequestID(child_id, request_id));
  if (i == pending_loaders_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingLoader(i);
}

void ResourceDispatcherHostImpl::RemovePendingLoader(
    const LoaderMap::iterator& iter) {
  ResourceRequestInfoImpl* info = iter->second->GetRequestInfo();

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemory(-1, *info);

  pending_loaders_.erase(iter);
}

void ResourceDispatcherHostImpl::CancelRequest(int child_id,
                                               int request_id) {
  ResourceLoader* loader = GetLoader(child_id, request_id);
  if (!loader) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }

  RemovePendingRequest(child_id, request_id);
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::GetOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info) {
  OutstandingRequestsStatsMap::iterator entry =
      outstanding_requests_stats_map_.find(info.GetChildID());
  OustandingRequestsStats stats = { 0, 0 };
  if (entry != outstanding_requests_stats_map_.end())
    stats = entry->second;
  return stats;
}

void ResourceDispatcherHostImpl::UpdateOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info,
    const OustandingRequestsStats& stats) {
  if (stats.memory_cost == 0 && stats.num_requests == 0)
    outstanding_requests_stats_map_.erase(info.GetChildID());
  else
    outstanding_requests_stats_map_[info.GetChildID()] = stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsMemory(
    int count,
    const ResourceRequestInfoImpl& info) {
  DCHECK_EQ(1, abs(count));

  // Retrieve the previous value (defaulting to 0 if not found).
  OustandingRequestsStats stats = GetOutstandingRequestsStats(info);

  // Insert/update the total; delete entries when their count reaches 0.
  stats.memory_cost += count * info.memory_cost();
  DCHECK_GE(stats.memory_cost, 0);
  UpdateOutstandingRequestsStats(info, stats);

  return stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsCount(
    int count,
    ResourceRequestInfoImpl* info) {
  DCHECK_EQ(1, abs(count));
  num_in_flight_requests_ += count;

  // Keep track of whether this request is counting toward the number of
  // in-flight requests for this process, in case we need to transfer it to
  // another process. This should be a toggle.
  DCHECK_NE(info->counted_as_in_flight_request(), count > 0);
  info->set_counted_as_in_flight_request(count > 0);

  OustandingRequestsStats stats = GetOutstandingRequestsStats(*info);
  stats.num_requests += count;
  DCHECK_GE(stats.num_requests, 0);
  UpdateOutstandingRequestsStats(*info, stats);

  return stats;
}

bool ResourceDispatcherHostImpl::HasSufficientResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  OustandingRequestsStats stats = IncrementOutstandingRequestsCount(1, info);

  if (stats.num_requests > max_num_in_flight_requests_per_process_)
    return false;
  if (num_in_flight_requests_ > max_num_in_flight_requests_)
    return false;

  return true;
}

void ResourceDispatcherHostImpl::FinishedWithResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  IncrementOutstandingRequestsCount(-1, info);
}

void ResourceDispatcherHostImpl::BeginNavigationRequest(
    ResourceContext* resource_context,
    const NavigationRequestInfo& info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    NavigationURLLoaderImplCore* loader,
    ServiceWorkerNavigationHandleCore* service_worker_handle_core) {
  // PlzNavigate: BeginNavigationRequest currently should only be used for the
  // browser-side navigations project.
  CHECK(IsBrowserSideNavigationEnabled());

  ResourceType resource_type = info.is_main_frame ?
      RESOURCE_TYPE_MAIN_FRAME : RESOURCE_TYPE_SUB_FRAME;

  // Do not allow browser plugin guests to navigate to non-web URLs, since they
  // cannot swap processes or grant bindings. Do not check external protocols
  // here because they're checked in
  // ChromeResourceDispatcherHostDelegate::HandleExternalProtocol.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  bool is_external_protocol =
      !resource_context->GetRequestContext()->job_factory()->IsHandledURL(
          info.common_params.url);
  bool non_web_url_in_guest =
      info.is_for_guests_only &&
      !policy->IsWebSafeScheme(info.common_params.url.scheme()) &&
      !is_external_protocol;

  if (is_shutdown_ || non_web_url_in_guest ||
      // TODO(davidben): Check ShouldServiceRequest here. This is important; it
      // needs to be checked relative to the child that /requested/ the
      // navigation. It's where file upload checks, etc., come in.
      (delegate_ && !delegate_->ShouldBeginRequest(
          info.common_params.method,
          info.common_params.url,
          resource_type,
          resource_context))) {
    loader->NotifyRequestFailed(false, net::ERR_ABORTED);
    return;
  }

  const net::URLRequestContext* request_context =
      resource_context->GetRequestContext();

  int load_flags = info.begin_params.load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (info.is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // TODO(davidben): BuildLoadFlagsForRequest includes logic for
  // CanSendCookiesForOrigin and CanReadRawCookies. Is this needed here?

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  std::unique_ptr<net::URLRequest> new_request;
  new_request = request_context->CreateRequest(
      info.common_params.url, net::HIGHEST, nullptr);

  new_request->set_method(info.common_params.method);
  new_request->set_first_party_for_cookies(
      info.first_party_for_cookies);
  new_request->set_initiator(info.request_initiator);
  if (info.is_main_frame) {
    new_request->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  SetReferrerForRequest(new_request.get(), info.common_params.referrer);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(info.begin_params.headers);
  new_request->SetExtraRequestHeaders(headers);

  new_request->SetLoadFlags(load_flags);

  storage::BlobStorageContext* blob_context = GetBlobStorageContext(
      GetChromeBlobStorageContextForResourceContext(resource_context));

  // Resolve elements from request_body and prepare upload data.
  ResourceRequestBodyImpl* body = info.common_params.post_data.get();
  if (body) {
    AttachRequestBodyBlobDataHandles(body, blob_context);
    // TODO(davidben): The FileSystemContext is null here. In the case where
    // another renderer requested this navigation, this should be the same
    // FileSystemContext passed into ShouldServiceRequest.
    new_request->set_upload(UploadDataStreamBuilder::Build(
        body, blob_context,
        nullptr,  // file_system_context
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get()));
  }

  // Make extra info and read footer (contains request ID).
  //
  // TODO(davidben): Associate the request with the FrameTreeNode and/or tab so
  // that IO thread -> UI thread hops will work.
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      PROCESS_TYPE_BROWSER,
      -1,  // child_id
      -1,  // route_id
      info.frame_tree_node_id,
      -1,  // request_data.origin_pid,
      MakeRequestID(),
      -1,  // request_data.render_frame_id,
      info.is_main_frame, info.parent_is_main_frame, resource_type,
      info.common_params.transition,
      // should_replace_current_entry. This was only maintained at layer for
      // request transfers and isn't needed for browser-side navigations.
      false,
      false,  // is download
      false,  // is stream
      info.common_params.allow_download, info.begin_params.has_user_gesture,
      true,   // enable_load_timing
      false,  // enable_upload_progress
      false,  // do_not_prompt_for_login
      info.common_params.referrer.policy,
      // TODO(davidben): This is only used for prerenders. Replace
      // is_showing with something for that. Or maybe it just comes from the
      // same mechanism as the cookie one.
      blink::WebPageVisibilityStateVisible, resource_context,
      base::WeakPtr<ResourceMessageFilter>(),  // filter
      info.report_raw_headers,
      true,   // is_async
      IsUsingLoFi(info.common_params.lofi_state, delegate_, *new_request,
                  resource_context, info.is_main_frame),
      // The original_headers field is for stale-while-revalidate but the
      // feature doesn't work with PlzNavigate, so it's just a placeholder
      // here.
      // TODO(ricea): Make the feature work with stale-while-revalidate
      // and clean this up.
      std::string(),  // original_headers
      info.common_params.post_data,
      // TODO(mek): Currently initiated_in_secure_context is only used for
      // subresource requests, so it doesn't matter what value it gets here.
      // If in the future this changes this should be updated to somehow get a
      // meaningful value.
      false);  // initiated_in_secure_context
  extra_info->set_navigation_ui_data(std::move(navigation_ui_data));

  if (service_worker_handle_core) {
    extra_info->set_service_worker_context(
        service_worker_handle_core->context_wrapper());
  }

  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(),
        blob_context->GetBlobDataFromPublicURL(new_request->url()));
  }

  RequestContextFrameType frame_type =
      info.is_main_frame ? REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL
                         : REQUEST_CONTEXT_FRAME_TYPE_NESTED;
  ServiceWorkerRequestHandler::InitializeForNavigation(
      new_request.get(), service_worker_handle_core, blob_context,
      info.begin_params.skip_service_worker, resource_type,
      info.begin_params.request_context_type, frame_type,
      info.are_ancestors_secure, info.common_params.post_data);

  // TODO(davidben): Attach AppCacheInterceptor.

  std::unique_ptr<ResourceHandler> handler(
      new NavigationResourceHandler(new_request.get(), loader, delegate()));

  // TODO(davidben): Pass in the appropriate appcache_service. Also fix the
  // dependency on child_id/route_id. Those are used by the ResourceScheduler;
  // currently it's a no-op.
  handler =
      AddStandardHandlers(new_request.get(), resource_type, resource_context,
                          info.begin_params.request_context_type,
                          nullptr,  // appcache_service
                          -1,       // child_id
                          -1,       // route_id
                          std::move(handler));

  BeginRequestInternal(std::move(new_request), std::move(handler));
}

void ResourceDispatcherHostImpl::EnableStaleWhileRevalidateForTesting() {
  if (!async_revalidation_manager_)
    async_revalidation_manager_.reset(new AsyncRevalidationManager);
}

void ResourceDispatcherHostImpl::SetLoaderDelegate(
    LoaderDelegate* loader_delegate) {
  loader_delegate_ = loader_delegate;
}

void ResourceDispatcherHostImpl::OnRenderFrameDeleted(
    const GlobalFrameRoutingId& global_routing_id) {
  CancelRequestsForRoute(global_routing_id);
}

void ResourceDispatcherHostImpl::OnRequestResourceWithMojo(
    int routing_id,
    int request_id,
    const ResourceRequest& request,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client,
    ResourceMessageFilter* filter) {
  filter_ = filter;
  OnRequestResourceInternal(routing_id, request_id, request,
                            std::move(mojo_request),
                            std::move(url_loader_client));
  filter_ = nullptr;
}

void ResourceDispatcherHostImpl::OnSyncLoadWithMojo(
    int routing_id,
    int request_id,
    const ResourceRequest& request_data,
    ResourceMessageFilter* filter,
    const SyncLoadResultCallback& result_handler) {
  filter_ = filter;
  BeginRequest(request_id, request_data, result_handler, routing_id,
               nullptr, nullptr);
  filter_ = nullptr;
}

// static
int ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
    net::URLRequest* request) {
  // The following fields should be a minor size contribution (experimentally
  // on the order of 100). However since they are variable length, it could
  // in theory be a sizeable contribution.
  int strings_cost = 0;
  for (net::HttpRequestHeaders::Iterator it(request->extra_request_headers());
       it.GetNext();) {
    strings_cost += it.name().length() + it.value().length();
  }
  strings_cost +=
      request->original_url().parsed_for_possibly_invalid_spec().Length() +
      request->referrer().size() + request->method().size();

  // Note that this expression will typically be dominated by:
  // |kAvgBytesPerOutstandingRequest|.
  return kAvgBytesPerOutstandingRequest + strings_cost;
}

void ResourceDispatcherHostImpl::BeginRequestInternal(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceHandler> handler) {
  DCHECK(!request->is_pending());
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());

  if ((TimeTicks::Now() - last_user_gesture_time_) <
      TimeDelta::FromMilliseconds(kUserGestureWindowMs)) {
    request->SetLoadFlags(request->load_flags() | net::LOAD_MAYBE_USER_GESTURE);
  }

  // Add the memory estimate that starting this request will consume.
  info->set_memory_cost(CalculateApproximateMemoryCost(request.get()));

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  OustandingRequestsStats stats = IncrementOutstandingRequestsMemory(1, *info);
  if (stats.memory_cost > max_outstanding_requests_cost_per_process_) {
    // We call "CancelWithError()" as a way of setting the net::URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);

    bool defer = false;
    handler->OnResponseCompleted(request->status(), &defer);
    if (defer) {
      // TODO(darin): The handler is not ready for us to kill the request. Oops!
      NOTREACHED();
    }

    IncrementOutstandingRequestsMemory(-1, *info);

    // A ResourceHandler must not outlive its associated URLRequest.
    handler.reset();
    return;
  }

  std::unique_ptr<ResourceLoader> loader(new ResourceLoader(
      std::move(request), std::move(handler), this));

  GlobalFrameRoutingId id(info->GetChildID(), info->GetRenderFrameID());
  BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.find(id);
  if (iter != blocked_loaders_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(std::move(loader));
    return;
  }

  StartLoading(info, std::move(loader));
}

void ResourceDispatcherHostImpl::InitializeURLRequest(
    net::URLRequest* request,
    const Referrer& referrer,
    bool is_download,
    int render_process_host_id,
    int render_view_routing_id,
    int render_frame_routing_id,
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!request->is_pending());

  SetReferrerForRequest(request, referrer);

  ResourceRequestInfoImpl* info =
      CreateRequestInfo(render_process_host_id, render_view_routing_id,
                        render_frame_routing_id, is_download, context);
  // Request takes ownership.
  info->AssociateWithRequest(request);
}

void ResourceDispatcherHostImpl::BeginURLRequest(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceHandler> handler,
    bool is_download,
    bool is_content_initiated,
    bool do_not_prompt_for_login,
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!request->is_pending());

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());
  DCHECK(info);
  info->set_do_not_prompt_for_login(do_not_prompt_for_login);

  // TODO(ananta)
  // Find a better place for notifying the delegate about the download start.
  if (is_download && delegate()) {
    // TODO(ananta)
    // Investigate whether the blob logic should apply for the SaveAs case and
    // if yes then move the code below outside the if block.
    if (request->original_url().SchemeIs(url::kBlobScheme) &&
        !storage::BlobProtocolHandler::GetRequestBlobDataHandle(
            request.get())) {
      ChromeBlobStorageContext* blob_context =
          GetChromeBlobStorageContextForResourceContext(context);
      storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
          request.get(),
          blob_context->context()->GetBlobDataFromPublicURL(
              request->original_url()));
    }
    handler = HandleDownloadStarted(
        request.get(), std::move(handler), is_content_initiated,
        true /* force_download */, true /* is_new_request */);
  }
  BeginRequestInternal(std::move(request), std::move(handler));
}

int ResourceDispatcherHostImpl::MakeRequestID() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return --request_id_;
}

void ResourceDispatcherHostImpl::CancelRequestFromRenderer(
    GlobalRequestID request_id) {
  // When the old renderer dies, it sends a message to us to cancel its
  // requests.
  if (IsTransferredNavigation(request_id))
    return;

  ResourceLoader* loader = GetLoader(request_id);

  // It is possible that the request has been completed and removed from the
  // loader queue but the client has not processed the request completed message
  // before issuing a cancel. This happens frequently for beacons which are
  // canceled in the response received handler.
  if (!loader)
    return;

  loader->CancelRequest(true);
}

void ResourceDispatcherHostImpl::StartLoading(
    ResourceRequestInfoImpl* info,
    std::unique_ptr<ResourceLoader> loader) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/456331 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456331 ResourceDispatcherHostImpl::StartLoading"));

  ResourceLoader* loader_ptr = loader.get();
  DCHECK(pending_loaders_[info->GetGlobalRequestID()] == nullptr);
  pending_loaders_[info->GetGlobalRequestID()] = std::move(loader);

  loader_ptr->StartRequest();
}

void ResourceDispatcherHostImpl::OnUserGesture() {
  last_user_gesture_time_ = TimeTicks::Now();
}

net::URLRequest* ResourceDispatcherHostImpl::GetURLRequest(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  if (!loader)
    return NULL;

  return loader->request();
}

// static
bool ResourceDispatcherHostImpl::LoadInfoIsMoreInteresting(const LoadInfo& a,
                                                           const LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64_t b_uploading_size = 0;
  if (b.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state.state > b.load_state.state;
}

// static
void ResourceDispatcherHostImpl::UpdateLoadStateOnUI(
    LoaderDelegate* loader_delegate, std::unique_ptr<LoadInfoList> infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<LoadInfoMap> info_map =
      PickMoreInterestingLoadInfos(std::move(infos));
  for (const auto& load_info: *info_map) {
    loader_delegate->LoadStateChanged(
        load_info.first,
        load_info.second.url, load_info.second.load_state,
        load_info.second.upload_position, load_info.second.upload_size);
  }
}

// static
std::unique_ptr<ResourceDispatcherHostImpl::LoadInfoMap>
ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
    std::unique_ptr<LoadInfoList> infos) {
  std::unique_ptr<LoadInfoMap> info_map(new LoadInfoMap);
  for (const auto& load_info : *infos) {
    WebContents* web_contents = load_info.web_contents_getter.Run();
    if (!web_contents)
      continue;

    auto existing = info_map->find(web_contents);
    if (existing == info_map->end() ||
        LoadInfoIsMoreInteresting(load_info, existing->second)) {
      (*info_map)[web_contents] = load_info;
    }
  }
  return info_map;
}

std::unique_ptr<ResourceDispatcherHostImpl::LoadInfoList>
ResourceDispatcherHostImpl::GetLoadInfoForAllRoutes() {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);

  for (const auto& loader : pending_loaders_) {
    net::URLRequest* request = loader.second->request();
    net::UploadProgress upload_progress = request->GetUploadProgress();

    LoadInfo load_info;
    load_info.web_contents_getter =
        loader.second->GetRequestInfo()->GetWebContentsGetterForRequest();
    load_info.url = request->url();
    load_info.load_state = request->GetLoadState();
    load_info.upload_size = upload_progress.size();
    load_info.upload_position = upload_progress.position();
    infos->push_back(load_info);
  }
  return infos;
}

void ResourceDispatcherHostImpl::UpdateLoadInfo() {
  std::unique_ptr<LoadInfoList> infos(GetLoadInfoForAllRoutes());

  // Stop the timer if there are no more pending requests. Future new requests
  // will restart it as necessary.
  // Also stop the timer if there are no loading clients, to avoid waking up
  // unnecessarily when there is a long running (hanging get) request.
  if (infos->empty() || !scheduler_->HasLoadingClients()) {
    update_load_states_timer_->Stop();
    return;
  }

  // We need to be able to compare all requests to find the most important one
  // per tab. Since some requests may be navigation requests and we don't have
  // their render frame routing IDs yet (which is what we have for subresource
  // requests), we must go to the UI thread and compare the requests using their
  // WebContents.
  BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(UpdateLoadStateOnUI,
                   loader_delegate_, base::Passed(&infos)));
}

void ResourceDispatcherHostImpl::BlockRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blocked_loaders_map_.find(global_routing_id) ==
         blocked_loaders_map_.end())
      << "BlockRequestsForRoute called  multiple time for the same RFH";
  blocked_loaders_map_[global_routing_id] =
      base::MakeUnique<BlockedLoadersList>();
}

void ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  ProcessBlockedRequestsForRoute(global_routing_id, false);
}

void ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  ProcessBlockedRequestsForRoute(global_routing_id, true);
}

void ResourceDispatcherHostImpl::ProcessBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id,
    bool cancel_requests) {
  BlockedLoadersMap::iterator iter =
      blocked_loaders_map_.find(global_routing_id);
  if (iter == blocked_loaders_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedLoadersList* loaders = iter->second.get();
  std::unique_ptr<BlockedLoadersList> deleter(std::move(iter->second));

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_loaders_map_.erase(iter);

  for (std::unique_ptr<ResourceLoader>& loader : *loaders) {
    ResourceRequestInfoImpl* info = loader->GetRequestInfo();
    if (cancel_requests) {
      IncrementOutstandingRequestsMemory(-1, *info);
    } else {
      StartLoading(info, std::move(loader));
    }
  }
}

ResourceDispatcherHostImpl::HttpAuthRelationType
ResourceDispatcherHostImpl::HttpAuthRelationTypeOf(
    const GURL& request_url,
    const GURL& first_party) {
  if (!first_party.is_valid())
    return HTTP_AUTH_RELATION_TOP;

  if (net::registry_controlled_domains::SameDomainOrHost(
          first_party, request_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    return HTTP_AUTH_RELATION_SAME_DOMAIN;

  if (allow_cross_origin_auth_prompt())
    return HTTP_AUTH_RELATION_ALLOWED_CROSS;

  return HTTP_AUTH_RELATION_BLOCKED_CROSS;
}

bool ResourceDispatcherHostImpl::allow_cross_origin_auth_prompt() {
  return allow_cross_origin_auth_prompt_;
}

bool ResourceDispatcherHostImpl::IsTransferredNavigation(
    const GlobalRequestID& id) const {
  ResourceLoader* loader = GetLoader(id);
  return loader ? loader->is_transferring() : false;
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(
    const GlobalRequestID& id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LoaderMap::const_iterator i = pending_loaders_.find(id);
  if (i == pending_loaders_.end())
    return NULL;

  return i->second.get();
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(int child_id,
                                                      int request_id) const {
  return GetLoader(GlobalRequestID(child_id, request_id));
}

void ResourceDispatcherHostImpl::RegisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DelegateMap::iterator it = delegate_map_.find(id);
  if (it == delegate_map_.end()) {
    it = delegate_map_.insert(
                           std::make_pair(
                               id,
                               new base::ObserverList<ResourceMessageDelegate>))
             .first;
  }
  it->second->AddObserver(delegate);
}

void ResourceDispatcherHostImpl::UnregisterResourceMessageDelegate(
    const GlobalRequestID& id, ResourceMessageDelegate* delegate) {
  DCHECK(base::ContainsKey(delegate_map_, id));
  DelegateMap::iterator it = delegate_map_.find(id);
  DCHECK(it->second->HasObserver(delegate));
  it->second->RemoveObserver(delegate);
  if (!it->second->might_have_observers()) {
    delete it->second;
    delegate_map_.erase(it);
  }
}

int ResourceDispatcherHostImpl::BuildLoadFlagsForRequest(
    const ResourceRequest& request_data,
    int child_id,
    bool is_sync_load) {
  int load_flags = request_data.load_flags;

  // Although EV status is irrelevant to sub-frames and sub-resources, we have
  // to perform EV certificate verification on all resources because an HTTP
  // keep-alive connection created to load a sub-frame or a sub-resource could
  // be reused to load a main frame.
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_data.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
  } else if (request_data.resource_type == RESOURCE_TYPE_PREFETCH) {
    load_flags |= net::LOAD_PREFETCH;
  }

  if (is_sync_load)
    load_flags |= net::LOAD_IGNORE_LIMITS;

  return load_flags;
}

bool ResourceDispatcherHostImpl::ShouldServiceRequest(
    int process_type,
    int child_id,
    const ResourceRequest& request_data,
    const net::HttpRequestHeaders& headers,
    ResourceMessageFilter* filter,
    ResourceContext* resource_context) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(child_id, request_data.url)) {
    VLOG(1) << "Denied unauthorized request for "
            << request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is using an illegal Origin header.  If so, kill it.
  std::string origin_string;
  bool has_origin =
      headers.GetHeader("Origin", &origin_string) && origin_string != "null";
  if (has_origin) {
    GURL origin(origin_string);
    if (!policy->CanSetAsOriginHeader(child_id, origin)) {
      VLOG(1) << "Killed renderer for illegal origin: " << origin_string;
      bad_message::ReceivedBadMessage(filter, bad_message::RDH_ILLEGAL_ORIGIN);
      return false;
    }
  }

  // Check if the renderer is permitted to upload the requested files.
  if (request_data.request_body.get()) {
    const std::vector<ResourceRequestBodyImpl::Element>* uploads =
        request_data.request_body->elements();
    std::vector<ResourceRequestBodyImpl::Element>::const_iterator iter;
    for (iter = uploads->begin(); iter != uploads->end(); ++iter) {
      if (iter->type() == ResourceRequestBodyImpl::Element::TYPE_FILE &&
          !policy->CanReadFile(child_id, iter->path())) {
        NOTREACHED() << "Denied unauthorized upload of "
                     << iter->path().value();
        return false;
      }
      if (iter->type() ==
          ResourceRequestBodyImpl::Element::TYPE_FILE_FILESYSTEM) {
        storage::FileSystemURL url =
            filter->file_system_context()->CrackURL(iter->filesystem_url());
        if (!policy->CanReadFileSystemFile(child_id, url)) {
          NOTREACHED() << "Denied unauthorized upload of "
                       << iter->filesystem_url().spec();
          return false;
        }
      }
    }
  }
  return true;
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::HandleDownloadStarted(
    net::URLRequest* request,
    std::unique_ptr<ResourceHandler> handler,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request) {
  if (delegate()) {
    const ResourceRequestInfoImpl* request_info(
        ResourceRequestInfoImpl::ForRequest(request));
    ScopedVector<ResourceThrottle> throttles;
    delegate()->DownloadStarting(request, request_info->GetContext(),
                                 is_content_initiated, true, is_new_request,
                                 &throttles);
    if (!throttles.empty()) {
      handler.reset(new ThrottlingResourceHandler(std::move(handler), request,
                                                  std::move(throttles)));
    }
  }
  return handler;
}

}  // namespace content
