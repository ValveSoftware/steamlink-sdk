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

#include "core/animation/Animation.h"

#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleChangeReason.h"
#include "core/events/AnimationPlayerEvent.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

static unsigned nextSequenceNumber()
{
    static unsigned next = 0;
    return ++next;
}

}

Animation* Animation::create(AnimationEffect* effect, AnimationTimeline* timeline)
{
    if (!timeline) {
        // FIXME: Support creating animations without a timeline.
        return nullptr;
    }

    Animation* animation = new Animation(timeline->document()->contextDocument(), *timeline, effect);
    animation->suspendIfNeeded();

    if (timeline) {
        timeline->animationAttached(*animation);
        animation->attachCompositorTimeline();
    }

    return animation;
}

Animation::Animation(ExecutionContext* executionContext, AnimationTimeline& timeline, AnimationEffect* content)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , m_playState(Idle)
    , m_playbackRate(1)
    , m_startTime(nullValue())
    , m_holdTime(0)
    , m_sequenceNumber(nextSequenceNumber())
    , m_content(content)
    , m_timeline(&timeline)
    , m_paused(false)
    , m_held(false)
    , m_isPausedForTesting(false)
    , m_isCompositedAnimationDisabledForTesting(false)
    , m_outdated(false)
    , m_finished(true)
    , m_compositorState(nullptr)
    , m_compositorPending(false)
    , m_compositorGroup(0)
    , m_preFinalizerRegistered(false)
    , m_currentTimePending(false)
    , m_stateIsBeingUpdated(false)
    , m_effectSuppressed(false)
{
    if (m_content) {
        if (m_content->animation()) {
            m_content->animation()->cancel();
            m_content->animation()->setEffect(0);
        }
        m_content->attach(this);
    }
    InspectorInstrumentation::didCreateAnimation(m_timeline->document(), m_sequenceNumber);
}

Animation::~Animation()
{
    destroyCompositorPlayer();
}

void Animation::dispose()
{
    destroyCompositorPlayer();
    // If the AnimationTimeline and its Animation objects are
    // finalized by the same GC, we have to eagerly clear out
    // this Animation object's compositor player registration.
    ASSERT(!m_compositorPlayer);
}

double Animation::effectEnd() const
{
    return m_content ? m_content->endTimeInternal() : 0;
}

bool Animation::limited(double currentTime) const
{
    return (m_playbackRate < 0 && currentTime <= 0) || (m_playbackRate > 0 && currentTime >= effectEnd());
}

void Animation::setCurrentTime(double newCurrentTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playStateInternal() == Idle)
        m_paused = true;

    m_currentTimePending = false;
    m_playState = Unset;
    setCurrentTimeInternal(newCurrentTime / 1000, TimingUpdateOnDemand);

    if (calculatePlayState() == Finished)
        m_startTime = calculateStartTime(newCurrentTime);
}

void Animation::setCurrentTimeInternal(double newCurrentTime, TimingUpdateReason reason)
{
    ASSERT(std::isfinite(newCurrentTime));

    bool oldHeld = m_held;
    bool outdated = false;
    bool isLimited = limited(newCurrentTime);
    m_held = m_paused || !m_playbackRate || isLimited || std::isnan(m_startTime);
    if (m_held) {
        if (!oldHeld || m_holdTime != newCurrentTime)
            outdated = true;
        m_holdTime = newCurrentTime;
        if (m_paused || !m_playbackRate) {
            m_startTime = nullValue();
        } else if (isLimited && std::isnan(m_startTime) && reason == TimingUpdateForAnimationFrame) {
            m_startTime = calculateStartTime(newCurrentTime);
        }
    } else {
        m_holdTime = nullValue();
        m_startTime = calculateStartTime(newCurrentTime);
        m_finished = false;
        outdated = true;
    }

    if (outdated) {
        setOutdated();
    }
}

