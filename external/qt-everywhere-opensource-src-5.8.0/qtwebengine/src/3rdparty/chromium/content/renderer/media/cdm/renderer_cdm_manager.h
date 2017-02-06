// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_RENDERER_CDM_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_CDM_RENDERER_CDM_MANAGER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/media/cdm_messages_enums.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
}

namespace content {

class ProxyMediaKeys;

// Class for managing all the CDM objects in the same RenderFrame.
class RendererCdmManager : public RenderFrameObserver {
 public:
  // Constructs a RendererCdmManager object for the |render_frame|.
  explicit RendererCdmManager(RenderFrame* render_frame);
  ~RendererCdmManager() override;

  // RenderFrameObserver overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Encrypted media related methods.
  void InitializeCdm(int cdm_id,
                     uint32_t promise_id,
                     ProxyMediaKeys* media_keys,
                     const std::string& key_system,
                     const GURL& security_origin,
                     bool use_hw_secure_codecs);
  void SetServerCertificate(int cdm_id,
                            uint32_t promise_id,
                            const std::vector<uint8_t>& certificate);
  void CreateSessionAndGenerateRequest(
      int cdm_id,
      uint32_t promise_id,
      media::MediaKeys::SessionType session_type,
      CdmHostMsg_CreateSession_InitDataType init_data_type,
      const std::vector<uint8_t>& init_data);
  void LoadSession(int cdm_id,
                   uint32_t promise_id,
                   media::MediaKeys::SessionType session_type,
                   const std::string& session_id);
  void UpdateSession(int cdm_id,
                     uint32_t promise_id,
                     const std::string& session_id,
                     const std::vector<uint8_t>& response);
  void CloseSession(int cdm_id,
                    uint32_t promise_id,
                    const std::string& session_id);
  void RemoveSession(int cdm_id,
                     uint32_t promise_id,
                     const std::string& session_id);
  void DestroyCdm(int cdm_id);

  // Registers a ProxyMediaKeys object. Returns allocated CDM ID.
  int RegisterMediaKeys(ProxyMediaKeys* media_keys);

  // Unregisters a ProxyMediaKeys object identified by |cdm_id|.
  void UnregisterMediaKeys(int cdm_id);

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Gets the pointer to ProxyMediaKeys given the |cdm_id|.
  ProxyMediaKeys* GetMediaKeys(int cdm_id);

  // Message handlers.
  void OnSessionMessage(int cdm_id,
                        const std::string& session_id,
                        media::MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message,
                        const GURL& legacy_destination_url);
  void OnSessionClosed(int cdm_id, const std::string& session_id);
  void OnLegacySessionError(int cdm_id,
                            const std::string& session_id,
                            media::MediaKeys::Exception exception,
                            uint32_t system_code,
                            const std::string& error_message);
  void OnSessionKeysChange(
      int cdm_id,
      const std::string& session_id,
      bool has_additional_usable_key,
      const std::vector<media::CdmKeyInformation>& key_info_vector);
  void OnSessionExpirationUpdate(int cdm_id,
                                 const std::string& session_id,
                                 const base::Time& new_expiry_time);

  void OnPromiseResolved(int cdm_id, uint32_t promise_id);
  void OnPromiseResolvedWithSession(int cdm_id,
                                    uint32_t promise_id,
                                    const std::string& session_id);
  void OnPromiseRejected(int cdm_id,
                         uint32_t promise_id,
                         media::MediaKeys::Exception exception,
                         uint32_t system_code,
                         const std::string& error_message);

  // CDM ID should be unique per renderer frame.
  // TODO(xhwang): Use uint32_t to prevent undefined overflow behavior.
  int next_cdm_id_;

  // CDM ID to ProxyMediaKeys mapping.
  std::map<int, ProxyMediaKeys*> proxy_media_keys_map_;

  DISALLOW_COPY_AND_ASSIGN(RendererCdmManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_RENDERER_CDM_MANAGER_H_
