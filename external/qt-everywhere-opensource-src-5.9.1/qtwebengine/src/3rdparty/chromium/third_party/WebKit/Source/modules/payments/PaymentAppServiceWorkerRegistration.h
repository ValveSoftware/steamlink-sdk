// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentAppServiceWorkerRegistration_h
#define PaymentAppServiceWorkerRegistration_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaymentAppManager;
class ScriptState;
class ServiceWorkerRegistration;

class PaymentAppServiceWorkerRegistration final
    : public GarbageCollectedFinalized<PaymentAppServiceWorkerRegistration>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(PaymentAppServiceWorkerRegistration);
  WTF_MAKE_NONCOPYABLE(PaymentAppServiceWorkerRegistration);

 public:
  virtual ~PaymentAppServiceWorkerRegistration();
  static PaymentAppServiceWorkerRegistration& from(ServiceWorkerRegistration&);

  static PaymentAppManager* paymentAppManager(ScriptState*,
                                              ServiceWorkerRegistration&);
  PaymentAppManager* paymentAppManager(ScriptState*);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PaymentAppServiceWorkerRegistration(ServiceWorkerRegistration*);
  static const char* supplementName();

  Member<ServiceWorkerRegistration> m_registration;
  Member<PaymentAppManager> m_paymentAppManager;
};

}  // namespace blink

#endif  // PaymentAppServiceWorkerRegistration_h
