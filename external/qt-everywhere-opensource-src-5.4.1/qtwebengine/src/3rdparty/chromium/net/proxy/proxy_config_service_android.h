// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_ANDROID_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_config_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

class ProxyConfig;

class NET_EXPORT ProxyConfigServiceAndroid : public ProxyConfigService {
 public:
  // Callback that returns the value of the property identified by the provided
  // key. If it was not found, an empty string is returned. Note that this
  // interface does not let you distinguish an empty property from a
  // non-existing property. This callback is invoked on the JNI thread.
  typedef base::Callback<std::string (const std::string& property)>
      GetPropertyCallback;

  // Separate class whose instance is owned by the Delegate class implemented in
  // the .cc file.
  class JNIDelegate {
   public:
    virtual ~JNIDelegate() {}

    // Called from Java (on JNI thread) to signal that the proxy settings have
    // changed. The string and int arguments (the host/port pair for the proxy)
    // are either a host/port pair or ("", 0) to indicate "no proxy".
    virtual void ProxySettingsChangedTo(JNIEnv*, jobject, jstring, jint) = 0;

    // Called from Java (on JNI thread) to signal that the proxy settings have
    // changed. New proxy settings are fetched from the system property store.
    virtual void ProxySettingsChanged(JNIEnv*, jobject) = 0;
  };

  ProxyConfigServiceAndroid(base::SequencedTaskRunner* network_task_runner,
                            base::SequencedTaskRunner* jni_task_runner);

  virtual ~ProxyConfigServiceAndroid();

  // Register JNI bindings.
  static bool Register(JNIEnv* env);

  // ProxyConfigService:
  // Called only on the network thread.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual ConfigAvailability GetLatestProxyConfig(ProxyConfig* config) OVERRIDE;

 private:
  friend class ProxyConfigServiceAndroidTestBase;
  class Delegate;

  // For tests.
  ProxyConfigServiceAndroid(base::SequencedTaskRunner* network_task_runner,
                            base::SequencedTaskRunner* jni_task_runner,
                            GetPropertyCallback get_property_callback);

  // For tests.
  void ProxySettingsChanged();

  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceAndroid);
};

} // namespace net

#endif // NET_PROXY_PROXY_CONFIG_SERVICE_ANDROID_H_
