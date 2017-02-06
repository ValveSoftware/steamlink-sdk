// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/filtering_network_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_permission.h"

namespace content {

FilteringNetworkManager::FilteringNetworkManager(
    rtc::NetworkManager* network_manager,
    const GURL& requesting_origin,
    media::MediaPermission* media_permission)
    : network_manager_(network_manager),
      media_permission_(media_permission),
      requesting_origin_(requesting_origin),
      weak_ptr_factory_(this) {
  thread_checker_.DetachFromThread();
  set_enumeration_permission(ENUMERATION_BLOCKED);

  // If the feature is not enabled, just return ALLOWED as it's requested.
  if (!media_permission_) {
    started_permission_check_ = true;
    set_enumeration_permission(ENUMERATION_ALLOWED);
    VLOG(3) << "media_permission is not passed, granting permission";
    return;
  }
}

FilteringNetworkManager::~FilteringNetworkManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This helps to catch the case if permission never comes back.
  if (!start_updating_time_.is_null())
    ReportMetrics(false);
}

base::WeakPtr<FilteringNetworkManager> FilteringNetworkManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FilteringNetworkManager::Initialize() {
  rtc::NetworkManagerBase::Initialize();
  if (media_permission_)
    CheckPermission();
}

void FilteringNetworkManager::StartUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(started_permission_check_);

  if (start_updating_time_.is_null()) {
    start_updating_time_ = base::TimeTicks::Now();
    network_manager_->SignalNetworksChanged.connect(
        this, &FilteringNetworkManager::OnNetworksChanged);
  }

  network_manager_->StartUpdating();
  ++start_count_;

  if (sent_first_update_ || should_fire_event())
    FireEventIfStarted();
}

void FilteringNetworkManager::StopUpdating() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_manager_->StopUpdating();
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void FilteringNetworkManager::GetNetworks(NetworkList* networks) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  networks->clear();

  if (enumeration_permission() == ENUMERATION_ALLOWED)
    network_manager_->GetNetworks(networks);

  VLOG(3) << "GetNetworks() returns " << networks->size() << " networks.";
}

bool FilteringNetworkManager::GetDefaultLocalAddress(
    int family,
    rtc::IPAddress* ipaddress) const {
  return network_manager_->GetDefaultLocalAddress(family, ipaddress);
}

void FilteringNetworkManager::CheckPermission() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!started_permission_check_);

  started_permission_check_ = true;
  pending_permission_checks_ = 2;

  // Request for media permission asynchronously.
  media_permission_->HasPermission(
      media::MediaPermission::AUDIO_CAPTURE, requesting_origin_,
      base::Bind(&FilteringNetworkManager::OnPermissionStatus, GetWeakPtr()));
  media_permission_->HasPermission(
      media::MediaPermission::VIDEO_CAPTURE, requesting_origin_,
      base::Bind(&FilteringNetworkManager::OnPermissionStatus, GetWeakPtr()));
}

void FilteringNetworkManager::OnPermissionStatus(bool granted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(pending_permission_checks_, 0);
  VLOG(3) << "OnPermissionStatus: " << granted;

  --pending_permission_checks_;

  if (granted)
    set_enumeration_permission(ENUMERATION_ALLOWED);

  if (should_fire_event())
    FireEventIfStarted();
}

void FilteringNetworkManager::OnNetworksChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  received_networks_changed_since_last_firing_ = true;
  if (enumeration_permission() == ENUMERATION_ALLOWED)
    FireEventIfStarted();
}

void FilteringNetworkManager::ReportMetrics(bool report_start_latency) {
  if (sent_first_update_)
    return;

  if (report_start_latency)
    ReportTimeToUpdateNetworkList(base::TimeTicks::Now() -
                                  start_updating_time_);

  ReportIPPermissionStatus(GetIPPermissionStatus());
}

IPPermissionStatus FilteringNetworkManager::GetIPPermissionStatus() const {
  if (enumeration_permission() == ENUMERATION_ALLOWED) {
    return media_permission_ ? PERMISSION_GRANTED_WITH_CHECKING
                             : PERMISSION_GRANTED_WITHOUT_CHECKING;
  }

  if (!pending_permission_checks_ &&
      enumeration_permission() == ENUMERATION_BLOCKED) {
    return PERMISSION_DENIED;
  }

  return PERMISSION_UNKNOWN;
}

bool FilteringNetworkManager::should_fire_event() const {
  // We should signal network changes when the permisison is denied and we have
  // never fired any SignalNetworksChanged. Or 2) permission is granted and we
  // have received new SignalNetworksChanged from |network_manager_| yet to
  // be broadcast.
  bool permission_blocked_but_never_fired =
      (GetIPPermissionStatus() == PERMISSION_DENIED) && !sent_first_update_;
  bool permission_allowed_pending_networks_update =
      enumeration_permission() == ENUMERATION_ALLOWED &&
      received_networks_changed_since_last_firing_;

  return permission_blocked_but_never_fired ||
         permission_allowed_pending_networks_update;
}

void FilteringNetworkManager::FireEventIfStarted() {
  if (!start_count_)
    return;

  ReportMetrics(true);

  // Post a task to avoid reentrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FilteringNetworkManager::SendNetworksChangedSignal,
                            GetWeakPtr()));

  sent_first_update_ = true;
  received_networks_changed_since_last_firing_ = false;
}

void FilteringNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
