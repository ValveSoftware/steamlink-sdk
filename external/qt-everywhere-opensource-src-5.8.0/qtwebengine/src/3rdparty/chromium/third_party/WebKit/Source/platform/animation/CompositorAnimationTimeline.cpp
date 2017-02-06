// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationTimeline.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"

namespace blink {

CompositorAnimationTimeline::CompositorAnimationTimeline()
    : m_animationTimeline(cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId()))
{
}

CompositorAnimationTimeline::~CompositorAnimationTimeline()
{
    // Detach timeline from host, otherwise it stays there (leaks) until
    // compositor shutdown.
    if (m_animationTimeline->animation_host())
        m_animationTimeline->animation_host()->RemoveAnimationTimeline(m_animationTimeline);
}

cc::AnimationTimeline* CompositorAnimationTimeline::animationTimeline() const
{
    return m_animationTimeline.get();
}

CompositorAnimationHost CompositorAnimationTimeline::compositorAnimationHost()
{
    return CompositorAnimationHost(m_animationTimeline->animation_host());
}

void CompositorAnimationTimeline::playerAttached(const blink::CompositorAnimationPlayerClient& client)
{
    if (client.compositorPlayer())
        m_animationTimeline->AttachPlayer(client.compositorPlayer()->animationPlayer());
}

void CompositorAnimationTimeline::playerDestroyed(const blink::CompositorAnimationPlayerClient& client)
{
    if (client.compositorPlayer())
        m_animationTimeline->DetachPlayer(client.compositorPlayer()->animationPlayer());
}

} // namespace blink