// Update timing to reflect updated animation clock due to tick
void Animation::updateCurrentTimingState(TimingUpdateReason reason)
{
    if (m_playState == Idle)
        return;
    if (m_held) {
        double newCurrentTime = m_holdTime;
        if (m_playState == Finished && !isNull(m_startTime) && m_timeline) {
            // Add hystersis due to floating point error accumulation
            if (!limited(calculateCurrentTime() + 0.001 * m_playbackRate)) {
                // The current time became unlimited, eg. due to a backwards
                // seek of the timeline.
                newCurrentTime = calculateCurrentTime();
            } else if (!limited(m_holdTime)) {
                // The hold time became unlimited, eg. due to the effect
                // becoming longer.
                newCurrentTime = clampTo<double>(calculateCurrentTime(), 0, effectEnd());
            }
        }
        setCurrentTimeInternal(newCurrentTime, reason);
    } else if (limited(calculateCurrentTime())) {
        m_held = true;
        m_holdTime = m_playbackRate < 0 ? 0 : effectEnd();
    }
}

double Animation::startTime(bool& isNull) const
{
    double result = startTime();
    isNull = std::isnan(result);
    return result;
}

double Animation::startTime() const
{
    return m_startTime * 1000;
}

double Animation::currentTime(bool& isNull)
{
    double result = currentTime();
    isNull = std::isnan(result);
    return result;
}

double Animation::currentTime()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playStateInternal() == Idle || (!m_held && !hasStartTime()))
        return std::numeric_limits<double>::quiet_NaN();

    return currentTimeInternal() * 1000;
}

double Animation::currentTimeInternal() const
{
    double result = m_held ? m_holdTime : calculateCurrentTime();
#if ENABLE(ASSERT)
    // We can't enforce this check during Unset due to other
    // assertions.
    if (m_playState != Unset) {
        const_cast<Animation*>(this)->updateCurrentTimingState(TimingUpdateOnDemand);
        ASSERT(result == (m_held ? m_holdTime : calculateCurrentTime()));
    }
#endif
    return result;
}

double Animation::unlimitedCurrentTimeInternal() const
{
#if ENABLE(ASSERT)
    currentTimeInternal();
#endif
    return playStateInternal() == Paused || isNull(m_startTime)
        ? currentTimeInternal()
        : calculateCurrentTime();
}

bool Animation::preCommit(int compositorGroup, bool startOnCompositor)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    bool softChange = m_compositorState && (paused() || m_compositorState->playbackRate != m_playbackRate);
    bool hardChange = m_compositorState && (m_compositorState->effectChanged || m_compositorState->startTime != m_startTime);

    // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
    // not a Cancel, but we can't communicate these to the compositor yet.

    bool changed = softChange || hardChange;
    bool shouldCancel = (!playing() && m_compositorState) || changed;
    bool shouldStart = playing() && (!m_compositorState || changed);

    if (startOnCompositor && shouldCancel && shouldStart && m_compositorState && m_compositorState->pendingAction == Start) {
        // Restarting but still waiting for a start time.
        return false;
    }

    if (shouldCancel) {
        cancelAnimationOnCompositor();
        m_compositorState = nullptr;
    }

    ASSERT(!m_compositorState || !std::isnan(m_compositorState->startTime));

    if (!shouldStart) {
        m_currentTimePending = false;
    }

    if (shouldStart) {
        m_compositorGroup = compositorGroup;
        if (startOnCompositor) {
            if (isCandidateForAnimationOnCompositor())
                createCompositorPlayer();

            if (maybeStartAnimationOnCompositor())
                m_compositorState = wrapUnique(new CompositorState(*this));
            else
                cancelIncompatibleAnimationsOnCompositor();
        }
    }

    return true;
}

void Animation::postCommit(double timelineTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    m_compositorPending = false;

    if (!m_compositorState || m_compositorState->pendingAction == None)
        return;

    switch (m_compositorState->pendingAction) {
    case Start:
        if (!std::isnan(m_compositorState->startTime)) {
            ASSERT(m_startTime == m_compositorState->startTime);
            m_compositorState->pendingAction = None;
        }
        break;
    case Pause:
    case PauseThenStart:
        ASSERT(std::isnan(m_startTime));
        m_compositorState->pendingAction = None;
        setCurrentTimeInternal((timelineTime - m_compositorState->startTime) * m_playbackRate, TimingUpdateForAnimationFrame);
        m_currentTimePending = false;
        break;
    default:
        NOTREACHED();
    }
}

