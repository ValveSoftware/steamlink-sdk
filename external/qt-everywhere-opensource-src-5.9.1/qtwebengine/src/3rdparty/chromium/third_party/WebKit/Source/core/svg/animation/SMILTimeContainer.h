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

#ifndef SMILTimeContainer_h
#define SMILTimeContainer_h

#include "core/dom/QualifiedName.h"
#include "platform/Timer.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class SMILTime;
class SVGElement;
class SVGSMILElement;
class SVGSVGElement;

class SMILTimeContainer : public GarbageCollectedFinalized<SMILTimeContainer> {
 public:
  static SMILTimeContainer* create(SVGSVGElement& owner) {
    return new SMILTimeContainer(owner);
  }
  ~SMILTimeContainer();

  void schedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
  void unschedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
  void notifyIntervalsChanged();

  double elapsed() const;

  bool isPaused() const;
  bool isStarted() const;

  void start();
  void pause();
  void resume();
  void setElapsed(double);

  void serviceAnimations();
  bool hasAnimations() const;

  void setDocumentOrderIndexesDirty() { m_documentOrderIndexesDirty = true; }

  // Advance the animation timeline a single frame.
  void advanceFrameForTesting();

  DECLARE_TRACE();

 private:
  explicit SMILTimeContainer(SVGSVGElement& owner);

  enum FrameSchedulingState {
    // No frame scheduled.
    Idle,
    // Scheduled a wakeup to update the animation values.
    SynchronizeAnimations,
    // Scheduled a wakeup to trigger an animation frame.
    FutureAnimationFrame,
    // Scheduled a animation frame for continuous update.
    AnimationFrame
  };

  enum AnimationPolicyOnceAction {
    // Restart OnceTimer if the timeline is not paused.
    RestartOnceTimerIfNotPaused,
    // Restart OnceTimer.
    RestartOnceTimer,
    // Cancel OnceTimer.
    CancelOnceTimer
  };

  bool isTimelineRunning() const;
  void synchronizeToDocumentTimeline();
  void scheduleAnimationFrame(double delayTime);
  void cancelAnimationFrame();
  void wakeupTimerFired(TimerBase*);
  void scheduleAnimationPolicyTimer();
  void cancelAnimationPolicyTimer();
  void animationPolicyTimerFired(TimerBase*);
  ImageAnimationPolicy animationPolicy() const;
  bool handleAnimationPolicy(AnimationPolicyOnceAction);
  bool canScheduleFrame(SMILTime earliestFireTime) const;
  void updateAnimationsAndScheduleFrameIfNeeded(double elapsed,
                                                bool seekToTime = false);
  SMILTime updateAnimations(double elapsed, bool seekToTime);
  void serviceOnNextFrame();
  void scheduleWakeUp(double delayTime, FrameSchedulingState);
  bool hasPendingSynchronization() const;

  void updateDocumentOrderIndexes();

  SVGSVGElement& ownerSVGElement() const;
  Document& document() const;

  // The latest "restart" time for the time container's timeline. If the
  // timeline has not been manipulated (seeked, paused) this will be zero.
  double m_presentationTime;
  // The time on the document timeline corresponding to |m_presentationTime|.
  double m_referenceTime;

  FrameSchedulingState m_frameSchedulingState;
  bool m_started;  // The timeline has been started.
  bool m_paused;   // The timeline is paused.

  bool m_documentOrderIndexesDirty;

  Timer<SMILTimeContainer> m_wakeupTimer;
  Timer<SMILTimeContainer> m_animationPolicyOnceTimer;

  using ElementAttributePair = std::pair<WeakMember<SVGElement>, QualifiedName>;
  using AnimationsLinkedHashSet = HeapLinkedHashSet<WeakMember<SVGSMILElement>>;
  using GroupedAnimationsMap =
      HeapHashMap<ElementAttributePair, Member<AnimationsLinkedHashSet>>;
  GroupedAnimationsMap m_scheduledAnimations;

  Member<SVGSVGElement> m_ownerSVGElement;

#if ENABLE(ASSERT)
  bool m_preventScheduledAnimationsChanges;
#endif
};
}  // namespace blink

#endif  // SMILTimeContainer_h
