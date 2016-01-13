// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_SAVE_BLOCKER_IMPL_H_
#define CONTENT_BROWSER_POWER_SAVE_BLOCKER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/power_save_blocker.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class PowerSaveBlockerImpl : public PowerSaveBlocker {
 public:
  PowerSaveBlockerImpl(PowerSaveBlockerType type, const std::string& reason);
  virtual ~PowerSaveBlockerImpl();

#if defined(OS_ANDROID)
  // In Android platform, the kPowerSaveBlockPreventDisplaySleep type of
  // PowerSaveBlocker should associated with the ViewAndroid,
  // so the blocker could be removed by platform if the view isn't visble
  void InitDisplaySleepBlocker(gfx::NativeView view_android);
#endif

 private:
  class Delegate;

  // Implementations of this class may need a second object with different
  // lifetime than the RAII container, or additional storage. This member is
  // here for that purpose. If not used, just define the class as an empty
  // RefCounted (or RefCountedThreadSafe) like so to make it compile:
  // class PowerSaveBlocker::Delegate
  //     : public base::RefCounted<PowerSaveBlocker::Delegate> {
  //  private:
  //   friend class base::RefCounted<Delegate>;
  //   ~Delegate() {}
  // };
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_SAVE_BLOCKER_IMPL_H_
