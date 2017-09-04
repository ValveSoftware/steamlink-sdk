// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"

namespace blink {
class WebURL;
class WebServiceWorkerProviderClient;
}

namespace content {

class ServiceWorkerDispatcher;
class ServiceWorkerProviderContext;
class ThreadSafeSender;

// This class corresponds to one ServiceWorkerContainer interface in
// JS context (i.e. navigator.serviceWorker).
class WebServiceWorkerProviderImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerProvider) {
 public:
  WebServiceWorkerProviderImpl(ThreadSafeSender* thread_safe_sender,
                               ServiceWorkerProviderContext* context);
  ~WebServiceWorkerProviderImpl() override;

  void setClient(blink::WebServiceWorkerProviderClient* client) override;

  void registerServiceWorker(const blink::WebURL& pattern,
                             const blink::WebURL& script_url,
                             WebServiceWorkerRegistrationCallbacks*) override;
  void getRegistration(const blink::WebURL& document_url,
                       WebServiceWorkerGetRegistrationCallbacks*) override;
  void getRegistrations(WebServiceWorkerGetRegistrationsCallbacks*) override;
  void getRegistrationForReady(
      WebServiceWorkerGetRegistrationForReadyCallbacks*) override;
  bool validateScopeAndScriptURL(const blink::WebURL& pattern,
                                 const blink::WebURL& script_url,
                                 blink::WebString* error_message) override;

  int provider_id() const;

 private:
  void RemoveProviderClient();
  ServiceWorkerDispatcher* GetDispatcher();

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<ServiceWorkerProviderContext> context_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerProviderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
