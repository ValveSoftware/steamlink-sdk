// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailability_h
#define PresentationAvailability_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/page/PageLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose whether there is a presentation display available for |url|. The
// object will be initialized with a default value passed via ::take() and will
// then subscribe to receive callbacks if the status for |url| were to
// change. The object will only listen to changes when required.
class MODULES_EXPORT PresentationAvailability final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public PageLifecycleObserver
    , public WebPresentationAvailabilityObserver {
    USING_GARBAGE_COLLECTED_MIXIN(PresentationAvailability);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PresentationAvailability* take(ScriptPromiseResolver*, const KURL&, bool);
    ~PresentationAvailability() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // WebPresentationAvailabilityObserver implementation.
    void availabilityChanged(bool) override;
    const WebURL url() const override;

    // ActiveScriptWrappable implementation.
    bool hasPendingActivity() const final;

    // ActiveDOMObject implementation.
    void suspend() override;
    void resume() override;
    void stop() override;

    // PageLifecycleObserver implementation.
    void pageVisibilityChanged() override;

    bool value() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    DECLARE_VIRTUAL_TRACE();

protected:
    // EventTarget implementation.
    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;

private:
    // Current state of the ActiveDOMObject. It is Active when created. It
    // becomes Suspended when suspend() is called and moves back to Active if
    // resume() is called. It becomes Inactive when stop() is called or at
    // destruction time.
    enum class State : char {
        Active,
        Suspended,
        Inactive,
    };

    PresentationAvailability(ExecutionContext*, const KURL&, bool);

    void setState(State);
    void updateListening();

    const KURL m_url;
    bool m_value;
    State m_state;
};

} // namespace blink

#endif // PresentationAvailability_h
