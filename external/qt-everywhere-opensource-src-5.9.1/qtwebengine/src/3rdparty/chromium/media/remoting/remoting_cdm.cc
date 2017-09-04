// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_cdm.h"

#include "media/base/cdm_promise.h"

namespace media {

// TODO(xjz): Merge this with erickung's implementation.
RemotingCdm::RemotingCdm(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb,
    std::unique_ptr<RemotingCdmController> remoting_cdm_controller)
    : remoting_cdm_controller_(std::move(remoting_cdm_controller)),
      remoting_cdm_context_(this) {
  DCHECK(remoting_cdm_controller_);

  // TODO(xjz): Not implemented. Need to merge with erickung's implementation.
}

RemotingCdm::~RemotingCdm() {}

void RemotingCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

void RemotingCdm::CreateSessionAndGenerateRequest(
    SessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

void RemotingCdm::LoadSession(SessionType session_type,
                              const std::string& session_id,
                              std::unique_ptr<NewSessionCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

void RemotingCdm::UpdateSession(const std::string& session_id,
                                const std::vector<uint8_t>& response,
                                std::unique_ptr<SimpleCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

void RemotingCdm::CloseSession(const std::string& session_id,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

void RemotingCdm::RemoveSession(const std::string& session_id,
                                std::unique_ptr<SimpleCdmPromise> promise) {
  // TODO(xjz): Merge with erickung's implementation.
  NOTIMPLEMENTED();
}

CdmContext* RemotingCdm::GetCdmContext() {
  return &remoting_cdm_context_;
}

RemotingSourceImpl* RemotingCdm::GetRemotingSource() {
  return remoting_cdm_controller_->remoting_source();
}

}  // namespace media
