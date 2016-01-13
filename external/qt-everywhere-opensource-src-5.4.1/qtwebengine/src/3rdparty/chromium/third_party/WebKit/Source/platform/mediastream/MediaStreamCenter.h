/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamCenter_h
#define MediaStreamCenter_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebMediaStreamCenterClient.h"
#include "public/platform/WebVector.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebMediaStream;
class WebMediaStreamCenter;
class WebMediaStreamTrack;
}

namespace WebCore {

class AudioSourceProvider;
class MediaStreamComponent;
class MediaStreamDescriptor;
class MediaStreamTrackSourcesRequest;

class PLATFORM_EXPORT MediaStreamCenter FINAL : public blink::WebMediaStreamCenterClient {
    WTF_MAKE_NONCOPYABLE(MediaStreamCenter);
public:
    virtual ~MediaStreamCenter();

    static MediaStreamCenter& instance();

    bool getMediaStreamTrackSources(PassRefPtrWillBeRawPtr<MediaStreamTrackSourcesRequest>);

    void didCreateMediaStreamTrack(MediaStreamComponent*);
    void didSetMediaStreamTrackEnabled(MediaStreamComponent*);
    bool didStopMediaStreamTrack(MediaStreamComponent*);
    PassOwnPtr<AudioSourceProvider> createWebAudioSourceFromMediaStreamTrack(MediaStreamComponent*);

    void didCreateMediaStream(MediaStreamDescriptor*);
    void didCreateMediaStreamAndTracks(MediaStreamDescriptor*);
    bool didAddMediaStreamTrack(MediaStreamDescriptor*, MediaStreamComponent*);
    bool didRemoveMediaStreamTrack(MediaStreamDescriptor*, MediaStreamComponent*);
    void didStopLocalMediaStream(MediaStreamDescriptor*);

    // blink::WebMediaStreamCenterClient
    virtual void stopLocalMediaStream(const blink::WebMediaStream&) OVERRIDE;

private:
    MediaStreamCenter();

    OwnPtr<blink::WebMediaStreamCenter> m_private;
};

} // namespace WebCore

#endif // MediaStreamCenter_h
