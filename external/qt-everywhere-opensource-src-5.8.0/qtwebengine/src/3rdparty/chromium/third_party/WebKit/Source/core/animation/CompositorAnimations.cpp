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

#include "core/animation/CompositorAnimations.h"

#include "core/animation/AnimationEffect.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/dom/DOMNodeIds.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/animation/AnimationTranslationUtil.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFilterKeyframe.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"
#include "platform/animation/CompositorTransformKeyframe.h"
#include "platform/geometry/FloatBox.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace blink {

namespace {

void getKeyframeValuesForProperty(const KeyframeEffectModelBase* effect, PropertyHandle property, double scale, PropertySpecificKeyframeVector& values)
{
    ASSERT(values.isEmpty());

    for (const auto& keyframe : effect->getPropertySpecificKeyframes(property)) {
        double offset = keyframe->offset() * scale;
        values.append(keyframe->cloneWithOffset(offset));
    }
}

bool considerAnimationAsIncompatible(const Animation& animation, const Animation& animationToAdd)
{
    if (&animation == &animationToAdd)
        return false;

    switch (animation.playStateInternal()) {
    case Animation::Idle:
        return false;
    case Animation::Pending:
    case Animation::Running:
        return true;
    case Animation::Paused:
    case Animation::Finished:
        return Animation::hasLowerPriority(&animationToAdd, &animation);
    default:
        NOTREACHED();
        return true;
    }
}

bool isTransformRelatedCSSProperty(const PropertyHandle property)
{
    return property.isCSSProperty()
        && (property.cssProperty() == CSSPropertyRotate
        || property.cssProperty() == CSSPropertyScale
        || property.cssProperty() == CSSPropertyTransform
        || property.cssProperty() == CSSPropertyTranslate);
}


bool isTransformRelatedAnimation(const Element& targetElement, const Animation* animation)
{
    return animation->affects(targetElement, CSSPropertyTransform)
        || animation->affects(targetElement, CSSPropertyRotate)
        || animation->affects(targetElement, CSSPropertyScale)
        || animation->affects(targetElement, CSSPropertyTranslate);
}

bool hasIncompatibleAnimations(const Element& targetElement, const Animation& animationToAdd, const EffectModel& effectToAdd)
{
    const bool affectsOpacity = effectToAdd.affects(PropertyHandle(CSSPropertyOpacity));
    const bool affectsTransform = effectToAdd.isTransformRelatedEffect();
    const bool affectsFilter = effectToAdd.affects(PropertyHandle(CSSPropertyFilter));
    const bool affectsBackdropFilter = effectToAdd.affects(PropertyHandle(CSSPropertyBackdropFilter));

    if (!targetElement.hasAnimations())
        return false;

    ElementAnimations* elementAnimations = targetElement.elementAnimations();
    ASSERT(elementAnimations);

    for (const auto& entry : elementAnimations->animations()) {
        const Animation* attachedAnimation = entry.key;
        if (!considerAnimationAsIncompatible(*attachedAnimation, animationToAdd))
            continue;

        if ((affectsOpacity && attachedAnimation->affects(targetElement, CSSPropertyOpacity))
            || (affectsTransform && isTransformRelatedAnimation(targetElement, attachedAnimation))
            || (affectsFilter && attachedAnimation->affects(targetElement, CSSPropertyFilter))
            || (affectsBackdropFilter && attachedAnimation->affects(targetElement, CSSPropertyBackdropFilter)))
            return true;
    }

    return false;
}

} // namespace

bool CompositorAnimations::isCompositableProperty(CSSPropertyID property)
{
    for (CSSPropertyID id : compositableProperties) {
        if (property == id)
            return true;
    }
    return false;
}

const CSSPropertyID CompositorAnimations::compositableProperties[7] = {
    CSSPropertyOpacity,
    CSSPropertyRotate,
    CSSPropertyScale,
    CSSPropertyTransform,
    CSSPropertyTranslate,
    CSSPropertyFilter,
    CSSPropertyBackdropFilter
};

