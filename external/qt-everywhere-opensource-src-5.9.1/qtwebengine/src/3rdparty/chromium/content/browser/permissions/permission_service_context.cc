// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include <utility>

#include "content/browser/permissions/permission_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::PermissionObserverPtr;

namespace content {

class PermissionServiceContext::PermissionSubscription {
 public:
  PermissionSubscription(PermissionServiceContext* context,
                         PermissionObserverPtr observer)
      : context_(context), observer_(std::move(observer)) {
    observer_.set_connection_error_handler(base::Bind(
        &PermissionSubscription::OnConnectionError, base::Unretained(this)));
  }

  ~PermissionSubscription() {
    DCHECK_NE(id_, 0);
    BrowserContext* browser_context = context_->GetBrowserContext();
    DCHECK(browser_context);
    if (browser_context->GetPermissionManager()) {
      browser_context->GetPermissionManager()
          ->UnsubscribePermissionStatusChange(id_);
    }
  }

  void OnConnectionError() {
    DCHECK_NE(id_, 0);
    context_->ObserverHadConnectionError(id_);
  }

  void OnPermissionStatusChanged(blink::mojom::PermissionStatus status) {
    observer_->OnPermissionStatusChange(status);
  }

  void set_id(int id) { id_ = id; }

 private:
  PermissionServiceContext* context_;
  PermissionObserverPtr observer_;
  int id_ = 0;
};

PermissionServiceContext::PermissionServiceContext(
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      render_process_host_(nullptr) {
}

PermissionServiceContext::PermissionServiceContext(
    RenderProcessHost* render_process_host)
    : WebContentsObserver(nullptr),
      render_frame_host_(nullptr),
      render_process_host_(render_process_host) {
}

PermissionServiceContext::~PermissionServiceContext() {
}

void PermissionServiceContext::CreateService(
    mojo::InterfaceRequest<blink::mojom::PermissionService> request) {
  services_.push_back(
      base::MakeUnique<PermissionServiceImpl>(this, std::move(request)));
}

void PermissionServiceContext::CreateSubscription(
    PermissionType permission_type,
    const url::Origin& origin,
    PermissionObserverPtr observer) {
  BrowserContext* browser_context = GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return;

  auto subscription =
      base::MakeUnique<PermissionSubscription>(this, std::move(observer));
  GURL requesting_origin(origin.Serialize());
  GURL embedding_origin = GetEmbeddingOrigin();
  int subscription_id =
      browser_context->GetPermissionManager()->SubscribePermissionStatusChange(
          permission_type, requesting_origin,
          // If the embedding_origin is empty, we'll use the |origin| instead.
          embedding_origin.is_empty() ? requesting_origin : embedding_origin,
          base::Bind(&PermissionSubscription::OnPermissionStatusChanged,
                     base::Unretained(subscription.get())));
  subscription->set_id(subscription_id);
  subscriptions_[subscription_id] = std::move(subscription);
}

void PermissionServiceContext::ServiceHadConnectionError(
    PermissionServiceImpl* service) {
  auto it = std::find_if(
      services_.begin(), services_.end(),
      [service](const std::unique_ptr<PermissionServiceImpl>& this_service) {
        return service == this_service.get();
      });
  DCHECK(it != services_.end());
  services_.erase(it);
}

void PermissionServiceContext::ObserverHadConnectionError(int subscription_id) {
  auto it = subscriptions_.find(subscription_id);
  DCHECK(it != subscriptions_.end());
  subscriptions_.erase(it);
}

void PermissionServiceContext::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CancelPendingOperations(old_host);
}

void PermissionServiceContext::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  CancelPendingOperations(render_frame_host);
}

void PermissionServiceContext::DidNavigateAnyFrame(
    RenderFrameHost* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  CancelPendingOperations(render_frame_host);
}

void PermissionServiceContext::CancelPendingOperations(
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host_);
  if (render_frame_host != render_frame_host_)
    return;

  for (const auto& service : services_)
    service->CancelPendingOperations();

  subscriptions_.clear();
}

BrowserContext* PermissionServiceContext::GetBrowserContext() const {
  if (!web_contents()) {
    DCHECK(render_process_host_);
    return render_process_host_->GetBrowserContext();
  }
  return web_contents()->GetBrowserContext();
}

GURL PermissionServiceContext::GetEmbeddingOrigin() const {
  return web_contents() ? web_contents()->GetLastCommittedURL().GetOrigin()
                        : GURL();
}

RenderFrameHost* PermissionServiceContext::render_frame_host() const {
  return render_frame_host_;
}

} // namespace content
