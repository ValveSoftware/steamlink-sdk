// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

const char kUserDataKey[] = "SWProviderKey";

// Must be unique in the child process.
int GetNextProviderId() {
  static base::StaticAtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

// Returns whether it's possible for a document whose frame is a descendant of
// |frame| to be a secure context, not considering scheme exceptions (since any
// document can be a secure context if it has a scheme exception). See
// Document::isSecureContextImpl for more details.
bool IsFrameSecure(blink::WebFrame* frame) {
  while (frame) {
    if (!frame->getSecurityOrigin().isPotentiallyTrustworthy())
      return false;
    frame = frame->parent();
  }
  return true;
}

}  // namespace

void ServiceWorkerNetworkProvider::AttachToDocumentState(
    base::SupportsUserData* datasource_userdata,
    std::unique_ptr<ServiceWorkerNetworkProvider> network_provider) {
  datasource_userdata->SetUserData(&kUserDataKey, network_provider.release());
}

ServiceWorkerNetworkProvider* ServiceWorkerNetworkProvider::FromDocumentState(
    base::SupportsUserData* datasource_userdata) {
  return static_cast<ServiceWorkerNetworkProvider*>(
      datasource_userdata->GetUserData(&kUserDataKey));
}

// static
std::unique_ptr<ServiceWorkerNetworkProvider>
ServiceWorkerNetworkProvider::CreateForNavigation(
    int route_id,
    blink::WebLocalFrame* frame,
    bool content_initiated) {
  // Determine if a ServiceWorkerNetworkProvider should be created and properly
  // initialized for the navigation. A default ServiceWorkerNetworkProvider
  // will always be created since it is expected in a certain number of places,
  // however it will have an invalid id.
  bool should_create_provider_for_window =
      ((frame->effectiveSandboxFlags() & blink::WebSandboxFlags::Origin) !=
       blink::WebSandboxFlags::Origin);

  // Now create the ServiceWorkerNetworkProvider (with invalid id if needed).
  std::unique_ptr<ServiceWorkerNetworkProvider> network_provider;
  if (should_create_provider_for_window) {
    // Ideally Document::isSecureContext would be called here, but the document
    // is not created yet, and due to redirects the URL may change. So pass
    // is_parent_frame_secure to the browser process, so it can determine the
    // context security when deciding whether to allow a service worker to
    // control the document.
    const bool is_parent_frame_secure = IsFrameSecure(frame->parent());

    network_provider = std::unique_ptr<ServiceWorkerNetworkProvider>(
        new ServiceWorkerNetworkProvider(route_id,
                                         SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                         is_parent_frame_secure));
  } else {
    network_provider = std::unique_ptr<ServiceWorkerNetworkProvider>(
        new ServiceWorkerNetworkProvider());
  }
  return network_provider;
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider(
    int route_id,
    ServiceWorkerProviderType provider_type,
    bool is_parent_frame_secure)
    : provider_id_(GetNextProviderId()) {
  DCHECK(provider_id_ != kInvalidServiceWorkerProviderId);
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  context_ = new ServiceWorkerProviderContext(
      provider_id_, provider_type,
      ChildThreadImpl::current()->thread_safe_sender());
  ChildThreadImpl::current()->Send(new ServiceWorkerHostMsg_ProviderCreated(
      provider_id_, route_id, provider_type, is_parent_frame_secure));
}

ServiceWorkerNetworkProvider::ServiceWorkerNetworkProvider()
    : provider_id_(kInvalidServiceWorkerProviderId) {}

ServiceWorkerNetworkProvider::~ServiceWorkerNetworkProvider() {
  if (provider_id_ == kInvalidServiceWorkerProviderId)
    return;
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  ChildThreadImpl::current()->Send(
      new ServiceWorkerHostMsg_ProviderDestroyed(provider_id_));
}

void ServiceWorkerNetworkProvider::SetServiceWorkerVersionId(
    int64_t version_id,
    int embedded_worker_id) {
  DCHECK_NE(kInvalidServiceWorkerProviderId, provider_id_);
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  ChildThreadImpl::current()->Send(new ServiceWorkerHostMsg_SetVersionId(
      provider_id_, version_id, embedded_worker_id));
}

bool ServiceWorkerNetworkProvider::IsControlledByServiceWorker() const {
  return context() && context()->controller();
}

}  // namespace content
