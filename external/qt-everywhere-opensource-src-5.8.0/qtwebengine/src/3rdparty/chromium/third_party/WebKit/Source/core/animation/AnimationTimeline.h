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

#ifndef AnimationTimeline_h
#define AnimationTimeline_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/animation/EffectModel.h"
#include "core/dom/Element.h"
#include "platform/Timer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class Document;
class AnimationEffect;

// AnimationTimeline is constructed and owned by Document, and tied to its lifecycle.
class CORE_EXPORT AnimationTimeline final : public GarbageCollectedFinalized<AnimationTimeline>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    USING_PRE_FINALIZER(AnimationTimeline, dispose);
public:
    class PlatformTiming : public GarbageCollectedFinalized<PlatformTiming> {
    public:
        // Calls AnimationTimeline's wake() method after duration seconds.
        virtual void wakeAfter(double duration) = 0;
        virtual void serviceOnNextFrame() = 0;
        virtual ~PlatformTiming() { }
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    static AnimationTimeline* create(Document*, PlatformTiming* = nullptr);
    ~AnimationTimeline();
    void dispose();

    void serviceAnimations(TimingUpdateReason);
    void scheduleNextService();

    Animation* play(AnimationEffect*);
    HeapVector<Member<Animation>> getAnimations();

    void animationAttached(Animation&);

    bool isActive();
    bool hasPendingUpdates() const { return !m_animationsNeedingUpdate.isEmpty(); }
    double zeroTime();
    double currentTime(bool& isNull);
    double currentTime();
    double currentTimeInternal(bool& isNull);
    double currentTimeInternal();
    void setCurrentTime(double);
    void setCurrentTimeInternal(double);
    double effectiveTime();
    void pauseAnimationsForTesting(double);

    void setAllCompositorPending(bool sourceChanged = false);
    void setOutdatedAnimation(Animation*);
    void clearOutdatedAnimation(Animation*);
    bool hasOutdatedAnimation() const { return m_outdatedAnimationCount > 0; }
    bool needsAnimationTimingUpdate();
    void invalidateKeyframeEffects();

    void setPlaybackRate(double);
    double playbackRate() const;

    CompositorAnimationTimeline* compositorTimeline() const { return m_compositorTimeline.get(); }

    Document* document() { return m_document.get(); }
    void wake();
    void resetForTesting();

    DECLARE_TRACE();

protected:
    AnimationTimeline(Document*, PlatformTiming*);

private:
    Member<Document> m_document;
    double m_zeroTime;
    bool m_zeroTimeInitialized;
    unsigned m_outdatedAnimationCount;
    // Animations which will be updated on the next frame
    // i.e. current, in effect, or had timing changed
    HeapHashSet<Member<Animation>> m_animationsNeedingUpdate;
    HeapHashSet<WeakMember<Animation>> m_animations;

    double m_playbackRate;

    friend class SMILTimeContainer;
    static const double s_minimumDelay;

    Member<PlatformTiming> m_timing;
    double m_lastCurrentTimeInternal;

    std::unique_ptr<CompositorAnimationTimeline> m_compositorTimeline;

    class AnimationTimelineTiming final : public PlatformTiming {
    public:
        AnimationTimelineTiming(AnimationTimeline* timeline)
            : m_timeline(timeline)
            , m_timer(this, &AnimationTimelineTiming::timerFired)
        {
            ASSERT(m_timeline);
        }

        void wakeAfter(double duration) override;
        void serviceOnNextFrame() override;

        void timerFired(Timer<AnimationTimelineTiming>*) { m_timeline->wake(); }

        DECLARE_VIRTUAL_TRACE();

    private:
        Member<AnimationTimeline> m_timeline;
        Timer<AnimationTimelineTiming> m_timer;
    };

    friend class AnimationAnimationTimelineTest;
};

} // namespace blink

#endif
