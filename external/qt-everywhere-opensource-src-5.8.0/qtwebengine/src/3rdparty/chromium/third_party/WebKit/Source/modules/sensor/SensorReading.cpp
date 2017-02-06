// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReading.h"

namespace blink {

SensorReading::SensorReading()
{
}

SensorReading::SensorReading(bool providesTimeStamp, DOMHighResTimeStamp timeStamp)
    : m_canProvideTimeStamp(providesTimeStamp)
    , m_timeStamp(timeStamp)
{
}

DOMHighResTimeStamp SensorReading::timeStamp(bool& isNull)
{
    if (m_canProvideTimeStamp)
        return m_timeStamp;

    isNull = true;
    return 0;
}

SensorReading::~SensorReading()
{
}

DEFINE_TRACE(SensorReading)
{
}

} // namespace blink
