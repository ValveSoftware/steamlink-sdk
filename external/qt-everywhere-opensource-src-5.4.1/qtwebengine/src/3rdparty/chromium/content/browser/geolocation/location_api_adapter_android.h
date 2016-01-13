// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_API_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_API_ADAPTER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"

namespace content {
class LocationProviderAndroid;
struct Geoposition;

// Interacts with JNI and reports back to AndroidLocationProvider.
// This class creates a LocationProvider java object and listens for
// updates.
// The simplified flow is:
// GeolocationProvider runs in a Geolocation Thread and fetches geolocation data
// from a LocationProvider.
// AndroidLocationProvider access a singleton AndroidLocationApiAdapter
// AndroidLocationApiAdapter calls via JNI and uses the main thread Looper
// in the java side to listen for location updates. We then bounce these updates
// to the Geolocation thread.
// Note that AndroidLocationApiAdapter is a singleton and there's at most only
// one AndroidLocationProvider that has called Start().
class AndroidLocationApiAdapter {
 public:
  // Starts the underlying location provider, returns true if successful.
  // Called on the Geolocation thread.
  bool Start(LocationProviderAndroid* location_provider, bool high_accuracy);
  // Stops the underlying location provider.
  // Called on the Geolocation thread.
  void Stop();

  // Returns our singleton.
  static AndroidLocationApiAdapter* GetInstance();

  // Called when initializing chrome_view to obtain a pointer to the java class.
  static bool RegisterGeolocationService(JNIEnv* env);

  // Called by JNI on main thread looper.
  static void OnNewLocationAvailable(double latitude,
                                     double longitude,
                                     double time_stamp,
                                     bool has_altitude, double altitude,
                                     bool has_accuracy, double accuracy,
                                     bool has_heading, double heading,
                                     bool has_speed, double speed);
  static void OnNewErrorAvailable(JNIEnv* env, jstring message);

 private:
  friend struct DefaultSingletonTraits<AndroidLocationApiAdapter>;
  AndroidLocationApiAdapter();
  ~AndroidLocationApiAdapter();

  void CreateJavaObject(JNIEnv* env);

  // Called on the JNI main thread looper.
  void OnNewGeopositionInternal(const Geoposition& geoposition);

  /// Called on the Geolocation thread.
  static void NotifyProviderNewGeoposition(const Geoposition& geoposition);

  base::android::ScopedJavaGlobalRef<jobject>
      java_location_provider_android_object_;
  LocationProviderAndroid* location_provider_;

  // Guards against the following member which is accessed on Geolocation
  // thread and the JNI main thread looper.
  base::Lock lock_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_API_ADAPTER_ANDROID_H_