void Animation::notifyCompositorStartTime(double timelineTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    if (m_compositorState) {
        ASSERT(m_compositorState->pendingAction == Start);
        ASSERT(std::isnan(m_compositorState->startTime));

        double initialCompositorHoldTime = m_compositorState->holdTime;
        m_compositorState->pendingAction = None;
        m_compositorState->startTime = timelineTime + currentTimeInternal() / -m_playbackRate;

        if (m_startTime == timelineTime) {
            // The start time was set to the incoming compositor start time.
            // Unlikely, but possible.
            // FIXME: Depending on what changed above this might still be pending.
            // Maybe...
            m_currentTimePending = false;
            return;
        }

        if (!std::isnan(m_startTime) || currentTimeInternal() != initialCompositorHoldTime) {
            // A new start time or current time was set while starting.
            setCompositorPending(true);
            return;
        }
    }

    notifyStartTime(timelineTime);
}

void Animation::notifyStartTime(double timelineTime)
{
    if (playing()) {
        ASSERT(std::isnan(m_startTime));
        ASSERT(m_held);

        if (m_playbackRate == 0) {
            setStartTimeInternal(timelineTime);
        } else {
            setStartTimeInternal(timelineTime + currentTimeInternal() / -m_playbackRate);
        }

        // FIXME: This avoids marking this animation as outdated needlessly when a start time
        // is notified, but we should refactor how outdating works to avoid this.
        clearOutdated();
        m_currentTimePending = false;
    }
}

bool Animation::affects(const Element& element, CSSPropertyID property) const
{
    if (!m_content || !m_content->isKeyframeEffect())
        return false;

    const KeyframeEffect* effect = toKeyframeEffect(m_content.get());
    return (effect->target() == &element) && effect->affects(PropertyHandle(property));
}

double Animation::calculateStartTime(double currentTime) const
{
    return m_timeline->effectiveTime() - currentTime / m_playbackRate;
}

double Animation::calculateCurrentTime() const
{
    if (isNull(m_startTime) || !m_timeline)
        return 0;
    return (m_timeline->effectiveTime() - m_startTime) * m_playbackRate;
}

void Animation::setStartTime(double startTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (startTime == m_startTime)
        return;

    m_currentTimePending = false;
    m_playState = Unset;
    m_paused = false;
    setStartTimeInternal(startTime / 1000);
}

void Animation::setStartTimeInternal(double newStartTime)
{
    ASSERT(!m_paused);
    ASSERT(std::isfinite(newStartTime));
    ASSERT(newStartTime != m_startTime);

    bool hadStartTime = hasStartTime();
    double previousCurrentTime = currentTimeInternal();
    m_startTime = newStartTime;
    if (m_held && m_playbackRate) {
        // If held, the start time would still be derrived from the hold time.
        // Force a new, limited, current time.
        m_held = false;
        double currentTime = calculateCurrentTime();
        if (m_playbackRate > 0 && currentTime > effectEnd()) {
            currentTime = effectEnd();
        } else if (m_playbackRate < 0 && currentTime < 0) {
            currentTime = 0;
        }
        setCurrentTimeInternal(currentTime, TimingUpdateOnDemand);
    }
    updateCurrentTimingState(TimingUpdateOnDemand);
    double newCurrentTime = currentTimeInternal();

    if (previousCurrentTime != newCurrentTime) {
        setOutdated();
    } else if (!hadStartTime && m_timeline) {
        // Even though this animation is not outdated, time to effect change is
        // infinity until start time is set.
        m_timeline->wake();
    }
}

