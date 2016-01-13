// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_APPLICATION_STATUS_LISTENER_H_
#define BASE_ANDROID_APPLICATION_STATUS_LISTENER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"

namespace base {
namespace android {

// Define application state values like APPLICATION_STATE_VISIBLE in a
// way that ensures they're always the same than their Java counterpart.
enum ApplicationState {
#define DEFINE_APPLICATION_STATE(x, y) APPLICATION_STATE_##x = y,
#include "base/android/application_state_list.h"
#undef DEFINE_APPLICATION_STATE
};

// A native helper class to listen to state changes of the Android
// Application. This mirrors org.chromium.base.ApplicationStatus.
// any thread.
//
// To start listening, create a new instance, passing a callback to a
// function that takes an ApplicationState parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
//
// Example:
//
//    void OnApplicationStateChange(ApplicationState state) {
//       ...
//    }
//
//    // Start listening.
//    ApplicationStatusListener* my_listener =
//        new ApplicationStatusListener(
//            base::Bind(&OnApplicationStateChange));
//
//    ...
//
//    // Stop listening.
//    delete my_listener
//
class BASE_EXPORT ApplicationStatusListener {
 public:
  typedef base::Callback<void(ApplicationState)> ApplicationStateChangeCallback;

  explicit ApplicationStatusListener(
      const ApplicationStateChangeCallback& callback);
  ~ApplicationStatusListener();

  // Internal use: must be public to be called from base_jni_registrar.cc
  static bool RegisterBindings(JNIEnv* env);

  // Internal use only: must be public to be called from JNI and unit tests.
  static void NotifyApplicationStateChange(ApplicationState state);

 private:
  void Notify(ApplicationState state);

  ApplicationStateChangeCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationStatusListener);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_APPLICATION_STATUS_LISTENER_H_