bool CompositorAnimations::getAnimatedBoundingBox(FloatBox& box, const EffectModel& effect, double minValue, double maxValue)
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertyHandleSet properties = keyframeEffect.properties();

    if (properties.isEmpty())
        return true;

    minValue = std::min(minValue, 0.0);
    maxValue = std::max(maxValue, 1.0);

    for (const auto& property : properties) {
        if (!property.isCSSProperty())
            continue;

        // TODO: Add the ability to get expanded bounds for filters as well.
        if (!isTransformRelatedCSSProperty(property))
            continue;

        const PropertySpecificKeyframeVector& frames = keyframeEffect.getPropertySpecificKeyframes(property);
        if (frames.isEmpty() || frames.size() < 2)
            continue;

        FloatBox originalBox(box);

        for (size_t j = 0; j < frames.size() - 1; ++j) {
            const AnimatableTransform* startTransform = toAnimatableTransform(frames[j]->getAnimatableValue().get());
            const AnimatableTransform* endTransform = toAnimatableTransform(frames[j+1]->getAnimatableValue().get());
            if (!startTransform || !endTransform)
                return false;

            // TODO: Add support for inflating modes other than Replace.
            if (frames[j]->composite() != EffectModel::CompositeReplace)
                return false;

            const TimingFunction& timing = frames[j]->easing();
            double min = 0;
            double max = 1;
            if (j == 0) {
                float frameLength = frames[j+1]->offset();
                if (frameLength > 0) {
                    min = minValue / frameLength;
                }
            }

            if (j == frames.size() - 2) {
                float frameLength = frames[j+1]->offset() - frames[j]->offset();
                if (frameLength > 0) {
                    max = 1 + (maxValue - 1) / frameLength;
                }
            }

            FloatBox bounds;
            timing.range(&min, &max);
            if (!endTransform->transformOperations().blendedBoundsForBox(originalBox, startTransform->transformOperations(), min, max, &bounds))
                return false;
            box.expandTo(bounds);
        }
    }
    return true;
}

// -----------------------------------------------------------------------
// CompositorAnimations public API
// -----------------------------------------------------------------------

bool CompositorAnimations::isCandidateForAnimationOnCompositor(const Timing& timing, const Element& targetElement, const Animation* animationToAdd, const EffectModel& effect, double animationPlaybackRate)
{
    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    PropertyHandleSet properties = keyframeEffect.properties();
    if (properties.isEmpty())
        return false;

    unsigned transformPropertyCount = 0;
    for (const auto& property : properties) {
        if (!property.isCSSProperty())
            return false;

        if (isTransformRelatedCSSProperty(property)) {
            if (targetElement.layoutObject() && targetElement.layoutObject()->isInline() && !targetElement.layoutObject()->isInlineBlockOrInlineTable()) {
                return false;
            }
            transformPropertyCount++;
        }

        const PropertySpecificKeyframeVector& keyframes = keyframeEffect.getPropertySpecificKeyframes(property);
        ASSERT(keyframes.size() >= 2);
        for (const auto& keyframe : keyframes) {
            // FIXME: Determine candidacy based on the CSSValue instead of a snapshot AnimatableValue.
            bool isNeutralKeyframe = keyframe->isCSSPropertySpecificKeyframe() && !toCSSPropertySpecificKeyframe(keyframe.get())->value() && keyframe->composite() == EffectModel::CompositeAdd;
            if ((keyframe->composite() != EffectModel::CompositeReplace && !isNeutralKeyframe) || !keyframe->getAnimatableValue())
                return false;

            switch (property.cssProperty()) {
            case CSSPropertyOpacity:
                break;
            case CSSPropertyRotate:
            case CSSPropertyScale:
            case CSSPropertyTranslate:
            case CSSPropertyTransform:
                if (toAnimatableTransform(keyframe->getAnimatableValue().get())->transformOperations().dependsOnBoxSize())
                    return false;
                break;
            case CSSPropertyFilter:
            case CSSPropertyBackdropFilter: {
                const FilterOperations& operations = toAnimatableFilterOperations(keyframe->getAnimatableValue().get())->operations();
                if (operations.hasFilterThatMovesPixels())
                    return false;
                break;
            }
            default:
                // any other types are not allowed to run on compositor.
                return false;
            }
        }
    }

    // TODO: Support multiple transform property animations on the compositor
    if (transformPropertyCount > 1)
        return false;

    if (animationToAdd && hasIncompatibleAnimations(targetElement, *animationToAdd, effect))
        return false;

    CompositorTiming out;
    if (!convertTimingForCompositor(timing, 0, out, animationPlaybackRate))
        return false;

    return true;
}

