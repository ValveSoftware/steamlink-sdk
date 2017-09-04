// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/payments/payment_app_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContext::PaymentAppContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PaymentAppContext::~PaymentAppContext() {
  DCHECK(services_.empty());
}

void PaymentAppContext::Init(
    scoped_refptr<ServiceWorkerContextWrapper> context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&PaymentAppContext::ShutdownOnIO, this));
}

void PaymentAppContext::CreateService(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContext::CreateServiceOnIOThread, this,
                 base::Passed(&request)));
}

void PaymentAppContext::ServiceHadConnectionError(PaymentAppManager* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(services_, service));

  services_.erase(service);
}

void PaymentAppContext::CreateServiceOnIOThread(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PaymentAppManager* service = new PaymentAppManager(this, std::move(request));
  services_[service] = base::WrapUnique(service);
}

void PaymentAppContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_.clear();
}

}  // namespace content
