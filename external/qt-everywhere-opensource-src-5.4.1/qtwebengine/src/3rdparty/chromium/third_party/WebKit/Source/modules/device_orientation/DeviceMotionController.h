// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceMotionController_h
#define DeviceMotionController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DeviceSingleWindowEventController.h"

namespace WebCore {

class DeviceMotionData;
class Event;

class DeviceMotionController FINAL : public NoBaseWillBeGarbageCollectedFinalized<DeviceMotionController>, public DeviceSingleWindowEventController, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceMotionController);
public:
    virtual ~DeviceMotionController();

    static const char* supplementName();
    static DeviceMotionController& from(Document&);

private:
    explicit DeviceMotionController(Document&);

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

#endif // DeviceMotionController_h
