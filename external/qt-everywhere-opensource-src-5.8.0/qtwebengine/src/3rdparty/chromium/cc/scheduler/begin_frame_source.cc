// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_source.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/scheduler.h"

namespace cc {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
static const double kDoubleTickDivisor = 2.0;
}

// BeginFrameObserverBase -----------------------------------------------
BeginFrameObserverBase::BeginFrameObserverBase()
    : last_begin_frame_args_(), dropped_begin_frame_args_(0) {
}

const BeginFrameArgs& BeginFrameObserverBase::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}
void BeginFrameObserverBase::OnBeginFrame(const BeginFrameArgs& args) {
  DCHECK(args.IsValid());
  DCHECK(args.frame_time >= last_begin_frame_args_.frame_time);
  bool used = OnBeginFrameDerivedImpl(args);
  if (used) {
    last_begin_frame_args_ = args;
  } else {
    ++dropped_begin_frame_args_;
  }
}

// SyntheticBeginFrameSource ---------------------------------------------
SyntheticBeginFrameSource::~SyntheticBeginFrameSource() = default;

// BackToBackBeginFrameSource --------------------------------------------
BackToBackBeginFrameSource::BackToBackBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : time_source_(std::move(time_source)), weak_factory_(this) {
  time_source_->SetClient(this);
  // The time_source_ ticks immediately, so we SetActive(true) for a single
  // tick when we need it, and keep it as SetActive(false) otherwise.
  time_source_->SetTimebaseAndInterval(base::TimeTicks(), base::TimeDelta());
}

BackToBackBeginFrameSource::~BackToBackBeginFrameSource() = default;

void BackToBackBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());
  observers_.insert(obs);
  pending_begin_frame_observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);
}

void BackToBackBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());
  observers_.erase(obs);
  pending_begin_frame_observers_.erase(obs);
  if (observers_.empty())
    time_source_->SetActive(false);
}

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs,
                                                size_t remaining_frames) {
  if (remaining_frames == 0 && observers_.find(obs) != observers_.end()) {
    pending_begin_frame_observers_.insert(obs);
    time_source_->SetActive(true);
  }
}

void BackToBackBeginFrameSource::OnTimerTick() {
  base::TimeTicks frame_time = time_source_->LastTickTime();
  base::TimeDelta default_interval = BeginFrameArgs::DefaultInterval();
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, frame_time, frame_time + default_interval,
      default_interval, BeginFrameArgs::NORMAL);

  // This must happen after getting the LastTickTime() from the time source.
  time_source_->SetActive(false);

  std::unordered_set<BeginFrameObserver*> pending_observers;
  pending_observers.swap(pending_begin_frame_observers_);
  for (BeginFrameObserver* obs : pending_observers)
    obs->OnBeginFrame(args);
}

// DelayBasedBeginFrameSource ---------------------------------------------
DelayBasedBeginFrameSource::DelayBasedBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : time_source_(std::move(time_source)) {
  time_source_->SetClient(this);
}

DelayBasedBeginFrameSource::~DelayBasedBeginFrameSource() = default;

void DelayBasedBeginFrameSource::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (!authoritative_interval_.is_zero()) {
    interval = authoritative_interval_;
  } else if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = BeginFrameArgs::DefaultInterval();
  }

  last_timebase_ = timebase;
  time_source_->SetTimebaseAndInterval(timebase, interval);
}

void DelayBasedBeginFrameSource::SetAuthoritativeVSyncInterval(
    base::TimeDelta interval) {
  authoritative_interval_ = interval;
  OnUpdateVSyncParameters(last_timebase_, interval);
}

BeginFrameArgs DelayBasedBeginFrameSource::CreateBeginFrameArgs(
    base::TimeTicks frame_time,
    BeginFrameArgs::BeginFrameArgsType type) {
  return BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time,
                                time_source_->NextTickTime(),
                                time_source_->Interval(), type);
}

void DelayBasedBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);
  BeginFrameArgs args = CreateBeginFrameArgs(
      time_source_->NextTickTime() - time_source_->Interval(),
      BeginFrameArgs::MISSED);
  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() ||
      (args.frame_time >
       last_args.frame_time + args.interval / kDoubleTickDivisor)) {
    obs->OnBeginFrame(args);
  }
}

void DelayBasedBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty())
    time_source_->SetActive(false);
}

void DelayBasedBeginFrameSource::OnTimerTick() {
  BeginFrameArgs args = CreateBeginFrameArgs(time_source_->LastTickTime(),
                                             BeginFrameArgs::NORMAL);
  std::unordered_set<BeginFrameObserver*> observers(observers_);
  for (auto& obs : observers) {
    BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (args.frame_time >
         last_args.frame_time + args.interval / kDoubleTickDivisor))
      obs->OnBeginFrame(args);
  }
}

}  // namespace cc
