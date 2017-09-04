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

#include "core/animation/AnimationEffectReadOnly.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/TimingCalculations.h"

namespace blink {

namespace {

Timing::FillMode resolvedFillMode(Timing::FillMode fillMode, bool isAnimation) {
  if (fillMode != Timing::FillMode::AUTO)
    return fillMode;
  if (isAnimation)
    return Timing::FillMode::NONE;
  return Timing::FillMode::BOTH;
}

}  // namespace

AnimationEffectReadOnly::AnimationEffectReadOnly(const Timing& timing,
                                                 EventDelegate* eventDelegate)
    : m_animation(nullptr),
      m_timing(timing),
      m_eventDelegate(eventDelegate),
      m_calculated(),
      m_needsUpdate(true),
      m_lastUpdateTime(nullValue()) {
  m_timing.assertValid();
}

double AnimationEffectReadOnly::iterationDuration() const {
  double result = std::isnan(m_timing.iterationDuration)
                      ? intrinsicIterationDuration()
                      : m_timing.iterationDuration;
  DCHECK_GE(result, 0);
  return result;
}

double AnimationEffectReadOnly::repeatedDuration() const {
  const double result =
      multiplyZeroAlwaysGivesZero(iterationDuration(), m_timing.iterationCount);
  DCHECK_GE(result, 0);
  return result;
}

double AnimationEffectReadOnly::activeDurationInternal() const {
  const double result =
      m_timing.playbackRate
          ? repeatedDuration() / std::abs(m_timing.playbackRate)
          : std::numeric_limits<double>::infinity();
  DCHECK_GE(result, 0);
  return result;
}

void AnimationEffectReadOnly::updateSpecifiedTiming(const Timing& timing) {
  // FIXME: Test whether the timing is actually different?
  m_timing = timing;
  invalidate();
  if (m_animation)
    m_animation->setOutdated();
  specifiedTimingChanged();
}

void AnimationEffectReadOnly::getComputedTiming(
    ComputedTimingProperties& computedTiming) {
  // ComputedTimingProperties members.
  computedTiming.setEndTime(endTimeInternal() * 1000);
  computedTiming.setActiveDuration(activeDurationInternal() * 1000);

  if (ensureCalculated().isInEffect) {
    computedTiming.setLocalTime(ensureCalculated().localTime * 1000);
    computedTiming.setProgress(ensureCalculated().progress);
    computedTiming.setCurrentIteration(ensureCalculated().currentIteration);
  } else {
    computedTiming.setLocalTimeToNull();
    computedTiming.setProgressToNull();
    computedTiming.setCurrentIterationToNull();
  }

  // KeyframeEffectOptions members.
  computedTiming.setDelay(specifiedTiming().startDelay * 1000);
  computedTiming.setEndDelay(specifiedTiming().endDelay * 1000);
  computedTiming.setFill(Timing::fillModeString(resolvedFillMode(
      specifiedTiming().fillMode, isKeyframeEffectReadOnly())));
  computedTiming.setIterationStart(specifiedTiming().iterationStart);
  computedTiming.setIterations(specifiedTiming().iterationCount);

  UnrestrictedDoubleOrString duration;
  duration.setUnrestrictedDouble(iterationDuration() * 1000);
  computedTiming.setDuration(duration);

  computedTiming.setDirection(
      Timing::playbackDirectionString(specifiedTiming().direction));
  computedTiming.setEasing(specifiedTiming().timingFunction->toString());
}

ComputedTimingProperties AnimationEffectReadOnly::getComputedTiming() {
  ComputedTimingProperties result;
  getComputedTiming(result);
  return result;
}

void AnimationEffectReadOnly::updateInheritedTime(
    double inheritedTime,
    TimingUpdateReason reason) const {
  bool needsUpdate = m_needsUpdate ||
                     (m_lastUpdateTime != inheritedTime &&
                      !(isNull(m_lastUpdateTime) && isNull(inheritedTime))) ||
                     (animation() && animation()->effectSuppressed());
  m_needsUpdate = false;
  m_lastUpdateTime = inheritedTime;

  const double localTime = inheritedTime;
  double timeToNextIteration = std::numeric_limits<double>::infinity();
  if (needsUpdate) {
    const double activeDuration = this->activeDurationInternal();

    const Phase currentPhase =
        calculatePhase(activeDuration, localTime, m_timing);
    // FIXME: parentPhase depends on groups being implemented.
    const AnimationEffectReadOnly::Phase parentPhase =
        AnimationEffectReadOnly::PhaseActive;
    const double activeTime = calculateActiveTime(
        activeDuration,
        resolvedFillMode(m_timing.fillMode, isKeyframeEffectReadOnly()),
        localTime, parentPhase, currentPhase, m_timing);

    double currentIteration;
    double progress;
    if (const double iterationDuration = this->iterationDuration()) {
      const double startOffset = multiplyZeroAlwaysGivesZero(
          m_timing.iterationStart, iterationDuration);
      DCHECK_GE(startOffset, 0);
      const double scaledActiveTime = calculateScaledActiveTime(
          activeDuration, activeTime, startOffset, m_timing);
      const double iterationTime = calculateIterationTime(
          iterationDuration, repeatedDuration(), scaledActiveTime, startOffset,
          currentPhase, m_timing);

      currentIteration = calculateCurrentIteration(
          iterationDuration, iterationTime, scaledActiveTime, m_timing);
      const double transformedTime = calculateTransformedTime(
          currentIteration, iterationDuration, iterationTime, m_timing);

      // The infinite iterationDuration case here is a workaround because
      // the specified behaviour does not handle infinite durations well.
      // There is an open issue against the spec to fix this:
      // https://github.com/w3c/web-animations/issues/142
      if (!std::isfinite(iterationDuration))
        progress = fmod(m_timing.iterationStart, 1.0);
      else
        progress = transformedTime / iterationDuration;

      if (!isNull(iterationTime)) {
        timeToNextIteration = (iterationDuration - iterationTime) /
                              std::abs(m_timing.playbackRate);
        if (activeDuration - activeTime < timeToNextIteration)
          timeToNextIteration = std::numeric_limits<double>::infinity();
      }
    } else {
      const double localIterationDuration = 1;
      const double localRepeatedDuration =
          localIterationDuration * m_timing.iterationCount;
      DCHECK_GE(localRepeatedDuration, 0);
      const double localActiveDuration =
          m_timing.playbackRate
              ? localRepeatedDuration / std::abs(m_timing.playbackRate)
              : std::numeric_limits<double>::infinity();
      DCHECK_GE(localActiveDuration, 0);
      const double localLocalTime =
          localTime < m_timing.startDelay
              ? localTime
              : localActiveDuration + m_timing.startDelay;
      const AnimationEffectReadOnly::Phase localCurrentPhase =
          calculatePhase(localActiveDuration, localLocalTime, m_timing);
      const double localActiveTime = calculateActiveTime(
          localActiveDuration,
          resolvedFillMode(m_timing.fillMode, isKeyframeEffectReadOnly()),
          localLocalTime, parentPhase, localCurrentPhase, m_timing);
      const double startOffset =
          m_timing.iterationStart * localIterationDuration;
      DCHECK_GE(startOffset, 0);
      const double scaledActiveTime = calculateScaledActiveTime(
          localActiveDuration, localActiveTime, startOffset, m_timing);
      const double iterationTime = calculateIterationTime(
          localIterationDuration, localRepeatedDuration, scaledActiveTime,
          startOffset, currentPhase, m_timing);

      currentIteration = calculateCurrentIteration(
          localIterationDuration, iterationTime, scaledActiveTime, m_timing);
      progress = calculateTransformedTime(
          currentIteration, localIterationDuration, iterationTime, m_timing);
    }

    m_calculated.currentIteration = currentIteration;
    m_calculated.progress = progress;

    m_calculated.phase = currentPhase;
    m_calculated.isInEffect = !isNull(activeTime);
    m_calculated.isInPlay = getPhase() == PhaseActive;
    m_calculated.isCurrent = getPhase() == PhaseBefore || isInPlay();
    m_calculated.localTime = m_lastUpdateTime;
  }

  // Test for events even if timing didn't need an update as the animation may
  // have gained a start time.
  // FIXME: Refactor so that we can DCHECK(m_animation) here, this is currently
  // required to be nullable for testing.
  if (reason == TimingUpdateForAnimationFrame &&
      (!m_animation || m_animation->hasStartTime() || m_animation->paused())) {
    if (m_eventDelegate)
      m_eventDelegate->onEventCondition(*this);
  }

  if (needsUpdate) {
    // FIXME: This probably shouldn't be recursive.
    updateChildrenAndEffects();
    m_calculated.timeToForwardsEffectChange =
        calculateTimeToEffectChange(true, localTime, timeToNextIteration);
    m_calculated.timeToReverseEffectChange =
        calculateTimeToEffectChange(false, localTime, timeToNextIteration);
  }
}

const AnimationEffectReadOnly::CalculatedTiming&
AnimationEffectReadOnly::ensureCalculated() const {
  if (!m_animation)
    return m_calculated;
  if (m_animation->outdated())
    m_animation->update(TimingUpdateOnDemand);
  DCHECK(!m_animation->outdated());
  return m_calculated;
}

AnimationEffectTimingReadOnly* AnimationEffectReadOnly::timing() {
  return AnimationEffectTimingReadOnly::create(this);
}

DEFINE_TRACE(AnimationEffectReadOnly) {
  visitor->trace(m_animation);
  visitor->trace(m_eventDelegate);
}

}  // namespace blink