void Animation::setEffect(AnimationEffect* newEffect)
{
    if (m_content == newEffect)
        return;
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, SetCompositorPendingWithEffectChanged);

    double storedCurrentTime = currentTimeInternal();
    if (m_content)
        m_content->detach();
    m_content = newEffect;
    if (newEffect) {
        // FIXME: This logic needs to be updated once groups are implemented
        if (newEffect->animation()) {
            newEffect->animation()->cancel();
            newEffect->animation()->setEffect(0);
        }
        newEffect->attach(this);
        setOutdated();
    }
    setCurrentTimeInternal(storedCurrentTime, TimingUpdateOnDemand);
}

const char* Animation::playStateString(AnimationPlayState playState)
{
    switch (playState) {
    case Idle:
        return "idle";
    case Pending:
        return "pending";
    case Running:
        return "running";
    case Paused:
        return "paused";
    case Finished:
        return "finished";
    default:
        NOTREACHED();
        return "";
    }
}

Animation::AnimationPlayState Animation::playStateInternal() const
{
    ASSERT(m_playState != Unset);
    return m_playState;
}

Animation::AnimationPlayState Animation::calculatePlayState()
{
    if (m_paused && !m_currentTimePending)
        return Paused;
    if (m_playState == Idle)
        return Idle;
    if (m_currentTimePending || (isNull(m_startTime) && m_playbackRate != 0))
        return Pending;
    if (limited())
        return Finished;
    return Running;
}

void Animation::pause(ExceptionState& exceptionState)
{
    if (m_paused)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    double newCurrentTime = currentTimeInternal();
    if (calculatePlayState() == Idle) {
        if (m_playbackRate < 0 && effectEnd() == std::numeric_limits<double>::infinity()) {
            exceptionState.throwDOMException(InvalidStateError, "Cannot pause, Animation has infinite target effect end.");
            return;
        }
        newCurrentTime = m_playbackRate < 0 ? effectEnd() : 0;
    }

    m_playState = Unset;
    m_paused = true;
    m_currentTimePending = true;
    setCurrentTimeInternal(newCurrentTime, TimingUpdateOnDemand);
}

void Animation::unpause()
{
    if (!m_paused)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_currentTimePending = true;
    unpauseInternal();
}

void Animation::unpauseInternal()
{
    if (!m_paused)
        return;
    m_paused = false;
    setCurrentTimeInternal(currentTimeInternal(), TimingUpdateOnDemand);
}

void Animation::play(ExceptionState& exceptionState)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    double currentTime = this->currentTimeInternal();
    if (m_playbackRate < 0 && currentTime <= 0 && effectEnd() == std::numeric_limits<double>::infinity()) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot play reversed Animation with infinite target effect end.");
        return;
    }

    if (!playing()) {
        m_startTime = nullValue();
    }

    if (playStateInternal() == Idle) {
        m_held = true;
        m_holdTime = 0;
    }

    m_playState = Unset;
    m_finished = false;
    unpauseInternal();

    if (m_playbackRate > 0 && (currentTime < 0 || currentTime >= effectEnd())) {
        m_startTime = nullValue();
        setCurrentTimeInternal(0, TimingUpdateOnDemand);
    } else if (m_playbackRate < 0 && (currentTime <= 0 || currentTime > effectEnd())) {
        m_startTime = nullValue();
        setCurrentTimeInternal(effectEnd(), TimingUpdateOnDemand);
    }
}

void Animation::reverse(ExceptionState& exceptionState)
{
    if (!m_playbackRate) {
        return;
    }

    setPlaybackRateInternal(-m_playbackRate);
    play(exceptionState);
}

void Animation::finish(ExceptionState& exceptionState)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (!m_playbackRate) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot finish Animation with a playbackRate of 0.");
        return;
    }
    if (m_playbackRate > 0 && effectEnd() == std::numeric_limits<double>::infinity()) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot finish Animation with an infinite target effect end.");
        return;
    }

    // Avoid updating start time when already finished.
    if (calculatePlayState() == Finished)
        return;

    double newCurrentTime = m_playbackRate < 0 ? 0 : effectEnd();
    setCurrentTimeInternal(newCurrentTime, TimingUpdateOnDemand);
    m_paused = false;
    m_currentTimePending = false;
    m_startTime = calculateStartTime(newCurrentTime);
    m_playState = Finished;
}

