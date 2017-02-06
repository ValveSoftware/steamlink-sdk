// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle_android.h"

#include "base/bind.h"
#include "base/location.h"
#include "device/usb/usb_device.h"
#include "jni/ChromeUsbConnection_jni.h"

namespace device {

// static
bool UsbDeviceHandleAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbConnection_jni.h
}

// static
scoped_refptr<UsbDeviceHandleAndroid> UsbDeviceHandleAndroid::Create(
    JNIEnv* env,
    scoped_refptr<UsbDevice> device,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::android::JavaRef<jobject>& usb_connection) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbConnection_create(env, usb_connection.obj());
  base::ScopedFD fd(
      Java_ChromeUsbConnection_getFileDescriptor(env, wrapper.obj()));
  return make_scoped_refptr(new UsbDeviceHandleAndroid(
      device, std::move(fd), blocking_task_runner, wrapper));
}

UsbDeviceHandleAndroid::UsbDeviceHandleAndroid(
    scoped_refptr<UsbDevice> device,
    base::ScopedFD fd,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::android::JavaRef<jobject>& wrapper)
    : UsbDeviceHandleUsbfs(device, std::move(fd), blocking_task_runner),
      j_object_(wrapper) {}

UsbDeviceHandleAndroid::~UsbDeviceHandleAndroid() {}

void UsbDeviceHandleAndroid::CloseBlocking() {
  ReleaseFileDescriptor();
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleAndroid::CloseConnection, this));
}

void UsbDeviceHandleAndroid::CloseConnection() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeUsbConnection_close(env, j_object_.obj());
  j_object_.Reset();
}

}  // namespace device
