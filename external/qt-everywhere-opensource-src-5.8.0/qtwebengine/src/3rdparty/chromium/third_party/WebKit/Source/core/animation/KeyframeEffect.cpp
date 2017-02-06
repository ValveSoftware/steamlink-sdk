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

#include "core/animation/KeyframeEffect.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/PropertyHandle.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/UseCounter.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGElement.h"

namespace blink {

KeyframeEffect* KeyframeEffect::create(Element* target, EffectModel* model, const Timing& timing, Priority priority, EventDelegate* eventDelegate)
{
    return new KeyframeEffect(target, model, timing, priority, eventDelegate);
}

KeyframeEffect* KeyframeEffect::create(ExecutionContext* executionContext, Element* element, const EffectModelOrDictionarySequenceOrDictionary& effectInput, double duration, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
    if (element)
        UseCounter::count(element->document(), UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
    Timing timing;
    if (!TimingInput::convert(duration, timing, exceptionState))
        return nullptr;
    return create(element, EffectInput::convert(element, effectInput, executionContext, exceptionState), timing);
}

KeyframeEffect* KeyframeEffect::create(ExecutionContext* executionContext, Element* element, const EffectModelOrDictionarySequenceOrDictionary& effectInput, const KeyframeEffectOptions& timingInput, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
    if (element)
        UseCounter::count(element->document(), UseCounter::AnimationConstructorKeyframeListEffectObjectTiming);
    Timing timing;
    Document* document = element ? &element->document() : nullptr;
    if (!TimingInput::convert(timingInput, timing, document, exceptionState))
        return nullptr;
    return create(element, EffectInput::convert(element, effectInput, executionContext, exceptionState), timing);
}

KeyframeEffect* KeyframeEffect::create(ExecutionContext* executionContext, Element* element, const EffectModelOrDictionarySequenceOrDictionary& effectInput, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsAPIEnabled());
    if (element)
        UseCounter::count(element->document(), UseCounter::AnimationConstructorKeyframeListEffectNoTiming);
    return create(element, EffectInput::convert(element, effectInput, executionContext, exceptionState), Timing());
}

KeyframeEffect::KeyframeEffect(Element* target, EffectModel* model, const Timing& timing, Priority priority, EventDelegate* eventDelegate)
    : AnimationEffect(timing, eventDelegate)
    , m_target(target)
    , m_model(model)
    , m_sampledEffect(nullptr)
    , m_priority(priority)
{
}

KeyframeEffect::~KeyframeEffect()
{
}

void KeyframeEffect::attach(Animation* animation)
{
    if (m_target) {
        m_target->ensureElementAnimations().animations().add(animation);
        m_target->setNeedsAnimationStyleRecalc();
        if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() && m_target->isSVGElement())
            toSVGElement(m_target)->setWebAnimationsPending();
    }
    AnimationEffect::attach(animation);
}

void KeyframeEffect::detach()
{
    if (m_target)
        m_target->elementAnimations()->animations().remove(animation());
    if (m_sampledEffect)
        clearEffects();
    AnimationEffect::detach();
}

void KeyframeEffect::specifiedTimingChanged()
{
    if (animation()) {
        // FIXME: Needs to consider groups when added.
        ASSERT(animation()->effect() == this);
        animation()->setCompositorPending(true);
    }
}

static AnimationStack& ensureAnimationStack(Element* element)
{
    return element->ensureElementAnimations().animationStack();
}

bool KeyframeEffect::hasMultipleTransformProperties() const
{
    if (!m_target->computedStyle())
        return false;

    unsigned transformPropertyCount = 0;
    if (m_target->computedStyle()->hasTransformOperations())
        transformPropertyCount++;
    if (m_target->computedStyle()->rotate())
        transformPropertyCount++;
    if (m_target->computedStyle()->scale())
        transformPropertyCount++;
    if (m_target->computedStyle()->translate())
        transformPropertyCount++;
    return transformPropertyCount > 1;
}

