// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSession_h
#define MediaSession_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/WebMediaSession.h"
#include <memory>

namespace blink {

class MediaMetadata;
class ScriptState;

class MODULES_EXPORT MediaSession final
    : public GarbageCollectedFinalized<MediaSession>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaSession* create(ExecutionContext*, ExceptionState&);

    WebMediaSession* getWebMediaSession() { return m_webMediaSession.get(); }

    ScriptPromise activate(ScriptState*);
    ScriptPromise deactivate(ScriptState*);

    void setMetadata(MediaMetadata*);
    MediaMetadata* metadata() const;

    DECLARE_VIRTUAL_TRACE();

private:
    friend class MediaSessionTest;

    explicit MediaSession(std::unique_ptr<WebMediaSession>);

    std::unique_ptr<WebMediaSession> m_webMediaSession;
    Member<MediaMetadata> m_metadata;
};

} // namespace blink

#endif // MediaSession_h
