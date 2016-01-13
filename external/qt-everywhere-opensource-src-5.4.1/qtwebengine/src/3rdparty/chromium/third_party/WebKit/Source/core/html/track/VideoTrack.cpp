// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/track/VideoTrack.h"

#include "core/html/HTMLMediaElement.h"

namespace WebCore {

VideoTrack::VideoTrack(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool selected)
    : TrackBase(TrackBase::VideoTrack, label, language, id)
    , m_selected(selected)
{
    ScriptWrappable::init(this);
    setKind(kind);
}

VideoTrack::~VideoTrack()
{
}

void VideoTrack::setSelected(bool selected)
{
    if (selected == m_selected)
        return;

    m_selected = selected;

    if (mediaElement()) {
        blink::WebMediaPlayer::TrackId selectedTrackId = trackId();
        mediaElement()->selectedVideoTrackChanged(selected ? &selectedTrackId : 0);
    }
}

const AtomicString& VideoTrack::alternativeKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("alternative", AtomicString::ConstructFromLiteral));
    return keyword;
}

const AtomicString& VideoTrack::captionsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("captions", AtomicString::ConstructFromLiteral));
    return keyword;
}

const AtomicString& VideoTrack::mainKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main", AtomicString::ConstructFromLiteral));
    return keyword;
}

const AtomicString& VideoTrack::signKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("sign", AtomicString::ConstructFromLiteral));
    return keyword;
}

const AtomicString& VideoTrack::subtitlesKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("subtitles", AtomicString::ConstructFromLiteral));
    return keyword;
}

const AtomicString& VideoTrack::commentaryKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("commentary", AtomicString::ConstructFromLiteral));
    return keyword;
}

bool VideoTrack::isValidKind(const AtomicString& kind) const
{
    return (kind == alternativeKeyword())
        || (kind == captionsKeyword())
        || (kind == mainKeyword())
        || (kind == signKeyword())
        || (kind == subtitlesKeyword())
        || (kind == commentaryKeyword());
}

AtomicString VideoTrack::defaultKind() const
{
    return emptyAtom;
}

}
