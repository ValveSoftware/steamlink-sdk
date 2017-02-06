// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/AudioTrack.h"

#include "core/html/HTMLMediaElement.h"

namespace blink {

AudioTrack::AudioTrack(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool enabled)
    : TrackBase(WebMediaPlayer::AudioTrack, kind, label, language, id)
    , m_enabled(enabled)
{
}

AudioTrack::~AudioTrack()
{
}

DEFINE_TRACE(AudioTrack)
{
    TrackBase::trace(visitor);
}

void AudioTrack::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    m_enabled = enabled;

    if (mediaElement())
        mediaElement()->audioTrackChanged(id(), enabled);
}

const AtomicString& AudioTrack::alternativeKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("alternative"));
    return keyword;
}

const AtomicString& AudioTrack::descriptionsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("descriptions"));
    return keyword;
}

const AtomicString& AudioTrack::mainKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main"));
    return keyword;
}

const AtomicString& AudioTrack::mainDescriptionsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main-desc"));
    return keyword;
}

const AtomicString& AudioTrack::translationKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("translation"));
    return keyword;
}

const AtomicString& AudioTrack::commentaryKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("commentary"));
    return keyword;
}

bool AudioTrack::isValidKindKeyword(const String& kind)
{
    return kind == alternativeKeyword()
        || kind == descriptionsKeyword()
        || kind == mainKeyword()
        || kind == mainDescriptionsKeyword()
        || kind == translationKeyword()
        || kind == commentaryKeyword()
        || kind == emptyAtom;
}

} // namespace blink
