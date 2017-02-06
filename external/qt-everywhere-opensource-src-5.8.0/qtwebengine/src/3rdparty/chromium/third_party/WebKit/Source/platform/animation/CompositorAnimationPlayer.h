// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationPlayer_h
#define CompositorAnimationPlayer_h

#include "base/memory/ref_counted.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_player.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CompositorAnimation;
class CompositorAnimationDelegate;
class WebLayer;

// A compositor representation for AnimationPlayer.
class PLATFORM_EXPORT CompositorAnimationPlayer : public cc::AnimationDelegate {
    WTF_MAKE_NONCOPYABLE(CompositorAnimationPlayer);
public:
    static std::unique_ptr<CompositorAnimationPlayer> create()
    {
        return wrapUnique(new CompositorAnimationPlayer());
    }

    ~CompositorAnimationPlayer();

    cc::AnimationPlayer* animationPlayer() const;

    // An animation delegate is notified when animations are started and
    // stopped. The CompositorAnimationPlayer does not take ownership of the delegate, and it is
    // the responsibility of the client to reset the layer's delegate before
    // deleting the delegate.
    void setAnimationDelegate(CompositorAnimationDelegate*);

    void attachElement(const CompositorElementId&);
    void detachElement();
    bool isElementAttached() const;

    void addAnimation(CompositorAnimation*);
    void removeAnimation(uint64_t animationId);
    void pauseAnimation(uint64_t animationId, double timeOffset);
    void abortAnimation(uint64_t animationId);

private:
    CompositorAnimationPlayer();

    // cc::AnimationDelegate implementation.
    void NotifyAnimationStarted(base::TimeTicks monotonicTime, cc::TargetProperty::Type, int group) override;
    void NotifyAnimationFinished(base::TimeTicks monotonicTime, cc::TargetProperty::Type, int group) override;
    void NotifyAnimationAborted(base::TimeTicks monotonicTime, cc::TargetProperty::Type, int group) override;
    void NotifyAnimationTakeover(base::TimeTicks monotonicTime, cc::TargetProperty::Type, double animationStartTime, std::unique_ptr<cc::AnimationCurve>) override;

    scoped_refptr<cc::AnimationPlayer> m_animationPlayer;
    CompositorAnimationDelegate* m_delegate;
};

} // namespace blink

#endif // CompositorAnimationPlayer_h
