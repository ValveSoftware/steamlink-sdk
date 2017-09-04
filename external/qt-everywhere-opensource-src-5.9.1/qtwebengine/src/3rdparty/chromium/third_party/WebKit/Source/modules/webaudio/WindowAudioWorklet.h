// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowAudioWorklet_h
#define WindowAudioWorklet_h

#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class AudioWorklet;
class DOMWindow;
class LocalDOMWindow;
class Worklet;

class MODULES_EXPORT WindowAudioWorklet final
    : public GarbageCollected<WindowAudioWorklet>,
      public Supplement<LocalDOMWindow>,
      public DOMWindowProperty {
  USING_GARBAGE_COLLECTED_MIXIN(WindowAudioWorklet);

 public:
  static WindowAudioWorklet& from(LocalDOMWindow&);
  static Worklet* audioWorklet(DOMWindow&);
  AudioWorklet* audioWorklet();

  DECLARE_TRACE();

 private:
  explicit WindowAudioWorklet(LocalDOMWindow&);
  static const char* supplementName();

  Member<AudioWorklet> m_audioWorklet;
};

}  // namespace blink

#endif  // WindowAudioWorklet_h
