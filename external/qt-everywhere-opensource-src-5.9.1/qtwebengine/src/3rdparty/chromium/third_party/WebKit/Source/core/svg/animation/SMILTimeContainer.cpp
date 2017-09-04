/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/animation/SMILTimeContainer.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/ElementTraversal.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/animation/SMILTime.h"
#include "core/svg/animation/SVGSMILElement.h"
#include <algorithm>

namespace blink {

static const double initialFrameDelay = 0.025;
static const double animationPolicyOnceDuration = 3.000;

SMILTimeContainer::SMILTimeContainer(SVGSVGElement& owner)
    : m_presentationTime(0),
      m_referenceTime(0),
      m_frameSchedulingState(Idle),
      m_started(false),
      m_paused(false),
      m_documentOrderIndexesDirty(false),
      m_wakeupTimer(this, &SMILTimeContainer::wakeupTimerFired),
      m_animationPolicyOnceTimer(this,
                                 &SMILTimeContainer::animationPolicyTimerFired),
      m_ownerSVGElement(&owner)
#if ENABLE(ASSERT)
      ,
      m_preventScheduledAnimationsChanges(false)
#endif
{
}

SMILTimeContainer::~SMILTimeContainer() {
  cancelAnimationFrame();
  cancelAnimationPolicyTimer();
  ASSERT(!m_wakeupTimer.isActive());
#if ENABLE(ASSERT)
  ASSERT(!m_preventScheduledAnimationsChanges);
#endif
}

void SMILTimeContainer::schedule(SVGSMILElement* animation,
                                 SVGElement* target,
                                 const QualifiedName& attributeName) {
  DCHECK_EQ(animation->timeContainer(), this);
  DCHECK(target);
  DCHECK(animation->hasValidTarget());

#if ENABLE(ASSERT)
  ASSERT(!m_preventScheduledAnimationsChanges);
#endif

  ElementAttributePair key(target, attributeName);
  Member<AnimationsLinkedHashSet>& scheduled =
      m_scheduledAnimations.add(key, nullptr).storedValue->value;
  if (!scheduled)
    scheduled = new AnimationsLinkedHashSet;
  ASSERT(!scheduled->contains(animation));
  scheduled->add(animation);

  SMILTime nextFireTime = animation->nextProgressTime();
  if (nextFireTime.isFinite())
    notifyIntervalsChanged();
}

void SMILTimeContainer::unschedule(SVGSMILElement* animation,
                                   SVGElement* target,
                                   const QualifiedName& attributeName) {
  ASSERT(animation->timeContainer() == this);

#if ENABLE(ASSERT)
  ASSERT(!m_preventScheduledAnimationsChanges);
#endif

  ElementAttributePair key(target, attributeName);
  GroupedAnimationsMap::iterator it = m_scheduledAnimations.find(key);
  ASSERT(it != m_scheduledAnimations.end());
  AnimationsLinkedHashSet* scheduled = it->value.get();
  ASSERT(scheduled);
  AnimationsLinkedHashSet::iterator itAnimation = scheduled->find(animation);
  ASSERT(itAnimation != scheduled->end());
  scheduled->remove(itAnimation);

  if (scheduled->isEmpty())
    m_scheduledAnimations.remove(it);
}

bool SMILTimeContainer::hasAnimations() const {
  return !m_scheduledAnimations.isEmpty();
}

bool SMILTimeContainer::hasPendingSynchronization() const {
  return m_frameSchedulingState == SynchronizeAnimations &&
         m_wakeupTimer.isActive() && !m_wakeupTimer.nextFireInterval();
}

void SMILTimeContainer::notifyIntervalsChanged() {
  if (!isStarted())
    return;
  // Schedule updateAnimations() to be called asynchronously so multiple
  // intervals can change with updateAnimations() only called once at the end.
  if (hasPendingSynchronization())
    return;
  cancelAnimationFrame();
  scheduleWakeUp(0, SynchronizeAnimations);
}

double SMILTimeContainer::elapsed() const {
  if (!isStarted())
    return 0;

  if (isPaused())
    return m_presentationTime;

  return m_presentationTime +
         (document().timeline().currentTimeInternal() - m_referenceTime);
}

void SMILTimeContainer::synchronizeToDocumentTimeline() {
  m_referenceTime = document().timeline().currentTimeInternal();
}

bool SMILTimeContainer::isPaused() const {
  // If animation policy is "none", the timeline is always paused.
  return m_paused || animationPolicy() == ImageAnimationPolicyNoAnimation;
}

bool SMILTimeContainer::isStarted() const {
  return m_started;
}

bool SMILTimeContainer::isTimelineRunning() const {
  return isStarted() && !isPaused();
}

void SMILTimeContainer::start() {
  RELEASE_ASSERT(!isStarted());

  if (!document().isActive())
    return;

  if (!handleAnimationPolicy(RestartOnceTimerIfNotPaused))
    return;

  // Sample the document timeline to get a time reference for the "presentation
  // time".
  synchronizeToDocumentTimeline();
  m_started = true;

  // If the "presentation time" is non-zero, the timeline was modified via
  // setElapsed() before the document began.  In this case pass on
  // 'seekToTime=true' to updateAnimations() to issue a seek.
  SMILTime earliestFireTime =
      updateAnimations(m_presentationTime, m_presentationTime ? true : false);
  if (!canScheduleFrame(earliestFireTime))
    return;
  // If the timeline is running, and there are pending animation updates,
  // always perform the first update after the timeline was started using
  // the wake-up mechanism.
  double delayTime = earliestFireTime.value() - m_presentationTime;
  scheduleWakeUp(std::max(initialFrameDelay, delayTime), SynchronizeAnimations);
}

void SMILTimeContainer::pause() {
  if (!handleAnimationPolicy(CancelOnceTimer))
    return;
  DCHECK(!isPaused());

  if (isStarted()) {
    m_presentationTime = elapsed();
    cancelAnimationFrame();
  }
  // Update the flag after sampling elapsed().
  m_paused = true;
}

void SMILTimeContainer::resume() {
  if (!handleAnimationPolicy(RestartOnceTimer))
    return;
  DCHECK(isPaused());

  m_paused = false;

  if (!isStarted())
    return;

  synchronizeToDocumentTimeline();
  scheduleWakeUp(0, SynchronizeAnimations);
}

void SMILTimeContainer::setElapsed(double elapsed) {
  m_presentationTime = elapsed;

  // If the document hasn't finished loading, |m_presentationTime| will be
  // used as the start time to seek to once it's possible.
  if (!isStarted())
    return;

  if (!handleAnimationPolicy(RestartOnceTimerIfNotPaused))
    return;

  cancelAnimationFrame();

  if (!isPaused())
    synchronizeToDocumentTimeline();

#if ENABLE(ASSERT)
  m_preventScheduledAnimationsChanges = true;
#endif
  for (const auto& entry : m_scheduledAnimations) {
    if (!entry.key.first)
      continue;

    AnimationsLinkedHashSet* scheduled = entry.value.get();
    for (SVGSMILElement* element : *scheduled)
      element->reset();
  }
#if ENABLE(ASSERT)
  m_preventScheduledAnimationsChanges = false;
#endif

  updateAnimationsAndScheduleFrameIfNeeded(elapsed, true);
}

void SMILTimeContainer::scheduleAnimationFrame(double delayTime) {
  DCHECK(std::isfinite(delayTime));
  DCHECK(isTimelineRunning());
  DCHECK(!m_wakeupTimer.isActive());

  if (delayTime < AnimationTimeline::s_minimumDelay) {
    serviceOnNextFrame();
  } else {
    scheduleWakeUp(delayTime - AnimationTimeline::s_minimumDelay,
                   FutureAnimationFrame);
  }
}

void SMILTimeContainer::cancelAnimationFrame() {
  m_frameSchedulingState = Idle;
  m_wakeupTimer.stop();
}

void SMILTimeContainer::scheduleWakeUp(
    double delayTime,
    FrameSchedulingState frameSchedulingState) {
  ASSERT(frameSchedulingState == SynchronizeAnimations ||
         frameSchedulingState == FutureAnimationFrame);
  m_wakeupTimer.startOneShot(delayTime, BLINK_FROM_HERE);
  m_frameSchedulingState = frameSchedulingState;
}

void SMILTimeContainer::wakeupTimerFired(TimerBase*) {
  ASSERT(m_frameSchedulingState == SynchronizeAnimations ||
         m_frameSchedulingState == FutureAnimationFrame);
  if (m_frameSchedulingState == FutureAnimationFrame) {
    ASSERT(isTimelineRunning());
    m_frameSchedulingState = Idle;
    serviceOnNextFrame();
  } else {
    m_frameSchedulingState = Idle;
    updateAnimationsAndScheduleFrameIfNeeded(elapsed());
  }
}

void SMILTimeContainer::scheduleAnimationPolicyTimer() {
  m_animationPolicyOnceTimer.startOneShot(animationPolicyOnceDuration,
                                          BLINK_FROM_HERE);
}

void SMILTimeContainer::cancelAnimationPolicyTimer() {
  m_animationPolicyOnceTimer.stop();
}

void SMILTimeContainer::animationPolicyTimerFired(TimerBase*) {
  pause();
}

ImageAnimationPolicy SMILTimeContainer::animationPolicy() const {
  Settings* settings = document().settings();
  if (!settings)
    return ImageAnimationPolicyAllowed;

  return settings->imageAnimationPolicy();
}

bool SMILTimeContainer::handleAnimationPolicy(
    AnimationPolicyOnceAction onceAction) {
  ImageAnimationPolicy policy = animationPolicy();
  // If the animation policy is "none", control is not allowed.
  // returns false to exit flow.
  if (policy == ImageAnimationPolicyNoAnimation)
    return false;
  // If the animation policy is "once",
  if (policy == ImageAnimationPolicyAnimateOnce) {
    switch (onceAction) {
      case RestartOnceTimerIfNotPaused:
        if (isPaused())
          break;
      /* fall through */
      case RestartOnceTimer:
        scheduleAnimationPolicyTimer();
        break;
      case CancelOnceTimer:
        cancelAnimationPolicyTimer();
        break;
    }
  }
  if (policy == ImageAnimationPolicyAllowed) {
    // When the SVG owner element becomes detached from its document,
    // the policy defaults to ImageAnimationPolicyAllowed; there's
    // no way back. If the policy had been "once" prior to that,
    // ensure cancellation of its timer.
    if (onceAction == CancelOnceTimer)
      cancelAnimationPolicyTimer();
  }
  return true;
}

void SMILTimeContainer::updateDocumentOrderIndexes() {
  unsigned timingElementCount = 0;
  for (SVGSMILElement& element :
       Traversal<SVGSMILElement>::descendantsOf(ownerSVGElement()))
    element.setDocumentOrderIndex(timingElementCount++);
  m_documentOrderIndexesDirty = false;
}

struct PriorityCompare {
  PriorityCompare(double elapsed) : m_elapsed(elapsed) {}
  bool operator()(const Member<SVGSMILElement>& a,
                  const Member<SVGSMILElement>& b) {
    // FIXME: This should also consider possible timing relations between the
    // elements.
    SMILTime aBegin = a->intervalBegin();
    SMILTime bBegin = b->intervalBegin();
    // Frozen elements need to be prioritized based on their previous interval.
    aBegin = a->isFrozen() && m_elapsed < aBegin ? a->previousIntervalBegin()
                                                 : aBegin;
    bBegin = b->isFrozen() && m_elapsed < bBegin ? b->previousIntervalBegin()
                                                 : bBegin;
    if (aBegin == bBegin)
      return a->documentOrderIndex() < b->documentOrderIndex();
    return aBegin < bBegin;
  }
  double m_elapsed;
};

SVGSVGElement& SMILTimeContainer::ownerSVGElement() const {
  return *m_ownerSVGElement;
}

Document& SMILTimeContainer::document() const {
  return ownerSVGElement().document();
}

void SMILTimeContainer::serviceOnNextFrame() {
  if (document().view()) {
    document().view()->scheduleAnimation();
    m_frameSchedulingState = AnimationFrame;
  }
}

void SMILTimeContainer::serviceAnimations() {
  if (m_frameSchedulingState != AnimationFrame)
    return;

  m_frameSchedulingState = Idle;
  updateAnimationsAndScheduleFrameIfNeeded(elapsed());
}

bool SMILTimeContainer::canScheduleFrame(SMILTime earliestFireTime) const {
  // If there's synchronization pending (most likely due to syncbases), then
  // let that complete first before attempting to schedule a frame.
  if (hasPendingSynchronization())
    return false;
  if (!isTimelineRunning())
    return false;
  return earliestFireTime.isFinite();
}

void SMILTimeContainer::updateAnimationsAndScheduleFrameIfNeeded(
    double elapsed,
    bool seekToTime) {
  if (!document().isActive())
    return;

  SMILTime earliestFireTime = updateAnimations(elapsed, seekToTime);
  if (!canScheduleFrame(earliestFireTime))
    return;
  double delayTime = earliestFireTime.value() - elapsed;
  scheduleAnimationFrame(delayTime);
}

SMILTime SMILTimeContainer::updateAnimations(double elapsed, bool seekToTime) {
  ASSERT(document().isActive());
  SMILTime earliestFireTime = SMILTime::unresolved();

#if ENABLE(ASSERT)
  // This boolean will catch any attempts to schedule/unschedule
  // scheduledAnimations during this critical section.  Similarly, any elements
  // removed will unschedule themselves, so this will catch modification of
  // animationsToApply.
  m_preventScheduledAnimationsChanges = true;
#endif

  if (m_documentOrderIndexesDirty)
    updateDocumentOrderIndexes();

  HeapHashSet<ElementAttributePair> invalidKeys;
  using AnimationsVector = HeapVector<Member<SVGSMILElement>>;
  AnimationsVector animationsToApply;
  AnimationsVector scheduledAnimationsInSameGroup;
  for (const auto& entry : m_scheduledAnimations) {
    if (!entry.key.first || entry.value->isEmpty()) {
      invalidKeys.add(entry.key);
      continue;
    }

    // Sort according to priority. Elements with later begin time have higher
    // priority.  In case of a tie, document order decides.
    // FIXME: This should also consider timing relationships between the
    // elements. Dependents have higher priority.
    copyToVector(*entry.value, scheduledAnimationsInSameGroup);
    std::sort(scheduledAnimationsInSameGroup.begin(),
              scheduledAnimationsInSameGroup.end(), PriorityCompare(elapsed));

    AnimationsVector sandwich;
    for (const auto& itAnimation : scheduledAnimationsInSameGroup) {
      SVGSMILElement* animation = itAnimation.get();
      DCHECK_EQ(animation->timeContainer(), this);
      DCHECK(animation->hasValidTarget());

      // This will calculate the contribution from the animation and update
      // timing.
      if (animation->progress(elapsed, seekToTime)) {
        sandwich.append(animation);
      } else {
        animation->clearAnimatedType();
      }

      SMILTime nextFireTime = animation->nextProgressTime();
      if (nextFireTime.isFinite())
        earliestFireTime = std::min(nextFireTime, earliestFireTime);
    }

    if (!sandwich.isEmpty()) {
      // Results are accumulated to the first animation that animates and
      // contributes to a particular element/attribute pair.
      // Only reset the animated type to the base value once for
      // the lowest priority animation that animates and
      // contributes to a particular element/attribute pair.
      SVGSMILElement* resultElement = sandwich.first();
      resultElement->resetAnimatedType();

      // Go through the sandwich from lowest prio to highest and generate
      // the animated value (if any.)
      for (const auto& animation : sandwich)
        animation->updateAnimatedValue(resultElement);

      animationsToApply.append(resultElement);
    }
  }
  m_scheduledAnimations.removeAll(invalidKeys);

  if (animationsToApply.isEmpty()) {
#if ENABLE(ASSERT)
    m_preventScheduledAnimationsChanges = false;
#endif
    return earliestFireTime;
  }

  UseCounter::count(&document(), UseCounter::SVGSMILAnimationAppliedEffect);

  std::sort(animationsToApply.begin(), animationsToApply.end(),
            PriorityCompare(elapsed));

  // Apply results to target elements.
  for (const auto& timedElement : animationsToApply)
    timedElement->applyResultsToTarget();

#if ENABLE(ASSERT)
  m_preventScheduledAnimationsChanges = false;
#endif

  for (const auto& timedElement : animationsToApply) {
    if (timedElement->isConnected() && timedElement->isSVGDiscardElement()) {
      SVGElement* targetElement = timedElement->targetElement();
      if (targetElement && targetElement->isConnected()) {
        targetElement->remove(IGNORE_EXCEPTION);
        DCHECK(!targetElement->isConnected());
      }

      if (timedElement->isConnected()) {
        timedElement->remove(IGNORE_EXCEPTION);
        DCHECK(!timedElement->isConnected());
      }
    }
  }
  return earliestFireTime;
}

void SMILTimeContainer::advanceFrameForTesting() {
  setElapsed(elapsed() + initialFrameDelay);
}

DEFINE_TRACE(SMILTimeContainer) {
  visitor->trace(m_scheduledAnimations);
  visitor->trace(m_ownerSVGElement);
}

}  // namespace blink
