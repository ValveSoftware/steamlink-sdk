// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/player_tracker_impl.h"

#include <utility>

#include "base/stl_util.h"

namespace media {

PlayerTrackerImpl::PlayerCallbacks::PlayerCallbacks(
    base::Closure new_key_cb,
    base::Closure cdm_unset_cb)
    : new_key_cb(new_key_cb), cdm_unset_cb(cdm_unset_cb) {
}

PlayerTrackerImpl::PlayerCallbacks::~PlayerCallbacks() {
}

PlayerTrackerImpl::PlayerTrackerImpl() : next_registration_id_(1) {}

PlayerTrackerImpl::~PlayerTrackerImpl() {}

int PlayerTrackerImpl::RegisterPlayer(const base::Closure& new_key_cb,
                                      const base::Closure& cdm_unset_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int registration_id = next_registration_id_++;
  DCHECK(!ContainsKey(player_callbacks_map_, registration_id));
  player_callbacks_map_.insert(std::make_pair(
      registration_id, PlayerCallbacks(new_key_cb, cdm_unset_cb)));
  return registration_id;
}

void PlayerTrackerImpl::UnregisterPlayer(int registration_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ContainsKey(player_callbacks_map_, registration_id))
      << registration_id;
  player_callbacks_map_.erase(registration_id);
}

void PlayerTrackerImpl::NotifyNewKey() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::map<int, PlayerCallbacks>::iterator iter = player_callbacks_map_.begin();
  for (; iter != player_callbacks_map_.end(); ++iter)
    iter->second.new_key_cb.Run();
}

void PlayerTrackerImpl::NotifyCdmUnset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::map<int, PlayerCallbacks>::iterator iter = player_callbacks_map_.begin();
  for (; iter != player_callbacks_map_.end(); ++iter)
    iter->second.cdm_unset_cb.Run();
}

}  // namespace media
