// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowAnimationWorklet_h
#define WindowAnimationWorklet_h

#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class AnimationWorklet;
class DOMWindow;
class LocalDOMWindow;
class Worklet;

class MODULES_EXPORT WindowAnimationWorklet final
    : public GarbageCollected<WindowAnimationWorklet>,
      public Supplement<LocalDOMWindow>,
      public DOMWindowProperty {
  USING_GARBAGE_COLLECTED_MIXIN(WindowAnimationWorklet);

 public:
  static WindowAnimationWorklet& from(LocalDOMWindow&);
  static Worklet* animationWorklet(DOMWindow&);
  AnimationWorklet* animationWorklet();

  void frameDestroyed() override;

  DECLARE_TRACE();

 private:
  explicit WindowAnimationWorklet(LocalDOMWindow&);
  static const char* supplementName();

  Member<AnimationWorklet> m_animationWorklet;
};

}  // namespace blink

#endif  // WindowAnimationWorklet_h
