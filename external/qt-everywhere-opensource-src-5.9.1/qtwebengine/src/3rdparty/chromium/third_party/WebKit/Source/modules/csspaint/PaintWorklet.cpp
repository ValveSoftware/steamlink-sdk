// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"

namespace blink {

// static
PaintWorklet* PaintWorklet::create(LocalFrame* frame) {
  PaintWorklet* worklet = new PaintWorklet(frame);
  worklet->suspendIfNeeded();
  return worklet;
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : Worklet(frame),
      m_paintWorkletGlobalScope(PaintWorkletGlobalScope::create(
          frame,
          frame->document()->url(),
          frame->document()->userAgent(),
          frame->document()->getSecurityOrigin(),
          toIsolate(frame->document()))) {}

PaintWorklet::~PaintWorklet() {}

PaintWorkletGlobalScope* PaintWorklet::workletGlobalScopeProxy() const {
  return m_paintWorkletGlobalScope.get();
}

CSSPaintDefinition* PaintWorklet::findDefinition(const String& name) {
  return m_paintWorkletGlobalScope->findDefinition(name);
}

void PaintWorklet::addPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  return m_paintWorkletGlobalScope->addPendingGenerator(name, generator);
}

DEFINE_TRACE(PaintWorklet) {
  visitor->trace(m_paintWorkletGlobalScope);
  Worklet::trace(visitor);
}

}  // namespace blink
