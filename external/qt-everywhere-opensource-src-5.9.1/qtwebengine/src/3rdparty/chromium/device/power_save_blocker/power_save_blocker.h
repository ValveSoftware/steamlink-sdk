// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_
#define DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "device/power_save_blocker/power_save_blocker_export.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace device {

// A RAII-style class to block the system from entering low-power (sleep) mode.
// This class is thread-safe; it may be constructed and deleted on any thread.
class DEVICE_POWER_SAVE_BLOCKER_EXPORT PowerSaveBlocker {
 public:
  enum PowerSaveBlockerType {
    // Prevent the application from being suspended. On some platforms, apps may
    // be suspended when they are not visible to the user. This type of block
    // requests that the app continue to run in that case, and on all platforms
    // prevents the system from sleeping.
    // Example use cases: downloading a file, playing audio.
    kPowerSaveBlockPreventAppSuspension,

    // Prevent the display from going to sleep. This also has the side effect of
    // preventing the system from sleeping, but does not necessarily prevent the
    // app from being suspended on some platforms if the user hides it.
    // Example use case: playing video.
    kPowerSaveBlockPreventDisplaySleep,
  };

  // Reasons why power-saving features may be blocked.
  enum Reason {
    // Audio is being played.
    kReasonAudioPlayback,
    // Video is being played.
    kReasonVideoPlayback,
    // Power-saving is blocked for some other reason.
    kReasonOther,
  };

  // Pass in the type of power save blocking desired. If multiple types of
  // blocking are desired, instantiate one PowerSaveBlocker for each type.
  // |reason| and |description| (a more-verbose, human-readable justification of
  // the blocking) may be provided to the underlying system APIs on some
  // platforms.
  PowerSaveBlocker(
      PowerSaveBlockerType type,
      Reason reason,
      const std::string& description,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner);
  virtual ~PowerSaveBlocker();

#if defined(OS_ANDROID)
  // On Android, the kPowerSaveBlockPreventDisplaySleep type of
  // PowerSaveBlocker should associated with a View, so the blocker can be
  // removed by the platform. Note that |view_android| is guaranteed to be
  // valid only for the lifetime of this call; hence it should not be cached
  // internally.
  DEVICE_POWER_SAVE_BLOCKER_EXPORT void InitDisplaySleepBlocker(
      ui::ViewAndroid* view_android);
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

#if defined(USE_X11)
  // Since display sleep prevention also implies system suspend prevention, for
  // the Linux FreeDesktop API case, there needs to be a second delegate to
  // block system suspend when screen saver / display sleep is blocked.
  scoped_refptr<Delegate> freedesktop_suspend_delegate_;
#endif

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

}  // namespace device

#endif  // DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_
