// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorProviderProxy_h
#define SensorProviderProxy_h

#include "core/frame/LocalFrame.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom-blink.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class SensorProxy;
class SensorReadingFactory;

// This class wraps 'SensorProvider' mojo interface and it manages
// 'SensorProxy' instances.
class SensorProviderProxy final
    : public GarbageCollectedFinalized<SensorProviderProxy>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(SensorProviderProxy);
  WTF_MAKE_NONCOPYABLE(SensorProviderProxy);

 public:
  static SensorProviderProxy* from(LocalFrame*);

  ~SensorProviderProxy();

  SensorProxy* createSensor(device::mojom::blink::SensorType,
                            std::unique_ptr<SensorReadingFactory>);

  SensorProxy* getSensor(device::mojom::blink::SensorType);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class SensorProxy;  // To call sensorProvider().

  explicit SensorProviderProxy(LocalFrame*);
  static const char* supplementName();

  device::mojom::blink::SensorProvider* sensorProvider() const {
    return m_sensorProvider.get();
  }

  void onSensorProviderConnectionError();

  using SensorsSet = HeapHashSet<WeakMember<SensorProxy>>;
  SensorsSet m_sensors;

  device::mojom::blink::SensorProviderPtr m_sensorProvider;
};

}  // namespace blink

#endif  // SensorProviderProxy_h
