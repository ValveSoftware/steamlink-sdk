/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2012, 2013 Apple Inc.  All rights reserved.
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

#ifndef TextTrackCue_h
#define TextTrackCue_h

#include "core/events/EventTarget.h"
#include "core/html/HTMLDivElement.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;

class TextTrackCue : public RefCountedWillBeRefCountedGarbageCollected<TextTrackCue>, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(TextTrackCue);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TextTrackCue);
public:
    static const AtomicString& cueShadowPseudoId()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, cue, ("cue", AtomicString::ConstructFromLiteral));
        return cue;
    }

    virtual ~TextTrackCue() { }

    TextTrack* track() const;
    void setTrack(TextTrack*);

    Node* owner() const;

    const AtomicString& id() const { return m_id; }
    void setId(const AtomicString&);

    double startTime() const { return m_startTime; }
    void setStartTime(double);

    double endTime() const { return m_endTime; }
    void setEndTime(double);

    bool pauseOnExit() const { return m_pauseOnExit; }
    void setPauseOnExit(bool);

    int cueIndex();
    void invalidateCueIndex();

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtrWillBeRawPtr<Event>) OVERRIDE;

    bool isActive();
    void setIsActive(bool);

    virtual void updateDisplay(const IntSize& videoSize, HTMLDivElement& container) = 0;

    // FIXME: Consider refactoring to eliminate or merge the following three members.
    // https://code.google.com/p/chromium/issues/detail?id=322434
    virtual void updateDisplayTree(double movieTime) = 0;
    virtual void removeDisplayTree() = 0;
    virtual void notifyRegionWhenRemovingDisplayTree(bool notifyRegion) = 0;

    virtual const AtomicString& interfaceName() const OVERRIDE;

#ifndef NDEBUG
    virtual String toString() const = 0;
#endif

    DEFINE_ATTRIBUTE_EVENT_LISTENER(enter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(exit);

    virtual void trace(Visitor*) OVERRIDE;

protected:
    TextTrackCue(double start, double end);

    void cueWillChange();
    virtual void cueDidChange();

private:
    AtomicString m_id;
    double m_startTime;
    double m_endTime;
    int m_cueIndex;

    RawPtrWillBeMember<TextTrack> m_track;

    bool m_isActive : 1;
    bool m_pauseOnExit : 1;
};

} // namespace WebCore

#endif
