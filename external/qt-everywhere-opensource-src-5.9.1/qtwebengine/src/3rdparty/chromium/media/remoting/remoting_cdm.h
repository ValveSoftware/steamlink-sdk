// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CDM_H_
#define MEDIA_REMOTING_REMOTING_CDM_H_

#include "media/base/cdm_factory.h"
#include "media/base/media_keys.h"
#include "media/remoting/remoting_cdm_context.h"
#include "media/remoting/remoting_cdm_controller.h"

namespace media {

// TODO(xjz): Merge this with erickung's implementation.
class RemotingCdm : public MediaKeys {
 public:
  RemotingCdm(const std::string& key_system,
              const GURL& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb,
              std::unique_ptr<RemotingCdmController> remoting_cdm_controller);

  // MediaKeys implementations.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) override;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

  RemotingSourceImpl* GetRemotingSource();

 private:
  ~RemotingCdm() override;

  const std::unique_ptr<RemotingCdmController> remoting_cdm_controller_;
  RemotingCdmContext remoting_cdm_context_;

  DISALLOW_COPY_AND_ASSIGN(RemotingCdm);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CDM_H_
