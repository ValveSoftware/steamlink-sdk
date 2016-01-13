/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InbandTextTrack_h
#define InbandTextTrack_h

#include "core/html/track/TextTrack.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebInbandTextTrackClient.h"
#include "wtf/RefPtr.h"

namespace blink {
class WebInbandTextTrack;
class WebString;
}

namespace WebCore {

class Document;
class MediaPlayer;
class TextTrackCue;

class InbandTextTrack FINAL : public TextTrack, public blink::WebInbandTextTrackClient {
public:
    static PassRefPtrWillBeRawPtr<InbandTextTrack> create(Document&, blink::WebInbandTextTrack*);
    virtual ~InbandTextTrack();

    size_t inbandTrackIndex();
    virtual void setTrackList(TextTrackList*) OVERRIDE FINAL;

private:
    InbandTextTrack(Document&, blink::WebInbandTextTrack*);

    virtual void addWebVTTCue(double, double, const blink::WebString&, const blink::WebString&, const blink::WebString&) OVERRIDE;

    blink::WebInbandTextTrack* m_webTrack;
};

} // namespace WebCore

#endif
