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

#ifndef SVGImageChromeClient_h
#define SVGImageChromeClient_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/loader/EmptyClients.h"
#include "platform/Timer.h"
#include <memory>

namespace blink {

class SVGImage;

class CORE_EXPORT SVGImageChromeClient final : public EmptyChromeClient {
public:
    static SVGImageChromeClient* create(SVGImage*);

    bool isSVGImageChromeClient() const override;

    SVGImage* image() const { return m_image; }

    void suspendAnimation();
    void resumeAnimation();
    bool isSuspended() const { return m_timelineState >= Suspended; }

private:
    explicit SVGImageChromeClient(SVGImage*);

    void chromeDestroyed() override;
    void invalidateRect(const IntRect&) override;
    void scheduleAnimation(Widget*) override;

    void setTimer(Timer<SVGImageChromeClient>*);
    void animationTimerFired(Timer<SVGImageChromeClient>*);

    SVGImage* m_image;
    std::unique_ptr<Timer<SVGImageChromeClient>> m_animationTimer;
    enum {
        Running,
        Suspended,
        SuspendedWithAnimationPending,
    } m_timelineState;

    FRIEND_TEST_ALL_PREFIXES(SVGImageTest, TimelineSuspendAndResume);
    FRIEND_TEST_ALL_PREFIXES(SVGImageTest, ResetAnimation);
};

DEFINE_TYPE_CASTS(SVGImageChromeClient, ChromeClient, client, client->isSVGImageChromeClient(), client.isSVGImageChromeClient());

} // namespace blink

#endif // SVGImageChromeClient_h
