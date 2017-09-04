// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace arc {

extern ArcBridgeService* g_arc_bridge_service;

namespace {
constexpr int64_t kReconnectDelayInSeconds = 5;
}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    const scoped_refptr<base::TaskRunner>& blocking_task_runner)
    : session_started_(false),
      factory_(base::Bind(ArcSession::Create, blocking_task_runner)),
      weak_factory_(this) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {
  if (arc_session_)
    arc_session_->RemoveObserver(this);

  DCHECK(g_arc_bridge_service == this);
  g_arc_bridge_service = nullptr;
}

void ArcBridgeServiceImpl::RequestStart() {
  DCHECK(CalledOnValidThread());
  if (session_started_)
    return;
  VLOG(1) << "Session started";
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::RequestStop() {
  DCHECK(CalledOnValidThread());
  if (!session_started_)
    return;
  VLOG(1) << "Session ended";
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnShutdown() {
  DCHECK(CalledOnValidThread());
  VLOG(1) << "OnShutdown";
  if (!session_started_)
    return;
  session_started_ = false;
  reconnect_ = false;
  if (arc_session_)
    arc_session_->OnShutdown();
}

void ArcBridgeServiceImpl::SetArcSessionFactoryForTesting(
    const ArcSessionFactory& factory) {
  DCHECK(!factory.is_null());
  factory_ = factory;
}

void ArcBridgeServiceImpl::DisableReconnectDelayForTesting() {
  use_delay_before_reconnecting_ = false;
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  VLOG(1) << "Prerequisites changed. "
          << "state=" << static_cast<uint32_t>(state())
          << ", session_started=" << session_started_;
  if (state() == State::STOPPED) {
    if (!session_started_)
      return;
    VLOG(0) << "Prerequisites met, starting ARC";
    SetStopReason(StopReason::SHUTDOWN);

    if (arc_session_)
      arc_session_->RemoveObserver(this);

    SetState(State::CONNECTING);
    arc_session_ = factory_.Run();
    arc_session_->AddObserver(this);
    arc_session_->Start();
  } else {
    if (session_started_)
      return;
    VLOG(0) << "Prerequisites stopped being met, stopping ARC";
    StopInstance();
  }
}

void ArcBridgeServiceImpl::StopInstance() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    VLOG(1) << "StopInstance() called when ARC is not running";
    return;
  }

  // We were explicitly asked to stop, so do not reconnect.
  reconnect_ = false;

  VLOG(1) << "Stopping ARC";
  DCHECK(arc_session_.get());
  SetState(State::STOPPING);

  // Note: this can call OnStopped() internally as a callback.
  arc_session_->Stop();
}

void ArcBridgeServiceImpl::OnReady() {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  // The container can be considered to have been successfully launched, so
  // restart if the connection goes down without being requested.
  reconnect_ = true;
  VLOG(0) << "ARC ready";
  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  VLOG(0) << "ARC stopped: " << stop_reason;
  arc_session_->RemoveObserver(this);
  arc_session_.reset();
  SetStopReason(stop_reason);
  SetState(State::STOPPED);

  if (reconnect_) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting the container again.
    reconnect_ = false;
    VLOG(0) << "ARC reconnecting";
    if (use_delay_before_reconnecting_) {
      // Instead of immediately trying to restart the container, give it some
      // time to finish tearing down in case it is still in the process of
      // stopping.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::Bind(&ArcBridgeServiceImpl::PrerequisitesChanged,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kReconnectDelayInSeconds));
    } else {
      // Restart immediately.
      PrerequisitesChanged();
    }
  }
}

}  // namespace arc
