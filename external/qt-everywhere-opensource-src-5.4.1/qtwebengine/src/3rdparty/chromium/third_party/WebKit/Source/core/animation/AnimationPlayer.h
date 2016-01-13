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

#ifndef AnimationPlayer_h
#define AnimationPlayer_h

#include "core/animation/AnimationNode.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class AnimationTimeline;
class ExceptionState;

class AnimationPlayer FINAL : public RefCountedWillBeRefCountedGarbageCollected<AnimationPlayer>
    , public ActiveDOMObject
    , public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(AnimationPlayer);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AnimationPlayer);
public:

    ~AnimationPlayer();
    static PassRefPtrWillBeRawPtr<AnimationPlayer> create(ExecutionContext*, AnimationTimeline&, AnimationNode*);

    // Returns whether the player is finished.
    bool update(TimingUpdateReason);

    // timeToEffectChange returns:
    //  infinity  - if this player is no longer in effect
    //  0         - if this player requires an update on the next frame
    //  n         - if this player requires an update after 'n' units of time
    double timeToEffectChange();

    void cancel();

    double currentTime();
    void setCurrentTime(double newCurrentTime);

    double currentTimeInternal();
    void setCurrentTimeInternal(double newCurrentTime);

    bool paused() const { return m_paused && !m_isPausedForTesting; }
    void pause();
    void play();
    void reverse();
    void finish(ExceptionState&);
    bool finished() { return limited(currentTimeInternal()); }
    // FIXME: Resolve whether finished() should just return the flag, and
    // remove this method.
    bool finishedInternal() const { return m_finished; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(finish);

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual bool dispatchEvent(PassRefPtrWillBeRawPtr<Event>) OVERRIDE;

    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);
    const AnimationTimeline* timeline() const { return m_timeline; }
    AnimationTimeline* timeline() { return m_timeline; }

#if !ENABLE(OILPAN)
    void timelineDestroyed() { m_timeline = nullptr; }
#endif

    bool hasStartTime() const { return !isNull(m_startTime); }
    double startTime() const { return m_startTime * 1000; }
    double startTimeInternal() const { return m_startTime; }
    void setStartTime(double startTime) { setStartTimeInternal(startTime / 1000); }
    void setStartTimeInternal(double, bool isUpdateFromCompositor = false);

    const AnimationNode* source() const { return m_content.get(); }
    AnimationNode* source() { return m_content.get(); }
    AnimationNode* source(bool& isNull) { isNull = !m_content; return m_content.get(); }
    void setSource(AnimationNode*);

    double timeLag() { return timeLagInternal() * 1000; }
    double timeLagInternal() { return currentTimeWithoutLag() - currentTimeInternal(); }

    // Pausing via this method is not reflected in the value returned by
    // paused() and must never overlap with pausing via pause().
    void pauseForTesting(double pauseTime);
    // This should only be used for CSS
    void unpause();

    void setOutdated();
    bool outdated() { return m_outdated; }

    bool canStartAnimationOnCompositor();
    bool maybeStartAnimationOnCompositor();
    void cancelAnimationOnCompositor();
    void schedulePendingAnimationOnCompositor();
    bool hasActiveAnimationsOnCompositor();

    class SortInfo {
    public:
        friend class AnimationPlayer;
        bool operator<(const SortInfo& other) const;
        double startTime() const { return m_startTime; }
    private:
        SortInfo(unsigned sequenceNumber, double startTime)
            : m_sequenceNumber(sequenceNumber)
            , m_startTime(startTime)
        {
        }
        unsigned m_sequenceNumber;
        double m_startTime;
    };

    const SortInfo& sortInfo() const { return m_sortInfo; }

    static bool hasLowerPriority(AnimationPlayer* player1, AnimationPlayer* player2)
    {
        return player1->sortInfo() < player2->sortInfo();
    }

#if !ENABLE(OILPAN)
    // Checks if the AnimationStack is the last reference holder to the Player.
    // This won't be needed when AnimationPlayer is moved to Oilpan.
    bool canFree() const;
#endif

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    AnimationPlayer(ExecutionContext*, AnimationTimeline&, AnimationNode*);
    double sourceEnd() const;
    bool limited(double currentTime) const;
    double currentTimeWithoutLag() const;
    double currentTimeWithLag() const;
    void updateTimingState(double newCurrentTime);
    void updateCurrentTimingState();

    double m_playbackRate;
    double m_startTime;
    double m_holdTime;
    double m_storedTimeLag;

    SortInfo m_sortInfo;

    RefPtrWillBeMember<AnimationNode> m_content;
    RawPtrWillBeMember<AnimationTimeline> m_timeline;
    // Reflects all pausing, including via pauseForTesting().
    bool m_paused;
    bool m_held;
    bool m_isPausedForTesting;

    // This indicates timing information relevant to the player has changed by
    // means other than the ordinary progression of time
    bool m_outdated;

    bool m_finished;
    // Holds a 'finished' event queued for asynchronous dispatch via the
    // ScriptedAnimationController. This object remains active until the
    // event is actually dispatched.
    RefPtrWillBeMember<Event> m_pendingFinishedEvent;
};

} // namespace

#endif
