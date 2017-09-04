// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletGlobalScope.h"

#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

AnimationWorkletGlobalScope* AnimationWorkletGlobalScope::create(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AnimationWorkletGlobalScope(
      url, userAgent, std::move(securityOrigin), isolate, thread);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(url,
                                 userAgent,
                                 std::move(securityOrigin),
                                 isolate,
                                 thread) {}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() {}

}  // namespace blink
