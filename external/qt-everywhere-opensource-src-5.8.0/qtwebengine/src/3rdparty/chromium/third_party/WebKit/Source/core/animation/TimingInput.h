// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimingInput_h
#define TimingInput_h

#include "core/CoreExport.h"
#include "core/animation/Timing.h"
#include "wtf/Allocator.h"

namespace blink {

class Dictionary;
class Document;
class ExceptionState;
class KeyframeEffectOptions;
class UnrestrictedDoubleOrString;

class CORE_EXPORT TimingInput {
    STATIC_ONLY(TimingInput);
public:
    static bool convert(const KeyframeEffectOptions& timingInput, Timing& timingOutput, Document*, ExceptionState&);
    static bool convert(double duration, Timing& timingOutput, ExceptionState&);

    static void setStartDelay(Timing&, double startDelay);
    static void setEndDelay(Timing&, double endDelay);
    static void setFillMode(Timing&, const String& fillMode);
    static bool setIterationStart(Timing&, double iterationStart, ExceptionState&);
    static bool setIterationCount(Timing&, double iterationCount, ExceptionState&);
    static bool setIterationDuration(Timing&, const UnrestrictedDoubleOrString&, ExceptionState&);
    static void setPlaybackRate(Timing&, double playbackRate);
    static void setPlaybackDirection(Timing&, const String& direction);
    static bool setTimingFunction(Timing&, const String& timingFunctionString, Document*, ExceptionState&);
};

} // namespace blink

#endif
