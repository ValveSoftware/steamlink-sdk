// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_SERVICE_H_
#define CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_SERVICE_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task_runner.h"
#include "base/timer/timer.h"
#include "content/browser/power_profiler/power_data_provider.h"
#include "content/browser/power_profiler/power_profiler_observer.h"
#include "content/common/content_export.h"

namespace content {

// A class used to query power information and notify the observers.
class CONTENT_EXPORT PowerProfilerService {
 public:
  static PowerProfilerService* GetInstance();

  // Add and remove an observer.
  void AddObserver(PowerProfilerObserver* observer);
  void RemoveObserver(PowerProfilerObserver* observer);

  bool IsAvailable();

  virtual ~PowerProfilerService();

 private:
  enum Status {
    UNINITIALIZED,
    INITIALIZED,  // Initialized, profiling has not started.
    PROFILING
  };

  friend struct DefaultSingletonTraits<PowerProfilerService>;
  friend class PowerProfilerServiceTest;

  PowerProfilerService();

  PowerProfilerService(scoped_ptr<PowerDataProvider> provider,
                       scoped_refptr<base::TaskRunner> task_runner,
                       const base::TimeDelta& sample_period);

  void Start();
  void Stop();

  // Query power data from PowerDataProvider, executes on the WorkerPool thread.
  void QueryDataOnTaskRunner();

  // Initiate the query on the UI thread, the task is delegated to
  // QueryDataOnTaskRunner.
  void QueryData();

  // Executes on the UI thread.
  void Notify(const PowerEventVector&);

  base::RepeatingTimer<PowerProfilerService> query_power_timer_;
  scoped_refptr<base::TaskRunner> task_runner_;

  Status status_;

  // Sampling period of power data measurement.
  base::TimeDelta sample_period_;
  ObserverList<PowerProfilerObserver> observers_;

  scoped_ptr<PowerDataProvider> data_provider_;

  DISALLOW_COPY_AND_ASSIGN(PowerProfilerService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_PROFILER_POWER_PROFILER_SERVICE_H_
