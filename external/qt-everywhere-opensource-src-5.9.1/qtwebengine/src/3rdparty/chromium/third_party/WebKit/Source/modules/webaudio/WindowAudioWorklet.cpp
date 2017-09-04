// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/WindowAudioWorklet.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/webaudio/AudioWorklet.h"

namespace blink {

WindowAudioWorklet::WindowAudioWorklet(LocalDOMWindow& window)
    : DOMWindowProperty(window.frame()) {}

const char* WindowAudioWorklet::supplementName() {
  return "WindowAudioWorklet";
}

// static
WindowAudioWorklet& WindowAudioWorklet::from(LocalDOMWindow& window) {
  WindowAudioWorklet* supplement = static_cast<WindowAudioWorklet*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new WindowAudioWorklet(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Worklet* WindowAudioWorklet::audioWorklet(DOMWindow& window) {
  return from(toLocalDOMWindow(window)).audioWorklet();
}

AudioWorklet* WindowAudioWorklet::audioWorklet() {
  if (!m_audioWorklet && frame())
    m_audioWorklet = AudioWorklet::create(frame());
  return m_audioWorklet.get();
}

DEFINE_TRACE(WindowAudioWorklet) {
  visitor->trace(m_audioWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
  DOMWindowProperty::trace(visitor);
}

}  // namespace blink
