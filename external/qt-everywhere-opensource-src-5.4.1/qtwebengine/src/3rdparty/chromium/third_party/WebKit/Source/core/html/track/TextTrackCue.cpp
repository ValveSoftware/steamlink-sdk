/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"
#include "core/html/track/TextTrackCue.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/events/Event.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCueList.h"

namespace WebCore {

static const int invalidCueIndex = -1;

TextTrackCue::TextTrackCue(double start, double end)
    : m_startTime(start)
    , m_endTime(end)
    , m_cueIndex(invalidCueIndex)
    , m_track(nullptr)
    , m_isActive(false)
    , m_pauseOnExit(false)
{
}

void TextTrackCue::cueWillChange()
{
    if (m_track)
        m_track->cueWillChange(this);
}

void TextTrackCue::cueDidChange()
{
    if (m_track)
        m_track->cueDidChange(this);
}

TextTrack* TextTrackCue::track() const
{
    return m_track;
}

void TextTrackCue::setTrack(TextTrack* track)
{
    m_track = track;
}

Node* TextTrackCue::owner() const
{
    return m_track ? m_track->owner() : 0;
}

void TextTrackCue::setId(const AtomicString& id)
{
    if (m_id == id)
        return;

    cueWillChange();
    m_id = id;
    cueDidChange();
}

void TextTrackCue::setStartTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_startTime == value || value < 0)
        return;

    cueWillChange();
    m_startTime = value;
    cueDidChange();
}

void TextTrackCue::setEndTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_endTime == value || value < 0)
        return;

    cueWillChange();
    m_endTime = value;
    cueDidChange();
}

void TextTrackCue::setPauseOnExit(bool value)
{
    if (m_pauseOnExit == value)
        return;

    cueWillChange();
    m_pauseOnExit = value;
    cueDidChange();
}

int TextTrackCue::cueIndex()
{
    if (m_cueIndex == invalidCueIndex)
        m_cueIndex = track()->cues()->getCueIndex(this);

    return m_cueIndex;
}

void TextTrackCue::invalidateCueIndex()
{
    m_cueIndex = invalidCueIndex;
}

bool TextTrackCue::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::disabledKeyword())
        return false;

    return EventTarget::dispatchEvent(event);
}

bool TextTrackCue::isActive()
{
    return m_isActive && track() && track()->mode() != TextTrack::disabledKeyword();
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;

    // Remove the display tree as soon as the cue becomes inactive.
    if (!active)
        removeDisplayTree();
}

const AtomicString& TextTrackCue::interfaceName() const
{
    return EventTargetNames::TextTrackCue;
}

void TextTrackCue::trace(Visitor* visitor)
{
    visitor->trace(m_track);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
