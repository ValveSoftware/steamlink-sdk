// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <SensorsApi.h>
#include <Sensors.h>  // NOLINT

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/win/iunknown_impl.h"
#include "device/generic_sensor/generic_sensor_consts.h"
#include "device/generic_sensor/platform_sensor_provider_win.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::WithArgs;

namespace device {

using mojom::SensorType;

template <class Interface>
class MockCOMInterface : public Interface, public base::win::IUnknownImpl {
 public:
  // IUnknown interface
  ULONG STDMETHODCALLTYPE AddRef() override { return IUnknownImpl::AddRef(); }
  ULONG STDMETHODCALLTYPE Release() override { return IUnknownImpl::Release(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == __uuidof(Interface)) {
      *ppv = static_cast<Interface*>(this);
      AddRef();
      return S_OK;
    }
    return IUnknownImpl::QueryInterface(riid, ppv);
  }

 protected:
  ~MockCOMInterface() override = default;
};

// Mock class for ISensorManager COM interface.
class MockISensorManager : public MockCOMInterface<ISensorManager> {
 public:
  // ISensorManager interface
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorsByCategory,
                             HRESULT(REFSENSOR_CATEGORY_ID category,
                                     ISensorCollection** sensors_found));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorsByType,
                             HRESULT(REFSENSOR_TYPE_ID sensor_id,
                                     ISensorCollection** sensors_found));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorByID,
                             HRESULT(REFSENSOR_ID sensor_id, ISensor** sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventSink,
                             HRESULT(ISensorManagerEvents* event_sink));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RequestPermissions,
                             HRESULT(HWND parent,
                                     ISensorCollection* sensors,
                                     BOOL is_modal));

 protected:
  ~MockISensorManager() override = default;
};

// Mock class for ISensorCollection COM interface.
class MockISensorCollection : public MockCOMInterface<ISensorCollection> {
 public:
  // ISensorCollection interface
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetAt,
                             HRESULT(ULONG index, ISensor** sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetCount,
                             HRESULT(ULONG* count));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Add, HRESULT(ISensor* sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Remove,
                             HRESULT(ISensor* sensor));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RemoveByID,
                             HRESULT(REFSENSOR_ID sensor_id));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Clear, HRESULT());

 protected:
  ~MockISensorCollection() override = default;
};

// Mock class for ISensor COM interface.
class MockISensor : public MockCOMInterface<ISensor> {
 public:
  // ISensor interface
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, GetID, HRESULT(SENSOR_ID* id));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetCategory,
                             HRESULT(SENSOR_CATEGORY_ID* category));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetType,
                             HRESULT(SENSOR_TYPE_ID* type));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetFriendlyName,
                             HRESULT(BSTR* name));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetProperty,
                             HRESULT(REFPROPERTYKEY key,
                                     PROPVARIANT* property));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetProperties,
                             HRESULT(IPortableDeviceKeyCollection* keys,
                                     IPortableDeviceValues** properties));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSupportedDataFields,
                             HRESULT(IPortableDeviceKeyCollection** data));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetProperties,
                             HRESULT(IPortableDeviceValues* properties,
                                     IPortableDeviceValues** results));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SupportsDataField,
                             HRESULT(REFPROPERTYKEY key,
                                     VARIANT_BOOL* is_supported));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetState,
                             HRESULT(SensorState* state));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetData,
                             HRESULT(ISensorDataReport** data_report));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SupportsEvent,
                             HRESULT(REFGUID event_guid,
                                     VARIANT_BOOL* is_supported));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetEventInterest,
                             HRESULT(GUID** values, ULONG* count));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventInterest,
                             HRESULT(GUID* values, ULONG count));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetEventSink,
                             HRESULT(ISensorEvents* pEvents));

 protected:
  ~MockISensor() override = default;
};

// Mock class for ISensorDataReport COM interface.
class MockISensorDataReport : public MockCOMInterface<ISensorDataReport> {
 public:
  // ISensorDataReport interface
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTimestamp,
                             HRESULT(SYSTEMTIME* timestamp));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorValue,
                             HRESULT(REFPROPERTYKEY key, PROPVARIANT* value));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorValues,
                             HRESULT(IPortableDeviceKeyCollection* keys,
                                     IPortableDeviceValues** values));

 protected:
  ~MockISensorDataReport() override = default;
};

