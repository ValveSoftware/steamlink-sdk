// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_PROXY_MEDIA_KEYS_H_
#define CONTENT_RENDERER_MEDIA_CDM_PROXY_MEDIA_KEYS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/media_keys.h"

class GURL;

namespace content {

class RendererCdmManager;

// A MediaKeys proxy that wraps the EME part of RendererCdmManager.
class ProxyMediaKeys : public media::MediaKeys, public media::CdmContext {
 public:
  static void Create(
      const std::string& key_system,
      const GURL& security_origin,
      bool use_hw_secure_codecs,
      RendererCdmManager* manager,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::LegacySessionErrorCB& legacy_session_error_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      const media::CdmCreatedCB& cdm_created_cb);

  // MediaKeys implementation.
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      SessionType session_type,
      const std::string& session_id,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  media::CdmContext* GetCdmContext() override;

  // media::CdmContext implementation.
  media::Decryptor* GetDecryptor() override;
  int GetCdmId() const override;

  // Callbacks.
  void OnSessionMessage(const std::string& session_id,
                        media::MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message,
                        const GURL& legacy_destination_url);
  void OnSessionClosed(const std::string& session_id);
  void OnLegacySessionError(const std::string& session_id,
                            media::MediaKeys::Exception exception,
                            uint32_t system_code,
                            const std::string& error_message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 const base::Time& new_expiry_time);

  void OnPromiseResolved(uint32_t promise_id);
  void OnPromiseResolvedWithSession(uint32_t promise_id,
                                    const std::string& session_id);
  void OnPromiseRejected(uint32_t promise_id,
                         media::MediaKeys::Exception exception,
                         uint32_t system_code,
                         const std::string& error_message);

 private:
  ProxyMediaKeys(
      RendererCdmManager* manager,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::LegacySessionErrorCB& legacy_session_error_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb);

  ~ProxyMediaKeys() override;

  void InitializeCdm(const std::string& key_system,
                     const GURL& security_origin,
                     bool use_hw_secure_codecs,
                     std::unique_ptr<media::SimpleCdmPromise> promise);

  RendererCdmManager* manager_;
  int cdm_id_;

  media::SessionMessageCB session_message_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::LegacySessionErrorCB legacy_session_error_cb_;
  media::SessionKeysChangeCB session_keys_change_cb_;
  media::SessionExpirationUpdateCB session_expiration_update_cb_;

  media::CdmPromiseAdapter cdm_promise_adapter_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMediaKeys);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_PROXY_MEDIA_KEYS_H_
