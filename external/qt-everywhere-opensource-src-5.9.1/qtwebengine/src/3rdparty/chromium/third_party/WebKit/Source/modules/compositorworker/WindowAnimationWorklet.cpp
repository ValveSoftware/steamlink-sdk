// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/WindowAnimationWorklet.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/compositorworker/AnimationWorklet.h"

namespace blink {

WindowAnimationWorklet::WindowAnimationWorklet(LocalDOMWindow& window)
    : DOMWindowProperty(window.frame()) {}

const char* WindowAnimationWorklet::supplementName() {
  return "WindowAnimationWorklet";
}

// static
WindowAnimationWorklet& WindowAnimationWorklet::from(LocalDOMWindow& window) {
  WindowAnimationWorklet* supplement = static_cast<WindowAnimationWorklet*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new WindowAnimationWorklet(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Worklet* WindowAnimationWorklet::animationWorklet(DOMWindow& window) {
  return from(toLocalDOMWindow(window)).animationWorklet();
}

AnimationWorklet* WindowAnimationWorklet::animationWorklet() {
  if (!m_animationWorklet && frame())
    m_animationWorklet = AnimationWorklet::create(frame());
  return m_animationWorklet.get();
}

void WindowAnimationWorklet::frameDestroyed() {
  m_animationWorklet.clear();
  DOMWindowProperty::frameDestroyed();
}

DEFINE_TRACE(WindowAnimationWorklet) {
  visitor->trace(m_animationWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
  DOMWindowProperty::trace(visitor);
}

}  // namespace blink
