// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm/proxy_media_keys.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/renderer/media/cdm/renderer_cdm_manager.h"
#include "media/base/cdm_initialized_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"

namespace content {

void ProxyMediaKeys::Create(
    const std::string& key_system,
    const GURL& security_origin,
    bool use_hw_secure_codecs,
    RendererCdmManager* manager,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const media::CdmCreatedCB& cdm_created_cb) {
  DCHECK(manager);
  scoped_refptr<ProxyMediaKeys> proxy_media_keys(new ProxyMediaKeys(
      manager, session_message_cb, session_closed_cb, legacy_session_error_cb,
      session_keys_change_cb, session_expiration_update_cb));

  // ProxyMediaKeys ownership passed to the promise.
  std::unique_ptr<media::CdmInitializedPromise> promise(
      new media::CdmInitializedPromise(cdm_created_cb, proxy_media_keys));

  proxy_media_keys->InitializeCdm(key_system, security_origin,
                                  use_hw_secure_codecs, std::move(promise));
}

void ProxyMediaKeys::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->SetServerCertificate(cdm_id_, promise_id, certificate);
}

void ProxyMediaKeys::CreateSessionAndGenerateRequest(
    SessionType session_type,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  CdmHostMsg_CreateSession_InitDataType create_session_init_data_type =
      INIT_DATA_TYPE_WEBM;
  switch (init_data_type) {
    case media::EmeInitDataType::CENC:
      create_session_init_data_type = INIT_DATA_TYPE_CENC;
      break;
    case media::EmeInitDataType::WEBM:
      create_session_init_data_type = INIT_DATA_TYPE_WEBM;
      break;
    case media::EmeInitDataType::KEYIDS:
    case media::EmeInitDataType::UNKNOWN:
      DLOG(ERROR) << "Unsupported EME CreateSession init data type";
      promise->reject(NOT_SUPPORTED_ERROR, 0,
                      "Unsupported EME CreateSession init data type");
      return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->CreateSessionAndGenerateRequest(cdm_id_, promise_id, session_type,
                                            create_session_init_data_type,
                                            init_data);
}

void ProxyMediaKeys::LoadSession(
    SessionType session_type,
    const std::string& session_id,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->LoadSession(cdm_id_, promise_id, session_type, session_id);
}

void ProxyMediaKeys::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->UpdateSession(cdm_id_, promise_id, session_id, response);
}

void ProxyMediaKeys::CloseSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->CloseSession(cdm_id_, promise_id, session_id);
}

void ProxyMediaKeys::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->RemoveSession(cdm_id_, promise_id, session_id);
}

media::CdmContext* ProxyMediaKeys::GetCdmContext() {
  return this;
}

media::Decryptor* ProxyMediaKeys::GetDecryptor() {
  return nullptr;
}

int ProxyMediaKeys::GetCdmId() const {
  return cdm_id_;
}

void ProxyMediaKeys::OnSessionMessage(
    const std::string& session_id,
    media::MediaKeys::MessageType message_type,
    const std::vector<uint8_t>& message,
    const GURL& legacy_destination_url) {
  session_message_cb_.Run(session_id, message_type, message,
                          legacy_destination_url);
}

void ProxyMediaKeys::OnSessionClosed(const std::string& session_id) {
  session_closed_cb_.Run(session_id);
}

void ProxyMediaKeys::OnLegacySessionError(const std::string& session_id,
                                          media::MediaKeys::Exception exception,
                                          uint32_t system_code,
                                          const std::string& error_message) {
  legacy_session_error_cb_.Run(session_id, exception, system_code,
                               error_message);
}

void ProxyMediaKeys::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         media::CdmKeysInfo keys_info) {
  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void ProxyMediaKeys::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  session_expiration_update_cb_.Run(session_id, new_expiry_time);
}

void ProxyMediaKeys::OnPromiseResolved(uint32_t promise_id) {
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void ProxyMediaKeys::OnPromiseResolvedWithSession(
    uint32_t promise_id,
    const std::string& session_id) {
  cdm_promise_adapter_.ResolvePromise(promise_id, session_id);
}

void ProxyMediaKeys::OnPromiseRejected(uint32_t promise_id,
                                       media::MediaKeys::Exception exception,
                                       uint32_t system_code,
                                       const std::string& error_message) {
  cdm_promise_adapter_.RejectPromise(promise_id, exception, system_code,
                                     error_message);
}

ProxyMediaKeys::ProxyMediaKeys(
    RendererCdmManager* manager,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb)
    : manager_(manager),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  cdm_id_ = manager->RegisterMediaKeys(this);
}

ProxyMediaKeys::~ProxyMediaKeys() {
  manager_->DestroyCdm(cdm_id_);
  manager_->UnregisterMediaKeys(cdm_id_);
  cdm_promise_adapter_.Clear();
}

void ProxyMediaKeys::InitializeCdm(
    const std::string& key_system,
    const GURL& security_origin,
    bool use_hw_secure_codecs,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  manager_->InitializeCdm(cdm_id_, promise_id, this, key_system,
                          security_origin, use_hw_secure_codecs);
}

}  // namespace content
