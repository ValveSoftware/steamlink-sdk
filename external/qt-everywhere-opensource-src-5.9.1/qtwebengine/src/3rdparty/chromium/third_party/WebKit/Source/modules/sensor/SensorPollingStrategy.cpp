// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorPollingStrategy.h"

#include "wtf/CurrentTime.h"

namespace blink {

SensorPollingStrategy::SensorPollingStrategy(
    double pollingPeriod,
    std::unique_ptr<Function<void()>> func)
    : m_pollingPeriod(pollingPeriod),
      m_pollFunc(std::move(func)),
      m_timer(this, &SensorPollingStrategy::pollForData) {}

SensorPollingStrategy::~SensorPollingStrategy() = default;

// Polls the buffer continuously using the given 'pollingPeriod'.
class ContiniousSensorPollingStrategy : public SensorPollingStrategy {
 public:
  ContiniousSensorPollingStrategy(double pollingPeriod,
                                  std::unique_ptr<Function<void()>> func)
      : SensorPollingStrategy(pollingPeriod, std::move(func)) {}

 private:
  // SensorPollingStrategy overrides.
  void startPolling() override;
  void stopPolling() override;

  void pollForData(TimerBase*) override;
};

void ContiniousSensorPollingStrategy::startPolling() {
  (*m_pollFunc)();
  m_timer.startRepeating(m_pollingPeriod, BLINK_FROM_HERE);
}

void ContiniousSensorPollingStrategy::stopPolling() {
  m_timer.stop();
}

void ContiniousSensorPollingStrategy::pollForData(TimerBase*) {
  (*m_pollFunc)();
}

// Polls the buffer on signal from platform but not more frequently
// than the given 'pollingPeriod'.
class OnChangeSensorPollingStrategy : public SensorPollingStrategy {
 public:
  OnChangeSensorPollingStrategy(double pollingPeriod,
                                std::unique_ptr<Function<void()>> func)
      : SensorPollingStrategy(pollingPeriod, std::move(func)),
        m_polling(false),
        m_lastPollingTimestamp(0.0) {}

 private:
  // SensorPollingStrategy overrides.
  void startPolling() override;
  void stopPolling() override;
  void onSensorReadingChanged() override;

  void pollForData(TimerBase*) override;

  bool m_polling;
  double m_lastPollingTimestamp;
};

void OnChangeSensorPollingStrategy::startPolling() {
  (*m_pollFunc)();
  m_polling = true;
}

void OnChangeSensorPollingStrategy::stopPolling() {
  m_polling = false;
}

void OnChangeSensorPollingStrategy::onSensorReadingChanged() {
  if (!m_polling || m_timer.isActive())
    return;
  double elapsedTime =
      WTF::monotonicallyIncreasingTime() - m_lastPollingTimestamp;

  if (elapsedTime >= m_pollingPeriod) {
    m_lastPollingTimestamp = WTF::monotonicallyIncreasingTime();
    (*m_pollFunc)();
  } else {
    m_timer.startOneShot(m_pollingPeriod - elapsedTime, BLINK_FROM_HERE);
  }
}

void OnChangeSensorPollingStrategy::pollForData(TimerBase*) {
  if (!m_polling)
    return;
  m_lastPollingTimestamp = WTF::monotonicallyIncreasingTime();
  (*m_pollFunc)();
}

// static
std::unique_ptr<SensorPollingStrategy> SensorPollingStrategy::create(
    double pollingPeriod,
    std::unique_ptr<Function<void()>> func,
    device::mojom::blink::ReportingMode mode) {
  if (mode == device::mojom::blink::ReportingMode::CONTINUOUS)
    return std::unique_ptr<SensorPollingStrategy>(
        new ContiniousSensorPollingStrategy(pollingPeriod, std::move(func)));

  return std::unique_ptr<SensorPollingStrategy>(
      new OnChangeSensorPollingStrategy(pollingPeriod, std::move(func)));
}

}  // namespace blink
