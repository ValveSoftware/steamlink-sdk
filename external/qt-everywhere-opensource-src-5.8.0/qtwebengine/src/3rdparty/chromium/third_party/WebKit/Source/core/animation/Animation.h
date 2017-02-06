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

#ifndef Animation_h
#define Animation_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/animation/AnimationEffect.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMException.h"
#include "core/events/EventTarget.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class AnimationTimeline;
class CompositorAnimationPlayer;
class Element;
class ExceptionState;

class CORE_EXPORT Animation final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public CompositorAnimationDelegate
    , public CompositorAnimationPlayerClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(Animation);
    USING_PRE_FINALIZER(Animation, dispose);
public:
    enum AnimationPlayState {
        Unset,
        Idle,
        Pending,
        Running,
        Paused,
        Finished
    };

    static Animation* create(AnimationEffect*, AnimationTimeline*);
    ~Animation();
    void dispose();

    // Returns whether the animation is finished.
    bool update(TimingUpdateReason);

    // timeToEffectChange returns:
    //  infinity  - if this animation is no longer in effect
    //  0         - if this animation requires an update on the next frame
    //  n         - if this animation requires an update after 'n' units of time
    double timeToEffectChange();

    void cancel();

    double currentTime(bool& isNull);
    double currentTime();
    void setCurrentTime(double newCurrentTime);

    double currentTimeInternal() const;
    double unlimitedCurrentTimeInternal() const;

    void setCurrentTimeInternal(double newCurrentTime, TimingUpdateReason = TimingUpdateOnDemand);
    bool paused() const { return m_paused && !m_isPausedForTesting; }
    static const char* playStateString(AnimationPlayState);
    String playState() const { return playStateString(playStateInternal()); }
    AnimationPlayState playStateInternal() const;

    void pause(ExceptionState& = ASSERT_NO_EXCEPTION);
    void play(ExceptionState& = ASSERT_NO_EXCEPTION);
    void reverse(ExceptionState& = ASSERT_NO_EXCEPTION);
    void finish(ExceptionState& = ASSERT_NO_EXCEPTION);

    ScriptPromise finished(ScriptState*);
    ScriptPromise ready(ScriptState*);

    bool playing() const { return !(playStateInternal() == Idle || limited() || m_paused || m_isPausedForTesting); }
    bool limited() const { return limited(currentTimeInternal()); }
    bool finishedInternal() const { return m_finished; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(finish);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel);

    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;
    bool hasPendingActivity() const final;
    void stop() override;

    double playbackRate() const;
    void setPlaybackRate(double);
    const AnimationTimeline* timeline() const { return m_timeline; }
    AnimationTimeline* timeline() { return m_timeline; }

    double calculateStartTime(double currentTime) const;
    bool hasStartTime() const { return !isNull(m_startTime); }
    double startTime(bool& isNull) const;
    double startTime() const;
    double startTimeInternal() const { return m_startTime; }
    void setStartTime(double);
    void setStartTimeInternal(double);

    const AnimationEffect* effect() const { return m_content.get(); }
    AnimationEffect* effect() { return m_content.get(); }
    void setEffect(AnimationEffect*);

    void setId(const String& id) { m_id = id; }
    const String& id() const { return m_id; }

    // Pausing via this method is not reflected in the value returned by
    // paused() and must never overlap with pausing via pause().
    void pauseForTesting(double pauseTime);
    void disableCompositedAnimationForTesting();

    // This should only be used for CSS
    void unpause();

    void setOutdated();
    bool outdated() { return m_outdated; }

    bool canStartAnimationOnCompositor() const;
    bool isCandidateForAnimationOnCompositor() const;
    bool maybeStartAnimationOnCompositor();
    void cancelAnimationOnCompositor();
    void restartAnimationOnCompositor();
    void cancelIncompatibleAnimationsOnCompositor();
    bool hasActiveAnimationsOnCompositor();
    void setCompositorPending(bool effectChanged = false);
    void notifyCompositorStartTime(double timelineTime);
    void notifyStartTime(double timelineTime);
    // CompositorAnimationPlayerClient implementation.
    CompositorAnimationPlayer* compositorPlayer() const override { return m_compositorPlayer.get(); }

    bool affects(const Element&, CSSPropertyID) const;

    // Returns whether we should continue with the commit for this animation or wait until next commit.
    bool preCommit(int compositorGroup, bool startOnCompositor);
    void postCommit(double timelineTime);

    unsigned sequenceNumber() const { return m_sequenceNumber; }
    int compositorGroup() const { return m_compositorGroup; }

    static bool hasLowerPriority(const Animation* animation1, const Animation* animation2)
    {
        return animation1->sequenceNumber() < animation2->sequenceNumber();
    }

    bool effectSuppressed() const { return m_effectSuppressed; }
    void setEffectSuppressed(bool);

    void invalidateKeyframeEffect();

    DECLARE_VIRTUAL_TRACE();

