/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserMediaController_h
#define UserMediaController_h

#include "core/frame/LocalFrame.h"
#include "modules/mediastream/UserMediaClient.h"

namespace blink {

class MediaDevices;
class MediaDevicesRequest;
class UserMediaRequest;

class UserMediaController final : public GarbageCollected<UserMediaController>, public Supplement<LocalFrame> {
    USING_GARBAGE_COLLECTED_MIXIN(UserMediaController);
public:
    static UserMediaController* create(UserMediaClient*);

    DECLARE_VIRTUAL_TRACE();

    UserMediaClient* client() const { return m_client; }

    void requestUserMedia(UserMediaRequest*);
    void cancelUserMediaRequest(UserMediaRequest*);

    void requestMediaDevices(MediaDevicesRequest*);
    void cancelMediaDevicesRequest(MediaDevicesRequest*);

    void requestSources(MediaStreamTrackSourcesRequest*);

    void setMediaDeviceChangeObserver(MediaDevices*);

    static const char* supplementName();
    static UserMediaController* from(LocalFrame* frame) { return static_cast<UserMediaController*>(Supplement<LocalFrame>::from(frame, supplementName())); }

private:
    explicit UserMediaController(UserMediaClient*);

    UserMediaClient* m_client;
};

inline void UserMediaController::requestUserMedia(UserMediaRequest* request)
{
    m_client->requestUserMedia(request);
}

inline void UserMediaController::cancelUserMediaRequest(UserMediaRequest* request)
{
    m_client->cancelUserMediaRequest(request);
}

inline void UserMediaController::requestMediaDevices(MediaDevicesRequest* request)
{
    m_client->requestMediaDevices(request);
}

inline void UserMediaController::cancelMediaDevicesRequest(MediaDevicesRequest* request)
{
    m_client->cancelMediaDevicesRequest(request);
}

inline void UserMediaController::requestSources(MediaStreamTrackSourcesRequest* request)
{
    m_client->requestSources(request);
}

inline void UserMediaController::setMediaDeviceChangeObserver(MediaDevices* observer)
{
    m_client->setMediaDeviceChangeObserver(observer);
}

} // namespace blink

#endif // UserMediaController_h
