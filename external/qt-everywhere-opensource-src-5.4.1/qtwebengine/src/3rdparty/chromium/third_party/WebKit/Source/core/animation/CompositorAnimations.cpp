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

#include "config.h"
#include "core/animation/CompositorAnimations.h"

#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimatableFilterOperations.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/AnimationTranslationUtil.h"
#include "core/animation/CompositorAnimationsImpl.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAnimation.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFilterAnimationCurve.h"
#include "public/platform/WebFilterKeyframe.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"
#include "public/platform/WebTransformAnimationCurve.h"
#include "public/platform/WebTransformKeyframe.h"

#include <algorithm>
#include <cmath>

namespace WebCore {

namespace {

void getKeyframeValuesForProperty(const KeyframeEffectModelBase* effect, CSSPropertyID id, double scale, bool reverse, PropertySpecificKeyframeVector& values)
{
    ASSERT(values.isEmpty());
    const PropertySpecificKeyframeVector& group = effect->getPropertySpecificKeyframes(id);

    if (reverse) {
        for (size_t i = group.size(); i--;) {
            double offset = (1 - group[i]->offset()) * scale;
            values.append(group[i]->cloneWithOffset(offset));
        }
    } else {
        for (size_t i = 0; i < group.size(); ++i) {
            double offset = group[i]->offset() * scale;
            values.append(group[i]->cloneWithOffset(offset));
        }
    }
}

}

// -----------------------------------------------------------------------
// TimingFunctionReverser methods
// -----------------------------------------------------------------------

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const LinearTimingFunction* timefunc)
{
    return const_cast<LinearTimingFunction*>(timefunc);
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const CubicBezierTimingFunction* timefunc)
{
    switch (timefunc->subType()) {
    case CubicBezierTimingFunction::EaseIn:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
    case CubicBezierTimingFunction::EaseOut:
        return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
    case CubicBezierTimingFunction::EaseInOut:
        return const_cast<CubicBezierTimingFunction*>(timefunc);
    case CubicBezierTimingFunction::Ease: // Ease is not symmetrical
    case CubicBezierTimingFunction::Custom:
        return CubicBezierTimingFunction::create(1 - timefunc->x2(), 1 - timefunc->y2(), 1 - timefunc->x1(), 1 - timefunc->y1());
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

PassRefPtr<TimingFunction> CompositorAnimationsTimingFunctionReverser::reverse(const TimingFunction* timefunc)
{
    switch (timefunc->type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction* linear = toLinearTimingFunction(timefunc);
        return reverse(linear);
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = toCubicBezierTimingFunction(timefunc);
        return reverse(cubic);
    }

    // Steps function can not be reversed.
    case TimingFunction::StepsFunction:
    default:
        ASSERT_NOT_REACHED();
        return PassRefPtr<TimingFunction>();
    }
}

// -----------------------------------------------------------------------
// CompositorAnimations public API
// -----------------------------------------------------------------------

bool CompositorAnimations::isCandidateForAnimationOnCompositor(const Timing& timing, const AnimationEffect& effect)
{
    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(&effect);

    PropertySet properties = keyframeEffect.properties();

    if (properties.isEmpty())
        return false;

    for (PropertySet::const_iterator it = properties.begin(); it != properties.end(); ++it) {
        const PropertySpecificKeyframeVector& frames = keyframeEffect.getPropertySpecificKeyframes(*it);
        ASSERT(frames.size() >= 2);
        for (size_t i = 0; i < frames.size(); ++i) {
            const Keyframe::PropertySpecificKeyframe *frame = frames[i].get();
            // FIXME: Determine candidacy based on the CSSValue instead of a snapshot AnimatableValue.
            if (frame->composite() != AnimationEffect::CompositeReplace || !frame->getAnimatableValue())
                return false;

            switch (*it) {
            case CSSPropertyOpacity:
                break;
            case CSSPropertyTransform:
                if (toAnimatableTransform(frame->getAnimatableValue().get())->transformOperations().dependsOnBoxSize())
                    return false;
                break;
            case CSSPropertyWebkitFilter: {
                const FilterOperations& operations = toAnimatableFilterOperations(frame->getAnimatableValue().get())->operations();
                if (operations.hasFilterThatMovesPixels())
                    return false;
                break;
            }
            default:
                return false;
            }

            // FIXME: Remove this check when crbug.com/229405 is resolved
            if (i < frames.size() - 1 && frame->easing()->type() == TimingFunction::StepsFunction)
                return false;
        }
    }

    CompositorAnimationsImpl::CompositorTiming out;
    if (!CompositorAnimationsImpl::convertTimingForCompositor(timing, out))
        return false;

    // FIXME: We should support non-linear timing functions in the compositor
    // eventually.
    if (timing.timingFunction->type() != TimingFunction::LinearFunction)
        return false;

    return true;
}

bool CompositorAnimations::canStartAnimationOnCompositor(const Element& element)
{
    return element.renderer() && element.renderer()->compositingState() == PaintsIntoOwnBacking;
}

bool CompositorAnimations::startAnimationOnCompositor(const Element& element, double startTime, const Timing& timing, const AnimationEffect& effect, Vector<int>& startedAnimationIds)
{
    ASSERT(startedAnimationIds.isEmpty());
    ASSERT(isCandidateForAnimationOnCompositor(timing, effect));
    ASSERT(canStartAnimationOnCompositor(element));

    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(&effect);

    RenderLayer* layer = toRenderBoxModelObject(element.renderer())->layer();
    ASSERT(layer);

    Vector<OwnPtr<blink::WebAnimation> > animations;
    CompositorAnimationsImpl::getAnimationOnCompositor(timing, startTime, keyframeEffect, animations);
    ASSERT(!animations.isEmpty());
    for (size_t i = 0; i < animations.size(); ++i) {
        int id = animations[i]->id();
        if (!layer->compositedLayerMapping()->mainGraphicsLayer()->addAnimation(animations[i].release())) {
            // FIXME: We should know ahead of time whether these animations can be started.
            for (size_t j = 0; j < startedAnimationIds.size(); ++j)
                cancelAnimationOnCompositor(element, startedAnimationIds[j]);
            startedAnimationIds.clear();
            return false;
        }
        startedAnimationIds.append(id);
    }
    ASSERT(!startedAnimationIds.isEmpty());
    return true;
}

void CompositorAnimations::cancelAnimationOnCompositor(const Element& element, int id)
{
    if (!canStartAnimationOnCompositor(element)) {
        // When an element is being detached, we cancel any associated
        // AnimationPlayers for CSS animations. But by the time we get
        // here the mapping will have been removed.
        // FIXME: Defer remove/pause operations until after the
        // compositing update.
        return;
    }
    toRenderBoxModelObject(element.renderer())->layer()->compositedLayerMapping()->mainGraphicsLayer()->removeAnimation(id);
}

void CompositorAnimations::pauseAnimationForTestingOnCompositor(const Element& element, int id, double pauseTime)
{
    // FIXME: canStartAnimationOnCompositor queries compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    if (!canStartAnimationOnCompositor(element)) {
        ASSERT_NOT_REACHED();
        return;
    }
    toRenderBoxModelObject(element.renderer())->layer()->compositedLayerMapping()->mainGraphicsLayer()->pauseAnimation(id, pauseTime);
}

