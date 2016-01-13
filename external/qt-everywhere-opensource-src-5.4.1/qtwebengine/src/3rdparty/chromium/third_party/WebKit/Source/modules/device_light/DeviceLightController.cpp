// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_light/DeviceLightController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/page/Page.h"
#include "modules/device_light/DeviceLightDispatcher.h"
#include "modules/device_light/DeviceLightEvent.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace WebCore {

DeviceLightController::DeviceLightController(Document& document)
    : DeviceSingleWindowEventController(document)
{
}

DeviceLightController::~DeviceLightController()
{
    stopUpdating();
}

const char* DeviceLightController::supplementName()
{
    return "DeviceLightController";
}

DeviceLightController& DeviceLightController::from(Document& document)
{
    DeviceLightController* controller = static_cast<DeviceLightController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceLightController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtrWillBeNoop(controller));
    }
    return *controller;
}

bool DeviceLightController::hasLastData()
{
    return DeviceLightDispatcher::instance().latestDeviceLightData() >= 0;
}

void DeviceLightController::registerWithDispatcher()
{
    DeviceLightDispatcher::instance().addController(this);
}

void DeviceLightController::unregisterWithDispatcher()
{
    DeviceLightDispatcher::instance().removeController(this);
}

PassRefPtrWillBeRawPtr<Event> DeviceLightController::lastEvent() const
{
    return DeviceLightEvent::create(EventTypeNames::devicelight,
        DeviceLightDispatcher::instance().latestDeviceLightData());
}

bool DeviceLightController::isNullEvent(Event* event) const
{
    DeviceLightEvent* lightEvent = toDeviceLightEvent(event);
    return lightEvent->value() == std::numeric_limits<double>::infinity();
}

const AtomicString& DeviceLightController::eventTypeName() const
{
    return EventTypeNames::devicelight;
}

} // namespace WebCore
