/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "modules/mediastream/MediaStreamTrackSourcesRequestImpl.h"

#include "modules/mediastream/MediaStreamTrackSourcesCallback.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSourceInfo.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<MediaStreamTrackSourcesRequestImpl> MediaStreamTrackSourcesRequestImpl::create(const String& origin, PassOwnPtr<MediaStreamTrackSourcesCallback> callback)
{
    return adoptRefWillBeNoop(new MediaStreamTrackSourcesRequestImpl(origin, callback));
}

MediaStreamTrackSourcesRequestImpl::MediaStreamTrackSourcesRequestImpl(const String& origin, PassOwnPtr<MediaStreamTrackSourcesCallback> callback)
    : m_callback(callback)
    , m_scheduledEventTimer(this, &MediaStreamTrackSourcesRequestImpl::scheduledEventTimerFired)
{
    m_origin = origin;
}

MediaStreamTrackSourcesRequestImpl::~MediaStreamTrackSourcesRequestImpl()
{
}

void MediaStreamTrackSourcesRequestImpl::requestSucceeded(const blink::WebVector<blink::WebSourceInfo>& webSourceInfos)
{
    ASSERT(m_callback && !m_scheduledEventTimer.isActive());

    for (size_t i = 0; i < webSourceInfos.size(); ++i)
        m_sourceInfos.append(SourceInfo::create(webSourceInfos[i]));

    m_protect = this;
    m_scheduledEventTimer.startOneShot(0, FROM_HERE);
}

void MediaStreamTrackSourcesRequestImpl::scheduledEventTimerFired(Timer<MediaStreamTrackSourcesRequestImpl>*)
{
    m_callback->handleEvent(m_sourceInfos);
    m_callback.clear();
    m_protect.release();
}

void MediaStreamTrackSourcesRequestImpl::trace(Visitor* visitor)
{
    visitor->trace(m_sourceInfos);
    visitor->trace(m_protect);
    MediaStreamTrackSourcesRequest::trace(visitor);
}

} // namespace WebCore
