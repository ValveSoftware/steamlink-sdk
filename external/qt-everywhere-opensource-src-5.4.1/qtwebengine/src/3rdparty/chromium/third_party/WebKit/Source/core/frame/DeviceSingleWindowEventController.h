// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceSingleWindowEventController_h
#define DeviceSingleWindowEventController_h

#include "core/frame/DOMWindowLifecycleObserver.h"
#include "core/frame/DeviceEventControllerBase.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Document;
class Event;

class DeviceSingleWindowEventController : public DeviceEventControllerBase, public DOMWindowLifecycleObserver {
public:
    // Inherited from DeviceEventControllerBase.
    virtual void didUpdateData() OVERRIDE;

    // Inherited from DOMWindowLifecycleObserver.
    virtual void didAddEventListener(LocalDOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(LocalDOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveAllEventListeners(LocalDOMWindow*) OVERRIDE;

protected:
    explicit DeviceSingleWindowEventController(Document&);
    virtual ~DeviceSingleWindowEventController();

    void dispatchDeviceEvent(const PassRefPtrWillBeRawPtr<Event>);

    virtual PassRefPtrWillBeRawPtr<Event> lastEvent() const = 0;
    virtual const AtomicString& eventTypeName() const = 0;
    virtual bool isNullEvent(Event*) const = 0;

private:
    bool m_needsCheckingNullEvents;
    Document& m_document;
};

} // namespace WebCore

#endif // DeviceSingleWindowEventController_h
