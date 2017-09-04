// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerWindowClientCallback_h
#define ServiceWorkerWindowClientCallback_h

#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"

namespace blink {

class ScriptPromiseResolver;

class NavigateClientCallback : public WebServiceWorkerClientCallbacks {
 public:
  explicit NavigateClientCallback(ScriptPromiseResolver* resolver)
      : m_resolver(resolver) {}

  void onSuccess(std::unique_ptr<WebServiceWorkerClientInfo>) override;
  void onError(const WebServiceWorkerError&) override;

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
  WTF_MAKE_NONCOPYABLE(NavigateClientCallback);
};

}  // namespace blink

#endif  // ServiceWorkerWindowClientCallback_h
