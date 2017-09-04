// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/progress_reporter.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
extern "C" {
#include <X11/Xlib.h>
#include <X11/Xatom.h>
}
#include <sys/poll.h>
#include "ui/base/x/x11_util.h"  // nogncheck
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif                           // defined(USE_X11)

namespace gpu {

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GPU_EXPORT GpuWatchdogThread : public base::Thread,
                                     public base::PowerObserver,
                                     public gles2::ProgressReporter {
 public:
  ~GpuWatchdogThread() override;

  static std::unique_ptr<GpuWatchdogThread> Create();

  void CheckArmed();

  // Must be called after a PowerMonitor has been created. Can be called from
  // any thread.
  void AddPowerObserver();

  // gles2::ProgressReporter implementation:
  void ReportProgress() override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  // An object of this type intercepts the reception and completion of all tasks
  // on the watched thread and checks whether the watchdog is armed.
  class GpuWatchdogTaskObserver : public base::MessageLoop::TaskObserver {
   public:
    explicit GpuWatchdogTaskObserver(GpuWatchdogThread* watchdog);
    ~GpuWatchdogTaskObserver() override;

    // Implements MessageLoop::TaskObserver.
    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;

   private:
    GpuWatchdogThread* watchdog_;
  };

  GpuWatchdogThread();

  void OnAcknowledge();
  void OnCheck(bool after_suspend);
  void DeliberatelyTerminateToRecoverFromHang();
#if defined(USE_X11)
  void SetupXServer();
  void SetupXChangeProp();
  bool MatchXEventAtom(XEvent* event);
#endif

  void OnAddPowerObserver();

  // Implement PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

#if defined(OS_WIN)
  base::ThreadTicks GetWatchedThreadTime();
#endif

#if defined(USE_X11)
  int GetActiveTTY() const;
#endif

  base::MessageLoop* watched_message_loop_;
  base::TimeDelta timeout_;
  bool armed_;
  GpuWatchdogTaskObserver task_observer_;

  // |awaiting_acknowledge_| is only ever read on the watched thread, but may
  // be modified on either the watched or watchdog thread. Reads/writes should
  // be careful to ensure that appropriate synchronization is used.
  base::subtle::Atomic32 awaiting_acknowledge_;

  // True if the watchdog should wait for a certain amount of CPU to be used
  // before killing the process.
  bool use_thread_cpu_time_;

  // The number of consecutive acknowledgements that had a latency less than
  // 50ms.
  int responsive_acknowledge_count_;

#if defined(OS_WIN)
  void* watched_thread_handle_;
  base::ThreadTicks arm_cpu_time_;

  // This measures the time that the system has been running, in units of 100
  // ns.
  ULONGLONG arm_interrupt_time_;
#endif

  // Time after which it's assumed that the computer has been suspended since
  // the task was posted.
  base::Time suspension_timeout_;

  bool suspended_;

  // The time the last OnSuspend and OnResume was called.
  base::Time suspend_time_;
  base::Time resume_time_;

  // This is the time the last check was sent.
  base::Time check_time_;
  base::TimeTicks check_timeticks_;

#if defined(USE_X11)
  XDisplay* display_;
  gfx::AcceleratedWidget window_;
  XAtom atom_;
  FILE* tty_file_;
  int host_tty_;
#endif

  base::WeakPtrFactory<GpuWatchdogThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_