void CompositorAnimations::cancelIncompatibleAnimationsOnCompositor(const Element& targetElement, const Animation& animationToAdd, const EffectModel& effectToAdd)
{
    const bool affectsOpacity = effectToAdd.affects(PropertyHandle(CSSPropertyOpacity));
    const bool affectsTransform = effectToAdd.isTransformRelatedEffect();
    const bool affectsFilter = effectToAdd.affects(PropertyHandle(CSSPropertyFilter));
    const bool affectsBackdropFilter = effectToAdd.affects(PropertyHandle(CSSPropertyBackdropFilter));

    if (!targetElement.hasAnimations())
        return;

    ElementAnimations* elementAnimations = targetElement.elementAnimations();
    ASSERT(elementAnimations);

    for (const auto& entry : elementAnimations->animations()) {
        Animation* attachedAnimation = entry.key;
        if (!considerAnimationAsIncompatible(*attachedAnimation, animationToAdd))
            continue;

        if ((affectsOpacity && attachedAnimation->affects(targetElement, CSSPropertyOpacity))
            || (affectsTransform && isTransformRelatedAnimation(targetElement, attachedAnimation))
            || (affectsFilter && attachedAnimation->affects(targetElement, CSSPropertyFilter))
            || (affectsBackdropFilter && attachedAnimation->affects(targetElement, CSSPropertyBackdropFilter)))
            attachedAnimation->cancelAnimationOnCompositor();
    }
}

bool CompositorAnimations::canStartAnimationOnCompositor(const Element& element)
{
    if (!Platform::current()->isThreadedAnimationEnabled())
        return false;
    return element.layoutObject() && element.layoutObject()->compositingState() == PaintsIntoOwnBacking;
}

void CompositorAnimations::startAnimationOnCompositor(const Element& element, int group, double startTime, double timeOffset, const Timing& timing, const Animation& animation, const EffectModel& effect, Vector<int>& startedAnimationIds, double animationPlaybackRate)
{
    ASSERT(startedAnimationIds.isEmpty());
    ASSERT(isCandidateForAnimationOnCompositor(timing, element, &animation, effect, animationPlaybackRate));
    ASSERT(canStartAnimationOnCompositor(element));

    const KeyframeEffectModelBase& keyframeEffect = toKeyframeEffectModelBase(effect);

    Vector<std::unique_ptr<CompositorAnimation>> animations;
    getAnimationOnCompositor(timing, group, startTime, timeOffset, keyframeEffect, animations, animationPlaybackRate);
    ASSERT(!animations.isEmpty());
    for (auto& compositorAnimation : animations) {
        int id = compositorAnimation->id();
        CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
        ASSERT(compositorPlayer);
        compositorPlayer->addAnimation(compositorAnimation.release());
        startedAnimationIds.append(id);
    }
    ASSERT(!startedAnimationIds.isEmpty());
}

void CompositorAnimations::cancelAnimationOnCompositor(const Element& element, const Animation& animation, int id)
{
    if (!canStartAnimationOnCompositor(element)) {
        // When an element is being detached, we cancel any associated
        // Animations for CSS animations. But by the time we get
        // here the mapping will have been removed.
        // FIXME: Defer remove/pause operations until after the
        // compositing update.
        return;
    }
    CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
    if (compositorPlayer)
        compositorPlayer->removeAnimation(id);
}

