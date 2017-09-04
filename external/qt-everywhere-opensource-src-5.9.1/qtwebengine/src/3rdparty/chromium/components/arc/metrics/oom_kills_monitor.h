// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_
#define COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/simple_thread.h"

namespace arc {

// Traces kernel OOM kill events and lowmemorykiller (if enabled) events.
//
// OomKillsMonitor listens to kernel messages for both OOM kills and
// lowmemorykiller kills, then reports to UMA. It uses a non-joinable thread
// in order to avoid blocking shutdown.
//
// Note: There should be only one OomKillsMonitor instance globally at any given
// time, otherwise UMA would receive duplicate events.
class OomKillsMonitor : public base::DelegateSimpleThread::Delegate {
 public:
  // A handle representing the OomKillsMonitor's lifetime (the monitor itself
  // can't be destroyed per being a non-joinable Thread).
  class Handle {
   public:
    // Constructs a handle that will flag |outer| as shutting down on
    // destruction.
    explicit Handle(OomKillsMonitor* outer);

    ~Handle();

   private:
    OomKillsMonitor* const outer_;
  };

  // Instantiates the OomKillsMonitor instance and starts it. This must only
  // be invoked once per process.
  static Handle StartMonitoring();

 private:
  OomKillsMonitor();
  ~OomKillsMonitor() override;

  // Overridden from base::DelegateSimpleThread::Delegate:
  void Run() override;

  // A flag set when OomKillsMonitor is shutdown so that its thread can poll
  // it and attempt to wind down from that point (to avoid unnecessary work, not
  // because it blocks shutdown).
  base::AtomicFlag is_shutting_down_;

  // The underlying worker thread which is non-joinable to avoid blocking
  // shutdown.
  std::unique_ptr<base::DelegateSimpleThread> non_joinable_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(OomKillsMonitor);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_
