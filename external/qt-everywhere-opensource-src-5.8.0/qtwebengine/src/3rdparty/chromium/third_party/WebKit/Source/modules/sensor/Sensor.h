// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Sensor_h
#define Sensor_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/PlatformEventController.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/sensor/SensorOptions.h"
#include "modules/sensor/SensorState.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ScriptState;
class SensorReading;

class MODULES_EXPORT Sensor
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public PlatformEventController {
    USING_GARBAGE_COLLECTED_MIXIN(Sensor);
    DEFINE_WRAPPERTYPEINFO();

public:
    ~Sensor() override;

    void start(ScriptState*, ExceptionState&);
    void stop(ScriptState*, ExceptionState&);
    void updateState(SensorState);

    // EventTarget implementation.
    const AtomicString& interfaceName() const override { return EventTargetNames::Sensor; }
    ExecutionContext* getExecutionContext() const override { return ContextLifecycleObserver::getExecutionContext(); }

    // Getters
    String state() const;
    // TODO(riju): crbug.com/614797 .
    SensorReading* reading() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // ActiveDOMObject implementation.
    void suspend() override;
    void resume() override;
    void stop() override;
    bool hasPendingActivity() const override;

    DECLARE_VIRTUAL_TRACE();

protected:
    Sensor(ExecutionContext*, const SensorOptions&);
    void notifyStateChange();

    SensorState m_sensorState;
    Member<SensorReading> m_sensorReading;
    SensorOptions m_sensorOptions;
};

} // namespace blink

#endif // Sensor_h