void CompositorAnimations::pauseAnimationForTestingOnCompositor(const Element& element, const Animation& animation, int id, double pauseTime)
{
    // FIXME: canStartAnimationOnCompositor queries compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    if (!canStartAnimationOnCompositor(element)) {
        NOTREACHED();
        return;
    }
    CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
    ASSERT(compositorPlayer);
    compositorPlayer->pauseAnimation(id, pauseTime);
}

void CompositorAnimations::attachCompositedLayers(Element& element, const Animation& animation)
{
    if (!animation.compositorPlayer())
        return;

    if (!element.layoutObject() || !element.layoutObject()->isBoxModelObject())
        return;

    PaintLayer* layer = toLayoutBoxModelObject(element.layoutObject())->layer();

    if (!layer || !layer->isAllowedToQueryCompositingState()
        || !layer->compositedLayerMapping()
        || !layer->compositedLayerMapping()->mainGraphicsLayer())
        return;

    if (!layer->compositedLayerMapping()->mainGraphicsLayer()->platformLayer())
        return;

    CompositorAnimationPlayer* compositorPlayer = animation.compositorPlayer();
    compositorPlayer->attachElement(createCompositorElementId(DOMNodeIds::idForNode(&element), CompositorSubElementId::Primary));
}

bool CompositorAnimations::convertTimingForCompositor(const Timing& timing, double timeOffset, CompositorTiming& out, double animationPlaybackRate)
{
    timing.assertValid();

    // FIXME: Compositor does not know anything about endDelay.
    if (timing.endDelay != 0)
        return false;

    if (std::isnan(timing.iterationDuration) || !timing.iterationCount || !timing.iterationDuration)
        return false;

    out.adjustedIterationCount = std::isfinite(timing.iterationCount) ? timing.iterationCount : -1;
    out.scaledDuration = timing.iterationDuration;
    out.direction = timing.direction;
    // Compositor's time offset is positive for seeking into the animation.
    out.scaledTimeOffset = -timing.startDelay / animationPlaybackRate + timeOffset;
    out.playbackRate = timing.playbackRate * animationPlaybackRate;
    out.fillMode = timing.fillMode == Timing::FillModeAuto ? Timing::FillModeNone : timing.fillMode;
    out.iterationStart = timing.iterationStart;

    DCHECK_GT(out.scaledDuration, 0);
    DCHECK(std::isfinite(out.scaledTimeOffset));
    DCHECK(out.adjustedIterationCount > 0 || out.adjustedIterationCount == -1);
    DCHECK(std::isfinite(out.playbackRate) && out.playbackRate);
    DCHECK_GE(out.iterationStart, 0);

    return true;
}

namespace {

template<typename PlatformAnimationCurveType, typename PlatformAnimationKeyframeType>
void addCompositorKeyframeWithTimingFunction(PlatformAnimationCurveType& curve, const PlatformAnimationKeyframeType& keyframe, const TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.addCubicBezierKeyframe(keyframe, CubicBezierTimingFunction::EaseType::EASE);
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::kLinearFunction:
        curve.addLinearKeyframe(keyframe);
        break;

    case TimingFunction::kCubicBezierFunction: {
        const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(*timingFunction);
        if (cubic.getEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM)
            curve.addCubicBezierKeyframe(keyframe, cubic.x1(), cubic.y1(), cubic.x2(), cubic.y2());
        else
            curve.addCubicBezierKeyframe(keyframe, cubic.getEaseType());
        break;
    }

    case TimingFunction::kStepsFunction: {
        const StepsTimingFunction& steps = toStepsTimingFunction(*timingFunction);
        curve.addStepsKeyframe(keyframe, steps.numberOfSteps(), steps.getStepPosition());
        break;
    }

    default:
        NOTREACHED();
    }
}

