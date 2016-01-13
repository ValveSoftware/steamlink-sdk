// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightController_h
#define DeviceLightController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DeviceSingleWindowEventController.h"

namespace WebCore {

class LocalDOMWindow;
class Event;

class DeviceLightController FINAL : public NoBaseWillBeGarbageCollectedFinalized<DeviceLightController>, public DeviceSingleWindowEventController, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceLightController);
public:
    virtual ~DeviceLightController();

    static const char* supplementName();
    static DeviceLightController& from(Document&);

private:
    explicit DeviceLightController(Document&);

    // Inherited from DeviceEventControllerBase.
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;
    virtual bool hasLastData() OVERRIDE;

    // Inherited from DeviceSingleWindowEventController.
    virtual PassRefPtrWillBeRawPtr<Event> lastEvent() const OVERRIDE;
    virtual const AtomicString& eventTypeName() const OVERRIDE;
    virtual bool isNullEvent(Event*) const OVERRIDE;
};

} // namespace WebCore

#endif // DeviceLightController_h
