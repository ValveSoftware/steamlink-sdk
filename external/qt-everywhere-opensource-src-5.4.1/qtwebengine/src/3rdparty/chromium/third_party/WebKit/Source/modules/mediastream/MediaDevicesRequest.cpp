/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "modules/mediastream/MediaDevicesRequest.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "modules/mediastream/UserMediaController.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<MediaDevicesRequest> MediaDevicesRequest::create(ExecutionContext* context, UserMediaController* controller, PassOwnPtr<MediaDeviceInfoCallback> callback, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<MediaDevicesRequest> request = adoptRefWillBeRefCountedGarbageCollected(new MediaDevicesRequest(context, controller, callback));
    request->suspendIfNeeded();
    return request.release();
}

MediaDevicesRequest::MediaDevicesRequest(ExecutionContext* context, UserMediaController* controller, PassOwnPtr<MediaDeviceInfoCallback> callback)
    : ActiveDOMObject(context)
    , m_controller(controller)
    , m_callback(callback)
{
}

MediaDevicesRequest::~MediaDevicesRequest()
{
}

Document* MediaDevicesRequest::ownerDocument()
{
    if (ExecutionContext* context = executionContext()) {
        return toDocument(context);
    }

    return 0;
}

void MediaDevicesRequest::start()
{
    if (m_controller)
        m_controller->requestMediaDevices(this);
}

void MediaDevicesRequest::succeed(const MediaDeviceInfoVector& mediaDevices)
{
    if (!executionContext() || !m_callback)
        return;

    m_callback->handleEvent(mediaDevices);
}

void MediaDevicesRequest::stop()
{
    m_callback.clear();
    m_controller = 0;
}

} // namespace WebCore