ScriptPromise Animation::finished(ScriptState* scriptState)
{
    if (!m_finishedPromise) {
        m_finishedPromise = new AnimationPromise(scriptState->getExecutionContext(), this, AnimationPromise::Finished);
        if (playStateInternal() == Finished)
            m_finishedPromise->resolve(this);
    }
    return m_finishedPromise->promise(scriptState->world());
}

ScriptPromise Animation::ready(ScriptState* scriptState)
{
    if (!m_readyPromise) {
        m_readyPromise = new AnimationPromise(scriptState->getExecutionContext(), this, AnimationPromise::Ready);
        if (playStateInternal() != Pending)
            m_readyPromise->resolve(this);
    }
    return m_readyPromise->promise(scriptState->world());
}

const AtomicString& Animation::interfaceName() const
{
    return EventTargetNames::AnimationPlayer;
}

ExecutionContext* Animation::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

bool Animation::hasPendingActivity() const
{
    return m_pendingFinishedEvent || (!m_finished && hasEventListeners(EventTypeNames::finish));
}

void Animation::stop()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_finished = true;
    m_pendingFinishedEvent = nullptr;
}

DispatchEventResult Animation::dispatchEventInternal(Event* event)
{
    if (m_pendingFinishedEvent == event)
        m_pendingFinishedEvent = nullptr;
    return EventTargetWithInlineData::dispatchEventInternal(event);
}

double Animation::playbackRate() const
{
    return m_playbackRate;
}

void Animation::setPlaybackRate(double playbackRate)
{
    if (playbackRate == m_playbackRate)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    setPlaybackRateInternal(playbackRate);
}

void Animation::setPlaybackRateInternal(double playbackRate)
{
    ASSERT(std::isfinite(playbackRate));
    ASSERT(playbackRate != m_playbackRate);

    if (!limited() && !paused() && hasStartTime())
        m_currentTimePending = true;

    double storedCurrentTime = currentTimeInternal();
    if ((m_playbackRate < 0 && playbackRate >= 0) || (m_playbackRate > 0 && playbackRate <= 0))
        m_finished = false;

    m_playbackRate = playbackRate;
    m_startTime = std::numeric_limits<double>::quiet_NaN();
    setCurrentTimeInternal(storedCurrentTime, TimingUpdateOnDemand);
}

void Animation::clearOutdated()
{
    if (!m_outdated)
        return;
    m_outdated = false;
    if (m_timeline)
        m_timeline->clearOutdatedAnimation(this);
}

void Animation::setOutdated()
{
    if (m_outdated)
        return;
    m_outdated = true;
    if (m_timeline)
        m_timeline->setOutdatedAnimation(this);
}

bool Animation::canStartAnimationOnCompositor() const
{
    if (m_isCompositedAnimationDisabledForTesting  || effectSuppressed())
        return false;

    // FIXME: Timeline playback rates should be compositable
    if (m_playbackRate == 0 || (std::isinf(effectEnd()) && m_playbackRate < 0) || (timeline() && timeline()->playbackRate() != 1))
        return false;

    return m_timeline && m_content && m_content->isKeyframeEffect() && playing();
}

bool Animation::isCandidateForAnimationOnCompositor() const
{
    if (!canStartAnimationOnCompositor())
        return false;

    return toKeyframeEffect(m_content.get())->isCandidateForAnimationOnCompositor(m_playbackRate);
}

bool Animation::maybeStartAnimationOnCompositor()
{
    if (!canStartAnimationOnCompositor())
        return false;

    bool reversed = m_playbackRate < 0;

    double startTime = timeline()->zeroTime() + startTimeInternal();
    if (reversed) {
        startTime -= effectEnd() / fabs(m_playbackRate);
    }

    double timeOffset = 0;
    if (std::isnan(startTime)) {
        timeOffset = reversed ? effectEnd() - currentTimeInternal() : currentTimeInternal();
        timeOffset = timeOffset / fabs(m_playbackRate);
    }
    ASSERT(m_compositorGroup != 0);
    return toKeyframeEffect(m_content.get())->maybeStartAnimationOnCompositor(m_compositorGroup, startTime, timeOffset, m_playbackRate);
}

