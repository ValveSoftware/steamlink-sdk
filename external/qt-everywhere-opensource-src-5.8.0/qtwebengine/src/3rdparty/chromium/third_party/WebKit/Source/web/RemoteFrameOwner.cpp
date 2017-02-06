// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "web/RemoteFrameOwner.h"

#include "core/frame/LocalFrame.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

RemoteFrameOwner::RemoteFrameOwner(SandboxFlags flags, const WebFrameOwnerProperties& frameOwnerProperties)
    : m_sandboxFlags(flags)
    , m_scrolling(static_cast<ScrollbarMode>(frameOwnerProperties.scrollingMode))
    , m_marginWidth(frameOwnerProperties.marginWidth)
    , m_marginHeight(frameOwnerProperties.marginHeight)
    , m_allowFullscreen(frameOwnerProperties.allowFullscreen)
{
}

DEFINE_TRACE(RemoteFrameOwner)
{
    visitor->trace(m_frame);
    FrameOwner::trace(visitor);
}

void RemoteFrameOwner::setScrollingMode(WebFrameOwnerProperties::ScrollingMode mode)
{
    m_scrolling = static_cast<ScrollbarMode>(mode);
}

void RemoteFrameOwner::setContentFrame(Frame& frame)
{
    m_frame = &frame;
}

void RemoteFrameOwner::clearContentFrame()
{
    DCHECK_EQ(m_frame->owner(), this);
    m_frame = nullptr;
}

void RemoteFrameOwner::dispatchLoad()
{
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(toLocalFrame(*m_frame));
    webFrame->client()->dispatchLoad();
}

} // namespace blink
