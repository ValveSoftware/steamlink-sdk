// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_api_adapter_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "content/browser/geolocation/location_provider_android.h"
#include "jni/LocationProviderAdapter_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using content::AndroidLocationApiAdapter;

static void NewLocationAvailable(JNIEnv* env, jclass,
                                 jdouble latitude,
                                 jdouble longitude,
                                 jdouble time_stamp,
                                 jboolean has_altitude, jdouble altitude,
                                 jboolean has_accuracy, jdouble accuracy,
                                 jboolean has_heading, jdouble heading,
                                 jboolean has_speed, jdouble speed) {
  AndroidLocationApiAdapter::OnNewLocationAvailable(latitude, longitude,
      time_stamp, has_altitude, altitude, has_accuracy, accuracy,
      has_heading, heading, has_speed, speed);
}

static void NewErrorAvailable(JNIEnv* env, jclass, jstring message) {
  AndroidLocationApiAdapter::OnNewErrorAvailable(env, message);
}

namespace content {

AndroidLocationApiAdapter::AndroidLocationApiAdapter()
    : location_provider_(NULL) {
}

AndroidLocationApiAdapter::~AndroidLocationApiAdapter() {
  CHECK(!location_provider_);
  CHECK(!message_loop_.get());
  CHECK(java_location_provider_android_object_.is_null());
}

bool AndroidLocationApiAdapter::Start(
    LocationProviderAndroid* location_provider, bool high_accuracy) {
  JNIEnv* env = AttachCurrentThread();
  if (!location_provider_) {
    location_provider_ = location_provider;
    CHECK(java_location_provider_android_object_.is_null());
    CreateJavaObject(env);
    {
      base::AutoLock lock(lock_);
      CHECK(!message_loop_.get());
      message_loop_ = base::MessageLoopProxy::current();
    }
  }
  // At this point we should have all our pre-conditions ready, and they'd only
  // change in Stop() which must be called on the same thread as here.
  CHECK(location_provider_);
  CHECK(message_loop_.get());
  CHECK(!java_location_provider_android_object_.is_null());
  // We'll start receiving notifications from java in the main thread looper
  // until Stop() is called.
  return Java_LocationProviderAdapter_start(env,
      java_location_provider_android_object_.obj(), high_accuracy);
}

void AndroidLocationApiAdapter::Stop() {
  if (!location_provider_) {
    CHECK(!message_loop_.get());
    CHECK(java_location_provider_android_object_.is_null());
    return;
  }

  {
    base::AutoLock lock(lock_);
    message_loop_ = NULL;
  }

  location_provider_ = NULL;

  JNIEnv* env = AttachCurrentThread();
  Java_LocationProviderAdapter_stop(
      env, java_location_provider_android_object_.obj());
  java_location_provider_android_object_.Reset();
}

// static
void AndroidLocationApiAdapter::NotifyProviderNewGeoposition(
    const Geoposition& geoposition) {
  // Called on the geolocation thread, safe to access location_provider_ here.
  if (GetInstance()->location_provider_) {
    CHECK(GetInstance()->message_loop_->BelongsToCurrentThread());
    GetInstance()->location_provider_->NotifyNewGeoposition(geoposition);
  }
}

// static
void AndroidLocationApiAdapter::OnNewLocationAvailable(
    double latitude, double longitude, double time_stamp,
    bool has_altitude, double altitude,
    bool has_accuracy, double accuracy,
    bool has_heading, double heading,
    bool has_speed, double speed) {
  Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.timestamp = base::Time::FromDoubleT(time_stamp);
  if (has_altitude)
    position.altitude = altitude;
  if (has_accuracy)
    position.accuracy = accuracy;
  if (has_heading)
    position.heading = heading;
  if (has_speed)
    position.speed = speed;
  GetInstance()->OnNewGeopositionInternal(position);
}

// static
void AndroidLocationApiAdapter::OnNewErrorAvailable(JNIEnv* env,
                                                    jstring message) {
  Geoposition position_error;
  position_error.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  position_error.error_message =
      base::android::ConvertJavaStringToUTF8(env, message);
  GetInstance()->OnNewGeopositionInternal(position_error);
}

// static
AndroidLocationApiAdapter* AndroidLocationApiAdapter::GetInstance() {
  return Singleton<AndroidLocationApiAdapter>::get();
}

// static
bool AndroidLocationApiAdapter::RegisterGeolocationService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AndroidLocationApiAdapter::CreateJavaObject(JNIEnv* env) {
  // Create the Java AndroidLocationProvider object.
  java_location_provider_android_object_.Reset(
      Java_LocationProviderAdapter_create(env,
          base::android::GetApplicationContext()));
  CHECK(!java_location_provider_android_object_.is_null());
}

void AndroidLocationApiAdapter::OnNewGeopositionInternal(
    const Geoposition& geoposition) {
  base::AutoLock lock(lock_);
  if (!message_loop_.get())
    return;
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &AndroidLocationApiAdapter::NotifyProviderNewGeoposition,
          geoposition));
}

}  // namespace content
