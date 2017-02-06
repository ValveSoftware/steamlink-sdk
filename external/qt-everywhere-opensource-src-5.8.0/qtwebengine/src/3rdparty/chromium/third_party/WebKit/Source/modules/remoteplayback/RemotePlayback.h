// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlayback_h
#define RemotePlayback_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class HTMLMediaElement;
class LocalFrame;
class RemotePlaybackAvailability;
class ScriptPromiseResolver;

class RemotePlayback final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , private WebRemotePlaybackClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(RemotePlayback);
public:
    static RemotePlayback* create(HTMLMediaElement&);

    // EventTarget implementation.
    const WTF::AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    ScriptPromise getAvailability(ScriptState*);
    ScriptPromise connect(ScriptState*);

    String state() const;

    // ActiveScriptWrappable implementation.
    bool hasPendingActivity() const final;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit RemotePlayback(HTMLMediaElement&);

    void stateChanged(WebRemotePlaybackState) override;
    void availabilityChanged(bool available) override;
    void connectCancelled() override;

    WebRemotePlaybackState m_state;
    bool m_availability;
    HeapVector<Member<RemotePlaybackAvailability>> m_availabilityObjects;
    Member<HTMLMediaElement> m_mediaElement;
    HeapVector<Member<ScriptPromiseResolver>> m_connectPromiseResolvers;
};

} // namespace blink

#endif // RemotePlayback_h
