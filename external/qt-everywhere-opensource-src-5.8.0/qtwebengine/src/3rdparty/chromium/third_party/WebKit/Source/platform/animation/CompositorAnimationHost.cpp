// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationHost.h"

#include "cc/animation/scroll_offset_animations.h"

namespace blink {

CompositorAnimationHost::CompositorAnimationHost(cc::AnimationHost* host)
    : m_animationHost(host) {}

bool CompositorAnimationHost::isNull() const
{
    return !m_animationHost;
}

void CompositorAnimationHost::adjustImplOnlyScrollOffsetAnimation(cc::ElementId elementId, const gfx::Vector2dF& adjustment)
{
    if (!m_animationHost)
        return;

    m_animationHost->scroll_offset_animations().AddAdjustmentUpdate(elementId, adjustment);
}

void CompositorAnimationHost::takeOverImplOnlyScrollOffsetAnimation(cc::ElementId elementId)
{
    if (!m_animationHost)
        return;

    m_animationHost->scroll_offset_animations().AddTakeoverUpdate(elementId);
}

} // namespace blink
