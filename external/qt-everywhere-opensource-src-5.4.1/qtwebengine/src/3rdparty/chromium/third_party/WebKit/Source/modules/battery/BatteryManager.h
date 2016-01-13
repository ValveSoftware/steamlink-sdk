// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryManager_h
#define BatteryManager_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/frame/DeviceEventControllerBase.h"
#include "modules/EventTargetModules.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class BatteryStatus;

class BatteryManager FINAL : public RefCountedWillBeRefCountedGarbageCollected<BatteryManager>, public ActiveDOMObject, public DeviceEventControllerBase, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(BatteryManager);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BatteryManager);
public:
    virtual ~BatteryManager();
    static PassRefPtrWillBeRawPtr<BatteryManager> create(ExecutionContext*);

    // Returns a promise object that will be resolved with this BatteryManager.
    ScriptPromise startRequest(ScriptState*);

    // EventTarget implementation.
    virtual const WTF::AtomicString& interfaceName() const OVERRIDE { return EventTargetNames::BatteryManager; }
    virtual ExecutionContext* executionContext() const OVERRIDE { return ContextLifecycleObserver::executionContext(); }

    bool charging();
    double chargingTime();
    double dischargingTime();
    double level();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dischargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(levelchange);

    // Inherited from DeviceEventControllerBase.
    virtual void didUpdateData() OVERRIDE;
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;
    virtual bool hasLastData() OVERRIDE;

    // ActiveDOMObject implementation.
    virtual bool canSuspend() const { return true; }
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    enum State {
        NotStarted,
        Pending,
        Resolved,
    };

    explicit BatteryManager(ExecutionContext*);

    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
    RefPtrWillBeMember<BatteryStatus> m_batteryStatus;
    State m_state;
};

}

#endif // BatteryManager_h
