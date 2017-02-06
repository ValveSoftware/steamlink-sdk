// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorReading_h
#define SensorReading_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTimeStamp.h"
#include "modules/ModulesExport.h"

namespace blink {

class MODULES_EXPORT SensorReading : public GarbageCollectedFinalized<SensorReading>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static SensorReading* create()
    {
        return new SensorReading;
    }

    static SensorReading* create(bool providesTimeStamp, DOMHighResTimeStamp timestamp)
    {
        return new SensorReading(providesTimeStamp, timestamp);
    }

    virtual ~SensorReading();

    DOMHighResTimeStamp timeStamp(bool& isNull);

    void setTimeStamp(DOMHighResTimeStamp time) { m_timeStamp = time; }

    DECLARE_VIRTUAL_TRACE();

protected:
    bool m_canProvideTimeStamp;
    DOMHighResTimeStamp m_timeStamp;

    SensorReading();
    SensorReading(bool providesTimeStamp, DOMHighResTimeStamp timestamp);
};

} // namepsace blink

#endif // SensorReading_h
