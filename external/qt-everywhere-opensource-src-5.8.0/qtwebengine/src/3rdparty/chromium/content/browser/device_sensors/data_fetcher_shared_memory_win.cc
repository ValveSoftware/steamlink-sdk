// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory.h"

#include <GuidDef.h>
#include <InitGuid.h>
#include <PortableDeviceTypes.h>
#include <Sensors.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/win/iunknown_impl.h"
#include "base/win/windows_version.h"

namespace {

const double kMeanGravity = 9.80665;

void SetLightBuffer(content::DeviceLightHardwareBuffer* buffer, double lux) {
  DCHECK(buffer);
  buffer->seqlock.WriteBegin();
  buffer->data.value = lux;
  buffer->seqlock.WriteEnd();
}

}  // namespace


namespace content {

class DataFetcherSharedMemory::SensorEventSink
    : public ISensorEvents, public base::win::IUnknownImpl {
 public:
  SensorEventSink() {}
  ~SensorEventSink() override {}

  // IUnknown interface
  ULONG STDMETHODCALLTYPE AddRef() override {
    return IUnknownImpl::AddRef();
  }

  ULONG STDMETHODCALLTYPE Release() override {
    return IUnknownImpl::Release();
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == __uuidof(ISensorEvents)) {
      *ppv = static_cast<ISensorEvents*>(this);
      AddRef();
      return S_OK;
    }
    return IUnknownImpl::QueryInterface(riid, ppv);
  }

  // ISensorEvents interface
  STDMETHODIMP OnEvent(ISensor* sensor,
                       REFGUID event_id,
                       IPortableDeviceValues* event_data) override {
    return S_OK;
  }

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) override {
    return S_OK;
  }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) override {
    return S_OK;
  }

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* new_data) override {
    if (nullptr == new_data || nullptr == sensor)
      return E_INVALIDARG;
    return UpdateSharedMemoryBuffer(sensor, new_data) ? S_OK : E_FAIL;
  }

 protected:
  virtual bool UpdateSharedMemoryBuffer(
      ISensor* sensor, ISensorDataReport* new_data) = 0;

  void GetSensorValue(REFPROPERTYKEY property, ISensorDataReport* new_data,
      double* value, bool* has_value) {
    PROPVARIANT variant_value = {};
    if (SUCCEEDED(new_data->GetSensorValue(property, &variant_value))) {
      if (variant_value.vt == VT_R8)
        *value = variant_value.dblVal;
      else if (variant_value.vt == VT_R4)
        *value = variant_value.fltVal;
      *has_value = true;
    } else {
      *value = 0;
      *has_value = false;
    }
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(SensorEventSink);
};

class DataFetcherSharedMemory::SensorEventSinkOrientation
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkOrientation(
      DeviceOrientationHardwareBuffer* const buffer) : buffer_(buffer) {}
  ~SensorEventSinkOrientation() override {}

 protected:
  bool UpdateSharedMemoryBuffer(
      ISensor* sensor, ISensorDataReport* new_data) override {
    double alpha, beta, gamma;
    bool has_alpha, has_beta, has_gamma;

    GetSensorValue(SENSOR_DATA_TYPE_TILT_X_DEGREES, new_data, &beta,
        &has_beta);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Y_DEGREES, new_data, &gamma,
        &has_gamma);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Z_DEGREES, new_data, &alpha,
        &has_alpha);

    if (buffer_) {
      buffer_->seqlock.WriteBegin();
      buffer_->data.alpha = alpha;
      buffer_->data.hasAlpha = has_alpha;
      buffer_->data.beta = beta;
      buffer_->data.hasBeta = has_beta;
      buffer_->data.gamma = gamma;
      buffer_->data.hasGamma = has_gamma;
      buffer_->data.absolute = has_alpha || has_beta || has_gamma;
      buffer_->data.allAvailableSensorsAreActive = true;
      buffer_->seqlock.WriteEnd();
    }

    return true;
  }

 private:
  DeviceOrientationHardwareBuffer* const buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkOrientation);
};