// Returns true if transform, translate, rotate or scale is composited
// and a motion path or other transform properties
// has been introduced on the element
bool KeyframeEffect::hasIncompatibleStyle()
{
    if (!m_target->computedStyle())
        return false;

    bool affectsTransform = animation()->affects(*m_target, CSSPropertyTransform)
        || animation()->affects(*m_target, CSSPropertyScale)
        || animation()->affects(*m_target, CSSPropertyRotate)
        || animation()->affects(*m_target, CSSPropertyTranslate);

    if (animation()->hasActiveAnimationsOnCompositor()) {
        if (m_target->computedStyle()->hasMotionPath() && affectsTransform)
            return true;
        return hasMultipleTransformProperties();
    }

    return false;
}

void KeyframeEffect::applyEffects()
{
    ASSERT(isInEffect());
    ASSERT(animation());
    if (!m_target || !m_model)
        return;

    if (hasIncompatibleStyle())
        animation()->cancelAnimationOnCompositor();

    double iteration = currentIteration();
    ASSERT(iteration >= 0);
    bool changed = false;
    if (m_sampledEffect) {
        changed = m_model->sample(clampTo<int>(iteration, 0), progress(), iterationDuration(), m_sampledEffect->mutableInterpolations());
    } else {
        Vector<RefPtr<Interpolation>> interpolations;
        m_model->sample(clampTo<int>(iteration, 0), progress(), iterationDuration(), interpolations);
        if (!interpolations.isEmpty()) {
            SampledEffect* sampledEffect = SampledEffect::create(this);
            sampledEffect->mutableInterpolations().swap(interpolations);
            m_sampledEffect = sampledEffect;
            ensureAnimationStack(m_target).add(sampledEffect);
            changed = true;
        } else {
            return;
        }
    }

    if (changed) {
        m_target->setNeedsAnimationStyleRecalc();
        if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() && m_target->isSVGElement())
            toSVGElement(*m_target).setWebAnimationsPending();
    }
}

void KeyframeEffect::clearEffects()
{
    ASSERT(animation());
    ASSERT(m_sampledEffect);

    m_sampledEffect->clear();
    m_sampledEffect = nullptr;
    restartAnimationOnCompositor();
    m_target->setNeedsAnimationStyleRecalc();
    if (RuntimeEnabledFeatures::webAnimationsSVGEnabled() && m_target->isSVGElement())
        toSVGElement(*m_target).clearWebAnimatedAttributes();
    invalidate();
}

void KeyframeEffect::updateChildrenAndEffects() const
{
    if (!m_model)
        return;
    ASSERT(animation());
    if (isInEffect() && !animation()->effectSuppressed())
        const_cast<KeyframeEffect*>(this)->applyEffects();
    else if (m_sampledEffect)
        const_cast<KeyframeEffect*>(this)->clearEffects();
}

double KeyframeEffect::calculateTimeToEffectChange(bool forwards, double localTime, double timeToNextIteration) const
{
    const double startTime = specifiedTiming().startDelay;
    const double endTimeMinusEndDelay = startTime + activeDurationInternal();
    const double endTime = endTimeMinusEndDelay + specifiedTiming().endDelay;
    const double afterTime = std::min(endTimeMinusEndDelay, endTime);

    switch (getPhase()) {
    case PhaseNone:
        return std::numeric_limits<double>::infinity();
    case PhaseBefore:
        ASSERT(startTime >= localTime);
        return forwards
            ? startTime - localTime
            : std::numeric_limits<double>::infinity();
    case PhaseActive:
        if (forwards) {
            // Need service to apply fill / fire events.
            const double timeToEnd = afterTime - localTime;
            if (requiresIterationEvents()) {
                return std::min(timeToEnd, timeToNextIteration);
            }
            return timeToEnd;
        }
        return 0;
    case PhaseAfter:
        ASSERT(localTime >= afterTime);
        // If this KeyframeEffect is still in effect then it will need to update
        // when its parent goes out of effect. We have no way of knowing when
        // that will be, however, so the parent will need to supply it.
        return forwards
            ? std::numeric_limits<double>::infinity()
            : localTime - afterTime;
    default:
        NOTREACHED();
        return std::numeric_limits<double>::infinity();
    }
}

