// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemotePlaybackAvailability_h
#define RemotePlaybackAvailability_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose whether there is a remote playback device available for a media
// element. The object will be initialized with a default value passed via
// ::take() and will then listen to availability changes.
class RemotePlaybackAvailability final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ContextLifecycleObserver {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(RemotePlaybackAvailability);
public:
    static RemotePlaybackAvailability* take(ScriptPromiseResolver*, bool);
    ~RemotePlaybackAvailability() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    void availabilityChanged(bool);

    bool value() const;

    // ActiveScriptWrappable implementation.
    bool hasPendingActivity() const final;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    DECLARE_VIRTUAL_TRACE();

private:
    RemotePlaybackAvailability(ExecutionContext*, bool);

    bool m_value;
};

} // namespace blink

#endif // RemotePlaybackAvailability_h
