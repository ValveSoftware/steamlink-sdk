// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectTiming_h
#define AnimationEffectTiming_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class UnrestrictedDoubleOrString;

class CORE_EXPORT AnimationEffectTiming : public AnimationEffectTimingReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationEffectTiming* create(AnimationEffectReadOnly* parent);

  void setDelay(double);
  void setEndDelay(double);
  void setFill(String);
  void setIterationStart(double, ExceptionState&);
  void setIterations(double, ExceptionState&);
  void setDuration(const UnrestrictedDoubleOrString&, ExceptionState&);
  void setPlaybackRate(double);
  void setDirection(String);
  void setEasing(String, ExceptionState&);

  bool isAnimationEffectTiming() const override { return true; }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AnimationEffectTiming(AnimationEffectReadOnly*);
};

DEFINE_TYPE_CASTS(AnimationEffectTiming,
                  AnimationEffectTimingReadOnly,
                  timing,
                  timing->isAnimationEffectTiming(),
                  timing.isAnimationEffectTiming());

}  // namespace blink

#endif