void Animation::setCompositorPending(bool effectChanged)
{
    // FIXME: KeyframeEffect could notify this directly?
    if (!hasActiveAnimationsOnCompositor()) {
        destroyCompositorPlayer();
        m_compositorState.reset();
    }
    if (effectChanged && m_compositorState) {
        m_compositorState->effectChanged = true;
    }
    if (m_compositorPending || m_isPausedForTesting) {
        return;
    }
    if (!m_compositorState || m_compositorState->effectChanged
        || m_compositorState->playbackRate != m_playbackRate
        || m_compositorState->startTime != m_startTime) {
        m_compositorPending = true;
        timeline()->document()->compositorPendingAnimations().add(this);
    }
}

void Animation::cancelAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toKeyframeEffect(m_content.get())->cancelAnimationOnCompositor();

    destroyCompositorPlayer();
}

void Animation::restartAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toKeyframeEffect(m_content.get())->restartAnimationOnCompositor();
}

void Animation::cancelIncompatibleAnimationsOnCompositor()
{
    if (m_content && m_content->isKeyframeEffect())
        toKeyframeEffect(m_content.get())->cancelIncompatibleAnimationsOnCompositor();
}

bool Animation::hasActiveAnimationsOnCompositor()
{
    if (!m_content || !m_content->isKeyframeEffect())
        return false;

    return toKeyframeEffect(m_content.get())->hasActiveAnimationsOnCompositor();
}

bool Animation::update(TimingUpdateReason reason)
{
    if (!m_timeline)
        return false;

    PlayStateUpdateScope updateScope(*this, reason, DoNotSetCompositorPending);

    clearOutdated();
    bool idle = playStateInternal() == Idle;

    if (m_content) {
        double inheritedTime = idle || isNull(m_timeline->currentTimeInternal())
            ? nullValue()
            : currentTimeInternal();

        // Special case for end-exclusivity when playing backwards.
        if (inheritedTime == 0 && m_playbackRate < 0)
            inheritedTime = -1;
        m_content->updateInheritedTime(inheritedTime, reason);
    }

    if ((idle || limited()) && !m_finished) {
        if (reason == TimingUpdateForAnimationFrame && (idle || hasStartTime())) {
            if (idle) {
                const AtomicString& eventType = EventTypeNames::cancel;
                if (getExecutionContext() && hasEventListeners(eventType)) {
                    double eventCurrentTime = nullValue();
                    m_pendingCancelledEvent = AnimationPlayerEvent::create(eventType, eventCurrentTime, timeline()->currentTime());
                    m_pendingCancelledEvent->setTarget(this);
                    m_pendingCancelledEvent->setCurrentTarget(this);
                    m_timeline->document()->enqueueAnimationFrameEvent(m_pendingCancelledEvent);
                }
            } else {
                const AtomicString& eventType = EventTypeNames::finish;
                if (getExecutionContext() && hasEventListeners(eventType)) {
                    double eventCurrentTime = currentTimeInternal() * 1000;
                    m_pendingFinishedEvent = AnimationPlayerEvent::create(eventType, eventCurrentTime, timeline()->currentTime());
                    m_pendingFinishedEvent->setTarget(this);
                    m_pendingFinishedEvent->setCurrentTarget(this);
                    m_timeline->document()->enqueueAnimationFrameEvent(m_pendingFinishedEvent);
                }
            }
            m_finished = true;
        }
    }
    ASSERT(!m_outdated);
    return !m_finished || std::isfinite(timeToEffectChange());
}

