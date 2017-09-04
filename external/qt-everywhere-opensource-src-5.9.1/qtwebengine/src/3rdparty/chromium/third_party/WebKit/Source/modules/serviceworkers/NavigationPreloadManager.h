// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigationPreloadManager_h
#define NavigationPreloadManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ServiceWorkerRegistration;

class NavigationPreloadManager final
    : public GarbageCollected<NavigationPreloadManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NavigationPreloadManager* create(
      ServiceWorkerRegistration* registration) {
    return new NavigationPreloadManager(registration);
  }

  ScriptPromise enable(ScriptState*);
  ScriptPromise disable(ScriptState*);
  ScriptPromise setHeaderValue(ScriptState*, const String& value);
  ScriptPromise getState(ScriptState*);

  DECLARE_TRACE();

 private:
  explicit NavigationPreloadManager(ServiceWorkerRegistration*);

  ScriptPromise setEnabled(bool enable, ScriptState*);

  Member<ServiceWorkerRegistration> m_registration;
};

}  // namespace blink

#endif  // NavigationPreloadManager_h
