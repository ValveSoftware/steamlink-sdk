// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectTimingReadOnly_h
#define AnimationEffectTimingReadOnly_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "wtf/text/WTFString.h"

namespace blink {

class UnrestrictedDoubleOrString;

class CORE_EXPORT AnimationEffectTimingReadOnly
    : public GarbageCollected<AnimationEffectTimingReadOnly>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationEffectTimingReadOnly* create(AnimationEffectReadOnly* parent);
  double delay();
  double endDelay();
  String fill();
  double iterationStart();
  double iterations();
  void duration(UnrestrictedDoubleOrString&);
  double playbackRate();
  String direction();
  String easing();

  virtual bool isAnimationEffectTiming() const { return false; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  Member<AnimationEffectReadOnly> m_parent;
  explicit AnimationEffectTimingReadOnly(AnimationEffectReadOnly*);
};

}  // namespace blink

#endif