// Class that provides test harness support for generic sensor adaptation for
// Windows platform. Testing is mainly done by mocking main COM interfaces that
// are used to communicate with Sensors API.
// MockISensorManager    - mocks ISensorManager and responsible for fetching
//                         list of supported sensors.
// MockISensorCollection - mocks collection of ISensor objects.
// MockISensor           - mocks ISensor intrface.
// MockISensorDataReport - mocks IDataReport interface that is used to deliver
//                         data in OnDataUpdated event.
class PlatformSensorAndProviderTestWin : public ::testing::Test {
 public:
  void SetUp() override {
    sensor_ = new NiceMock<MockISensor>();
    sensor_collection_ = new NiceMock<MockISensorCollection>();
    sensor_manager_ = new NiceMock<MockISensorManager>();
    base::win::ScopedComPtr<ISensorManager> manager;
    sensor_manager_->QueryInterface(__uuidof(ISensorManager),
                                    manager.ReceiveVoid());

    // Overrides default ISensorManager with mocked interface.
    PlatformSensorProviderWin::GetInstance()->SetSensorManagerForTesting(
        manager);
  }

  void TearDown() override {
    base::win::ScopedComPtr<ISensorManager> null_manager;
    PlatformSensorProviderWin::GetInstance()->SetSensorManagerForTesting(
        null_manager);
  }

 protected:
  void SensorCreated(scoped_refptr<PlatformSensor> sensor) {
    platform_sensor_ = sensor;
    run_loop_->Quit();
  }

  // Sensor creation is asynchronous, therefore inner loop is used to wait for
  // PlatformSensorProvider::CreateSensorCallback completion.
  scoped_refptr<PlatformSensor> CreateSensor(mojom::SensorType type) {
    run_loop_ = base::MakeUnique<base::RunLoop>();
    PlatformSensorProviderWin::GetInstance()->CreateSensor(
        type, base::Bind(&PlatformSensorAndProviderTestWin::SensorCreated,
                         base::Unretained(this)));
    run_loop_->Run();
    scoped_refptr<PlatformSensor> sensor;
    sensor.swap(platform_sensor_);
    run_loop_ = nullptr;
    return sensor;
  }

  // Sets sensor with REFSENSOR_TYPE_ID |sensor| to be supported by mocked
  // ISensorMager and it will be present in ISensorCollection.
  void SetSupportedSensor(REFSENSOR_TYPE_ID sensor) {
    // Returns mock ISensorCollection.
    EXPECT_CALL(*sensor_manager_, GetSensorsByType(sensor, _))
        .WillOnce(Invoke(
            [this](REFSENSOR_TYPE_ID type, ISensorCollection** collection) {
              sensor_collection_->QueryInterface(
                  __uuidof(ISensorCollection),
                  reinterpret_cast<void**>(collection));
              return S_OK;
            }));

    // Returns number of ISensor objects in ISensorCollection, at the moment
    // only one ISensor interface instance is suported.
    EXPECT_CALL(*sensor_collection_, GetCount(_))
        .WillOnce(Invoke([](ULONG* count) {
          *count = 1;
          return S_OK;
        }));

    // Returns ISensor interface instance at index 0.
    EXPECT_CALL(*sensor_collection_, GetAt(0, _))
        .WillOnce(Invoke([this](ULONG index, ISensor** sensor) {
          sensor_->QueryInterface(__uuidof(ISensor),
                                  reinterpret_cast<void**>(sensor));
          return S_OK;
        }));

    // Handles |SetEventSink| call that is used to subscribe to sensor events
    // through ISensorEvents interface. ISensorEvents is stored and attached to
    // |sensor_events_| that is used later to generate fake error, state and
    // data change events.
    ON_CALL(*sensor_, SetEventSink(NotNull()))
        .WillByDefault(Invoke([this](ISensorEvents* events) {
          events->AddRef();
          sensor_events_.Attach(events);
          return S_OK;
        }));

    // When |SetEventSink| is called with nullptr, it means that client is no
    // longer interested in sensor events and ISensorEvents can be released.
    ON_CALL(*sensor_, SetEventSink(IsNull()))
        .WillByDefault(Invoke([this](ISensorEvents* events) {
          sensor_events_.Release();
          return S_OK;
        }));
  }

