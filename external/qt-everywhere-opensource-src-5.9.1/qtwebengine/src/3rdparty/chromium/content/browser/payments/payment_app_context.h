// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/payments/payment_app.mojom.h"
#include "content/common/content_export.h"

namespace content {

class PaymentAppManager;
class ServiceWorkerContextWrapper;

class CONTENT_EXPORT PaymentAppContext
    : public base::RefCountedThreadSafe<PaymentAppContext> {
 public:
  PaymentAppContext();

  // Init and Shutdown are for use on the UI thread when the
  // StoragePartition is being setup and torn down.
  void Init(scoped_refptr<ServiceWorkerContextWrapper> context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Create a PaymentAppManager that is owned by this. Call on the UI
  // thread.
  void CreateService(
      mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request);

  // Called by PaymentAppManager objects so that they can
  // be deleted. Call on the IO thread.
  void ServiceHadConnectionError(PaymentAppManager* service);

 protected:
  friend class base::RefCountedThreadSafe<PaymentAppContext>;
  virtual ~PaymentAppContext();

 private:
  void CreateServiceOnIOThread(
      mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request);

  void ShutdownOnIO();

  // The services are owned by this. They're either deleted
  // during ShutdownOnIO or when the channel is closed via
  // ServiceHadConnectionError. Only accessed on the IO thread.
  std::map<PaymentAppManager*, std::unique_ptr<PaymentAppManager>> services_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTEXT_H_
