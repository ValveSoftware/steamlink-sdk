// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageAnimator_h
#define PageAnimator_h

#include "core/CoreExport.h"
#include "core/animation/AnimationClock.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class Page;

class CORE_EXPORT PageAnimator final : public GarbageCollected<PageAnimator> {
public:
    static PageAnimator* create(Page&);
    DECLARE_TRACE();
    void scheduleVisualUpdate(LocalFrame*);
    void serviceScriptedAnimations(double monotonicAnimationStartTime);

    bool isServicingAnimations() const { return m_servicingAnimations; }

    // See documents of methods with the same names in FrameView class.
    void updateAllLifecyclePhases(LocalFrame& rootFrame);
    AnimationClock& clock() { return m_animationClock; }

private:
    explicit PageAnimator(Page&);

    Member<Page> m_page;
    bool m_servicingAnimations;
    bool m_updatingLayoutAndStyleForPainting;
    AnimationClock m_animationClock;
};

} // namespace blink

#endif // PageAnimator_h