  // Sets minimal reporting frequency for the mock sensor.
  void SetSupportedReportingFrequency(double frequency) {
    ON_CALL(*sensor_, GetProperty(SENSOR_PROPERTY_MIN_REPORT_INTERVAL, _))
        .WillByDefault(
            Invoke([frequency](REFPROPERTYKEY key, PROPVARIANT* pProperty) {
              pProperty->vt = VT_UI4;
              pProperty->ulVal =
                  (1 / frequency) * base::Time::kMillisecondsPerSecond;
              return S_OK;
            }));
  }

  // Generates OnLeave event, e.g. when sensor is disconnected.
  void GenerateLeaveEvent() {
    if (!sensor_events_)
      return;
    sensor_events_->OnLeave(SENSOR_ID());
  }

  // Generates OnStateChangedLeave event.
  void GenerateStateChangeEvent(SensorState state) {
    if (!sensor_events_)
      return;
    sensor_events_->OnStateChanged(sensor_.get(), state);
  }

  struct PropertyKeyCompare {
    bool operator()(REFPROPERTYKEY a, REFPROPERTYKEY b) const {
      if (a.fmtid == b.fmtid)
        return a.pid < b.pid;
      return false;
    }
  };
  using SensorData = std::map<PROPERTYKEY, double, PropertyKeyCompare>;

  // Generates OnDataUpdated event and creates ISensorDataReport with fake
  // |value| for property with |key|.
  void GenerateDataUpdatedEvent(const SensorData& values) {
    if (!sensor_events_)
      return;

    // MockISensorDataReport implements IUnknown that provides ref counting.
    // IUnknown::QueryInterface increases refcount if an object implements
    // requested interface. ScopedComPtr wraps received interface and destructs
    // it when there are not more references.
    auto* mock_report = new NiceMock<MockISensorDataReport>();
    base::win::ScopedComPtr<ISensorDataReport> data_report;
    mock_report->QueryInterface(__uuidof(ISensorDataReport),
                                data_report.ReceiveVoid());

    EXPECT_CALL(*mock_report, GetTimestamp(_))
        .WillOnce(Invoke([](SYSTEMTIME* timestamp) {
          GetSystemTime(timestamp);
          return S_OK;
        }));

    EXPECT_CALL(*mock_report, GetSensorValue(_, _))
        .WillRepeatedly(WithArgs<0, 1>(
            Invoke([&values](REFPROPERTYKEY key, PROPVARIANT* variant) {
              auto it = values.find(key);
              if (it == values.end())
                return E_FAIL;

              variant->vt = VT_R8;
              variant->dblVal = it->second;
              return S_OK;
            })));

    sensor_events_->OnDataUpdated(sensor_.get(), data_report.get());
  }

  scoped_refptr<MockISensorManager> sensor_manager_;
  scoped_refptr<MockISensorCollection> sensor_collection_;
  scoped_refptr<MockISensor> sensor_;
  base::win::ScopedComPtr<ISensorEvents> sensor_events_;
  base::MessageLoop message_loop_;
  scoped_refptr<PlatformSensor> platform_sensor_;
  // Inner run loop used to wait for async sensor creation callback.
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Mock for PlatformSensor's client interface that is used to deliver
// error and data changes notifications.
class MockPlatformSensorClient : public PlatformSensor::Client {
 public:
  MockPlatformSensorClient() = default;
  explicit MockPlatformSensorClient(scoped_refptr<PlatformSensor> sensor)
      : sensor_(sensor) {
    if (sensor_)
      sensor_->AddClient(this);

    ON_CALL(*this, IsNotificationSuspended()).WillByDefault(Return(false));
  }

  ~MockPlatformSensorClient() override {
    if (sensor_)
      sensor_->RemoveClient(this);
  }

  // PlatformSensor::Client interface.
  MOCK_METHOD0(OnSensorReadingChanged, void());
  MOCK_METHOD0(OnSensorError, void());
  MOCK_METHOD0(IsNotificationSuspended, bool());

