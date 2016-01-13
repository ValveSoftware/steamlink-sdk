// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_vsync_manager.h"

namespace ui {

CompositorVSyncManager::CompositorVSyncManager()
    : observer_list_(new ObserverListThreadSafe<Observer>()),
      authoritative_vsync_interval_(base::TimeDelta::FromSeconds(0)) {}

CompositorVSyncManager::~CompositorVSyncManager() {}

void CompositorVSyncManager::SetAuthoritativeVSyncInterval(
    base::TimeDelta interval) {
  base::TimeTicks timebase;
  {
    base::AutoLock lock(vsync_parameters_lock_);
    timebase = last_timebase_;
    authoritative_vsync_interval_ = interval;
    last_interval_ = interval;
  }
  NotifyObservers(timebase, interval);
}

void CompositorVSyncManager::UpdateVSyncParameters(base::TimeTicks timebase,
                                                   base::TimeDelta interval) {
  {
    base::AutoLock lock(vsync_parameters_lock_);
    if (authoritative_vsync_interval_ != base::TimeDelta::FromSeconds(0))
      interval = authoritative_vsync_interval_;
    last_timebase_ = timebase;
    last_interval_ = interval;
  }
  NotifyObservers(timebase, interval);
}

void CompositorVSyncManager::AddObserver(Observer* observer) {
  base::TimeTicks timebase;
  base::TimeDelta interval;
  {
    base::AutoLock lock(vsync_parameters_lock_);
    timebase = last_timebase_;
    interval = last_interval_;
  }
  observer_list_->AddObserver(observer);
  observer->OnUpdateVSyncParameters(timebase, interval);
}

void CompositorVSyncManager::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void CompositorVSyncManager::NotifyObservers(base::TimeTicks timebase,
                                             base::TimeDelta interval) {
  observer_list_->Notify(
      &CompositorVSyncManager::Observer::OnUpdateVSyncParameters,
      timebase,
      interval);
}

}  // namespace ui
