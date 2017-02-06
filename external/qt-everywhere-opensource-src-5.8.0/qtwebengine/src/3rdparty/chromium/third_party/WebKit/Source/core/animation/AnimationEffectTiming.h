// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectTiming_h
#define AnimationEffectTiming_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/animation/AnimationEffect.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class UnrestrictedDoubleOrString;

class CORE_EXPORT AnimationEffectTiming : public GarbageCollected<AnimationEffectTiming>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static AnimationEffectTiming* create(AnimationEffect* parent);
    double delay();
    double endDelay();
    String fill();
    double iterationStart();
    double iterations();
    void duration(UnrestrictedDoubleOrString&);
    double playbackRate();
    String direction();
    String easing();

    void setDelay(double);
    void setEndDelay(double);
    void setFill(String);
    void setIterationStart(double, ExceptionState&);
    void setIterations(double, ExceptionState&);
    void setDuration(const UnrestrictedDoubleOrString&, ExceptionState&);
    void setPlaybackRate(double);
    void setDirection(String);
    void setEasing(String, ExceptionState&);

    DECLARE_TRACE();

private:
    Member<AnimationEffect> m_parent;
    explicit AnimationEffectTiming(AnimationEffect*);
};

} // namespace blink

#endif
