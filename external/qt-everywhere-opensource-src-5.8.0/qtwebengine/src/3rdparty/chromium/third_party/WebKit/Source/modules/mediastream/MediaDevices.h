// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDevices_h
#define MediaDevices_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "platform/AsyncMethodRunner.h"

namespace blink {

class MediaStreamConstraints;
class MediaTrackSupportedConstraints;
class ScriptState;
class UserMediaController;

class MODULES_EXPORT MediaDevices final : public EventTargetWithInlineData, public ActiveScriptWrappable, public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(MediaDevices);
    DEFINE_WRAPPERTYPEINFO();
    USING_PRE_FINALIZER(MediaDevices, dispose);
public:
    static MediaDevices* create(ExecutionContext*);
    ~MediaDevices() override;

    ScriptPromise enumerateDevices(ScriptState*);
    void getSupportedConstraints(MediaTrackSupportedConstraints& result) { }
    ScriptPromise getUserMedia(ScriptState*, const MediaStreamConstraints&, ExceptionState&);
    void didChangeMediaDevices();

    // EventTarget overrides.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;
    void removeAllEventListeners() override;

    // ActiveScriptWrappable
    bool hasPendingActivity() const override;

    // ActiveDOMObject overrides.
    void stop() override;
    void suspend() override;
    void resume() override;

    DECLARE_VIRTUAL_TRACE();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange);

protected:
    // EventTarget overrides.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;
    void removedEventListener(const AtomicString& eventType, const RegisteredEventListener&) override;

private:
    explicit MediaDevices(ExecutionContext*);
    void scheduleDispatchEvent(Event*);
    void dispatchScheduledEvent();
    void startObserving();
    void stopObserving();
    UserMediaController* getUserMediaController();
    void dispose();

    bool m_observing;
    bool m_stopped;
    Member<AsyncMethodRunner<MediaDevices>> m_dispatchScheduledEventRunner;
    HeapVector<Member<Event>> m_scheduledEvents;
};

} // namespace blink

#endif
