// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_gatt_notify_session_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "jni/ChromeBluetoothRemoteGattCharacteristic_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
std::unique_ptr<BluetoothRemoteGattCharacteristicAndroid>
BluetoothRemoteGattCharacteristicAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    BluetoothRemoteGattServiceAndroid* service,
    const std::string& instance_id,
    jobject /* BluetoothGattCharacteristicWrapper */
    bluetooth_gatt_characteristic_wrapper,
    jobject /* ChromeBluetoothDevice */ chrome_bluetooth_device) {
  std::unique_ptr<BluetoothRemoteGattCharacteristicAndroid> characteristic(
      new BluetoothRemoteGattCharacteristicAndroid(adapter, service,
                                                   instance_id));

  JNIEnv* env = AttachCurrentThread();
  characteristic->j_characteristic_.Reset(
      Java_ChromeBluetoothRemoteGattCharacteristic_create(
          env, reinterpret_cast<intptr_t>(characteristic.get()),
          bluetooth_gatt_characteristic_wrapper,
          base::android::ConvertUTF8ToJavaString(env, instance_id).obj(),
          chrome_bluetooth_device));

  return characteristic;
}

BluetoothRemoteGattCharacteristicAndroid::
    ~BluetoothRemoteGattCharacteristicAndroid() {
  Java_ChromeBluetoothRemoteGattCharacteristic_onBluetoothRemoteGattCharacteristicAndroidDestruction(
      AttachCurrentThread(), j_characteristic_.obj());

  if (pending_start_notify_calls_.size()) {
    OnStartNotifySessionError(
        device::BluetoothRemoteGattService::GATT_ERROR_FAILED);
  }
}

// static
bool BluetoothRemoteGattCharacteristicAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(
      env);  // Generated in ChromeBluetoothRemoteGattCharacteristic_jni.h
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattCharacteristicAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_characteristic_);
}

std::string BluetoothRemoteGattCharacteristicAndroid::GetIdentifier() const {
  return instance_id_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicAndroid::GetUUID() const {
  return device::BluetoothUUID(ConvertJavaStringToUTF8(
      Java_ChromeBluetoothRemoteGattCharacteristic_getUUID(
          AttachCurrentThread(), j_characteristic_.obj())));
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicAndroid::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService*
BluetoothRemoteGattCharacteristicAndroid::GetService() const {
  return service_;
}

BluetoothRemoteGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicAndroid::GetProperties() const {
  return Java_ChromeBluetoothRemoteGattCharacteristic_getProperties(
      AttachCurrentThread(), j_characteristic_.obj());
}

BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

bool BluetoothRemoteGattCharacteristicAndroid::IsNotifying() const {
  NOTIMPLEMENTED();
  return false;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicAndroid::GetDescriptors() const {
  EnsureDescriptorsCreated();
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  for (const auto& map_iter : descriptors_)
    descriptors.push_back(map_iter.second);
  return descriptors;
}

BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicAndroid::GetDescriptor(
    const std::string& identifier) const {
  EnsureDescriptorsCreated();
  const auto& iter = descriptors_.find(identifier);
  if (iter == descriptors_.end())
    return nullptr;
  return iter->second;
}

void BluetoothRemoteGattCharacteristicAndroid::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  if (!pending_start_notify_calls_.empty()) {
    pending_start_notify_calls_.push_back(
        std::make_pair(callback, error_callback));
    return;
  }

  Properties properties = GetProperties();

  bool hasNotify = properties & PROPERTY_NOTIFY;
  bool hasIndicate = properties & PROPERTY_INDICATE;

  if (!hasNotify && !hasIndicate) {
    LOG(ERROR) << "Characteristic needs NOTIFY or INDICATE";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));
    return;
  }

  std::vector<BluetoothRemoteGattDescriptor*> ccc_descriptor =
      GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                               ClientCharacteristicConfigurationUuid());

  if (ccc_descriptor.size() != 1u) {
    LOG(ERROR) << "Found " << ccc_descriptor.size()
               << " client characteristic configuration descriptors.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   (ccc_descriptor.size() == 0)
                       ? BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED
                       : BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattCharacteristic_setCharacteristicNotification(
          AttachCurrentThread(), j_characteristic_.obj(), true)) {
    LOG(ERROR) << "Error enabling characteristic notification";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  std::vector<uint8_t> value(2);
  value[0] = hasNotify ? 1 : 2;

  pending_start_notify_calls_.push_back(
      std::make_pair(callback, error_callback));
  ccc_descriptor[0]->WriteRemoteDescriptor(
      value, base::Bind(&BluetoothRemoteGattCharacteristicAndroid::
                            OnStartNotifySessionSuccess,
                        base::Unretained(this)),
      base::Bind(
          &BluetoothRemoteGattCharacteristicAndroid::OnStartNotifySessionError,
          base::Unretained(this)));
}

void BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  if (read_pending_ || write_pending_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattCharacteristic_readRemoteCharacteristic(
          AttachCurrentThread(), j_characteristic_.obj())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  read_pending_ = true;
  read_callback_ = callback;
  read_error_callback_ = error_callback;
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (read_pending_ || write_pending_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_writeRemoteCharacteristic(
          env, j_characteristic_.obj(),
          base::android::ToJavaByteArray(env, new_value).obj())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  write_pending_ = true;
  write_callback_ = callback;
  write_error_callback_ = error_callback;
}

void BluetoothRemoteGattCharacteristicAndroid::OnChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jbyteArray>& value) {
  base::android::JavaByteArrayToByteVector(env, value, &value_);
  adapter_->NotifyGattCharacteristicValueChanged(this, value_);
}

void BluetoothRemoteGattCharacteristicAndroid::OnStartNotifySessionSuccess() {
  std::vector<PendingStartNotifyCall> reentrant_safe_callbacks;
  reentrant_safe_callbacks.swap(pending_start_notify_calls_);

  for (const auto& callback_pair : reentrant_safe_callbacks) {
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session(
        new BluetoothGattNotifySessionAndroid(instance_id_));
    callback_pair.first.Run(std::move(notify_session));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::OnStartNotifySessionError(
    BluetoothRemoteGattService::GattErrorCode error) {
  std::vector<PendingStartNotifyCall> reentrant_safe_callbacks;
  reentrant_safe_callbacks.swap(pending_start_notify_calls_);

  for (auto const& callback_pair : reentrant_safe_callbacks) {
    callback_pair.second.Run(error);
  }
}

void BluetoothRemoteGattCharacteristicAndroid::OnRead(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int32_t status,
    const JavaParamRef<jbyteArray>& value) {
  read_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  ValueCallback read_callback = read_callback_;
  ErrorCallback read_error_callback = read_error_callback_;
  read_callback_.Reset();
  read_error_callback_.Reset();

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !read_callback.is_null()) {
    base::android::JavaByteArrayToByteVector(env, value, &value_);
    adapter_->NotifyGattCharacteristicValueChanged(this, value_);
    read_callback.Run(value_);
  } else if (!read_error_callback.is_null()) {
    read_error_callback.Run(
        BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::OnWrite(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    int32_t status) {
  write_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  base::Closure write_callback = write_callback_;
  ErrorCallback write_error_callback = write_error_callback_;
  write_callback_.Reset();
  write_error_callback_.Reset();

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !write_callback.is_null()) {
    write_callback.Run();
    // TODO(https://crbug.com/545682): Call GattCharacteristicValueChanged.
  } else if (!write_error_callback.is_null()) {
    write_error_callback.Run(
        BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& instanceId,
    const JavaParamRef<jobject>& /* BluetoothGattDescriptorWrapper */
    bluetooth_gatt_descriptor_wrapper,
    const JavaParamRef<jobject>& /* ChromeBluetoothDevice */
    chrome_bluetooth_device) {
  std::string instanceIdString =
      base::android::ConvertJavaStringToUTF8(env, instanceId);

  DCHECK(!descriptors_.contains(instanceIdString));

  descriptors_.set(instanceIdString,
                   BluetoothRemoteGattDescriptorAndroid::Create(
                       instanceIdString, bluetooth_gatt_descriptor_wrapper,
                       chrome_bluetooth_device));
}

BluetoothRemoteGattCharacteristicAndroid::
    BluetoothRemoteGattCharacteristicAndroid(
        BluetoothAdapterAndroid* adapter,
        BluetoothRemoteGattServiceAndroid* service,
        const std::string& instance_id)
    : adapter_(adapter), service_(service), instance_id_(instance_id) {}

void BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated()
    const {
  if (!descriptors_.empty())
    return;

  Java_ChromeBluetoothRemoteGattCharacteristic_createDescriptors(
      AttachCurrentThread(), j_characteristic_.obj());
}

}  // namespace device
