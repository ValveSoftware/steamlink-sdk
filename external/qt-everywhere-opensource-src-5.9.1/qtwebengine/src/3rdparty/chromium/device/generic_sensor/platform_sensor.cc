// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/generic_sensor/public/cpp/platform_sensor_configuration.h"

namespace device {

PlatformSensor::PlatformSensor(mojom::SensorType type,
                               mojo::ScopedSharedBufferMapping mapping,
                               PlatformSensorProvider* provider)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shared_buffer_mapping_(std::move(mapping)),
      type_(type),
      provider_(provider),
      weak_factory_(this) {}

PlatformSensor::~PlatformSensor() {
  provider_->RemoveSensor(GetType());
}

mojom::SensorType PlatformSensor::GetType() const {
  return type_;
}

double PlatformSensor::GetMaximumSupportedFrequency() {
  return GetDefaultConfiguration().frequency();
}

bool PlatformSensor::StartListening(Client* client,
                                    const PlatformSensorConfiguration& config) {
  DCHECK(clients_.HasObserver(client));
  if (!CheckSensorConfiguration(config))
    return false;

  auto& config_list = config_map_[client];
  config_list.push_back(config);

  if (!UpdateSensorInternal(config_map_)) {
    config_list.pop_back();
    return false;
  }

  return true;
}

bool PlatformSensor::StopListening(Client* client,
                                   const PlatformSensorConfiguration& config) {
  DCHECK(clients_.HasObserver(client));
  auto client_entry = config_map_.find(client);
  if (client_entry == config_map_.end())
    return false;

  auto& config_list = client_entry->second;
  auto config_entry = std::find(config_list.begin(), config_list.end(), config);
  if (config_entry == config_list.end())
    return false;

  config_list.erase(config_entry);

  return UpdateSensorInternal(config_map_);
}

void PlatformSensor::UpdateSensor() {
  UpdateSensorInternal(config_map_);
}

void PlatformSensor::AddClient(Client* client) {
  DCHECK(client);
  clients_.AddObserver(client);
}

void PlatformSensor::RemoveClient(Client* client) {
  DCHECK(client);
  clients_.RemoveObserver(client);
  auto client_entry = config_map_.find(client);
  if (client_entry != config_map_.end()) {
    config_map_.erase(client_entry);
    UpdateSensorInternal(config_map_);
  }
}

void PlatformSensor::UpdateSensorReading(const SensorReading& reading,
                                         bool notify_clients) {
  ReadingBuffer* buffer =
      static_cast<ReadingBuffer*>(shared_buffer_mapping_.get());
  auto& seqlock = buffer->seqlock.value();
  seqlock.WriteBegin();
  buffer->reading = reading;
  seqlock.WriteEnd();

  if (notify_clients)
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensor::NotifySensorReadingChanged,
                              weak_factory_.GetWeakPtr()));
}

void PlatformSensor::NotifySensorReadingChanged() {
  for (auto& client : clients_) {
    if (!client.IsNotificationSuspended())
      client.OnSensorReadingChanged();
  }
}

void PlatformSensor::NotifySensorError() {
  for (auto& observer : clients_)
    observer.OnSensorError();
}

bool PlatformSensor::UpdateSensorInternal(const ConfigMap& configurations) {
  const PlatformSensorConfiguration* optimal_configuration = nullptr;
  for (const auto& pair : configurations) {
    if (pair.first->IsNotificationSuspended())
      continue;

    const auto& conf_list = pair.second;
    for (const auto& configuration : conf_list) {
      if (!optimal_configuration || configuration > *optimal_configuration) {
        optimal_configuration = &configuration;
      }
    }
  }

  if (!optimal_configuration) {
    StopSensor();
    return true;
  }

  return StartSensor(*optimal_configuration);
}

}  // namespace device