// -----------------------------------------------------------------------
// CompositorAnimationsImpl
// -----------------------------------------------------------------------

bool CompositorAnimationsImpl::convertTimingForCompositor(const Timing& timing, CompositorTiming& out)
{
    timing.assertValid();

    // All fill modes are supported (the calling code handles them).

    // FIXME: Support non-zero iteration start.
    if (timing.iterationStart)
        return false;

    // FIXME: Compositor only supports positive, integer iteration counts.
    // Zero iterations could be converted, but silly.
    if ((std::floor(timing.iterationCount) != timing.iterationCount) || timing.iterationCount <= 0)
        return false;

    if (std::isnan(timing.iterationDuration) || !timing.iterationDuration)
        return false;

    // FIXME: Support other playback rates
    if (timing.playbackRate != 1)
        return false;

    // All directions are supported.

    // Now attempt an actual conversion
    out.scaledDuration = timing.iterationDuration;
    ASSERT(out.scaledDuration > 0);

    double scaledStartDelay = timing.startDelay;
    if (scaledStartDelay > 0 && scaledStartDelay > out.scaledDuration * timing.iterationCount)
        return false;

    out.reverse = (timing.direction == Timing::PlaybackDirectionReverse
        || timing.direction == Timing::PlaybackDirectionAlternateReverse);
    out.alternate = (timing.direction == Timing::PlaybackDirectionAlternate
        || timing.direction == Timing::PlaybackDirectionAlternateReverse);

    if (!std::isfinite(timing.iterationCount)) {
        out.adjustedIterationCount = -1;
    } else {
        out.adjustedIterationCount = std::floor(timing.iterationCount);
        ASSERT(out.adjustedIterationCount > 0);
    }

    // Compositor's time offset is positive for seeking into the animation.
    out.scaledTimeOffset = -scaledStartDelay;
    return true;
}

