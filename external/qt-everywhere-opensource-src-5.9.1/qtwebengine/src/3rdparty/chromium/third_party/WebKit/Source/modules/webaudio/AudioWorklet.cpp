// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/ThreadedWorkletGlobalScopeProxy.h"

namespace blink {

// static
AudioWorklet* AudioWorklet::create(LocalFrame* frame) {
  AudioWorklet* worklet = new AudioWorklet(frame);
  worklet->suspendIfNeeded();
  return worklet;
}

AudioWorklet::AudioWorklet(LocalFrame* frame)
    : Worklet(frame),
      m_workletGlobalScopeProxy(new ThreadedWorkletGlobalScopeProxy()) {}

AudioWorklet::~AudioWorklet() {}

WorkletGlobalScopeProxy* AudioWorklet::workletGlobalScopeProxy() const {
  return m_workletGlobalScopeProxy.get();
}

DEFINE_TRACE(AudioWorklet) {
  Worklet::trace(visitor);
}

}  // namespace blink
