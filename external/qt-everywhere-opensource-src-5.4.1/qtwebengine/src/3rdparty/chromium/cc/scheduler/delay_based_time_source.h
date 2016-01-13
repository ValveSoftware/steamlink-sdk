// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_
#define CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/base/cc_export.h"

namespace base { class SingleThreadTaskRunner; }

namespace cc {

class CC_EXPORT TimeSourceClient {
 public:
  virtual void OnTimerTick() = 0;

 protected:
  virtual ~TimeSourceClient() {}
};

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing
// delays. DelayBasedTimeSource uses base::TimeTicks::Now as its timebase.
class CC_EXPORT DelayBasedTimeSource
    : public base::RefCounted<DelayBasedTimeSource> {
 public:
  static scoped_refptr<DelayBasedTimeSource> Create(
      base::TimeDelta interval, base::SingleThreadTaskRunner* task_runner);

  virtual void SetClient(TimeSourceClient* client);

  // TimeSource implementation
  virtual void SetTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval);

  virtual base::TimeTicks SetActive(bool active);
  virtual bool Active() const;

  // Get the last and next tick times. NextTickTime() returns null when
  // inactive.
  virtual base::TimeTicks LastTickTime() const;
  virtual base::TimeTicks NextTickTime() const;

  // Virtual for testing.
  virtual base::TimeTicks Now() const;

  virtual scoped_ptr<base::Value> AsValue() const;

 protected:
  DelayBasedTimeSource(base::TimeDelta interval,
                       base::SingleThreadTaskRunner* task_runner);
  virtual ~DelayBasedTimeSource();

  virtual std::string TypeString() const;

  base::TimeTicks NextTickTarget(base::TimeTicks now);
  void PostNextTickTask(base::TimeTicks now);
  void OnTimerFired();

  struct Parameters {
    Parameters(base::TimeDelta interval, base::TimeTicks tick_target)
        : interval(interval), tick_target(tick_target) {}
    base::TimeDelta interval;
    base::TimeTicks tick_target;
  };

  TimeSourceClient* client_;
  base::TimeTicks last_tick_time_;

  // current_parameters_ should only be written by PostNextTickTask.
  // next_parameters_ will take effect on the next call to PostNextTickTask.
  // Maintaining a pending set of parameters allows NextTickTime() to always
  // reflect the actual time we expect OnTimerFired to be called.
  Parameters current_parameters_;
  Parameters next_parameters_;

  bool active_;

  base::SingleThreadTaskRunner* task_runner_;
  base::WeakPtrFactory<DelayBasedTimeSource> weak_factory_;

 private:
  friend class base::RefCounted<DelayBasedTimeSource>;
  DISALLOW_COPY_AND_ASSIGN(DelayBasedTimeSource);
};

// DelayBasedTimeSource uses base::TimeTicks::HighResNow as its timebase.
class DelayBasedTimeSourceHighRes : public DelayBasedTimeSource {
 public:
  static scoped_refptr<DelayBasedTimeSourceHighRes> Create(
        base::TimeDelta interval, base::SingleThreadTaskRunner* task_runner);

  virtual base::TimeTicks Now() const OVERRIDE;

 protected:
  DelayBasedTimeSourceHighRes(base::TimeDelta interval,
                              base::SingleThreadTaskRunner* task_runner);
  virtual ~DelayBasedTimeSourceHighRes();

  virtual std::string TypeString() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DelayBasedTimeSourceHighRes);
};

}  // namespace cc

#endif  // CC_SCHEDULER_DELAY_BASED_TIME_SOURCE_H_
