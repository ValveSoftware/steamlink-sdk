// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/usb/usb_configuration_android.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_android.h"
#include "device/usb/usb_interface_android.h"
#include "device/usb/usb_service_android.h"
#include "device/usb/webusb_descriptors.h"
#include "jni/ChromeUsbDevice_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
bool UsbDeviceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbDevice_jni.h
}

// static
scoped_refptr<UsbDeviceAndroid> UsbDeviceAndroid::Create(
    JNIEnv* env,
    base::WeakPtr<UsbServiceAndroid> service,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const JavaRef<jobject>& usb_device) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbDevice_create(env, usb_device.obj());
  uint16_t device_version = 0;
  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 23)
    device_version = Java_ChromeUsbDevice_getDeviceVersion(env, wrapper.obj());
  base::string16 manufacturer_string, product_string, serial_number;
  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 21) {
    manufacturer_string = ConvertJavaStringToUTF16(
        env, Java_ChromeUsbDevice_getManufacturerName(env, wrapper.obj()));
    product_string = ConvertJavaStringToUTF16(
        env, Java_ChromeUsbDevice_getProductName(env, wrapper.obj()));
    serial_number = ConvertJavaStringToUTF16(
        env, Java_ChromeUsbDevice_getSerialNumber(env, wrapper.obj()));
  }
  return make_scoped_refptr(new UsbDeviceAndroid(
      env, service,
      0x0200,  // USB protocol version, not provided by the Android API.
      Java_ChromeUsbDevice_getDeviceClass(env, wrapper.obj()),
      Java_ChromeUsbDevice_getDeviceSubclass(env, wrapper.obj()),
      Java_ChromeUsbDevice_getDeviceProtocol(env, wrapper.obj()),
      Java_ChromeUsbDevice_getVendorId(env, wrapper.obj()),
      Java_ChromeUsbDevice_getProductId(env, wrapper.obj()), device_version,
      manufacturer_string, product_string, serial_number, blocking_task_runner,
      wrapper));
}

void UsbDeviceAndroid::RequestPermission(const ResultCallback& callback) {
  if (!permission_granted_ && service_) {
    request_permission_callbacks_.push_back(callback);
    service_->RequestDevicePermission(j_object_, device_id_);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, permission_granted_));
  }
}

void UsbDeviceAndroid::Open(const OpenCallback& callback) {
  scoped_refptr<UsbDeviceHandle> device_handle;
  if (service_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> connection =
        service_->OpenDevice(env, j_object_);
    if (!connection.is_null()) {
      device_handle = UsbDeviceHandleAndroid::Create(
          env, this, blocking_task_runner_, connection);
      handles().push_back(device_handle.get());
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, device_handle));
}

bool UsbDeviceAndroid::permission_granted() const {
  return permission_granted_;
}

UsbDeviceAndroid::UsbDeviceAndroid(
    JNIEnv* env,
    base::WeakPtr<UsbServiceAndroid> service,
    uint16_t usb_version,
    uint8_t device_class,
    uint8_t device_subclass,
    uint8_t device_protocol,
    uint16_t vendor_id,
    uint16_t product_id,
    uint16_t device_version,
    const base::string16& manufacturer_string,
    const base::string16& product_string,
    const base::string16& serial_number,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const JavaRef<jobject>& wrapper)
    : UsbDevice(usb_version,
                device_class,
                device_subclass,
                device_protocol,
                vendor_id,
                product_id,
                device_version,
                manufacturer_string,
                product_string,
                serial_number),
      blocking_task_runner_(blocking_task_runner),
      device_id_(Java_ChromeUsbDevice_getDeviceId(env, wrapper.obj())),
      service_(service),
      j_object_(wrapper) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 21) {
    ScopedJavaLocalRef<jobjectArray> configurations =
        Java_ChromeUsbDevice_getConfigurations(env, j_object_.obj());
    jsize count = env->GetArrayLength(configurations.obj());
    configurations_.reserve(count);
    for (jsize i = 0; i < count; ++i) {
      ScopedJavaLocalRef<jobject> config(
          env, env->GetObjectArrayElement(configurations.obj(), i));
      configurations_.push_back(UsbConfigurationAndroid::Convert(env, config));
    }
  } else {
    // Pre-lollipop only the first configuration was supported. Build a basic
    // configuration out of the available interfaces.
    UsbConfigDescriptor config(1,      // Configuration value, reasonable guess.
                               false,  // Self powered, arbitrary default.
                               false,  // Remote wakeup, rbitrary default.
                               0);     // Maximum power, aitrary default.

    ScopedJavaLocalRef<jobjectArray> interfaces =
        Java_ChromeUsbDevice_getInterfaces(env, wrapper.obj());
    jsize count = env->GetArrayLength(interfaces.obj());
    config.interfaces.reserve(count);
    for (jsize i = 0; i < count; ++i) {
      ScopedJavaLocalRef<jobject> interface(
          env, env->GetObjectArrayElement(interfaces.obj(), i));
      config.interfaces.push_back(UsbInterfaceAndroid::Convert(env, interface));
    }
    configurations_.push_back(config);
  }
}

UsbDeviceAndroid::~UsbDeviceAndroid() {}

void UsbDeviceAndroid::PermissionGranted(bool granted) {
  if (granted)
    Open(base::Bind(&UsbDeviceAndroid::OnDeviceOpenedToReadDescriptors, this));
  else
    CallRequestPermissionCallbacks(granted);
}

void UsbDeviceAndroid::CallRequestPermissionCallbacks(bool granted) {
  permission_granted_ = granted;
  std::list<ResultCallback> callbacks;
  callbacks.swap(request_permission_callbacks_);
  for (const auto& callback : callbacks)
    callback.Run(granted);
}

void UsbDeviceAndroid::OnDeviceOpenedToReadDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    ReadUsbDescriptors(
        device_handle,
        base::Bind(&UsbDeviceAndroid::OnReadDescriptors, this, device_handle));
  } else {
    CallRequestPermissionCallbacks(false);
  }
}

void UsbDeviceAndroid::OnReadDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<UsbDeviceDescriptor> descriptor) {
  if (!descriptor) {
    device_handle->Close();
    CallRequestPermissionCallbacks(false);
    return;
  }

  usb_version_ = descriptor->usb_version;
  device_version_ = descriptor->device_version;
  configurations_ = descriptor->configurations;

  if (usb_version_ >= 0x0210) {
    ReadWebUsbDescriptors(device_handle,
                          base::Bind(&UsbDeviceAndroid::OnReadWebUsbDescriptors,
                                     this, device_handle));
  } else {
    device_handle->Close();
    CallRequestPermissionCallbacks(true);
  }
}

void UsbDeviceAndroid::OnReadWebUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
    const GURL& landing_page) {
  if (allowed_origins)
    webusb_allowed_origins_ = std::move(allowed_origins);
  if (landing_page.is_valid())
    webusb_landing_page_ = landing_page;

  device_handle->Close();
  CallRequestPermissionCallbacks(true);
}

}  // namespace device
