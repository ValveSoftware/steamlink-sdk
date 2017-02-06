// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_revalidation_manager.h"

#include <tuple>
#include <utility>

#include "base/logging.h"
#include "content/browser/loader/async_revalidation_driver.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/load_flags.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace content {

AsyncRevalidationManager::AsyncRevalidationKey::AsyncRevalidationKey(
    const ResourceContext* resource_context,
    const net::HttpCache* http_cache,
    const GURL& url)
    : resource_context(resource_context),
      http_cache(http_cache),
      url_key(net::HttpUtil::SpecForRequest(url)) {}

AsyncRevalidationManager::AsyncRevalidationKey::AsyncRevalidationKey(
    const ResourceContext* resource_context)
    : resource_context(resource_context), http_cache(nullptr), url_key() {}

AsyncRevalidationManager::AsyncRevalidationKey::~AsyncRevalidationKey() {}

bool AsyncRevalidationManager::AsyncRevalidationKey::LessThan::operator()(
    const AsyncRevalidationKey& lhs,
    const AsyncRevalidationKey& rhs) const {
  return std::tie(lhs.resource_context, lhs.http_cache, lhs.url_key) <
         std::tie(rhs.resource_context, rhs.http_cache, rhs.url_key);
}

AsyncRevalidationManager::AsyncRevalidationManager() {}

AsyncRevalidationManager::~AsyncRevalidationManager() {
  DCHECK(in_progress_.empty());
}

void AsyncRevalidationManager::BeginAsyncRevalidation(
    const net::URLRequest& for_request,
    ResourceScheduler* scheduler) {
  DCHECK_EQ(for_request.url_chain().size(), 1u);
  const ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(&for_request);
  DCHECK(info);
  if (!info->filter()) {
    // The child has gone away and we can no longer get ResourceContext and
    // URLRequestContext to perform async revalidation.
    // This can happen in the following cases, ordered from bad to not-so-bad
    //
    // 1. PlzNavigate (but enabling PlzNavigate automatically disables
    //    stale-while-revalidate; see crbug.com/561609)
    // 2. <link rel=prefetch> may read a stale cache entry without a
    //    revalidation being performed if the original renderer goes away. This
    //    is a lost optimisation opportunity.
    //
    // Not an issue:
    // 1. Implicit downloads. This method is called before
    //    MimeTypeResourceHandler calls set_is_download, so the renderer process
    //    must still exist for the request not to have been canceled.
    // 2. Explicit downloads (ie. started via "Save As"). These never use
    //    stale-while-revalidate.
    // 3. Non-PlzNavigate navigations between renderers. The old renderer
    //    still exists when this method is called.
    // 4. <a ping>, navigation.sendBeacon() and Content-Security-Policy reports
    //    are POST requests, so they never use stale-while-revalidate.
    //
    // TODO(ricea): Resolve these lifetime issues. crbug.com/561591
    return;
  }

  ResourceContext* resource_context = nullptr;
  net::URLRequestContext* request_context = nullptr;

  // The embedder of //content needs to ensure that the URLRequestContext object
  // remains valid until after the ResourceContext object is destroyed.
  info->filter()->GetContexts(info->GetResourceType(), info->GetOriginPID(),
                              &resource_context, &request_context);

  AsyncRevalidationKey async_revalidation_key(
      resource_context, request_context->http_transaction_factory()->GetCache(),
      for_request.url());
  std::pair<AsyncRevalidationMap::iterator, bool> insert_result =
      in_progress_.insert(AsyncRevalidationMap::value_type(
          async_revalidation_key, std::unique_ptr<AsyncRevalidationDriver>()));
  if (!insert_result.second) {
    // A matching async revalidation is already in progress for this cache; we
    // don't need another one.
    return;
  }

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(info->original_headers());

  // Construct the request.
  std::unique_ptr<net::URLRequest> new_request = request_context->CreateRequest(
      for_request.url(), net::MINIMUM_PRIORITY, nullptr);

  new_request->set_method(for_request.method());
  new_request->set_first_party_for_cookies(
      for_request.first_party_for_cookies());
  new_request->set_initiator(for_request.initiator());
  new_request->set_first_party_url_policy(for_request.first_party_url_policy());

  new_request->SetReferrer(for_request.referrer());
  new_request->set_referrer_policy(for_request.referrer_policy());

  new_request->SetExtraRequestHeaders(headers);

  // Remove LOAD_SUPPORT_ASYNC_REVALIDATION and LOAD_MAIN_FRAME flags.
  // Also remove things which shouldn't have been there to begin with,
  // and unrecognised flags.
  int load_flags =
      for_request.load_flags() &
      (net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_BYPASS_PROXY |
       net::LOAD_VERIFY_EV_CERT | net::LOAD_DO_NOT_SEND_COOKIES |
       net::LOAD_DO_NOT_SEND_AUTH_DATA | net::LOAD_MAYBE_USER_GESTURE |
       net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY);
  new_request->SetLoadFlags(load_flags);

  // These values would be -1 if the request was created by PlzNavigate. This
  // would cause the async revalidation to start immediately.
  // stale-while-revalidate is disabled when PlzNavigate is enabled
  // to prevent this and other issues. See crbug.com/561610.
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  std::unique_ptr<ResourceThrottle> throttle =
      scheduler->ScheduleRequest(child_id, route_id, false, new_request.get());

  // AsyncRevalidationDriver does not outlive its entry in |in_progress_|,
  // so the iterator and |this| may be passed to base::Bind directly.
  insert_result.first->second.reset(new AsyncRevalidationDriver(
      std::move(new_request), std::move(throttle),
      base::Bind(&AsyncRevalidationManager::OnAsyncRevalidationComplete,
                 base::Unretained(this), insert_result.first)));
  insert_result.first->second->StartRequest();
}

void AsyncRevalidationManager::CancelAsyncRevalidationsForResourceContext(
    ResourceContext* resource_context) {
  // |resource_context| is the first part of the key, so elements to be
  // cancelled are contiguous in the map.
  AsyncRevalidationKey partial_key(resource_context);
  for (auto it = in_progress_.lower_bound(partial_key);
       it != in_progress_.end() &&
       it->first.resource_context == resource_context;) {
    it = in_progress_.erase(it);
  }
}

bool AsyncRevalidationManager::QualifiesForAsyncRevalidation(
    const ResourceRequest& request) {
  if (request.load_flags &
      (net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
       net::LOAD_VALIDATE_CACHE | net::LOAD_PREFERRING_CACHE |
       net::LOAD_ONLY_FROM_CACHE | net::LOAD_IGNORE_LIMITS |
       net::LOAD_PREFETCH)) {
    return false;
  }
  if (request.method != "GET")
    return false;
  // A GET request should not have a body.
  if (request.request_body.get())
    return false;
  if (!request.url.SchemeIsHTTPOrHTTPS())
    return false;

  return true;
}

void AsyncRevalidationManager::OnAsyncRevalidationComplete(
    AsyncRevalidationMap::iterator it) {
  in_progress_.erase(it);
}

}  // namespace content
