// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"

#include "device/generic_sensor/fake_platform_sensor.h"
#include "device/generic_sensor/fake_platform_sensor_provider.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using mojom::SensorInitParams;
using mojom::SensorType;

class TestSensorCreateCallback {
 public:
  TestSensorCreateCallback()
      : callback_(base::Bind(&TestSensorCreateCallback::SetResult,
                             base::Unretained(this))) {}

  scoped_refptr<PlatformSensor> WaitForResult() {
    run_loop_.Run();
    scoped_refptr<PlatformSensor> sensor = sensor_;
    sensor_ = nullptr;
    return sensor;
  }

  const PlatformSensorProvider::CreateSensorCallback& callback() const {
    return callback_;
  }

 private:
  void SetResult(scoped_refptr<PlatformSensor> sensor) {
    sensor_ = sensor;
    run_loop_.Quit();
  }

  const PlatformSensorProvider::CreateSensorCallback callback_;
  base::RunLoop run_loop_;
  scoped_refptr<PlatformSensor> sensor_;
};

class PlatformSensorTestClient : public PlatformSensor::Client {
 public:
  PlatformSensorTestClient()
      : notification_suspended_(false),
        sensor_reading_changed_(false),
        sensor_error_(false) {}

  ~PlatformSensorTestClient() override {
    if (sensor_)
      sensor_->RemoveClient(this);
  }

  // PlatformSensor::Client override.
  void OnSensorReadingChanged() override { sensor_reading_changed_ = true; }

  void OnSensorError() override { sensor_error_ = true; }

  bool IsNotificationSuspended() override { return notification_suspended_; }

  void set_notification_suspended(bool value) {
    notification_suspended_ = value;
  }

  void SetSensor(scoped_refptr<PlatformSensor> sensor) {
    sensor_ = sensor;
    sensor_->AddClient(this);
  }

  bool sensor_reading_changed() const { return sensor_reading_changed_; }

  bool sensor_error() const { return sensor_error_; }

 private:
  scoped_refptr<PlatformSensor> sensor_;
  bool notification_suspended_;
  bool sensor_reading_changed_;
  bool sensor_error_;
};

class PlatformSensorProviderTest : public ::testing::Test {
 public:
  PlatformSensorProviderTest()
      : provider_(new FakePlatformSensorProvider()),
        sensor_client_(new PlatformSensorTestClient()),
        message_loop_(new base::MessageLoopForIO) {}

 protected:
  scoped_refptr<PlatformSensor> CreateSensor(
      mojom::SensorType type,
      TestSensorCreateCallback* callback) {
    provider_->CreateSensor(type, callback->callback());
    return callback->WaitForResult();
  }

  std::unique_ptr<FakePlatformSensorProvider> provider_;
  std::unique_ptr<PlatformSensorTestClient> sensor_client_;
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(PlatformSensorProviderTest, CreateSensorsAndCheckType) {
  TestSensorCreateCallback callback1;
  scoped_refptr<PlatformSensor> sensor1 =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback1);
  EXPECT_TRUE(sensor1);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor1->GetType());

  TestSensorCreateCallback callback2;
  scoped_refptr<PlatformSensor> sensor2 =
      CreateSensor(SensorType::PROXIMITY, &callback2);
  EXPECT_TRUE(sensor2);
  EXPECT_EQ(SensorType::PROXIMITY, sensor2->GetType());

  TestSensorCreateCallback callback3;
  scoped_refptr<PlatformSensor> sensor3 =
      CreateSensor(SensorType::ACCELEROMETER, &callback3);
  EXPECT_TRUE(sensor3);
  EXPECT_EQ(SensorType::ACCELEROMETER, sensor3->GetType());

  TestSensorCreateCallback callback4;
  scoped_refptr<PlatformSensor> sensor4 =
      CreateSensor(SensorType::GYROSCOPE, &callback4);
  EXPECT_TRUE(sensor4);
  EXPECT_EQ(SensorType::GYROSCOPE, sensor4->GetType());

  TestSensorCreateCallback callback5;
  scoped_refptr<PlatformSensor> sensor5 =
      CreateSensor(SensorType::PRESSURE, &callback5);
  EXPECT_TRUE(sensor5);
  EXPECT_EQ(SensorType::PRESSURE, sensor5->GetType());
}

