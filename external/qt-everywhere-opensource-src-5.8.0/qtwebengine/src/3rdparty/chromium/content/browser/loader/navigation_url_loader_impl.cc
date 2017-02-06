// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/stream_handle.h"

namespace content {

NavigationURLLoaderImpl::NavigationURLLoaderImpl(
    BrowserContext* browser_context,
    std::unique_ptr<NavigationRequestInfo> request_info,
    ServiceWorkerContextWrapper* service_worker_context_wrapper,
    NavigationURLLoaderDelegate* delegate)
    : delegate_(delegate), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  core_ = new NavigationURLLoaderImplCore(weak_factory_.GetWeakPtr());

  // TODO(carlosk): extend this trace to support non-PlzNavigate navigations.
  // For the trace below we're using the NavigationURLLoaderImplCore as the
  // async trace id, |navigation_start| as the timestamp and reporting the
  // FrameTreeNode id as a parameter.
  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", core_,
      request_info->common_params.navigation_start.ToInternalValue(),
      "FrameTreeNode id", request_info->frame_tree_node_id);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::Start, base::Unretained(core_),
                 browser_context->GetResourceContext(),
                 service_worker_context_wrapper, base::Passed(&request_info)));
}

NavigationURLLoaderImpl::~NavigationURLLoaderImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, core_);
  core_ = nullptr;
}

void NavigationURLLoaderImpl::FollowRedirect() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::FollowRedirect,
                 base::Unretained(core_)));
}

void NavigationURLLoaderImpl::ProceedWithResponse() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::ProceedWithResponse,
                 base::Unretained(core_)));
}

void NavigationURLLoaderImpl::NotifyRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestRedirected(redirect_info, response);
}

void NavigationURLLoaderImpl::NotifyResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    std::unique_ptr<StreamHandle> body,
    std::unique_ptr<NavigationData> navigation_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnResponseStarted(response, std::move(body),
                               std::move(navigation_data));
}

void NavigationURLLoaderImpl::NotifyRequestFailed(bool in_cache,
                                                  int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestFailed(in_cache, net_error);
}

void NavigationURLLoaderImpl::NotifyRequestStarted(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestStarted(timestamp);
}

void NavigationURLLoaderImpl::NotifyServiceWorkerEncountered() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnServiceWorkerEncountered();
}

}  // namespace content
