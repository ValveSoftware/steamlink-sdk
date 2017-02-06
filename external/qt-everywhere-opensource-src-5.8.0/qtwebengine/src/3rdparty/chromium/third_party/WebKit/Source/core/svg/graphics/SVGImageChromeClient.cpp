/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/graphics/SVGImageChromeClient.h"

#include "core/svg/graphics/SVGImage.h"
#include "platform/graphics/ImageObserver.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"

namespace blink {

static const double animationFrameDelay = 0.025;

SVGImageChromeClient::SVGImageChromeClient(SVGImage* image)
    : m_image(image)
    , m_animationTimer(
        wrapUnique(new Timer<SVGImageChromeClient>(
            this, &SVGImageChromeClient::animationTimerFired)))
    , m_timelineState(Running)
{
}

SVGImageChromeClient* SVGImageChromeClient::create(SVGImage* image)
{
    return new SVGImageChromeClient(image);
}

bool SVGImageChromeClient::isSVGImageChromeClient() const
{
    return true;
}

void SVGImageChromeClient::chromeDestroyed()
{
    m_image = nullptr;
}

void SVGImageChromeClient::invalidateRect(const IntRect& r)
{
    // If m_image->m_page is null, we're being destructed, don't fire changedInRect() in that case.
    if (m_image && m_image->getImageObserver() && m_image->m_page)
        m_image->getImageObserver()->changedInRect(m_image, r);
}

void SVGImageChromeClient::suspendAnimation()
{
    if (m_image->hasAnimations()) {
        m_timelineState = SuspendedWithAnimationPending;
    } else {
        // Preserve SuspendedWithAnimationPending if set.
        m_timelineState = std::max(m_timelineState, Suspended);
    }
}

void SVGImageChromeClient::resumeAnimation()
{
    bool havePendingAnimation = m_timelineState == SuspendedWithAnimationPending;
    m_timelineState = Running;

    // If an animation frame was pending/requested while animations were
    // suspended, schedule a new animation frame.
    if (!havePendingAnimation)
        return;
    scheduleAnimation(nullptr);
}

void SVGImageChromeClient::scheduleAnimation(Widget*)
{
    // Because a single SVGImage can be shared by multiple pages, we can't key
    // our svg image layout on the page's real animation frame. Therefore, we
    // run this fake animation timer to trigger layout in SVGImages. The name,
    // "animationTimer", is to match the new requestAnimationFrame-based layout
    // approach.
    if (m_animationTimer->isActive())
        return;
    // Schedule the 'animation' ASAP if the image does not contain any
    // animations, but prefer a fixed, jittery, frame-delay if there're any
    // animations. Checking for pending/active animations could be more
    // stringent.
    double fireTime = 0;
    if (m_image->hasAnimations()) {
        if (m_timelineState >= Suspended)
            return;
        fireTime = animationFrameDelay;
    }
    m_animationTimer->startOneShot(fireTime, BLINK_FROM_HERE);
}

void SVGImageChromeClient::setTimer(Timer<SVGImageChromeClient>* timer)
{
    m_animationTimer = wrapUnique(timer);
}

void SVGImageChromeClient::animationTimerFired(Timer<SVGImageChromeClient>*)
{
    if (!m_image)
        return;

    // The SVGImageChromeClient object's lifetime is dependent on
    // the ImageObserver (an ImageResource) of its image. Should it
    // be dead and about to be lazily swept out, do not proceed.
    //
    // TODO(Oilpan): move (SVG)Image to the Oilpan heap, and avoid
    // this explicit lifetime check.
    if (ThreadHeap::willObjectBeLazilySwept(m_image->getImageObserver()))
        return;

    m_image->serviceAnimations(monotonicallyIncreasingTime());
}

} // namespace blink
