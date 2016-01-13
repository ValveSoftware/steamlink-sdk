// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_H_
#define CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#include "content/browser/battery_status/battery_status_service.h"

namespace content {

// Platform specific manager class for fetching battery status data.
class CONTENT_EXPORT BatteryStatusManager {
 public:
  explicit BatteryStatusManager(
      const BatteryStatusService::BatteryUpdateCallback& callback);
  virtual ~BatteryStatusManager();

  // Start listening for battery status changes. New updates are signalled
  // by invoking the callback provided at construction time.
  virtual bool StartListeningBatteryChange();

  // Stop listening for battery status changes.
  virtual void StopListeningBatteryChange();

#if defined(OS_ANDROID)
  // Must be called at startup.
  static bool Register(JNIEnv* env);

  // Called from Java via JNI.
  void GotBatteryStatus(JNIEnv*, jobject, jboolean charging,
                        jdouble charging_time, jdouble discharging_time,
                        jdouble level);
#endif

 protected:
  BatteryStatusManager();
  BatteryStatusService::BatteryUpdateCallback callback_;

 private:
#if defined(OS_ANDROID)
  // Java provider of battery status info.
  base::android::ScopedJavaGlobalRef<jobject> j_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManager);
};

}  // namespace content

#endif  // CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_H_
