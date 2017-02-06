// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/empty_network_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/p2p/network_manager_uma.h"

namespace content {

EmptyNetworkManager::EmptyNetworkManager(rtc::NetworkManager* network_manager)
    : network_manager_(network_manager), weak_ptr_factory_(this) {
  thread_checker_.DetachFromThread();
  set_enumeration_permission(ENUMERATION_BLOCKED);
}

EmptyNetworkManager::~EmptyNetworkManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EmptyNetworkManager::StartUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());

  FireEvent();
  updating_started_ = true;
}

void EmptyNetworkManager::StopUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EmptyNetworkManager::GetNetworks(NetworkList* networks) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  networks->clear();
}

bool EmptyNetworkManager::GetDefaultLocalAddress(
    int family,
    rtc::IPAddress* ipaddress) const {
  return network_manager_->GetDefaultLocalAddress(family, ipaddress);
}

void EmptyNetworkManager::FireEvent() {
  if (!updating_started_)
    ReportIPPermissionStatus(PERMISSION_NOT_REQUESTED);

  // Post a task to avoid reentrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmptyNetworkManager::SendNetworksChangedSignal,
                            weak_ptr_factory_.GetWeakPtr()));
}

void EmptyNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