double Animation::timeToEffectChange()
{
    ASSERT(!m_outdated);
    if (!hasStartTime() || m_held)
        return std::numeric_limits<double>::infinity();

    if (!m_content)
        return -currentTimeInternal() / m_playbackRate;
    double result = m_playbackRate > 0
        ? m_content->timeToForwardsEffectChange() / m_playbackRate
        : m_content->timeToReverseEffectChange() / -m_playbackRate;

    return !hasActiveAnimationsOnCompositor() && m_content->getPhase() == AnimationEffect::PhaseActive
        ? 0
        : result;
}

void Animation::cancel()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playStateInternal() == Idle)
        return;

    m_held = false;
    m_paused = false;
    m_playState = Idle;
    m_startTime = nullValue();
    m_currentTimePending = false;
}

void Animation::beginUpdatingState()
{
    // Nested calls are not allowed!
    ASSERT(!m_stateIsBeingUpdated);
    m_stateIsBeingUpdated = true;
}

void Animation::endUpdatingState()
{
    ASSERT(m_stateIsBeingUpdated);
    m_stateIsBeingUpdated = false;
}

void Animation::createCompositorPlayer()
{
    if (Platform::current()->isThreadedAnimationEnabled() && !m_compositorPlayer) {
        // We only need to pre-finalize if we are running animations on the compositor.
        if (!m_preFinalizerRegistered) {
            ThreadState::current()->registerPreFinalizer(this);
            m_preFinalizerRegistered = true;
        }

        ASSERT(Platform::current()->compositorSupport());
        m_compositorPlayer = CompositorAnimationPlayer::create();
        ASSERT(m_compositorPlayer);
        m_compositorPlayer->setAnimationDelegate(this);
        attachCompositorTimeline();
    }

    attachCompositedLayers();
}

void Animation::destroyCompositorPlayer()
{
    detachCompositedLayers();

    if (m_compositorPlayer) {
        detachCompositorTimeline();
        m_compositorPlayer->setAnimationDelegate(nullptr);
        m_compositorPlayer.reset();
    }
}

void Animation::attachCompositorTimeline()
{
    if (m_compositorPlayer) {
        CompositorAnimationTimeline* timeline = m_timeline ? m_timeline->compositorTimeline() : nullptr;
        if (timeline)
            timeline->playerAttached(*this);
    }
}

void Animation::detachCompositorTimeline()
{
    if (m_compositorPlayer) {
        CompositorAnimationTimeline* timeline = m_timeline ? m_timeline->compositorTimeline() : nullptr;
        if (timeline)
            timeline->playerDestroyed(*this);
    }
}

void Animation::attachCompositedLayers()
{
    if (!m_compositorPlayer)
        return;

    ASSERT(m_content);
    ASSERT(m_content->isKeyframeEffect());

    toKeyframeEffect(m_content.get())->attachCompositedLayers();
}

void Animation::detachCompositedLayers()
{
    if (m_compositorPlayer && m_compositorPlayer->isElementAttached())
        m_compositorPlayer->detachElement();
}

void Animation::notifyAnimationStarted(double monotonicTime, int group)
{
    timeline()->document()->compositorPendingAnimations().notifyCompositorAnimationStarted(monotonicTime, group);
}

Animation::PlayStateUpdateScope::PlayStateUpdateScope(Animation& animation, TimingUpdateReason reason, CompositorPendingChange compositorPendingChange)
    : m_animation(animation)
    , m_initialPlayState(m_animation->playStateInternal())
    , m_compositorPendingChange(compositorPendingChange)
{
    ASSERT(m_initialPlayState != Unset);
    m_animation->beginUpdatingState();
    m_animation->updateCurrentTimingState(reason);
}

