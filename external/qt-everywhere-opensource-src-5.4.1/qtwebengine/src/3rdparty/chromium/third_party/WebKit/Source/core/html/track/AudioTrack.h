// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioTrack_h
#define AudioTrack_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/track/TrackBase.h"

namespace WebCore {

class AudioTrack FINAL : public TrackBase, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<AudioTrack> create(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool enabled)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new AudioTrack(id, kind, label, language, enabled));
    }
    virtual ~AudioTrack();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool);

    // Valid kind keywords.
    static const AtomicString& alternativeKeyword();
    static const AtomicString& descriptionsKeyword();
    static const AtomicString& mainKeyword();
    static const AtomicString& mainDescriptionsKeyword();
    static const AtomicString& translationKeyword();
    static const AtomicString& commentaryKeyword();

private:
    AudioTrack(const String& id, const AtomicString& kind, const AtomicString& label, const AtomicString& language, bool enabled);

    // TrackBase
    virtual bool isValidKind(const AtomicString&) const OVERRIDE;
    virtual AtomicString defaultKind() const OVERRIDE;

    bool m_enabled;
};

}

#endif
