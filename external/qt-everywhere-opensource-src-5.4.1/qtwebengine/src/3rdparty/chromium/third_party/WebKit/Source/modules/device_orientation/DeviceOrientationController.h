// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationController_h
#define DeviceOrientationController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DeviceSingleWindowEventController.h"

namespace WebCore {

class DeviceOrientationData;
class Event;

class DeviceOrientationController FINAL : public NoBaseWillBeGarbageCollectedFinalized<DeviceOrientationController>, public DeviceSingleWindowEventController, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceOrientationController);
public:
    virtual ~DeviceOrientationController();

    static const char* supplementName();
    static DeviceOrientationController& from(Document&);

    // Inherited from DeviceSingleWindowEventController.
    void didUpdateData() OVERRIDE;

    void setOverride(DeviceOrientationData*);
    void clearOverride();

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit DeviceOrientationController(Document&);

    // Inherited from DeviceEventControllerBase.
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;
    virtual bool hasLastData() OVERRIDE;

    // Inherited from DeviceSingleWindowEventController.
    virtual PassRefPtrWillBeRawPtr<Event> lastEvent() const OVERRIDE;
    virtual const AtomicString& eventTypeName() const OVERRIDE;
    virtual bool isNullEvent(Event*) const OVERRIDE;

    DeviceOrientationData* lastData() const;

    RefPtrWillBeMember<DeviceOrientationData> m_overrideOrientationData;
};

} // namespace WebCore

#endif // DeviceOrientationController_h
