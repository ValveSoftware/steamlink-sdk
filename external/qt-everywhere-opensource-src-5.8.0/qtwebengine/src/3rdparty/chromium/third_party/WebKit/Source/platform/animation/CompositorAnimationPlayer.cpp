// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "public/platform/WebLayer.h"

namespace blink {

CompositorAnimationPlayer::CompositorAnimationPlayer()
    : m_animationPlayer(cc::AnimationPlayer::Create(cc::AnimationIdProvider::NextPlayerId()))
    , m_delegate()
{
}

CompositorAnimationPlayer::~CompositorAnimationPlayer()
{
    setAnimationDelegate(nullptr);
    // Detach player from timeline, otherwise it stays there (leaks) until
    // compositor shutdown.
    if (m_animationPlayer->animation_timeline())
        m_animationPlayer->animation_timeline()->DetachPlayer(m_animationPlayer);
}

cc::AnimationPlayer* CompositorAnimationPlayer::animationPlayer() const
{
    return m_animationPlayer.get();
}

void CompositorAnimationPlayer::setAnimationDelegate(CompositorAnimationDelegate* delegate)
{
    m_delegate = delegate;
    m_animationPlayer->set_animation_delegate(delegate ? this : nullptr);
}

void CompositorAnimationPlayer::attachElement(const CompositorElementId& id)
{
    m_animationPlayer->AttachElement(id);
}

void CompositorAnimationPlayer::detachElement()
{
    m_animationPlayer->DetachElement();
}

bool CompositorAnimationPlayer::isElementAttached() const
{
    return !!m_animationPlayer->element_id();
}

void CompositorAnimationPlayer::addAnimation(CompositorAnimation* animation)
{
    m_animationPlayer->AddAnimation(animation->passAnimation());
    delete animation;
}

void CompositorAnimationPlayer::removeAnimation(uint64_t animationId)
{
    m_animationPlayer->RemoveAnimation(animationId);
}

void CompositorAnimationPlayer::pauseAnimation(uint64_t animationId, double timeOffset)
{
    m_animationPlayer->PauseAnimation(animationId, timeOffset);
}

void CompositorAnimationPlayer::abortAnimation(uint64_t animationId)
{
    m_animationPlayer->AbortAnimation(animationId);
}

void CompositorAnimationPlayer::NotifyAnimationStarted(
    base::TimeTicks monotonicTime,
    cc::TargetProperty::Type targetProperty,
    int group)
{
    if (m_delegate)
        m_delegate->notifyAnimationStarted((monotonicTime - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationFinished(
    base::TimeTicks monotonicTime,
    cc::TargetProperty::Type targetProperty,
    int group)
{
    if (m_delegate)
        m_delegate->notifyAnimationFinished((monotonicTime - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationAborted(
    base::TimeTicks monotonicTime,
    cc::TargetProperty::Type targetProperty,
    int group)
{
    if (m_delegate)
        m_delegate->notifyAnimationAborted((monotonicTime - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationTakeover(
    base::TimeTicks monotonicTime,
    cc::TargetProperty::Type,
    double animationStartTime,
    std::unique_ptr<cc::AnimationCurve> curve)
{
    if (m_delegate) {
        m_delegate->notifyAnimationTakeover(
            (monotonicTime - base::TimeTicks()).InSecondsF(),
            animationStartTime,
            std::move(curve));
    }
}

} // namespace blink
