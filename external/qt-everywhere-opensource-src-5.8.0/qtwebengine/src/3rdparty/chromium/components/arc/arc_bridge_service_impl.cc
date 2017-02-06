// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service_impl.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {
constexpr int64_t kReconnectDelayInSeconds = 5;
}  // namespace

ArcBridgeServiceImpl::ArcBridgeServiceImpl(
    std::unique_ptr<ArcBridgeBootstrap> bootstrap)
    : bootstrap_(std::move(bootstrap)),
      binding_(this),
      session_started_(false),
      weak_factory_(this) {
  bootstrap_->set_delegate(this);
}

ArcBridgeServiceImpl::~ArcBridgeServiceImpl() {}

void ArcBridgeServiceImpl::HandleStartup() {
  DCHECK(CalledOnValidThread());
  if (session_started_)
    return;
  VLOG(1) << "Session started";
  session_started_ = true;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (!session_started_)
    return;
  VLOG(1) << "Session ended";
  session_started_ = false;
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::DisableReconnectDelayForTesting() {
  use_delay_before_reconnecting_ = false;
}

void ArcBridgeServiceImpl::PrerequisitesChanged() {
  DCHECK(CalledOnValidThread());
  VLOG(1) << "Prerequisites changed. "
          << "state=" << static_cast<uint32_t>(state())
          << ", available=" << available()
          << ", session_started=" << session_started_;
  if (state() == State::STOPPED) {
    if (!available() || !session_started_)
      return;
    VLOG(0) << "Prerequisites met, starting ARC";
    SetStopReason(StopReason::SHUTDOWN);
    SetState(State::CONNECTING);
    bootstrap_->Start();
  } else {
    if (available() && session_started_)
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

  VLOG(1) << "Stopping ARC";
  SetState(State::STOPPING);
  instance_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();
  bootstrap_->Stop();

  // We were explicitly asked to stop, so do not reconnect.
  reconnect_ = false;
}

void ArcBridgeServiceImpl::SetDetectedAvailability(bool arc_available) {
  DCHECK(CalledOnValidThread());
  if (available() == arc_available)
    return;
  VLOG(1) << "ARC available: " << arc_available;
  SetAvailable(arc_available);
  PrerequisitesChanged();
}

void ArcBridgeServiceImpl::OnConnectionEstablished(
    mojom::ArcBridgeInstancePtr instance) {
  DCHECK(CalledOnValidThread());
  if (state() != State::CONNECTING) {
    VLOG(1) << "StopInstance() called while connecting";
    return;
  }

  instance_ptr_ = std::move(instance);
  instance_ptr_.set_connection_error_handler(base::Bind(
      &ArcBridgeServiceImpl::OnChannelClosed, weak_factory_.GetWeakPtr()));

  instance_ptr_->Init(binding_.CreateInterfacePtrAndBind());

  // The container can be considered to have been successfully launched, so
  // restart if the connection goes down without being requested.
  reconnect_ = true;
  VLOG(0) << "ARC ready";
  SetState(State::READY);
}

void ArcBridgeServiceImpl::OnStopped(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  SetStopReason(stop_reason);
  SetState(State::STOPPED);
  VLOG(0) << "ARC stopped";
  if (reconnect_) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting the container again.
    reconnect_ = false;
    VLOG(0) << "ARC reconnecting";
    if (use_delay_before_reconnecting_) {
      // Instead of immediately trying to restart the container, give it some
      // time to finish tearing down in case it is still in the process of
      // stopping.
      base::MessageLoop::current()->task_runner()->PostDelayedTask(
          FROM_HERE, base::Bind(&ArcBridgeServiceImpl::PrerequisitesChanged,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kReconnectDelayInSeconds));
    } else {
      // Restart immediately.
      PrerequisitesChanged();
    }
  }
}

void ArcBridgeServiceImpl::OnChannelClosed() {
  DCHECK(CalledOnValidThread());
  if (state() == State::STOPPED || state() == State::STOPPING) {
    // This will happen when the instance is shut down. Ignore that case.
    return;
  }
  VLOG(1) << "Mojo connection lost";
  instance_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();
  CloseAllChannels();
}

}  // namespace arc