class DataFetcherSharedMemory::SensorEventSinkMotion
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkMotion(DeviceMotionHardwareBuffer* const buffer)
      : buffer_(buffer) {}
  ~SensorEventSinkMotion() override {}

 protected:
  bool UpdateSharedMemoryBuffer(
      ISensor* sensor, ISensorDataReport* new_data) override {

    SENSOR_TYPE_ID sensor_type = GUID_NULL;
    if (!SUCCEEDED(sensor->GetType(&sensor_type)))
      return false;

    if (IsEqualIID(sensor_type, SENSOR_TYPE_ACCELEROMETER_3D)) {
      double acceleration_including_gravity_x;
      double acceleration_including_gravity_y;
      double acceleration_including_gravity_z;
      bool has_acceleration_including_gravity_x;
      bool has_acceleration_including_gravity_y;
      bool has_acceleration_including_gravity_z;

      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_X_G, new_data,
          &acceleration_including_gravity_x,
          &has_acceleration_including_gravity_x);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Y_G, new_data,
          &acceleration_including_gravity_y,
          &has_acceleration_including_gravity_y);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Z_G, new_data,
          &acceleration_including_gravity_z,
          &has_acceleration_including_gravity_z);

      if (buffer_) {
        buffer_->seqlock.WriteBegin();
        buffer_->data.accelerationIncludingGravityX =
            -acceleration_including_gravity_x * kMeanGravity;
        buffer_->data.hasAccelerationIncludingGravityX =
            has_acceleration_including_gravity_x;
        buffer_->data.accelerationIncludingGravityY =
            -acceleration_including_gravity_y * kMeanGravity;
        buffer_->data.hasAccelerationIncludingGravityY =
            has_acceleration_including_gravity_y;
        buffer_->data.accelerationIncludingGravityZ =
            -acceleration_including_gravity_z * kMeanGravity;
        buffer_->data.hasAccelerationIncludingGravityZ =
            has_acceleration_including_gravity_z;
        // TODO(timvolodine): consider setting this after all
        // sensors have fired.
        buffer_->data.allAvailableSensorsAreActive = true;
        buffer_->seqlock.WriteEnd();
      }

    } else if (IsEqualIID(sensor_type, SENSOR_TYPE_GYROMETER_3D)) {
      double alpha, beta, gamma;
      bool has_alpha, has_beta, has_gamma;

      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND,
          new_data, &alpha, &has_alpha);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND,
          new_data, &beta, &has_beta);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND,
          new_data, &gamma, &has_gamma);

      if (buffer_) {
        buffer_->seqlock.WriteBegin();
        buffer_->data.rotationRateAlpha = alpha;
        buffer_->data.hasRotationRateAlpha = has_alpha;
        buffer_->data.rotationRateBeta = beta;
        buffer_->data.hasRotationRateBeta = has_beta;
        buffer_->data.rotationRateGamma = gamma;
        buffer_->data.hasRotationRateGamma = has_gamma;
        buffer_->data.allAvailableSensorsAreActive = true;
        buffer_->seqlock.WriteEnd();
      }
    }

    return true;
  }

 private:
  DeviceMotionHardwareBuffer* const buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkMotion);
};

class DataFetcherSharedMemory::SensorEventSinkLight
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkLight(DeviceLightHardwareBuffer* const buffer)
      : buffer_(buffer) {}
  ~SensorEventSinkLight() override {}

 protected:
  bool UpdateSharedMemoryBuffer(ISensor* sensor,
                                        ISensorDataReport* new_data) override {
    double lux;
    bool has_lux;

    GetSensorValue(SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX, new_data, &lux, &has_lux);

    if(!has_lux) {
      // Could not get lux value.
      return false;
    }

    SetLightBuffer(buffer_, lux);

    return true;
  }

 private:
  DeviceLightHardwareBuffer* const buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkLight);
};

