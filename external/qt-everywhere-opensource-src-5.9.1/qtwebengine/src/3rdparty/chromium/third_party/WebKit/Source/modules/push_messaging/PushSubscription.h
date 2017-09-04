// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushSubscription_h
#define PushSubscription_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class PushSubscriptionOptions;
class ServiceWorkerRegistration;
class ScriptPromiseResolver;
class ScriptState;
struct WebPushSubscription;

class PushSubscription final
    : public GarbageCollectedFinalized<PushSubscription>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushSubscription* take(ScriptPromiseResolver*,
                                std::unique_ptr<WebPushSubscription>,
                                ServiceWorkerRegistration*);
  static void dispose(WebPushSubscription* subscriptionRaw);

  virtual ~PushSubscription();

  KURL endpoint() const { return m_endpoint; }

  PushSubscriptionOptions* options() const { return m_options.get(); }

  DOMArrayBuffer* getKey(const AtomicString& name) const;
  ScriptPromise unsubscribe(ScriptState*);

  ScriptValue toJSONForBinding(ScriptState*);

  DECLARE_TRACE();

 private:
  PushSubscription(const WebPushSubscription&, ServiceWorkerRegistration*);

  KURL m_endpoint;

  Member<PushSubscriptionOptions> m_options;

  Member<DOMArrayBuffer> m_p256dh;
  Member<DOMArrayBuffer> m_auth;

  Member<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

}  // namespace blink

#endif  // PushSubscription_h