protected:
    DispatchEventResult dispatchEventInternal(Event*) override;
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;

private:
    Animation(ExecutionContext*, AnimationTimeline&, AnimationEffect*);

    void clearOutdated();

    double effectEnd() const;
    bool limited(double currentTime) const;

    AnimationPlayState calculatePlayState();
    double calculateCurrentTime() const;

    void unpauseInternal();
    void setPlaybackRateInternal(double);
    void updateCurrentTimingState(TimingUpdateReason);

    void beginUpdatingState();
    void endUpdatingState();

    void createCompositorPlayer();
    void destroyCompositorPlayer();
    void attachCompositorTimeline();
    void detachCompositorTimeline();
    void attachCompositedLayers();
    void detachCompositedLayers();
    // CompositorAnimationDelegate implementation.
    void notifyAnimationStarted(double monotonicTime, int group) override;
    void notifyAnimationFinished(double monotonicTime, int group) override { }
    void notifyAnimationAborted(double monotonicTime, int group) override { }

    String m_id;

    AnimationPlayState m_playState;
    double m_playbackRate;
    double m_startTime;
    double m_holdTime;

    unsigned m_sequenceNumber;

    typedef ScriptPromiseProperty<Member<Animation>, Member<Animation>, Member<DOMException>> AnimationPromise;
    Member<AnimationPromise> m_finishedPromise;
    Member<AnimationPromise> m_readyPromise;

    Member<AnimationEffect> m_content;
    Member<AnimationTimeline> m_timeline;

    // Reflects all pausing, including via pauseForTesting().
    bool m_paused;
    bool m_held;
    bool m_isPausedForTesting;
    bool m_isCompositedAnimationDisabledForTesting;

    // This indicates timing information relevant to the animation's effect
    // has changed by means other than the ordinary progression of time
    bool m_outdated;

    bool m_finished;
    // Holds a 'finished' event queued for asynchronous dispatch via the
    // ScriptedAnimationController. This object remains active until the
    // event is actually dispatched.
    Member<Event> m_pendingFinishedEvent;

    Member<Event> m_pendingCancelledEvent;

    enum CompositorAction {
        None,
        Pause,
        Start,
        PauseThenStart
    };

    class CompositorState {
        USING_FAST_MALLOC(CompositorState);
        WTF_MAKE_NONCOPYABLE(CompositorState);
    public:
        CompositorState(Animation& animation)
            : startTime(animation.m_startTime)
            , holdTime(animation.m_holdTime)
            , playbackRate(animation.m_playbackRate)
            , effectChanged(false)
            , pendingAction(Start)
        { }
        double startTime;
        double holdTime;
        double playbackRate;
        bool effectChanged;
        CompositorAction pendingAction;
    };

    enum CompositorPendingChange {
        SetCompositorPending,
        SetCompositorPendingWithEffectChanged,
        DoNotSetCompositorPending,
    };

    class PlayStateUpdateScope {
        STACK_ALLOCATED();
    public:
        PlayStateUpdateScope(Animation&, TimingUpdateReason, CompositorPendingChange = SetCompositorPending);
        ~PlayStateUpdateScope();
    private:
        Member<Animation> m_animation;
        AnimationPlayState m_initialPlayState;
        CompositorPendingChange m_compositorPendingChange;
    };

    // This mirrors the known compositor state. It is created when a compositor
    // animation is started. Updated once the start time is known and each time
    // modifications are pushed to the compositor.
    std::unique_ptr<CompositorState> m_compositorState;
    bool m_compositorPending;
    int m_compositorGroup;

    std::unique_ptr<CompositorAnimationPlayer> m_compositorPlayer;
    bool m_preFinalizerRegistered;

    bool m_currentTimePending;
    bool m_stateIsBeingUpdated;

    bool m_effectSuppressed;
};

} // namespace blink

#endif // Animation_h