namespace {

template<typename PlatformAnimationCurveType, typename PlatformAnimationKeyframeType>
void addKeyframeWithTimingFunction(PlatformAnimationCurveType& curve, const PlatformAnimationKeyframeType& keyframe, const TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.add(keyframe);
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::LinearFunction:
        curve.add(keyframe, blink::WebAnimationCurve::TimingFunctionTypeLinear);
        return;

    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* cubic = toCubicBezierTimingFunction(timingFunction);

        if (cubic->subType() == CubicBezierTimingFunction::Custom) {
            curve.add(keyframe, cubic->x1(), cubic->y1(), cubic->x2(), cubic->y2());
        } else {

            blink::WebAnimationCurve::TimingFunctionType easeType;
            switch (cubic->subType()) {
            case CubicBezierTimingFunction::Ease:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEase;
                break;
            case CubicBezierTimingFunction::EaseIn:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseIn;
                break;
            case CubicBezierTimingFunction::EaseOut:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseOut;
                break;
            case CubicBezierTimingFunction::EaseInOut:
                easeType = blink::WebAnimationCurve::TimingFunctionTypeEaseInOut;
                break;

            // Custom Bezier are handled seperately.
            case CubicBezierTimingFunction::Custom:
            default:
                ASSERT_NOT_REACHED();
                return;
            }

            curve.add(keyframe, easeType);
        }
        return;
    }

