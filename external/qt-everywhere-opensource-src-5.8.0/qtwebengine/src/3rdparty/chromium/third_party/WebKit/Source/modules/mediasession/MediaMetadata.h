// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaMetadata_h
#define MediaMetadata_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/WebMediaMetadata.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class MediaArtwork;
class MediaMetadataInit;

// Implementation of MediaMetadata interface from the Media Session API.
class MODULES_EXPORT MediaMetadata final
    : public GarbageCollectedFinalized<MediaMetadata>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaMetadata* create(ExecutionContext*, const MediaMetadataInit&);

    String title() const;
    String artist() const;
    String album() const;
    const HeapVector<Member<MediaArtwork>>& artwork() const;

    explicit operator WebMediaMetadata() const;

    DECLARE_VIRTUAL_TRACE();

private:
    MediaMetadata(ExecutionContext*, const MediaMetadataInit&);

    String m_title;
    String m_artist;
    String m_album;
    HeapVector<Member<MediaArtwork>> m_artwork;
};

} // namespace blink

#endif // MediaMetadata_h
