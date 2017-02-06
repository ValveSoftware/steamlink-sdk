// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkInformation_h
#define NetworkInformation_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/page/NetworkStateNotifier.h"
#include "public/platform/WebConnectionType.h"

namespace blink {

class ExecutionContext;

class NetworkInformation final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public NetworkStateNotifier::NetworkStateObserver {
    USING_GARBAGE_COLLECTED_MIXIN(NetworkInformation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static NetworkInformation* create(ExecutionContext*);
    ~NetworkInformation() override;

    String type() const;
    double downlinkMax() const;

    // NetworkStateObserver overrides.
    void connectionChange(WebConnectionType, double downlinkMaxMbps) override;

    // EventTarget overrides.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;
    void removeAllEventListeners() override;

    // ActiveScriptWrappable
    bool hasPendingActivity() const final;

    // ActiveDOMObject overrides.
    void stop() override;

    DECLARE_VIRTUAL_TRACE();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(typechange); // Deprecated

protected:
    // EventTarget overrides.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) final;
    void removedEventListener(const AtomicString& eventType, const RegisteredEventListener&) final;

private:
    explicit NetworkInformation(ExecutionContext*);
    void startObserving();
    void stopObserving();

    // Touched only on context thread.
    WebConnectionType m_type;

    // Touched only on context thread.
    double m_downlinkMaxMbps;

    // Whether this object is listening for events from NetworkStateNotifier.
    bool m_observing;

    // Whether ActiveDOMObject::stop has been called.
    bool m_contextStopped;
};

} // namespace blink

#endif // NetworkInformation_h