    case TimingFunction::StepsFunction:
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace anoymous

void CompositorAnimationsImpl::addKeyframesToCurve(blink::WebAnimationCurve& curve, const PropertySpecificKeyframeVector& keyframes, bool reverse)
{
    for (size_t i = 0; i < keyframes.size(); i++) {
        RefPtr<TimingFunction> reversedTimingFunction;
        const TimingFunction* keyframeTimingFunction = 0;
        if (i < keyframes.size() - 1) { // Ignore timing function of last frame.
            if (reverse) {
                reversedTimingFunction = CompositorAnimationsTimingFunctionReverser::reverse(keyframes[i + 1]->easing());
                keyframeTimingFunction = reversedTimingFunction.get();
            } else {
                keyframeTimingFunction = keyframes[i]->easing();
            }
        }

        // FIXME: This relies on StringKeyframes being eagerly evaluated, which will
        // not happen eventually. Instead we should extract the CSSValue here
        // and convert using another set of toAnimatableXXXOperations functions.
        const AnimatableValue* value = keyframes[i]->getAnimatableValue().get();

        switch (curve.type()) {
        case blink::WebAnimationCurve::AnimationCurveTypeFilter: {
            OwnPtr<blink::WebFilterOperations> ops = adoptPtr(blink::Platform::current()->compositorSupport()->createFilterOperations());
            bool converted = toWebFilterOperations(toAnimatableFilterOperations(value)->operations(), ops.get());
            ASSERT_UNUSED(converted, converted);

            blink::WebFilterKeyframe filterKeyframe(keyframes[i]->offset(), ops.release());
            blink::WebFilterAnimationCurve* filterCurve = static_cast<blink::WebFilterAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*filterCurve, filterKeyframe, keyframeTimingFunction);
            break;
        }
        case blink::WebAnimationCurve::AnimationCurveTypeFloat: {
            blink::WebFloatKeyframe floatKeyframe(keyframes[i]->offset(), toAnimatableDouble(value)->toDouble());
            blink::WebFloatAnimationCurve* floatCurve = static_cast<blink::WebFloatAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*floatCurve, floatKeyframe, keyframeTimingFunction);
            break;
        }
        case blink::WebAnimationCurve::AnimationCurveTypeTransform: {
            OwnPtr<blink::WebTransformOperations> ops = adoptPtr(blink::Platform::current()->compositorSupport()->createTransformOperations());
            toWebTransformOperations(toAnimatableTransform(value)->transformOperations(), ops.get());

            blink::WebTransformKeyframe transformKeyframe(keyframes[i]->offset(), ops.release());
            blink::WebTransformAnimationCurve* transformCurve = static_cast<blink::WebTransformAnimationCurve*>(&curve);
            addKeyframeWithTimingFunction(*transformCurve, transformKeyframe, keyframeTimingFunction);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void CompositorAnimationsImpl::getAnimationOnCompositor(const Timing& timing, double startTime, const KeyframeEffectModelBase& effect, Vector<OwnPtr<blink::WebAnimation> >& animations)
{
    ASSERT(animations.isEmpty());
    CompositorTiming compositorTiming;
    bool timingValid = convertTimingForCompositor(timing, compositorTiming);
    ASSERT_UNUSED(timingValid, timingValid);

    PropertySet properties = effect.properties();
    ASSERT(!properties.isEmpty());
    for (PropertySet::iterator it = properties.begin(); it != properties.end(); ++it) {

        PropertySpecificKeyframeVector values;
        getKeyframeValuesForProperty(&effect, *it, compositorTiming.scaledDuration, compositorTiming.reverse, values);

        blink::WebAnimation::TargetProperty targetProperty;
        OwnPtr<blink::WebAnimationCurve> curve;
        switch (*it) {
        case CSSPropertyOpacity: {
            targetProperty = blink::WebAnimation::TargetPropertyOpacity;

            blink::WebFloatAnimationCurve* floatCurve = blink::Platform::current()->compositorSupport()->createFloatAnimationCurve();
            addKeyframesToCurve(*floatCurve, values, compositorTiming.reverse);
            curve = adoptPtr(floatCurve);
            break;
        }
        case CSSPropertyWebkitFilter: {
            targetProperty = blink::WebAnimation::TargetPropertyFilter;
            blink::WebFilterAnimationCurve* filterCurve = blink::Platform::current()->compositorSupport()->createFilterAnimationCurve();
            addKeyframesToCurve(*filterCurve, values, compositorTiming.reverse);
            curve = adoptPtr(filterCurve);
            break;
        }
        case CSSPropertyTransform: {
            targetProperty = blink::WebAnimation::TargetPropertyTransform;
            blink::WebTransformAnimationCurve* transformCurve = blink::Platform::current()->compositorSupport()->createTransformAnimationCurve();
            addKeyframesToCurve(*transformCurve, values, compositorTiming.reverse);
            curve = adoptPtr(transformCurve);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            continue;
        }
        ASSERT(curve.get());

        OwnPtr<blink::WebAnimation> animation = adoptPtr(blink::Platform::current()->compositorSupport()->createAnimation(*curve, targetProperty));

        if (!std::isnan(startTime))
            animation->setStartTime(startTime);

        animation->setIterations(compositorTiming.adjustedIterationCount);
        animation->setTimeOffset(compositorTiming.scaledTimeOffset);
        animation->setAlternatesDirection(compositorTiming.alternate);

        animations.append(animation.release());
    }
    ASSERT(!animations.isEmpty());
}

} // namespace WebCore