DataFetcherSharedMemory::DataFetcherSharedMemory() {}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::FetcherType DataFetcherSharedMemory::GetType() const {
  return FETCHER_TYPE_SEPARATE_THREAD;
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      {
        orientation_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkOrientation(orientation_buffer_));
        bool inclinometer_available = RegisterForSensor(
            SENSOR_TYPE_INCLINOMETER_3D, sensor_inclinometer_.Receive(), sink);
        UMA_HISTOGRAM_BOOLEAN("InertialSensor.InclinometerWindowsAvailable",
            inclinometer_available);
        if (inclinometer_available)
          return true;
        // if no sensors are available set buffer to ready, to fire null-events.
        SetBufferAvailableState(consumer_type, true);
      }
      break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      {
        orientation_absolute_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkOrientation(orientation_absolute_buffer_));
        // Currently we use the same sensor as for orientation which provides
        // absolute angles.
        bool inclinometer_available = RegisterForSensor(
            SENSOR_TYPE_INCLINOMETER_3D,
            sensor_inclinometer_absolute_.Receive(), sink);
        // TODO(timvolodine): consider adding UMA.
        if (inclinometer_available)
          return true;
        // if no sensors are available set buffer to ready, to fire null-events.
        SetBufferAvailableState(consumer_type, true);
      }
      break;
    case CONSUMER_TYPE_MOTION:
      {
        motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkMotion(motion_buffer_));
        bool accelerometer_available = RegisterForSensor(
            SENSOR_TYPE_ACCELEROMETER_3D, sensor_accelerometer_.Receive(),
            sink);
        bool gyrometer_available = RegisterForSensor(
            SENSOR_TYPE_GYROMETER_3D, sensor_gyrometer_.Receive(), sink);
        UMA_HISTOGRAM_BOOLEAN("InertialSensor.AccelerometerWindowsAvailable",
            accelerometer_available);
        UMA_HISTOGRAM_BOOLEAN("InertialSensor.GyrometerWindowsAvailable",
            gyrometer_available);
        if (accelerometer_available || gyrometer_available) {
          motion_buffer_->seqlock.WriteBegin();
          motion_buffer_->data.interval = GetInterval().InMilliseconds();
          motion_buffer_->seqlock.WriteEnd();
          return true;
        }
        // if no sensors are available set buffer to ready, to fire null-events.
        SetBufferAvailableState(consumer_type, true);
      }
      break;
    case CONSUMER_TYPE_LIGHT:
      {
        light_buffer_ = static_cast<DeviceLightHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkLight(light_buffer_));
        bool sensor_light_available = RegisterForSensor(
            SENSOR_TYPE_AMBIENT_LIGHT, sensor_light_.Receive(), sink);
        if (sensor_light_available) {
          SetLightBuffer(light_buffer_, -1);
          return true;
        }

        // if no sensors are available, fire an Infinity event.
        SetLightBuffer(light_buffer_, std::numeric_limits<double>::infinity());
      }
      break;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  DisableSensors(consumer_type);
  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      SetBufferAvailableState(consumer_type, false);
      orientation_buffer_ = nullptr;
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      SetBufferAvailableState(consumer_type, false);
      orientation_absolute_buffer_ = nullptr;
      return true;
    case CONSUMER_TYPE_MOTION:
      SetBufferAvailableState(consumer_type, false);
      motion_buffer_ = nullptr;
      return true;
    case CONSUMER_TYPE_LIGHT:
      SetLightBuffer(light_buffer_, -1);
      light_buffer_ = nullptr;
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::RegisterForSensor(
    REFSENSOR_TYPE_ID sensor_type,
    ISensor** sensor,
    scoped_refptr<SensorEventSink> event_sink) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::ScopedComPtr<ISensorManager> sensor_manager;
  HRESULT hr = sensor_manager.CreateInstance(CLSID_SensorManager);
  if (FAILED(hr) || !sensor_manager.get())
    return false;

  base::win::ScopedComPtr<ISensorCollection> sensor_collection;
  hr = sensor_manager->GetSensorsByType(
      sensor_type, sensor_collection.Receive());

  if (FAILED(hr) || !sensor_collection.get())
    return false;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (FAILED(hr) || !count)
    return false;

  hr = sensor_collection->GetAt(0, sensor);
  if (FAILED(hr) || !(*sensor))
    return false;

  base::win::ScopedComPtr<IPortableDeviceValues> device_values;
  if (SUCCEEDED(device_values.CreateInstance(CLSID_PortableDeviceValues))) {
    if (SUCCEEDED(device_values->SetUnsignedIntegerValue(
        SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL,
        GetInterval().InMilliseconds()))) {
      base::win::ScopedComPtr<IPortableDeviceValues> return_values;
      (*sensor)->SetProperties(device_values.get(), return_values.Receive());
    }
  }

  base::win::ScopedComPtr<ISensorEvents> sensor_events;
  hr = event_sink->QueryInterface(
      __uuidof(ISensorEvents), sensor_events.ReceiveVoid());
  if (FAILED(hr) || !sensor_events.get())
    return false;

  hr = (*sensor)->SetEventSink(sensor_events.get());
  if (FAILED(hr))
    return false;

  return true;
}

void DataFetcherSharedMemory::DisableSensors(ConsumerType consumer_type) {
  switch(consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (sensor_inclinometer_.get()) {
        sensor_inclinometer_->SetEventSink(nullptr);
        sensor_inclinometer_.Release();
      }
      break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (sensor_inclinometer_absolute_.get()) {
        sensor_inclinometer_absolute_->SetEventSink(nullptr);
        sensor_inclinometer_absolute_.Release();
      }
      break;
    case CONSUMER_TYPE_MOTION:
      if (sensor_accelerometer_.get()) {
        sensor_accelerometer_->SetEventSink(nullptr);
        sensor_accelerometer_.Release();
      }
      if (sensor_gyrometer_.get()) {
        sensor_gyrometer_->SetEventSink(nullptr);
        sensor_gyrometer_.Release();
      }
      break;
    case CONSUMER_TYPE_LIGHT:
      if (sensor_light_.get()) {
        sensor_light_->SetEventSink(nullptr);
        sensor_light_.Release();
      }
      break;
    default:
      NOTREACHED();
  }
}

void DataFetcherSharedMemory::SetBufferAvailableState(
    ConsumerType consumer_type, bool enabled) {
  switch(consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.allAvailableSensorsAreActive = enabled;
        orientation_buffer_->seqlock.WriteEnd();
      }
      break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (orientation_absolute_buffer_) {
        orientation_absolute_buffer_->seqlock.WriteBegin();
        orientation_absolute_buffer_->data.allAvailableSensorsAreActive =
            enabled;
        orientation_absolute_buffer_->seqlock.WriteEnd();
      }
      break;
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.allAvailableSensorsAreActive = enabled;
        motion_buffer_->seqlock.WriteEnd();
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace content
