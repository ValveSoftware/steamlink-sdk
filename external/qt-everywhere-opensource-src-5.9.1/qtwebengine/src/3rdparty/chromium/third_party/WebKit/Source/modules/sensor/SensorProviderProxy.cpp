// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProviderProxy.h"

#include "modules/sensor/SensorProxy.h"
#include "modules/sensor/SensorReading.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

// SensorProviderProxy
SensorProviderProxy::SensorProviderProxy(LocalFrame* frame) {
  frame->interfaceProvider()->getInterface(mojo::GetProxy(&m_sensorProvider));
  m_sensorProvider.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&SensorProviderProxy::onSensorProviderConnectionError,
                wrapWeakPersistent(this))));
}

const char* SensorProviderProxy::supplementName() {
  return "SensorProvider";
}

SensorProviderProxy* SensorProviderProxy::from(LocalFrame* frame) {
  DCHECK(frame);
  SensorProviderProxy* result = static_cast<SensorProviderProxy*>(
      Supplement<LocalFrame>::from(*frame, supplementName()));
  if (!result) {
    result = new SensorProviderProxy(frame);
    Supplement<LocalFrame>::provideTo(*frame, supplementName(), result);
  }
  return result;
}

SensorProviderProxy::~SensorProviderProxy() {}

DEFINE_TRACE(SensorProviderProxy) {
  visitor->trace(m_sensors);
  Supplement<LocalFrame>::trace(visitor);
}

SensorProxy* SensorProviderProxy::createSensor(
    device::mojom::blink::SensorType type,
    std::unique_ptr<SensorReadingFactory> readingFactory) {
  DCHECK(!getSensor(type));

  SensorProxy* sensor = new SensorProxy(type, this, std::move(readingFactory));
  m_sensors.add(sensor);

  return sensor;
}

SensorProxy* SensorProviderProxy::getSensor(
    device::mojom::blink::SensorType type) {
  for (SensorProxy* sensor : m_sensors) {
    // TODO(Mikhail) : Hash sensors by type for efficiency.
    if (sensor->type() == type)
      return sensor;
  }

  return nullptr;
}

void SensorProviderProxy::onSensorProviderConnectionError() {
  if (!Platform::current()) {
    // TODO(rockot): Clean this up once renderer shutdown sequence is fixed.
    return;
  }

  m_sensorProvider.reset();
  for (SensorProxy* sensor : m_sensors)
    sensor->handleSensorError();
}

}  // namespace blink