template <typename PlatformAnimationCurveType>
void setTimingFunctionOnCurve(PlatformAnimationCurveType& curve, TimingFunction* timingFunction)
{
    if (!timingFunction) {
        curve.setLinearTimingFunction();
        return;
    }

    switch (timingFunction->type()) {
    case TimingFunction::kLinearFunction:
        curve.setLinearTimingFunction();
        break;

    case TimingFunction::kCubicBezierFunction: {
        const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(*timingFunction);
        if (cubic.getEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM)
            curve.setCubicBezierTimingFunction(cubic.x1(), cubic.y1(), cubic.x2(), cubic.y2());
        else
            curve.setCubicBezierTimingFunction(cubic.getEaseType());
        break;
    }

    case TimingFunction::kStepsFunction: {
        const StepsTimingFunction& steps = toStepsTimingFunction(*timingFunction);
        curve.setStepsTimingFunction(steps.numberOfSteps(), steps.getStepPosition());
        break;
    }

    default:
        NOTREACHED();
    }
}

void addKeyframeToCurve(CompositorFilterAnimationCurve& curve, Keyframe::PropertySpecificKeyframe* keyframe,
    const AnimatableValue* value, const TimingFunction* keyframeTimingFunction)
{
    std::unique_ptr<CompositorFilterOperations> ops = CompositorFilterOperations::create();
    toCompositorFilterOperations(toAnimatableFilterOperations(value)->operations(), ops.get());

    CompositorFilterKeyframe filterKeyframe(keyframe->offset(), std::move(ops));
    addCompositorKeyframeWithTimingFunction(curve, filterKeyframe, keyframeTimingFunction);
}

void addKeyframeToCurve(CompositorFloatAnimationCurve& curve, Keyframe::PropertySpecificKeyframe* keyframe,
    const AnimatableValue* value, const TimingFunction* keyframeTimingFunction)
{
    CompositorFloatKeyframe floatKeyframe(keyframe->offset(), toAnimatableDouble(value)->toDouble());
    addCompositorKeyframeWithTimingFunction(curve, floatKeyframe, keyframeTimingFunction);
}

void addKeyframeToCurve(CompositorTransformAnimationCurve& curve, Keyframe::PropertySpecificKeyframe* keyframe,
    const AnimatableValue* value, const TimingFunction* keyframeTimingFunction)
{
    std::unique_ptr<CompositorTransformOperations> ops = CompositorTransformOperations::create();
    toCompositorTransformOperations(toAnimatableTransform(value)->transformOperations(), ops.get());

    CompositorTransformKeyframe transformKeyframe(keyframe->offset(), std::move(ops));
    addCompositorKeyframeWithTimingFunction(curve, transformKeyframe, keyframeTimingFunction);
}

template <typename PlatformAnimationCurveType>
void addKeyframesToCurve(PlatformAnimationCurveType& curve, const AnimatableValuePropertySpecificKeyframeVector& keyframes)
{
    auto* lastKeyframe = keyframes.last().get();
    for (const auto& keyframe : keyframes) {
        const TimingFunction* keyframeTimingFunction = 0;
        if (keyframe != lastKeyframe) { // Ignore timing function of last frame.
            keyframeTimingFunction = &keyframe->easing();
        }

        // FIXME: This relies on StringKeyframes being eagerly evaluated, which will
        // not happen eventually. Instead we should extract the CSSValue here
        // and convert using another set of toAnimatableXXXOperations functions.
        const AnimatableValue* value = keyframe->getAnimatableValue().get();

        addKeyframeToCurve(curve, keyframe.get(), value, keyframeTimingFunction);
    }
}

} // namespace

