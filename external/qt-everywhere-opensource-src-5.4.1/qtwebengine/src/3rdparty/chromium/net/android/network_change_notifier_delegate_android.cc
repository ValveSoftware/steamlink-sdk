// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_delegate_android.h"

#include "base/logging.h"
#include "jni/NetworkChangeNotifier_jni.h"

namespace net {

namespace {

// Converts a Java side connection type (integer) to
// the native side NetworkChangeNotifier::ConnectionType.
NetworkChangeNotifier::ConnectionType ConvertConnectionType(
    jint connection_type) {
  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
    case NetworkChangeNotifier::CONNECTION_WIFI:
    case NetworkChangeNotifier::CONNECTION_2G:
    case NetworkChangeNotifier::CONNECTION_3G:
    case NetworkChangeNotifier::CONNECTION_4G:
    case NetworkChangeNotifier::CONNECTION_NONE:
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      break;
    default:
      NOTREACHED() << "Unknown connection type received: " << connection_type;
      return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
  return static_cast<NetworkChangeNotifier::ConnectionType>(connection_type);
}

}  // namespace

NetworkChangeNotifierDelegateAndroid::NetworkChangeNotifierDelegateAndroid()
    : observers_(new ObserverListThreadSafe<Observer>()) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_network_change_notifier_.Reset(
      Java_NetworkChangeNotifier_init(
          env, base::android::GetApplicationContext()));
  Java_NetworkChangeNotifier_addNativeObserver(
      env, java_network_change_notifier_.obj(),
      reinterpret_cast<intptr_t>(this));
  SetCurrentConnectionType(
      ConvertConnectionType(
          Java_NetworkChangeNotifier_getCurrentConnectionType(
              env, java_network_change_notifier_.obj())));
}

NetworkChangeNotifierDelegateAndroid::~NetworkChangeNotifierDelegateAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_->AssertEmpty();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_removeNativeObserver(
      env, java_network_change_notifier_.obj(),
      reinterpret_cast<intptr_t>(this));
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionType() const {
  base::AutoLock auto_lock(connection_type_lock_);
  return connection_type_;
}

void NetworkChangeNotifierDelegateAndroid::NotifyConnectionTypeChanged(
    JNIEnv* env,
    jobject obj,
    jint new_connection_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const ConnectionType actual_connection_type = ConvertConnectionType(
      new_connection_type);
  SetCurrentConnectionType(actual_connection_type);
  observers_->Notify(&Observer::OnConnectionTypeChanged);
}

jint NetworkChangeNotifierDelegateAndroid::GetConnectionType(JNIEnv*,
                                                             jobject) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetCurrentConnectionType();
}

void NetworkChangeNotifierDelegateAndroid::AddObserver(
    Observer* observer) {
  observers_->AddObserver(observer);
}

void NetworkChangeNotifierDelegateAndroid::RemoveObserver(
    Observer* observer) {
  observers_->RemoveObserver(observer);
}

// static
bool NetworkChangeNotifierDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentConnectionType(
    ConnectionType new_connection_type) {
  base::AutoLock auto_lock(connection_type_lock_);
  connection_type_ = new_connection_type;
}

void NetworkChangeNotifierDelegateAndroid::SetOnline() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, true);
}

void NetworkChangeNotifierDelegateAndroid::SetOffline() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, false);
}

}  // namespace net
