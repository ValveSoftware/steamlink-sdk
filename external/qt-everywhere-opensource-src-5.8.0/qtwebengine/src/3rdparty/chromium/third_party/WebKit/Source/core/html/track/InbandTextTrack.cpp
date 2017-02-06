/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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

#include "core/html/track/InbandTextTrack.h"

#include "core/html/HTMLMediaElement.h"
#include "core/html/track/vtt/VTTCue.h"
#include "public/platform/WebInbandTextTrack.h"
#include "public/platform/WebString.h"

using blink::WebInbandTextTrack;
using blink::WebString;

namespace blink {

namespace {

const AtomicString& textTrackKindToString(WebInbandTextTrack::Kind kind)
{
    switch (kind) {
    case WebInbandTextTrack::KindSubtitles:
        return TextTrack::subtitlesKeyword();
    case WebInbandTextTrack::KindCaptions:
        return TextTrack::captionsKeyword();
    case WebInbandTextTrack::KindDescriptions:
        return TextTrack::descriptionsKeyword();
    case WebInbandTextTrack::KindChapters:
        return TextTrack::chaptersKeyword();
    case WebInbandTextTrack::KindMetadata:
        return TextTrack::metadataKeyword();
    case WebInbandTextTrack::KindNone:
    default:
        break;
    }
    NOTREACHED();
    return TextTrack::subtitlesKeyword();
}

} // namespace

InbandTextTrack* InbandTextTrack::create(WebInbandTextTrack* webTrack)
{
    return new InbandTextTrack(webTrack);
}

InbandTextTrack::InbandTextTrack(WebInbandTextTrack* webTrack)
    : TextTrack(textTrackKindToString(webTrack->kind()), webTrack->label(), webTrack->language(), webTrack->id(), InBand)
    , m_webTrack(webTrack)
{
    m_webTrack->setClient(this);
}

InbandTextTrack::~InbandTextTrack()
{
    if (m_webTrack)
        m_webTrack->setClient(nullptr);
}

void InbandTextTrack::setTrackList(TextTrackList* trackList)
{
    TextTrack::setTrackList(trackList);
    if (trackList)
        return;

    DCHECK(m_webTrack);
    m_webTrack->setClient(nullptr);
    m_webTrack = nullptr;
}

void InbandTextTrack::addWebVTTCue(double start, double end, const WebString& id, const WebString& content, const WebString& settings)
{
    HTMLMediaElement* owner = mediaElement();
    DCHECK(owner);
    VTTCue* cue = VTTCue::create(owner->document(), start, end, content);
    cue->setId(id);
    cue->parseSettings(settings);
    addCue(cue);
}

} // namespace blink
