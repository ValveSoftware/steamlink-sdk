// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"

#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

AnimationWorkletMessagingProxy::AnimationWorkletMessagingProxy(
    ExecutionContext* executionContext)
    : ThreadedWorkletMessagingProxy(executionContext) {}

AnimationWorkletMessagingProxy::~AnimationWorkletMessagingProxy() {}

std::unique_ptr<WorkerThread>
AnimationWorkletMessagingProxy::createWorkerThread(double originTime) {
  return AnimationWorkletThread::create(loaderProxy(), workletObjectProxy());
}

}  // namespace blink
