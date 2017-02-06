/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositorAnimations_h
#define CompositorAnimations_h

#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "core/animation/Timing.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class Animation;
class CompositorAnimation;
class Element;
class FloatBox;
class KeyframeEffectModelBase;

class CORE_EXPORT CompositorAnimations {
    STATIC_ONLY(CompositorAnimations);
public:
    static bool isCompositableProperty(CSSPropertyID);
    static const CSSPropertyID compositableProperties[7];

    static bool isCandidateForAnimationOnCompositor(const Timing&, const Element&, const Animation*, const EffectModel&, double animationPlaybackRate);
    static void cancelIncompatibleAnimationsOnCompositor(const Element&, const Animation&, const EffectModel&);
    static bool canStartAnimationOnCompositor(const Element&);
    static void startAnimationOnCompositor(const Element&, int group, double startTime, double timeOffset, const Timing&, const Animation&, const EffectModel&, Vector<int>& startedAnimationIds, double animationPlaybackRate);
    static void cancelAnimationOnCompositor(const Element&, const Animation&, int id);
    static void pauseAnimationForTestingOnCompositor(const Element&, const Animation&, int id, double pauseTime);

    static void attachCompositedLayers(Element&, const Animation&);

    static bool getAnimatedBoundingBox(FloatBox&, const EffectModel&, double minValue, double maxValue);

    struct CompositorTiming {
        Timing::PlaybackDirection direction;
        double scaledDuration;
        double scaledTimeOffset;
        double adjustedIterationCount;
        double playbackRate;
        Timing::FillMode fillMode;
        double iterationStart;
    };

    static bool convertTimingForCompositor(const Timing&, double timeOffset, CompositorTiming& out, double animationPlaybackRate);

    static void getAnimationOnCompositor(const Timing&, int group, double startTime, double timeOffset, const KeyframeEffectModelBase&, Vector<std::unique_ptr<CompositorAnimation>>& animations, double animationPlaybackRate);
};

} // namespace blink

#endif