 private:
  scoped_refptr<PlatformSensor> sensor_;
};

// Tests that PlatformSensorManager returns null sensor when sensor
// is not implemented.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsNotImplemented) {
  EXPECT_CALL(*sensor_manager_, GetSensorsByType(SENSOR_TYPE_PRESSURE, _))
      .Times(0);
  EXPECT_FALSE(CreateSensor(SensorType::PRESSURE));
}

// Tests that PlatformSensorManager returns null sensor when sensor
// is implemented, but not supported by the hardware.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsNotSupported) {
  EXPECT_CALL(*sensor_manager_, GetSensorsByType(SENSOR_TYPE_AMBIENT_LIGHT, _))
      .WillOnce(Invoke([](REFSENSOR_TYPE_ID, ISensorCollection** result) {
        *result = nullptr;
        return E_FAIL;
      }));

  EXPECT_FALSE(CreateSensor(SensorType::AMBIENT_LIGHT));
}

// Tests that PlatformSensorManager returns correct sensor when sensor
// is supported by the hardware.
TEST_F(PlatformSensorAndProviderTestWin, SensorIsSupported) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_EQ(SensorType::AMBIENT_LIGHT, sensor->GetType());
}

// Tests that PlatformSensor::StartListening fails when provided reporting
// frequency is above hardware capabilities.
TEST_F(PlatformSensorAndProviderTestWin, StartFails) {
  SetSupportedReportingFrequency(1);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_FALSE(sensor->StartListening(client.get(), configuration));
}

// Tests that PlatformSensor::StartListening succeeds and notification about
// modified sensor reading is sent to the PlatformSensor::Client interface.
TEST_F(PlatformSensorAndProviderTestWin, SensorStarted) {
  SetSupportedReportingFrequency(10);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);

  EXPECT_CALL(*sensor_, SetEventSink(NotNull())).Times(1);
  EXPECT_CALL(*sensor_, SetEventSink(IsNull())).Times(1);
  EXPECT_CALL(*sensor_, SetProperties(NotNull(), _))
      .WillOnce(Invoke(
          [](IPortableDeviceValues* props, IPortableDeviceValues** result) {
            ULONG value = 0;
            HRESULT hr = props->GetUnsignedIntegerValue(
                SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, &value);
            EXPECT_TRUE(SUCCEEDED(hr));
            // 10Hz is 100msec
            EXPECT_THAT(value, 100);
            return hr;
          }));

  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));

  EXPECT_CALL(*client, OnSensorReadingChanged()).Times(1);
  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX, 3.14}});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that OnSensorError is called when sensor is disconnected.
TEST_F(PlatformSensorAndProviderTestWin, SensorRemoved) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(1);

  GenerateLeaveEvent();
  base::RunLoop().RunUntilIdle();
}

// Tests that OnSensorError is called when sensor is in an error state.
TEST_F(PlatformSensorAndProviderTestWin, SensorStateChangedToError) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(1);

  GenerateStateChangeEvent(SENSOR_STATE_ERROR);
  base::RunLoop().RunUntilIdle();
}

// Tests that OnSensorError is not called when sensor is in a ready state.
TEST_F(PlatformSensorAndProviderTestWin, SensorStateChangedToReady) {
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorError()).Times(0);

  GenerateStateChangeEvent(SENSOR_STATE_READY);
  base::RunLoop().RunUntilIdle();
}

// Tests that GetMaximumSupportedFrequency provides correct value.
TEST_F(PlatformSensorAndProviderTestWin, GetMaximumSupportedFrequency) {
  SetSupportedReportingFrequency(20);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(), 20);
}

// Tests that GetMaximumSupportedFrequency returns fallback value.
TEST_F(PlatformSensorAndProviderTestWin, GetMaximumSupportedFrequencyFallback) {
  SetSupportedReportingFrequency(0);
  SetSupportedSensor(SENSOR_TYPE_AMBIENT_LIGHT);
  auto sensor = CreateSensor(SensorType::AMBIENT_LIGHT);
  EXPECT_TRUE(sensor);
  EXPECT_THAT(sensor->GetMaximumSupportedFrequency(), 5);
}

