// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/application_status_listener.h"

#include <jni.h>

#include "base/lazy_instance.h"
#include "base/observer_list_threadsafe.h"
#include "jni/ApplicationStatus_jni.h"

namespace {
struct LeakyLazyObserverListTraits :
    base::internal::LeakyLazyInstanceTraits<
        ObserverListThreadSafe<base::android::ApplicationStatusListener> > {
  static ObserverListThreadSafe<base::android::ApplicationStatusListener>*
      New(void* instance) {
    ObserverListThreadSafe<base::android::ApplicationStatusListener>* ret =
        base::internal::LeakyLazyInstanceTraits<ObserverListThreadSafe<
            base::android::ApplicationStatusListener> >::New(instance);
    // Leaky.
    ret->AddRef();
    return ret;
  }
};

base::LazyInstance<ObserverListThreadSafe<
    base::android::ApplicationStatusListener>,
        LeakyLazyObserverListTraits> g_observers = LAZY_INSTANCE_INITIALIZER;

} // namespace

namespace base {
namespace android {

ApplicationStatusListener::ApplicationStatusListener(
    const ApplicationStatusListener::ApplicationStateChangeCallback& callback)
    : callback_(callback) {
  DCHECK(!callback_.is_null());
  g_observers.Get().AddObserver(this);

  Java_ApplicationStatus_registerThreadSafeNativeApplicationStateListener(
      base::android::AttachCurrentThread());
}

ApplicationStatusListener::~ApplicationStatusListener() {
  g_observers.Get().RemoveObserver(this);
}

void ApplicationStatusListener::Notify(ApplicationState state) {
  callback_.Run(state);
}

// static
bool ApplicationStatusListener::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void ApplicationStatusListener::NotifyApplicationStateChange(
    ApplicationState state) {
  g_observers.Get().Notify(&ApplicationStatusListener::Notify, state);
}

static void OnApplicationStateChange(JNIEnv* env,
                                     jclass clazz,
                                     jint new_state) {
  ApplicationState application_state = static_cast<ApplicationState>(new_state);
  ApplicationStatusListener::NotifyApplicationStateChange(application_state);
}

}  // namespace android
}  // namespace base