void KeyframeEffect::notifySampledEffectRemovedFromAnimationStack()
{
    m_sampledEffect = nullptr;
}

bool KeyframeEffect::isCandidateForAnimationOnCompositor(double animationPlaybackRate) const
{
    // Do not put transforms on compositor if more than one of them are defined
    // in computed style because they need to be explicitly ordered
    if (!model()
        || !m_target
        || (m_target->computedStyle() && m_target->computedStyle()->hasMotionPath())
        || hasMultipleTransformProperties())
        return false;

    return CompositorAnimations::isCandidateForAnimationOnCompositor(specifiedTiming(), *m_target, animation(), *model(), animationPlaybackRate);
}

bool KeyframeEffect::maybeStartAnimationOnCompositor(int group, double startTime, double currentTime, double animationPlaybackRate)
{
    ASSERT(!hasActiveAnimationsOnCompositor());
    if (!isCandidateForAnimationOnCompositor(animationPlaybackRate))
        return false;
    if (!CompositorAnimations::canStartAnimationOnCompositor(*m_target))
        return false;
    CompositorAnimations::startAnimationOnCompositor(*m_target, group, startTime, currentTime, specifiedTiming(), *animation(), *model(), m_compositorAnimationIds, animationPlaybackRate);
    ASSERT(!m_compositorAnimationIds.isEmpty());
    return true;
}

bool KeyframeEffect::hasActiveAnimationsOnCompositor() const
{
    return !m_compositorAnimationIds.isEmpty();
}

bool KeyframeEffect::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    return hasActiveAnimationsOnCompositor() && affects(PropertyHandle(property));
}

bool KeyframeEffect::affects(PropertyHandle property) const
{
    return m_model && m_model->affects(property);
}

bool KeyframeEffect::cancelAnimationOnCompositor()
{
    // FIXME: cancelAnimationOnCompositor is called from withins style recalc.
    // This queries compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;
    if (!hasActiveAnimationsOnCompositor())
        return false;
    if (!m_target || !m_target->layoutObject())
        return false;
    ASSERT(animation());
    for (const auto& compositorAnimationId : m_compositorAnimationIds)
        CompositorAnimations::cancelAnimationOnCompositor(*m_target, *animation(), compositorAnimationId);
    m_compositorAnimationIds.clear();
    return true;
}

void KeyframeEffect::restartAnimationOnCompositor()
{
    if (cancelAnimationOnCompositor())
        animation()->setCompositorPending(true);
}

void KeyframeEffect::cancelIncompatibleAnimationsOnCompositor()
{
    if (m_target && animation() && model())
        CompositorAnimations::cancelIncompatibleAnimationsOnCompositor(*m_target, *animation(), *model());
}

void KeyframeEffect::pauseAnimationForTestingOnCompositor(double pauseTime)
{
    ASSERT(hasActiveAnimationsOnCompositor());
    if (!m_target || !m_target->layoutObject())
        return;
    ASSERT(animation());
    for (const auto& compositorAnimationId : m_compositorAnimationIds)
        CompositorAnimations::pauseAnimationForTestingOnCompositor(*m_target, *animation(), compositorAnimationId, pauseTime);
}

void KeyframeEffect::attachCompositedLayers()
{
    ASSERT(m_target);
    ASSERT(animation());
    CompositorAnimations::attachCompositedLayers(*m_target, *animation());
}

DEFINE_TRACE(KeyframeEffect)
{
    visitor->trace(m_target);
    visitor->trace(m_model);
    visitor->trace(m_sampledEffect);
    AnimationEffect::trace(visitor);
}

} // namespace blink