// Tests that Accelerometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckAccelerometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle =
      PlatformSensorProviderWin::GetInstance()->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::ACCELEROMETER));

  SetSupportedSensor(SENSOR_TYPE_ACCELEROMETER_3D);
  auto sensor = CreateSensor(SensorType::ACCELEROMETER);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged()).Times(1);

  double x_accel = 0.25;
  double y_accel = -0.25;
  double z_accel = -0.5;
  GenerateDataUpdatedEvent({{SENSOR_DATA_TYPE_ACCELERATION_X_G, x_accel},
                            {SENSOR_DATA_TYPE_ACCELERATION_Y_G, y_accel},
                            {SENSOR_DATA_TYPE_ACCELERATION_Z_G, z_accel}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.values[0], -x_accel * kMeanGravity);
  EXPECT_THAT(buffer->reading.values[1], -y_accel * kMeanGravity);
  EXPECT_THAT(buffer->reading.values[2], -z_accel * kMeanGravity);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Gyroscope readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckGyroscopeReadingConversion) {
  mojo::ScopedSharedBufferHandle handle =
      PlatformSensorProviderWin::GetInstance()->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::GYROSCOPE));

  SetSupportedSensor(SENSOR_TYPE_GYROMETER_3D);
  auto sensor = CreateSensor(SensorType::GYROSCOPE);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged()).Times(1);

  double x_ang_accel = 0.0;
  double y_ang_accel = -1.8;
  double z_ang_accel = -98.7;

  GenerateDataUpdatedEvent(
      {{SENSOR_DATA_TYPE_ANGULAR_ACCELERATION_X_DEGREES_PER_SECOND_SQUARED,
        x_ang_accel},
       {SENSOR_DATA_TYPE_ANGULAR_ACCELERATION_Y_DEGREES_PER_SECOND_SQUARED,
        y_ang_accel},
       {SENSOR_DATA_TYPE_ANGULAR_ACCELERATION_Z_DEGREES_PER_SECOND_SQUARED,
        z_ang_accel}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.values[0],
              -x_ang_accel * kRadiansInDegreesPerSecond);
  EXPECT_THAT(buffer->reading.values[1],
              -y_ang_accel * kRadiansInDegreesPerSecond);
  EXPECT_THAT(buffer->reading.values[2],
              -z_ang_accel * kRadiansInDegreesPerSecond);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

// Tests that Magnetometer readings are correctly converted.
TEST_F(PlatformSensorAndProviderTestWin, CheckMagnetometerReadingConversion) {
  mojo::ScopedSharedBufferHandle handle =
      PlatformSensorProviderWin::GetInstance()->CloneSharedBufferHandle();
  mojo::ScopedSharedBufferMapping mapping = handle->MapAtOffset(
      sizeof(SensorReadingSharedBuffer),
      SensorReadingSharedBuffer::GetOffset(SensorType::MAGNETOMETER));

  SetSupportedSensor(SENSOR_TYPE_COMPASS_3D);
  auto sensor = CreateSensor(SensorType::MAGNETOMETER);
  EXPECT_TRUE(sensor);

  auto client = base::MakeUnique<NiceMock<MockPlatformSensorClient>>(sensor);
  PlatformSensorConfiguration configuration(10);
  EXPECT_TRUE(sensor->StartListening(client.get(), configuration));
  EXPECT_CALL(*client, OnSensorReadingChanged()).Times(1);

  double x_magn_field = 112.0;
  double y_magn_field = -162.0;
  double z_magn_field = 457.0;

  GenerateDataUpdatedEvent(
      {{SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_X_MILLIGAUSS, x_magn_field},
       {SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Y_MILLIGAUSS, y_magn_field},
       {SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Z_MILLIGAUSS, z_magn_field}});

  base::RunLoop().RunUntilIdle();
  SensorReadingSharedBuffer* buffer =
      static_cast<SensorReadingSharedBuffer*>(mapping.get());
  EXPECT_THAT(buffer->reading.values[0],
              -x_magn_field * kMicroteslaInMilligauss);
  EXPECT_THAT(buffer->reading.values[1],
              -y_magn_field * kMicroteslaInMilligauss);
  EXPECT_THAT(buffer->reading.values[2],
              -z_magn_field * kMicroteslaInMilligauss);
  EXPECT_TRUE(sensor->StopListening(client.get(), configuration));
}

}  // namespace device
