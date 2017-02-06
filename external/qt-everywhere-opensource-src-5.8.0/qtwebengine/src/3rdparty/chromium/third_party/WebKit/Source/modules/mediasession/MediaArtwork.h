// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaArtwork_h
#define MediaArtwork_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/WebMediaArtwork.h"

namespace blink {

class ExecutionContext;
class MediaArtworkInit;

class MODULES_EXPORT MediaArtwork final
    : public GarbageCollectedFinalized<MediaArtwork>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaArtwork* create(ExecutionContext*, const MediaArtworkInit&);

    String src() const;
    String sizes() const;
    String type() const;

    WebMediaArtwork* data() { return &m_data; }

    DEFINE_INLINE_TRACE() { }

private:
    MediaArtwork(ExecutionContext*, const MediaArtworkInit&);

    WebMediaArtwork m_data;
};

}

#endif // MediaArtwork_h
