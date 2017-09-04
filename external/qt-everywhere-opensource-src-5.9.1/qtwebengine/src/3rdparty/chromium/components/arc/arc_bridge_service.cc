// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"

namespace arc {

namespace {

const base::Feature kArcEnabledFeature{"EnableARC",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

// Weak pointer.  This class is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service = nullptr;

ArcBridgeService::ArcBridgeService()
    : state_(State::STOPPED),
      stop_reason_(StopReason::SHUTDOWN),
      weak_factory_(this) {}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() == State::STOPPING || state() == State::STOPPED);
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  if (!g_arc_bridge_service) {
    // ArcBridgeService may be indirectly referenced in unit tests where
    // ArcBridgeService is optional.
    LOG(ERROR) << "ArcBridgeService is not ready.";
    return nullptr;
  }
  DCHECK(g_arc_bridge_service->CalledOnValidThread());
  return g_arc_bridge_service;
}

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kArcEnabledFeature));
}

// static
bool ArcBridgeService::GetAvailable(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kArcAvailable);
}

void ArcBridgeService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::SetState(State state) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(state_, state);
  state_ = state;
  VLOG(2) << "State: " << static_cast<uint32_t>(state_);
  if (state_ == State::READY) {
    for (auto& observer : observer_list())
      observer.OnBridgeReady();
  } else if (state == State::STOPPED) {
    for (auto& observer : observer_list())
      observer.OnBridgeStopped(stop_reason_);
  }
}

void ArcBridgeService::SetStopReason(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  stop_reason_ = stop_reason;
}

bool ArcBridgeService::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

std::ostream& operator<<(
    std::ostream& os, ArcBridgeService::StopReason reason) {
  switch (reason) {
#define CASE_IMPL(val) \
    case ArcBridgeService::StopReason::val: \
      return os << #val

    CASE_IMPL(SHUTDOWN);
    CASE_IMPL(GENERIC_BOOT_FAILURE);
    CASE_IMPL(LOW_DISK_SPACE);
    CASE_IMPL(CRASH);
#undef CASE_IMPL
  }

  // In case of unexpected value, output the int value.
  return os << "StopReason(" << static_cast<int>(reason) << ")";
}

}  // namespace arc
