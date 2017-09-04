// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_provider_impl.h"

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"

using blink::WebURL;

namespace content {

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender,
    ServiceWorkerProviderContext* context)
    : thread_safe_sender_(thread_safe_sender),
      context_(context) {
  DCHECK(context_);
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
  // Make sure the provider client is removed.
  RemoveProviderClient();
}

void WebServiceWorkerProviderImpl::setClient(
    blink::WebServiceWorkerProviderClient* client) {
  if (!client) {
    RemoveProviderClient();
    return;
  }

  // TODO(kinuko): Here we could also register the current thread ID
  // on the provider context so that multiple WebServiceWorkerProviderImpl
  // (e.g. on document and on dedicated workers) can properly share
  // the single provider context across threads. (http://crbug.com/366538
  // for more context)
  GetDispatcher()->AddProviderClient(context_->provider_id(), client);

  if (!context_->controller())
    return;
  scoped_refptr<WebServiceWorkerImpl> controller =
      GetDispatcher()->GetOrCreateServiceWorker(
          ServiceWorkerHandleReference::Create(context_->controller()->info(),
                                               thread_safe_sender_.get()));
  client->setController(WebServiceWorkerImpl::CreateHandle(controller),
                        false /* shouldNotifyControllerChange */);
}

void WebServiceWorkerProviderImpl::registerServiceWorker(
    const WebURL& pattern,
    const WebURL& script_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  GetDispatcher()->RegisterServiceWorker(
      context_->provider_id(), pattern, script_url, callbacks);
}

void WebServiceWorkerProviderImpl::getRegistration(
    const blink::WebURL& document_url,
    WebServiceWorkerGetRegistrationCallbacks* callbacks) {
  GetDispatcher()->GetRegistration(
      context_->provider_id(), document_url, callbacks);
}

void WebServiceWorkerProviderImpl::getRegistrations(
    WebServiceWorkerGetRegistrationsCallbacks* callbacks) {
  GetDispatcher()->GetRegistrations(
      context_->provider_id(), callbacks);
}

void WebServiceWorkerProviderImpl::getRegistrationForReady(
    WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks) {
  GetDispatcher()->GetRegistrationForReady(context_->provider_id(), callbacks);
}

bool WebServiceWorkerProviderImpl::validateScopeAndScriptURL(
    const blink::WebURL& scope,
    const blink::WebURL& script_url,
    blink::WebString* error_message) {
  std::string error;
  bool has_error = ServiceWorkerUtils::ContainsDisallowedCharacter(
      scope, script_url, &error);
  if (has_error)
    *error_message = blink::WebString::fromUTF8(error);
  return !has_error;
}

int WebServiceWorkerProviderImpl::provider_id() const {
  return context_->provider_id();
}

void WebServiceWorkerProviderImpl::RemoveProviderClient() {
  // Remove the provider client, but only if the dispatcher is still there.
  // (For cleanup path we don't need to bother creating a new dispatcher)
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveProviderClient(context_->provider_id());
}

ServiceWorkerDispatcher* WebServiceWorkerProviderImpl::GetDispatcher() {
  return ServiceWorkerDispatcher::GetThreadSpecificInstance();
}

}  // namespace content
