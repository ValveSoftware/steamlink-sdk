// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decryptor.h"
#include "media/cdm/player_tracker_impl.h"

namespace chromecast {
namespace media {

CastCdm::CastCdm(MediaResourceTracker* media_resource_tracker)
    : media_resource_tracker_(media_resource_tracker),
      cast_cdm_context_(new CastCdmContext(this)) {
  DCHECK(media_resource_tracker);
  thread_checker_.DetachFromThread();
}

CastCdm::~CastCdm() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(player_tracker_impl_.get());
  player_tracker_impl_->NotifyCdmUnset();
  media_resource_tracker_->DecrementUsageCount();
}

void CastCdm::Initialize(
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::LegacySessionErrorCB& legacy_session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_resource_tracker_->IncrementUsageCount();
  player_tracker_impl_.reset(new ::media::PlayerTrackerImpl());

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  legacy_session_error_cb_ = legacy_session_error_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;

  InitializeInternal();
}

int CastCdm::RegisterPlayer(const base::Closure& new_key_cb,
                            const base::Closure& cdm_unset_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return player_tracker_impl_->RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void CastCdm::UnregisterPlayer(int registration_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  player_tracker_impl_->UnregisterPlayer(registration_id);
}

::media::CdmContext* CastCdm::GetCdmContext() {
  return cast_cdm_context_.get();
}

void CastCdm::OnSessionMessage(const std::string& session_id,
                               const std::vector<uint8_t>& message,
                               const GURL& destination_url,
                               ::media::MediaKeys::MessageType message_type) {
  session_message_cb_.Run(session_id, message_type, message, destination_url);
}

void CastCdm::OnSessionClosed(const std::string& session_id) {
  session_closed_cb_.Run(session_id);
}

void CastCdm::OnSessionKeysChange(const std::string& session_id,
                                  bool newly_usable_keys,
                                  ::media::CdmKeysInfo keys_info) {
  session_keys_change_cb_.Run(session_id, newly_usable_keys,
                              std::move(keys_info));

  if (newly_usable_keys)
    player_tracker_impl_->NotifyNewKey();
}

void CastCdm::KeyIdAndKeyPairsToInfo(const ::media::KeyIdAndKeyPairs& keys,
                                     ::media::CdmKeysInfo* keys_info) {
  DCHECK(keys_info);
  for (const std::pair<std::string, std::string>& key : keys) {
    std::unique_ptr<::media::CdmKeyInformation> cdm_key_information(
        new ::media::CdmKeyInformation(key.first,
                                       ::media::CdmKeyInformation::USABLE, 0));
    keys_info->push_back(cdm_key_information.release());
  }
}

// A default empty implementation for subclasses that don't need to provide
// any key system specific initialization.
void CastCdm::InitializeInternal() {}

}  // namespace media
}  // namespace chromecast
