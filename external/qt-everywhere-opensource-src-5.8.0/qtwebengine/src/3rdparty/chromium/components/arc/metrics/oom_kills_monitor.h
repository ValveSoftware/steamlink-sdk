// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_
#define COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"

namespace arc {

// Traces kernel OOM kill events and lowmemorykiller (if enabled) events.
//
// OomKillsMonitor listens to kernel messages for both OOM kills and
// lowmemorykiller kills, then reports to UMA.
//
// Note: There should be only one OomKillsMonitor instance globally at any given
// time, otherwise UMA would receive duplicate events.
class OomKillsMonitor {
 public:
  OomKillsMonitor();
  ~OomKillsMonitor();

  void Start();
  void Stop();

 private:
  // Keeps a reference to worker_pool_ in case |this| is deleted in
  // shutdown process while this thread returns from a blocking read.
  static void Run(scoped_refptr<base::SequencedWorkerPool> worker_pool);

  scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(OomKillsMonitor);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_OOM_KILLS_MONITOR_H_