void CompositorAnimations::getAnimationOnCompositor(const Timing& timing, int group, double startTime, double timeOffset, const KeyframeEffectModelBase& effect, Vector<std::unique_ptr<CompositorAnimation>>& animations, double animationPlaybackRate)
{
    ASSERT(animations.isEmpty());
    CompositorTiming compositorTiming;
    bool timingValid = convertTimingForCompositor(timing, timeOffset, compositorTiming, animationPlaybackRate);
    ASSERT_UNUSED(timingValid, timingValid);

    PropertyHandleSet properties = effect.properties();
    ASSERT(!properties.isEmpty());
    for (const auto& property : properties) {
        PropertySpecificKeyframeVector values;
        // If the animation duration is infinite, it doesn't make sense to scale
        // the keyframe offset, so use a scale of 1.0. This is connected to
        // the known issue of how the Web Animations spec handles infinite
        // durations. See https://github.com/w3c/web-animations/issues/142
        double scale = compositorTiming.scaledDuration;
        if (!std::isfinite(scale))
            scale = 1.0;
        getKeyframeValuesForProperty(&effect, property, scale, values);

        CompositorTargetProperty::Type targetProperty;
        std::unique_ptr<CompositorAnimationCurve> curve;
        switch (property.cssProperty()) {
        case CSSPropertyOpacity: {
            targetProperty = CompositorTargetProperty::OPACITY;
            std::unique_ptr<CompositorFloatAnimationCurve> floatCurve = CompositorFloatAnimationCurve::create();
            addKeyframesToCurve(*floatCurve, values);
            setTimingFunctionOnCurve(*floatCurve, timing.timingFunction.get());
            curve = std::move(floatCurve);
            break;
        }
        case CSSPropertyFilter:
        case CSSPropertyBackdropFilter: {
            targetProperty = CompositorTargetProperty::FILTER;
            std::unique_ptr<CompositorFilterAnimationCurve> filterCurve = CompositorFilterAnimationCurve::create();
            addKeyframesToCurve(*filterCurve, values);
            setTimingFunctionOnCurve(*filterCurve, timing.timingFunction.get());
            curve = std::move(filterCurve);
            break;
        }
        case CSSPropertyRotate:
        case CSSPropertyScale:
        case CSSPropertyTranslate:
        case CSSPropertyTransform: {
            targetProperty = CompositorTargetProperty::TRANSFORM;
            std::unique_ptr<CompositorTransformAnimationCurve> transformCurve = CompositorTransformAnimationCurve::create();
            addKeyframesToCurve(*transformCurve, values);
            setTimingFunctionOnCurve(*transformCurve, timing.timingFunction.get());
            curve = std::move(transformCurve);
            break;
        }
        default:
            NOTREACHED();
            continue;
        }
        ASSERT(curve.get());

        std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(*curve, targetProperty, group, 0);

        if (!std::isnan(startTime))
            animation->setStartTime(startTime);

        animation->setIterations(compositorTiming.adjustedIterationCount);
        animation->setIterationStart(compositorTiming.iterationStart);
        animation->setTimeOffset(compositorTiming.scaledTimeOffset);

        switch (compositorTiming.direction) {
        case Timing::PlaybackDirectionNormal:
            animation->setDirection(CompositorAnimation::Direction::NORMAL);
            break;
        case Timing::PlaybackDirectionReverse:
            animation->setDirection(CompositorAnimation::Direction::REVERSE);
            break;
        case Timing::PlaybackDirectionAlternate:
            animation->setDirection(CompositorAnimation::Direction::ALTERNATE_NORMAL);
            break;
        case Timing::PlaybackDirectionAlternateReverse:
            animation->setDirection(CompositorAnimation::Direction::ALTERNATE_REVERSE);
            break;
        default:
            NOTREACHED();
        }
        animation->setPlaybackRate(compositorTiming.playbackRate);

        switch (compositorTiming.fillMode) {
        case Timing::FillModeNone:
            animation->setFillMode(CompositorAnimation::FillMode::NONE);
            break;
        case Timing::FillModeForwards:
            animation->setFillMode(CompositorAnimation::FillMode::FORWARDS);
            break;
        case Timing::FillModeBackwards:
            animation->setFillMode(CompositorAnimation::FillMode::BACKWARDS);
            break;
        case Timing::FillModeBoth:
            animation->setFillMode(CompositorAnimation::FillMode::BOTH);
            break;
        default:
            NOTREACHED();
        }
        animations.append(std::move(animation));
    }
    ASSERT(!animations.isEmpty());
}

} // namespace blink
