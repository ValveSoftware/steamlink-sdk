// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletGlobalScope.h"

#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

AudioWorkletGlobalScope* AudioWorkletGlobalScope::create(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AudioWorkletGlobalScope(url, userAgent, std::move(securityOrigin),
                                     isolate, thread);
}

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
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

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() {}

}  // namespace blink
