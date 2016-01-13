// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoTrack_h
#define VideoTrack_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/track/TrackBase.h"

namespace WebCore {

class VideoTrack FINAL : public TrackBase, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<VideoTrack> create(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool selected)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new VideoTrack(id, kind, label, language, selected));
    }
    virtual ~VideoTrack();

    bool selected() const { return m_selected; }
    void setSelected(bool);

    // Set selected to false without notifying the owner media element. Used when
    // another video track is selected, implicitly deselecting this one.
    void clearSelected() { m_selected = false; }

    // Valid kind keywords.
    static const AtomicString& alternativeKeyword();
    static const AtomicString& captionsKeyword();
    static const AtomicString& mainKeyword();
    static const AtomicString& signKeyword();
    static const AtomicString& subtitlesKeyword();
    static const AtomicString& commentaryKeyword();

private:
    VideoTrack(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool selected);

    // TrackBase
    virtual bool isValidKind(const AtomicString&) const OVERRIDE;
    virtual AtomicString defaultKind() const OVERRIDE;

    bool m_selected;
};

}

#endif
