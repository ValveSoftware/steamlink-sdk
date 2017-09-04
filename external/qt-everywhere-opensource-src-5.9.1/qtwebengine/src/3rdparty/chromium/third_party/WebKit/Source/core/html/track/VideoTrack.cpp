// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/VideoTrack.h"

#include "core/html/HTMLMediaElement.h"

namespace blink {

VideoTrack::VideoTrack(const String& id,
                       const AtomicString& kind,
                       const AtomicString& label,
                       const AtomicString& language,
                       bool selected)
    : TrackBase(WebMediaPlayer::VideoTrack, kind, label, language, id),
      m_selected(selected) {}

VideoTrack::~VideoTrack() {}

DEFINE_TRACE(VideoTrack) {
  TrackBase::trace(visitor);
}

void VideoTrack::setSelected(bool selected) {
  if (selected == m_selected)
    return;

  m_selected = selected;

  if (mediaElement())
    mediaElement()->selectedVideoTrackChanged(this);
}

const AtomicString& VideoTrack::alternativeKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("alternative"));
  return keyword;
}

const AtomicString& VideoTrack::captionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("captions"));
  return keyword;
}

const AtomicString& VideoTrack::mainKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main"));
  return keyword;
}

const AtomicString& VideoTrack::signKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("sign"));
  return keyword;
}

const AtomicString& VideoTrack::subtitlesKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("subtitles"));
  return keyword;
}

const AtomicString& VideoTrack::commentaryKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("commentary"));
  return keyword;
}

bool VideoTrack::isValidKindKeyword(const String& kind) {
  return kind == alternativeKeyword() || kind == captionsKeyword() ||
         kind == mainKeyword() || kind == signKeyword() ||
         kind == subtitlesKeyword() || kind == commentaryKeyword() ||
         kind == emptyAtom;
}

}  // namespace blink
