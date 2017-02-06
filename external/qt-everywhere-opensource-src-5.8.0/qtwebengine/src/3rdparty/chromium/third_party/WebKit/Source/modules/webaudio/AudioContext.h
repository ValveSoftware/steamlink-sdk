// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioContext_h
#define AudioContext_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class ExceptionState;
class ScriptState;

// This is an AbstractAudioContext which actually plays sound, unlike an
// OfflineAudioContext which renders sound into a buffer.
class AudioContext : public AbstractAudioContext {
public:
    static AbstractAudioContext* create(Document&, ExceptionState&);

    ~AudioContext() override;
    DECLARE_VIRTUAL_TRACE();

    ScriptPromise closeContext(ScriptState*) final;
    bool isContextClosed() const final;

    ScriptPromise suspendContext(ScriptState*) final;
    ScriptPromise resumeContext(ScriptState*) final;

    bool hasRealtimeConstraint() final { return true; }

protected:
    AudioContext(Document&);

    void didClose() final;

private:
    void stopRendering();

    unsigned m_contextId;
    Member<ScriptPromiseResolver> m_closeResolver;
};

} // namespace blink

#endif // AudioContext_h
