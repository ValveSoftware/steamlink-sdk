// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_manager.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/payments/payment_app_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppManager::~PaymentAppManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PaymentAppManager::PaymentAppManager(
    PaymentAppContext* payment_app_context,
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request)
    : payment_app_context_(payment_app_context),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(payment_app_context);

  binding_.set_connection_error_handler(
      base::Bind(&PaymentAppManager::OnConnectionError,
                 base::Unretained(this)));
}

void PaymentAppManager::OnConnectionError() {
  payment_app_context_->ServiceHadConnectionError(this);
}

void PaymentAppManager::SetManifest(
    const std::string& scope,
    payments::mojom::PaymentAppManifestPtr manifest,
    const SetManifestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(payments::mojom::PaymentAppManifestError::NOT_IMPLEMENTED);
}

}  // namespace content