Animation::PlayStateUpdateScope::~PlayStateUpdateScope()
{
    AnimationPlayState oldPlayState = m_initialPlayState;
    AnimationPlayState newPlayState = m_animation->calculatePlayState();

    m_animation->m_playState = newPlayState;
    if (oldPlayState != newPlayState) {
        bool wasActive = oldPlayState == Pending || oldPlayState == Running;
        bool isActive = newPlayState == Pending || newPlayState == Running;
        if (!wasActive && isActive)
            TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("blink.animations,devtools.timeline,benchmark", "Animation", m_animation, "data", InspectorAnimationEvent::data(*m_animation));
        else if (wasActive && !isActive)
            TRACE_EVENT_NESTABLE_ASYNC_END1("blink.animations,devtools.timeline,benchmark", "Animation", m_animation, "endData", InspectorAnimationStateEvent::data(*m_animation));
        else
            TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("blink.animations,devtools.timeline,benchmark", "Animation", m_animation, "data", InspectorAnimationStateEvent::data(*m_animation));
    }

    // Ordering is important, the ready promise should resolve/reject before
    // the finished promise.
    if (m_animation->m_readyPromise && newPlayState != oldPlayState) {
        if (newPlayState == Idle) {
            if (m_animation->m_readyPromise->getState() == AnimationPromise::Pending) {
                m_animation->m_readyPromise->reject(DOMException::create(AbortError));
            }
            m_animation->m_readyPromise->reset();
            m_animation->m_readyPromise->resolve(m_animation);
        } else if (oldPlayState == Pending) {
            m_animation->m_readyPromise->resolve(m_animation);
        } else if (newPlayState == Pending) {
            ASSERT(m_animation->m_readyPromise->getState() != AnimationPromise::Pending);
            m_animation->m_readyPromise->reset();
        }
    }

    if (m_animation->m_finishedPromise && newPlayState != oldPlayState) {
        if (newPlayState == Idle) {
            if (m_animation->m_finishedPromise->getState() == AnimationPromise::Pending) {
                m_animation->m_finishedPromise->reject(DOMException::create(AbortError));
            }
            m_animation->m_finishedPromise->reset();
        } else if (newPlayState == Finished) {
            m_animation->m_finishedPromise->resolve(m_animation);
        } else if (oldPlayState == Finished) {
            m_animation->m_finishedPromise->reset();
        }
    }

    if (oldPlayState != newPlayState && (oldPlayState == Idle || newPlayState == Idle)) {
        m_animation->setOutdated();
    }

#if ENABLE(ASSERT)
    // Verify that current time is up to date.
    m_animation->currentTimeInternal();
#endif

    switch (m_compositorPendingChange) {
    case SetCompositorPending:
        m_animation->setCompositorPending();
        break;
    case SetCompositorPendingWithEffectChanged:
        m_animation->setCompositorPending(true);
        break;
    case DoNotSetCompositorPending:
        break;
    default:
        NOTREACHED();
        break;
    }
    m_animation->endUpdatingState();

    if (oldPlayState != newPlayState)
        InspectorInstrumentation::animationPlayStateChanged(m_animation->timeline()->document(), m_animation, oldPlayState, newPlayState);
}

void Animation::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
    if (eventType == EventTypeNames::finish)
        UseCounter::count(getExecutionContext(), UseCounter::AnimationFinishEvent);
}

void Animation::pauseForTesting(double pauseTime)
{
    RELEASE_ASSERT(!paused());
    setCurrentTimeInternal(pauseTime, TimingUpdateOnDemand);
    if (hasActiveAnimationsOnCompositor())
        toKeyframeEffect(m_content.get())->pauseAnimationForTestingOnCompositor(currentTimeInternal());
    m_isPausedForTesting = true;
    pause();
}

void Animation::setEffectSuppressed(bool suppressed)
{
    m_effectSuppressed = suppressed;
    if (suppressed)
        cancelAnimationOnCompositor();
}

void Animation::disableCompositedAnimationForTesting()
{
    m_isCompositedAnimationDisabledForTesting = true;
    cancelAnimationOnCompositor();
}

void Animation::invalidateKeyframeEffect()
{
    if (!m_content || !m_content->isKeyframeEffect())
        return;

    toKeyframeEffect(m_content.get())->target()->setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleSheetChange));
}

DEFINE_TRACE(Animation)
{
    visitor->trace(m_content);
    visitor->trace(m_timeline);
    visitor->trace(m_pendingFinishedEvent);
    visitor->trace(m_pendingCancelledEvent);
    visitor->trace(m_finishedPromise);
    visitor->trace(m_readyPromise);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