TEST_F(PlatformSensorProviderTest, CreateAndGetSensor) {
  // Create Ambient Light sensor.
  TestSensorCreateCallback callback1;
  scoped_refptr<PlatformSensor> sensor1 =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback1);
  EXPECT_TRUE(sensor1);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor1->GetType());

  // Try to get Gyroscope sensor, which has not been created yet.
  scoped_refptr<PlatformSensor> sensor2 =
      provider_->GetSensor(SensorType::GYROSCOPE);
  EXPECT_FALSE(sensor2);

  // Get Ambient Light sensor.
  scoped_refptr<PlatformSensor> sensor3 =
      provider_->GetSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor3);

  EXPECT_EQ(sensor1->GetType(), sensor3->GetType());
}

TEST_F(PlatformSensorProviderTest, CreateAndRemoveSensors) {
  TestSensorCreateCallback callback1;
  scoped_refptr<PlatformSensor> sensor1 =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback1);
  EXPECT_TRUE(sensor1);

  TestSensorCreateCallback callback2;
  scoped_refptr<PlatformSensor> sensor2 =
      CreateSensor(SensorType::PROXIMITY, &callback2);
  EXPECT_TRUE(sensor2);

  EXPECT_TRUE(provider_->HasSensors());

  EXPECT_CALL(*provider_, AllSensorsRemoved()).Times(1);

  sensor1 = nullptr;
  sensor2 = nullptr;

  EXPECT_FALSE(provider_->HasSensors());
}

TEST_F(PlatformSensorProviderTest, TestSensorLeaks) {
  // Create Ambient Light sensor.
  TestSensorCreateCallback callback1;
  scoped_refptr<PlatformSensor> sensor1 =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback1);
  EXPECT_TRUE(sensor1);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor1->GetType());

  // Sensor should be automatically destroyed.
  sensor1 = nullptr;
  scoped_refptr<PlatformSensor> sensor2 =
      provider_->GetSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_FALSE(sensor2);
}

// This test assumes that a mock sensor has a constant maximum frequency value
// of 50 hz (different from the base sensor class that has a range from 0 to
// 60) and tests whether a mock sensor can be started with a value range from 0
// to 60.
TEST_F(PlatformSensorProviderTest, StartListeningWithDifferentParameters) {
  const double too_high_frequency = 60;
  const double normal_frequency = 39;
  TestSensorCreateCallback callback;
  scoped_refptr<PlatformSensor> sensor =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback);
  FakePlatformSensor* fake_sensor =
      static_cast<FakePlatformSensor*>(sensor.get());
  EXPECT_TRUE(fake_sensor);
  sensor_client_->SetSensor(sensor);

  PlatformSensorConfiguration config(too_high_frequency);
  EXPECT_EQ(too_high_frequency, config.frequency());
  EXPECT_FALSE(fake_sensor->StartListening(sensor_client_.get(), config));
  EXPECT_FALSE(fake_sensor->started());

  config.set_frequency(normal_frequency);
  EXPECT_EQ(normal_frequency, config.frequency());
  EXPECT_TRUE(fake_sensor->StartListening(sensor_client_.get(), config));
  EXPECT_TRUE(fake_sensor->started());

  EXPECT_TRUE(fake_sensor->StopListening(sensor_client_.get(), config));
  EXPECT_FALSE(fake_sensor->started());
}

// If a client is in a suspended mode, a NotifySensorReadingChanged()
// notification must not be sent to the client but NotifySensorError() must be.
TEST_F(PlatformSensorProviderTest, TestNotificationSuspended) {
  const int num = 5;
  TestSensorCreateCallback callback;
  scoped_refptr<PlatformSensor> sensor =
      CreateSensor(SensorType::GYROSCOPE, &callback);
  FakePlatformSensor* fake_sensor =
      static_cast<FakePlatformSensor*>(sensor.get());

  std::vector<std::unique_ptr<PlatformSensorTestClient>> clients;
  for (int i = 0; i < num; i++) {
    std::unique_ptr<PlatformSensorTestClient> client(
        new PlatformSensorTestClient());
    client->SetSensor(fake_sensor);
    clients.push_back(std::move(client));
  }

  clients.front()->set_notification_suspended(true);
  fake_sensor->NotifySensorReadingChanged();
  fake_sensor->NotifySensorError();
  for (auto const& client : clients) {
    if (client == clients.front()) {
      EXPECT_FALSE(client->sensor_reading_changed());
      EXPECT_TRUE(client->sensor_error());
      continue;
    }
    EXPECT_TRUE(client->sensor_reading_changed());
    EXPECT_TRUE(client->sensor_error());
  }

  clients.front()->set_notification_suspended(false);
  fake_sensor->NotifySensorReadingChanged();
  fake_sensor->NotifySensorError();
  for (auto const& client : clients) {
    EXPECT_TRUE(client->sensor_reading_changed());
    EXPECT_TRUE(client->sensor_error());
  }
}

