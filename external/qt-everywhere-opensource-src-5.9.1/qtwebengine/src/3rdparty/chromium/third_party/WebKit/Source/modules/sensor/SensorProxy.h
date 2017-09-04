// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorProxy_h
#define SensorProxy_h

#include "core/dom/ExceptionCode.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class SensorProviderProxy;
class SensorReading;
class SensorReadingFactory;

// This class wraps 'Sensor' mojo interface and used by multiple
// JS sensor instances of the same type (within a single frame).
class SensorProxy final : public GarbageCollectedFinalized<SensorProxy>,
                          public device::mojom::blink::SensorClient {
  USING_PRE_FINALIZER(SensorProxy, dispose);
  WTF_MAKE_NONCOPYABLE(SensorProxy);

 public:
  class Observer : public GarbageCollectedMixin {
   public:
    // Has valid 'Sensor' binding, {add, remove}Configuration()
    // methods can be called.
    virtual void onSensorInitialized() {}
    // Platfrom sensort reading has changed (for 'ONCHANGE' reporting mode).
    virtual void onSensorReadingChanged() {}
    // An error has occurred.
    virtual void onSensorError(ExceptionCode,
                               const String& sanitizedMessage,
                               const String& unsanitizedMessage) {}
  };

  ~SensorProxy();

  void dispose();

  void addObserver(Observer*);
  void removeObserver(Observer*);

  void initialize();

  bool isInitializing() const { return m_state == Initializing; }
  bool isInitialized() const { return m_state == Initialized; }

  void addConfiguration(device::mojom::blink::SensorConfigurationPtr,
                        std::unique_ptr<Function<void(bool)>>);
  void removeConfiguration(device::mojom::blink::SensorConfigurationPtr,
                           std::unique_ptr<Function<void(bool)>>);

  void suspend();
  void resume();

  device::mojom::blink::SensorType type() const { return m_type; }
  device::mojom::blink::ReportingMode reportingMode() const { return m_mode; }

  // The |SensorReading| instance which is shared between sensor instances
  // of the same type.
  // Note: the returned value is reset after updateSensorReading() call.
  SensorReading* sensorReading() const { return m_reading; }

  const device::mojom::blink::SensorConfiguration* defaultConfig() const;

  double maximumFrequency() const { return m_maximumFrequency; }

  // Updates sensor reading from shared buffer.
  void updateSensorReading();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class SensorProviderProxy;
  SensorProxy(device::mojom::blink::SensorType,
              SensorProviderProxy*,
              std::unique_ptr<SensorReadingFactory>);

  // device::mojom::blink::SensorClient overrides.
  void RaiseError() override;
  void SensorReadingChanged() override;

  // Generic handler for a fatal error.
  // String parameters are intentionally copied.
  void handleSensorError(ExceptionCode = UnknownError,
                         String sanitizedMessage = String(),
                         String unsanitizedMessage = String());

  void onSensorCreated(device::mojom::blink::SensorInitParamsPtr,
                       device::mojom::blink::SensorClientRequest);

  bool tryReadFromBuffer(device::SensorReading& result);

  device::mojom::blink::SensorType m_type;
  device::mojom::blink::ReportingMode m_mode;
  Member<SensorProviderProxy> m_provider;
  using ObserversSet = HeapHashSet<WeakMember<Observer>>;
  ObserversSet m_observers;

  device::mojom::blink::SensorPtr m_sensor;
  device::mojom::blink::SensorConfigurationPtr m_defaultConfig;
  mojo::Binding<device::mojom::blink::SensorClient> m_clientBinding;

  enum State { Uninitialized, Initializing, Initialized };
  State m_state;
  mojo::ScopedSharedBufferHandle m_sharedBufferHandle;
  mojo::ScopedSharedBufferMapping m_sharedBuffer;
  bool m_suspended;
  Member<SensorReading> m_reading;
  std::unique_ptr<SensorReadingFactory> m_readingFactory;
  double m_maximumFrequency;

  using ReadingBuffer = device::SensorReadingSharedBuffer;
  static_assert(
      sizeof(ReadingBuffer) ==
          device::mojom::blink::SensorInitParams::kReadBufferSizeForTests,
      "Check reading buffer size for tests");
};

}  // namespace blink

#endif  // SensorProxy_h
