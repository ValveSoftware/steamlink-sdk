// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorProxy.h"

#include "core/frame/LocalFrame.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "modules/sensor/SensorReading.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"

using namespace device::mojom::blink;

namespace blink {

SensorProxy::SensorProxy(SensorType sensorType,
                         SensorProviderProxy* provider,
                         std::unique_ptr<SensorReadingFactory> readingFactory)
    : m_type(sensorType),
      m_mode(ReportingMode::CONTINUOUS),
      m_provider(provider),
      m_clientBinding(this),
      m_state(SensorProxy::Uninitialized),
      m_suspended(false),
      m_readingFactory(std::move(readingFactory)),
      m_maximumFrequency(0.0) {}

SensorProxy::~SensorProxy() {}

void SensorProxy::dispose() {
  m_clientBinding.Close();
}

DEFINE_TRACE(SensorProxy) {
  visitor->trace(m_reading);
  visitor->trace(m_observers);
  visitor->trace(m_provider);
}

void SensorProxy::addObserver(Observer* observer) {
  if (!m_observers.contains(observer))
    m_observers.add(observer);
}

void SensorProxy::removeObserver(Observer* observer) {
  m_observers.remove(observer);
}

void SensorProxy::initialize() {
  if (m_state != Uninitialized)
    return;

  if (!m_provider->sensorProvider()) {
    handleSensorError();
    return;
  }

  m_state = Initializing;
  auto callback = convertToBaseCallback(
      WTF::bind(&SensorProxy::onSensorCreated, wrapWeakPersistent(this)));
  m_provider->sensorProvider()->GetSensor(m_type, mojo::GetProxy(&m_sensor),
                                          callback);
}

void SensorProxy::addConfiguration(
    SensorConfigurationPtr configuration,
    std::unique_ptr<Function<void(bool)>> callback) {
  DCHECK(isInitialized());
  m_sensor->AddConfiguration(std::move(configuration),
                             convertToBaseCallback(std::move(callback)));
}

void SensorProxy::removeConfiguration(
    SensorConfigurationPtr configuration,
    std::unique_ptr<Function<void(bool)>> callback) {
  DCHECK(isInitialized());
  m_sensor->RemoveConfiguration(std::move(configuration),
                                convertToBaseCallback(std::move(callback)));
}

void SensorProxy::suspend() {
  DCHECK(isInitialized());
  if (m_suspended)
    return;

  m_sensor->Suspend();
  m_suspended = true;
}

void SensorProxy::resume() {
  DCHECK(isInitialized());
  if (!m_suspended)
    return;

  m_sensor->Resume();
  m_suspended = false;
}

const device::mojom::blink::SensorConfiguration* SensorProxy::defaultConfig()
    const {
  DCHECK(isInitialized());
  return m_defaultConfig.get();
}

void SensorProxy::updateSensorReading() {
  DCHECK(isInitialized());
  DCHECK(m_readingFactory);
  int readAttempts = 0;
  const int kMaxReadAttemptsCount = 10;
  device::SensorReading readingData;
  while (!tryReadFromBuffer(readingData)) {
    if (++readAttempts == kMaxReadAttemptsCount) {
      handleSensorError();
      return;
    }
  }

  m_reading = m_readingFactory->createSensorReading(readingData);
}

void SensorProxy::RaiseError() {
  handleSensorError();
}

void SensorProxy::SensorReadingChanged() {
  for (Observer* observer : m_observers)
    observer->onSensorReadingChanged();
}

void SensorProxy::handleSensorError(ExceptionCode code,
                                    String sanitizedMessage,
                                    String unsanitizedMessage) {
  if (!Platform::current()) {
    // TODO(rockot): Remove this hack once renderer shutdown sequence is fixed.
    return;
  }

  m_state = Uninitialized;
  // The m_sensor.reset() will release all callbacks and its bound parameters,
  // therefore, handleSensorError accepts messages by value.
  m_sensor.reset();
  m_sharedBuffer.reset();
  m_sharedBufferHandle.reset();
  m_defaultConfig.reset();
  m_clientBinding.Close();
  m_reading = nullptr;

  for (Observer* observer : m_observers)
    observer->onSensorError(code, sanitizedMessage, unsanitizedMessage);
}

void SensorProxy::onSensorCreated(SensorInitParamsPtr params,
                                  SensorClientRequest clientRequest) {
  DCHECK_EQ(Initializing, m_state);
  if (!params) {
    handleSensorError(NotFoundError, "Sensor is not present on the platform.");
    return;
  }
  const size_t kReadBufferSize = sizeof(ReadingBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  m_mode = params->mode;
  m_defaultConfig = std::move(params->default_configuration);
  if (!m_defaultConfig) {
    handleSensorError();
    return;
  }

  DCHECK(m_sensor.is_bound());
  m_clientBinding.Bind(std::move(clientRequest));

  m_sharedBufferHandle = std::move(params->memory);
  DCHECK(!m_sharedBuffer);
  m_sharedBuffer =
      m_sharedBufferHandle->MapAtOffset(kReadBufferSize, params->buffer_offset);

  if (!m_sharedBuffer) {
    handleSensorError();
    return;
  }

  m_maximumFrequency = params->maximum_frequency;
  DCHECK(m_maximumFrequency <= SensorConfiguration::kMaxAllowedFrequency);

  auto errorCallback =
      WTF::bind(&SensorProxy::handleSensorError, wrapWeakPersistent(this),
                UnknownError, String("Internal error"), String());
  m_sensor.set_connection_error_handler(
      convertToBaseCallback(std::move(errorCallback)));

  m_state = Initialized;
  for (Observer* observer : m_observers)
    observer->onSensorInitialized();
}

bool SensorProxy::tryReadFromBuffer(device::SensorReading& result) {
  DCHECK(isInitialized());
  const ReadingBuffer* buffer =
      static_cast<const ReadingBuffer*>(m_sharedBuffer.get());
  const device::OneWriterSeqLock& seqlock = buffer->seqlock.value();
  auto version = seqlock.ReadBegin();
  auto readingData = buffer->reading;
  if (seqlock.ReadRetry(version))
    return false;
  result = readingData;
  return true;
}

}  // namespace blink