// Tests that when all clients are removed, config maps are removed as well.
TEST_F(PlatformSensorProviderTest, TestAddRemoveClients) {
  const int num = 5;

  TestSensorCreateCallback callback;
  scoped_refptr<PlatformSensor> sensor =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback);
  FakePlatformSensor* fake_sensor =
      static_cast<FakePlatformSensor*>(sensor.get());
  EXPECT_TRUE(fake_sensor->config_map().empty());

  std::vector<std::unique_ptr<PlatformSensorTestClient>> clients;
  PlatformSensorConfiguration config(30);
  for (int i = 0; i < num; i++) {
    std::unique_ptr<PlatformSensorTestClient> client(
        new PlatformSensorTestClient());
    client->SetSensor(fake_sensor);
    EXPECT_TRUE(fake_sensor->StartListening(client.get(), config));
    EXPECT_TRUE(fake_sensor->started());

    clients.push_back(std::move(client));
  }
  EXPECT_FALSE(fake_sensor->config_map().empty());

  for (const auto& client : clients)
    fake_sensor->RemoveClient(client.get());

  EXPECT_TRUE(fake_sensor->config_map().empty());
}

// Tests a sensor cannot be updated if it has one suspended client.
TEST_F(PlatformSensorProviderTest, TestUpdateSensorOneClient) {
  TestSensorCreateCallback callback;
  scoped_refptr<PlatformSensor> sensor =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback);
  FakePlatformSensor* fake_sensor =
      static_cast<FakePlatformSensor*>(sensor.get());
  EXPECT_TRUE(fake_sensor->config_map().empty());

  sensor_client_->SetSensor(fake_sensor);

  PlatformSensorConfiguration config(30);
  fake_sensor->StartListening(sensor_client_.get(), config);

  sensor_client_->set_notification_suspended(true);
  EXPECT_TRUE(sensor_client_->IsNotificationSuspended());

  fake_sensor->UpdateSensor();
  EXPECT_FALSE(fake_sensor->started());

  sensor_client_->set_notification_suspended(false);
  EXPECT_FALSE(sensor_client_->IsNotificationSuspended());

  fake_sensor->UpdateSensor();
  EXPECT_TRUE(fake_sensor->started());
}

// Tests a sensor can be updated if it has one suspended client and other
// clients are not suspended.
TEST_F(PlatformSensorProviderTest, TestUpdateSensorManyClients) {
  const int num = 5;

  TestSensorCreateCallback callback;
  scoped_refptr<PlatformSensor> sensor =
      CreateSensor(SensorType::AMBIENT_LIGHT, &callback);
  FakePlatformSensor* fake_sensor =
      static_cast<FakePlatformSensor*>(sensor.get());
  EXPECT_TRUE(fake_sensor->config_map().empty());

  sensor_client_->SetSensor(fake_sensor);
  std::vector<std::unique_ptr<PlatformSensorTestClient>> clients;
  for (int i = 0; i < num; i++) {
    std::unique_ptr<PlatformSensorTestClient> client(
        new PlatformSensorTestClient());
    client->SetSensor(fake_sensor);
    clients.push_back(std::move(client));
  }

  double sensor_frequency = 30;
  PlatformSensorConfiguration config(sensor_frequency++);
  fake_sensor->StartListening(sensor_client_.get(), config);
  for (const auto& client : clients) {
    PlatformSensorConfiguration config(sensor_frequency++);
    fake_sensor->StartListening(client.get(), config);
  }

  sensor_client_->set_notification_suspended(true);
  EXPECT_TRUE(sensor_client_->IsNotificationSuspended());
  for (const auto& client : clients)
    EXPECT_FALSE(client->IsNotificationSuspended());

  fake_sensor->UpdateSensor();
  EXPECT_TRUE(fake_sensor->started());

  sensor_client_->set_notification_suspended(false);
  EXPECT_FALSE(sensor_client_->IsNotificationSuspended());
  for (const auto& client : clients)
    EXPECT_FALSE(client->IsNotificationSuspended());

  fake_sensor->UpdateSensor();
  EXPECT_TRUE(fake_sensor->started());
}

}  // namespace device
